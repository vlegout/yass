#include <stdlib.h>

#include <libyass/cpu.h>
#include <libyass/helpers.h>
#include <libyass/list.h>
#include <libyass/scheduler.h>
#include <libyass/task.h>

#include "csf.h"

struct yass_list *stalled;
struct yass_list *ready;
struct yass_list *running;

struct yass_list *stalled_tasks;
struct yass_list *ready_tasks;
struct yass_list *running_tasks;

int **cpu_task;

int *server_running;

struct server *servers;

const char *name()
{
	return "CSFD-50";
}

int offline(struct sched *sched)
{
	int b, cpu, i, j, n, schedulable;

	int n_cpus = yass_sched_get_ncpus(sched);
	int n_servers = yass_sched_get_nvms(sched);
	int n_tasks = yass_sched_get_ntasks(sched);

	int period = PERIOD;

	ready = yass_list_new(n_servers);
	running = yass_list_new(n_servers);
	stalled = yass_list_new(n_servers);

	ready_tasks = yass_list_new(n_tasks);
	running_tasks = yass_list_new(n_tasks);
	stalled_tasks = yass_list_new(n_tasks);

	for (i = 0; i < n_tasks; i++)
		yass_list_add(stalled_tasks, yass_task_get_id(sched, i));

	cpu_task = (int **)malloc(n_cpus * n_servers * sizeof(int));
	server_running  = (int *)malloc(n_cpus * sizeof(int));
	servers  = (struct server *)malloc(n_servers * sizeof(struct server));

	for (i = 0; i < n_cpus; i++) {
		cpu_task[i] = (int *)malloc(n_servers * sizeof(int));

		for (j = 0; j < n_servers; j++)
			cpu_task[i][j] = -1;

		server_running[i] = -1;
	}

	for (i = 0; i < n_servers; i++) {

		servers[i].period = period;

		for (b = INC; b < period; b += INC) {
			servers[i].budget = b;

			schedulable = is_sched_edf(servers, sched, i, 0.5);

			if (schedulable) {
				servers[i].budget = (int)b;
				break;
			}
		}

		servers[i].budget_init = servers[i].budget;
	}

	for (i = 0; i < n_servers; i++)
		yass_list_add(stalled, i);

	/*
	 * Assign servers to cpus. Assign servers according to
	 * utilization in a non-increasing order and assign each
	 * server to the cpu with the lowest utilization.
	 */

	for (i = n_servers - 1; i >= 0; i--) {

		/*
		 * Find the cpu with the lowest utilization
		 */
		cpu = get_lowest_utilization_cpu(sched, servers, cpu_task);

		n = 0;
		while (cpu_task[cpu][n] != -1 && n != n_servers - 1)
			n++;

		cpu_task[cpu][n] = i;
	}

	return 0;
}

static void use_slack_time(struct sched *sched, int vm)
{
	int i, id;
	int early = INT_MAX;
	int choice = -1;
	int slack = 0;

	int n_tasks = yass_sched_get_ntasks(sched);
	int n_servers = yass_sched_get_nvms(sched);

	for (i = 0; i < n_tasks; i++) {
		id = yass_task_get_id(sched, i);

		if (yass_task_get_vm(sched, id) != vm)
			continue;

		if (yass_task_get_next_release(sched, id) < servers[vm].deadline ||
		    yass_list_present(running_tasks, id) ||
		    yass_list_present(ready_tasks, id)) {
			slack = -1;
			break;
		}
	}

	if (slack == -1)
		return;

	/* Compute how much slack time we have */

	slack = servers[vm].budget - servers[vm].exec;

	if (slack < 0)
		return;

	for (i = 0; i < n_servers; i++) {
		if (i == vm)
			continue;

		if (servers[i].deadline < early) {
			choice = i;
			early = servers[i].deadline;
		}
	}

	if (choice == vm || choice == -1)
		return;

	servers[choice].budget += slack;
	servers[vm].exec = servers[vm].budget;
}

int schedule(struct sched *sched)
{
	int i, id, index, j, slack_vm;
	int ttd, ttd_candidate;
	int candidate, candidate_task, candidate_ttd;

	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);
	int n_servers = yass_sched_get_nvms(sched);

	exec_inc(sched, server_running, servers);

	slack_vm = check_terminated_tasks(sched, running, stalled,
					  running_tasks, stalled_tasks,
					  ready_tasks, servers, server_running);

	if (slack_vm != -1)
		use_slack_time(sched, slack_vm);

	deadline_miss(sched, running, stalled,
		      running_tasks, stalled_tasks,
		      ready_tasks, server_running);

	check_ready_tasks(sched, stalled, ready, stalled_tasks,
			  ready_tasks, servers);

	for (i = 0; i < n_cpus; i++) {
		id = -1;

		candidate = -1;
		ttd_candidate = INT_MAX;

		for (j = 0; j < n_servers; j++) {
			index = cpu_task[i][j];

			if (index == -1)
				continue;

			ttd = server_time_to_deadline(sched, index, servers);

			if ((yass_list_present(running, index) ||
			     yass_list_present(ready, index)) &&
			    ttd < ttd_candidate) {
				candidate = index;
				ttd_candidate = ttd;
			}
		}

		if (candidate != -1 && server_running[i] != candidate) {
			if (server_running[i] != -1) {
				yass_list_remove(running, server_running[i]);
				yass_list_add(ready, server_running[i]);
			}

			server_running[i] = candidate;
			yass_list_add(running, candidate);
			yass_list_remove(ready, candidate);

		}

		if (candidate == -1)
			continue;

		candidate_task = -1;
		candidate_ttd = INT_MAX;

		for (j = 0; j < n_tasks; j++) {
			id = yass_task_get_id(sched, j);

			if (yass_task_get_vm(sched, id) != candidate)
				continue;

			ttd = yass_task_time_to_deadline(sched, id);

			if ((yass_list_present(running_tasks, id) ||
			     yass_list_present(ready_tasks, id)) &&
			    ttd < candidate_ttd) {
				candidate_task = id;
				candidate_ttd = ttd;
			}
		}

		if (candidate_task != -1 && yass_cpu_get_task(sched, i) != candidate_task) {
			if (yass_cpu_is_active(sched, i))
				yass_preempt_task(sched, i, running_tasks, ready_tasks);

			yass_run_task(sched, i, candidate_task, ready_tasks, running_tasks);
		}

	}

	return 0;
}

int sched_close(struct sched *sched __attribute__ ((__unused__)))
{
	int i;

	yass_list_free(stalled);
	yass_list_free(ready);
	yass_list_free(running);

	yass_list_free(stalled_tasks);
	yass_list_free(ready_tasks);
	yass_list_free(running_tasks);

	free(server_running);
	free(servers);

	for (i = 0; i < yass_sched_get_ncpus(sched); i++)
		free(cpu_task[i]);

	free(cpu_task);

	return 0;
}
