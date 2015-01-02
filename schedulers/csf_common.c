
#include <math.h>

#include <libyass/common.h>
#include <libyass/cpu.h>
#include <libyass/helpers.h>
#include <libyass/list.h>
#include <libyass/scheduler.h>
#include <libyass/task.h>

#include "csf.h"

double compute_sbf(struct server *servers, double time, int vm)
{
	double sbf;

	int k = ceil((time - ((double)servers[vm].period - (double)servers[vm].budget)) / (double)servers[vm].period);

	if (k < 1)
		k = 1;

	if (time >= ((k + 1) * (double)servers[vm].period - 2 * (double)servers[vm].budget) &&
	    time <= ((k + 1) * (double)servers[vm].period - (double)servers[vm].budget)) {
		sbf = time - (k + 1) * ((double)servers[vm].period - (double)servers[vm].budget);
	} else {
		sbf = (k - 1) * (double)servers[vm].budget;
	}

	return sbf;
}

int is_sched_edf(struct server *servers, struct sched *sched, int vm, double prob)
{
	int i, id, j, k, l;
	double dbf, sbf, time;

	unsigned long long hyperperiod = yass_sched_get_hyperperiod_vm(sched, vm);

	double wcet, stddev;

	int n_tasks = yass_sched_get_ntasks(sched);

	int sorted[YASS_MAX_N_TASKS * YASS_MAX_N_TASK_EXECUTION];

	int **exec_times = yass_sched_get_exec_times(sched);

	for (i = 0; i < YASS_MAX_N_TASKS * YASS_MAX_N_TASK_EXECUTION; i++)
		sorted[i] = -1;

        if (prob != 0 && prob < 0) {
	        for (i = 0; i < n_tasks; i++) {
		        for (j = 0; j < YASS_MAX_N_TASK_EXECUTION; j++){
			        k = 0;

			        while (sorted[k] > exec_times[i][j] && k < YASS_MAX_N_TASKS * YASS_MAX_N_TASK_EXECUTION)
				        k++;

			        for (l = YASS_MAX_N_TASKS * YASS_MAX_N_TASK_EXECUTION - 1; l > k; l--)
				        sorted[l] = sorted[l - 1];

			        sorted[k] = exec_times[i][j];
		        }
	        }
        }

	for (time = 0; time <= hyperperiod; time++) {

		dbf = 0;

		for (i = 0; i < n_tasks; i++) {
			id = yass_task_get_id(sched, i);

			wcet = yass_task_get_wcet(sched, id);

			if (prob != 0 && prob > 0) {
				stddev = wcet / 10;

				wcet /= 2;
				wcet += sqrt((prob * (stddev * stddev) / (1 - prob)));
			} else if (prob != 0 && prob < 0) {
				wcet = sorted[(int)ceil(prob * n_tasks * YASS_MAX_N_TASK_EXECUTION)];
			}

			if (yass_task_get_vm(sched, id) == vm)
				dbf += floor(time / (double)yass_task_get_period(sched, id)) * wcet;
		}

		sbf = compute_sbf(servers, time, vm);

		if (dbf > sbf)
			return 0;
	}

	return 1;
}

int get_lowest_utilization_cpu(struct sched *sched, struct server *servers, int **cpu_task)
{
	int cpu, i, n, s;
	double u, u_min = INT_MAX;

	int n_servers = yass_sched_get_nvms(sched);

	cpu = -1;

	for (i = 0; i < yass_sched_get_ncpus(sched); i++) {
		n = 0;
		u  = 0;

		while (cpu_task[i][n] != -1 && n != n_servers - 1) {
			s = cpu_task[i][n];

			u += (double)servers[s].budget / (double)servers[s].period;

			n++;
		}

		if (u < u_min) {
			u_min = u;
			cpu = i;
		}
	}

	return cpu;
}

void exec_inc(struct sched *sched, int *server_running, struct server *servers)
{
	int i, id, speed;

	int n_cpus = yass_sched_get_ncpus(sched);

	for (i = 0; i < n_cpus; i++) {
		if (yass_cpu_is_active(sched, i)) {
			id = yass_cpu_get_task(sched, i);
			speed = yass_cpu_get_speed(sched, i);

			yass_task_exec_inc(sched, id, speed);
		}

		if (server_running[i] != -1)
			servers[server_running[i]].exec++;
	}
}

