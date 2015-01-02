#include <limits.h>
#include <stdlib.h>

#include <libyass/common.h>
#include <libyass/cpu.h>
#include <libyass/helpers.h>
#include <libyass/list.h>
#include <libyass/scheduler.h>
#include <libyass/task.h>

struct yass_list *stalled;
struct yass_list *ready;
struct yass_list *running;

int **cpu_task;

const char *name()
{
	return "Partitioned EDF";
}

__attribute__ ((__unused__))
static int is_schedulable(struct sched *sched)
{
	int i, id, index, j;

	unsigned long long e;
	unsigned long long h = yass_sched_get_hyperperiod(sched);

	for (i = 0; i < yass_sched_get_ncpus(sched); i++) {
		e = 0;

		for (j = 0; j < yass_sched_get_ntasks(sched); j++) {
			index = cpu_task[i][j];

			if (index != -1) {
				id = yass_task_get_id(sched, index);
				e += yass_task_get_exec_hyperperiod(sched, id);
			}
		}

		if (e > h)
			return 0;
	}

	return 1;
}

static int get_lowest_utilization_cpu(struct sched *sched)
{
	int cpu, i, id, n;
	unsigned long long e, e_min;

	int n_tasks = yass_sched_get_ntasks(sched);

	e_min = ULLONG_MAX;
	cpu = -1;

	for (i = 0; i < yass_sched_get_ncpus(sched); i++) {
		n = 0;
		e = 0;

		while (cpu_task[i][n] != -1 && n != n_tasks - 1) {
			id = yass_task_get_id(sched, cpu_task[i][n]);
			e += yass_task_get_exec_hyperperiod(sched, id);

			n++;
		}

		if (e < e_min) {
			e_min = e;
			cpu = i;
		}
	}

	return cpu;
}

int offline(struct sched *sched)
{
	int i, j;
	int cpu, n;

	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);

	ready = yass_list_new(n_tasks);
	running = yass_list_new(n_tasks);
	stalled = yass_list_new(n_tasks);

	cpu_task = (int **)malloc(n_cpus * n_tasks * sizeof(int));

	for (i = 0; i < n_tasks; i++)
		yass_list_add(stalled, yass_task_get_id(sched, i));

	for (i = 0; i < n_cpus; i++) {
		cpu_task[i] = (int *)malloc(n_tasks * sizeof(int));

		for (j = 0; j < n_tasks; j++)
			cpu_task[i][j] = -1;
	}

	/*
	 * Assign tasks to cpus. Assign tasks according to utilization
	 * in a non-increasing order and assign each task to the cpu
	 * with the lowest utilization.
	 */

	yass_sort_list_int(sched, stalled, yass_task_time_to_deadline);

	for (i = n_tasks - 1; i >= 0; i--) {

		/*
		 * Find the cpu with the lowest utilization
		 */
		cpu = get_lowest_utilization_cpu(sched);

		/* if (cpu == -1) */
		/* 	return -YASS_ERROR_NOT_SCHEDULABLE; */

		n = 0;
		while (cpu_task[cpu][n] != -1 && n != n_tasks - 1)
			n++;

		cpu_task[cpu][n] = i;
	}

	/* if (!is_schedulable(sched)) */
	/* 	return -YASS_ERROR_NOT_SCHEDULABLE; */
	/* else */
	return 0;
}

static void deadline_miss(struct sched *sched)
{
	int cpu, i, id, exec, period;

	int tick = yass_sched_get_tick(sched);

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		id = yass_task_get_id(sched, i);
		exec = yass_task_get_exec(sched, id);
		period = yass_task_get_period(sched, id);

		if (tick % period == 0 && exec != 0) {
			cpu = yass_task_get_cpu(sched, id);

			yass_sched_inc_deadline_misses(sched);

			if (cpu != -1) {
				yass_terminate_task(sched, cpu, id, running, stalled);
			} else {
				yass_task_set_exec(sched, id, 0);

				yass_list_remove(ready, id);
				yass_list_add(stalled, id);
			}

		} else if (tick % period == 0 && !yass_list_present(stalled, id)) {
			if (yass_list_present(ready, id))
				yass_list_remove(ready, id);
			if (yass_list_present(running, id))
				yass_list_remove(running, id);

			yass_list_add(stalled, id);
		}
	}
}

int schedule(struct sched *sched)
{
	int candidate, i, id, index, j;
	int ttd, ttd_candidate;

	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);
	int online = yass_sched_get_online(sched);

	yass_exec_inc(sched);

	if (online)
		yass_check_terminated_tasks(sched, running, stalled, YASS_ONLINE);
	else
		yass_check_terminated_tasks(sched, running, stalled, YASS_OFFLINE);

	/* if (yass_deadline_miss(sched)) */
	/* 	return -YASS_ERROR_NOT_SCHEDULABLE; */

	deadline_miss(sched);

	yass_check_ready_tasks(sched, stalled, ready);

	for (i = 0; i < n_cpus; i++) {
		id = -1;

		candidate = -1;
		ttd_candidate = INT_MAX;

		for (j = 0; j < n_tasks; j++) {
			index = cpu_task[i][j];

			if (index == -1)
				continue;

			id = yass_task_get_id(sched, index);
			ttd = yass_task_time_to_deadline(sched, id);

			if ((yass_list_present(running, id) ||
			     yass_list_present(ready, id)) &&
			    ttd < ttd_candidate) {
				candidate = id;
				ttd_candidate = ttd;
			}
		}

		if (candidate != -1 && yass_cpu_get_task(sched, i) != candidate) {
			if (yass_cpu_is_active(sched, i))
				yass_preempt_task(sched, i, running, ready);

			yass_run_task(sched, i, candidate, ready, running);
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

	for (i = 0; i < yass_sched_get_ncpus(sched); i++)
		free(cpu_task[i]);

	free(cpu_task);

	return 0;
}