void check_ready_tasks(struct sched *sched,
		       struct yass_list *stalled,
		       struct yass_list *ready,
		       struct yass_list *stalled_tasks,
		       struct yass_list *ready_tasks,
		       struct server *servers)
{
	int i, id, period;

	int tick = yass_sched_get_tick(sched);

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		id = yass_task_get_id(sched, i);
		period = yass_task_get_period(sched, id);

		if (tick % period == 0) {
			yass_list_remove(stalled_tasks, id);
			yass_list_add(ready_tasks, id);

			yass_task_set_release(sched, id, tick + period);
		}
	}

	for (i = 0; i < yass_sched_get_nvms(sched); i++) {
		period = servers[i].period;

		if (tick % period == 0 && yass_list_present(stalled, i)) {
			yass_list_remove(stalled, i);
			yass_list_add(ready, i);

			servers[i].exec = 0;
			servers[i].deadline = tick + period;

			servers[i].budget = servers[i].budget_init;
		}
	}
}

int check_terminated_tasks(struct sched *sched,
			   struct yass_list *running,
			   struct yass_list *stalled,
			   struct yass_list *running_tasks,
			   struct yass_list *stalled_tasks,
			   struct yass_list *ready_tasks,
			   struct server *servers,
			   int *server_running)
{
	int i, id, vm;
	int r = -1;

	int n_cpus = yass_sched_get_ncpus(sched);

	double exec, wcet;

	for (i = 0; i < n_cpus; i++) {

		/* Check tasks */

		vm = server_running[i];

		id = yass_cpu_get_task(sched, i);

		if (yass_cpu_is_active(sched, i)) {

			exec = yass_task_get_exec(sched, id);
			wcet = yass_task_get_aet(sched, id);

			if (exec >= wcet) {
				yass_terminate_task(sched, i, id, running_tasks,
						    stalled_tasks);

				if (r == -1)
					r = vm;
			}
		}

		/* Check servers */

		if (vm != - 1 && (servers[vm].exec >= servers[vm].budget || yass_sched_get_tick(sched) == servers[vm].deadline)) {
			yass_list_remove(running, vm);
			yass_list_add(stalled, vm);

			server_running[i] = -1;

			if (id != -1 && yass_list_present(running_tasks, id))
				yass_preempt_task(sched, i, running_tasks,
						  ready_tasks);

			if (r == vm)
				r = -1;
		}
	}

	return r;
}

int server_time_to_deadline(struct sched *sched, int index, struct server *servers)
{
	int tick = yass_sched_get_tick(sched);
	int deadline = servers[index].period;
	int next_release = servers[index].deadline;
	int period = servers[index].period;

	return next_release - period + deadline - tick;
}

void deadline_miss(struct sched *sched,
		   struct yass_list *running,
		   struct yass_list *stalled,
		   struct yass_list *running_tasks,
		   struct yass_list *stalled_tasks,
		   struct yass_list *ready_tasks,
		   int *server_running)
{
	int cpu, i, id, exec, period, vm;

	int tick = yass_sched_get_tick(sched);

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		id = yass_task_get_id(sched, i);
		exec = yass_task_get_exec(sched, id);
		period = yass_task_get_period(sched, id);

		if (tick % period == 0 && exec != 0) {
			cpu = yass_task_get_cpu(sched, id);

			yass_sched_inc_deadline_misses(sched);

			if (cpu != -1) {
				yass_terminate_task(sched, cpu, id, running_tasks, stalled_tasks);

				vm = yass_task_get_vm(sched, id);

				yass_list_remove(running, vm);
				yass_list_add(stalled, vm);

				server_running[cpu] = -1;

			} else {
				yass_task_set_exec(sched, id, 0);

				yass_list_remove(ready_tasks, id);
				yass_list_add(stalled_tasks, id);
			}

		} else if (tick % period == 0 && !yass_list_present(stalled_tasks, id)) {
			if (yass_list_present(ready_tasks, id))
				yass_list_remove(ready_tasks, id);
			if (yass_list_present(running_tasks, id))
				yass_list_remove(running_tasks, id);

			yass_list_add(stalled_tasks, id);
		}
	}
}
