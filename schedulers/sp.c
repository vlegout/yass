#include <limits.h>
#include <stdio.h>
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
	return "SP";
}

static unsigned long long get_cpu_execution(struct sched *sched, int cpu)
{
	int i, id;

	unsigned long long e = 0;

	int n_tasks = yass_sched_get_ntasks(sched);

	for (i = 0; i < n_tasks; i++) {
		if (cpu_task[cpu][i] != -1) {
			id = yass_task_get_id(sched, i);
			e += yass_task_get_exec_hyperperiod(sched, id);
		}
	}

	return e;
}

static int is_schedulable(struct sched *sched)
{
	int i;

	int n_cpus = yass_sched_get_ncpus(sched);

	unsigned long long h = yass_sched_get_hyperperiod(sched);

	for (i = 0; i < n_cpus; i++) {
		if (get_cpu_execution(sched, i) > h)
			return 0;
	}

	return 1;
}

static int get_lowest_utilization_cpu(struct sched *sched)
{
	int cpu, i;
	unsigned long long e, e_min;

	int n_cpus = yass_sched_get_ncpus(sched);

	e_min = ULLONG_MAX;
	cpu = -1;

	for (i = 0; i < n_cpus; i++) {
		e = get_cpu_execution(sched, i);

		if (e < e_min) {
			e_min = e;
			cpu = i;
		}
	}

	return cpu;
}

static int get_th(struct sched *sched, int id)
{
	int period = yass_task_get_period(sched, id);
	int wcet = yass_task_get_wcet(sched, id);

	return period - wcet;
}

/*
 * Sort tasks according to T - C
 */
static void sort_list_desc_tc(struct sched *sched)
{
	int i, j;
	int n, tmp;

	int id1, id2;
	int r1, r2;

	for (i = 0; i < yass_list_n(stalled); i++) {
		id1 = yass_list_get(stalled, i);
		r1 = get_th(sched, id1);

		n = -1;

		for (j = i + 1; j < yass_list_n(stalled); j++) {
			id2 = yass_list_get(stalled, j);

			r2 = get_th(sched, id2);

			if (r2 > r1 || (r2 == r1 && id2 < id1)) {
				n = j;
				r1 = r2;
				id1 = id2;
			}
		}

		if (n != -1) {
			tmp = yass_list_get(stalled, i);
			yass_list_set(stalled, i, yass_list_get(stalled, n));
			yass_list_set(stalled, n, tmp);
		}
	}
}

static int compute_th(struct sched *sched, int **th, int cpu, int index)
{
	int e = 0, i, id, tick;
	int period, wcet;

	tick = INT_MAX;

	for (i = 0; i < index; i++) {
		if (th[cpu][i] != -1) {
			id = yass_list_get(stalled, i);
			period = yass_task_get_period(sched, id);

			if (period < tick)
				tick = period;
		}
	}

	id = yass_list_get(stalled, index);
	period = yass_task_get_period(sched, id);

	if (period < tick)
		tick = period;

	e = 0;

	for (i = 0; i < index; i++) {
		if (th[cpu][i] != -1) {
			id = yass_list_get(stalled, i);
			period = yass_task_get_period(sched, id);
			wcet = yass_task_get_wcet(sched, id);

			e += wcet * ((int)(tick / period));
		}
	}

	id = yass_list_get(stalled, index);
	period = yass_task_get_period(sched, id);
	wcet = yass_task_get_wcet(sched, id);

	e += wcet * ((int)(tick / period));

	return tick - e;
}

__attribute__ ((__unused__))
static void print_table(struct sched *sched, int **table)
{
	int i, id, index, j;

	int n_cpus = yass_sched_get_ncpus(sched);

	for (i = 0; i < n_cpus; i++) {
		printf("cpu %d\n", i);
		for (j = 0; j < yass_list_n(stalled); j++) {
			id = yass_list_get(stalled, j);
			index = yass_task_get_from_id(sched, id);

			if (table[i][index] != -1)
				printf(" %d: %d\n", id, table[i][index]);
		}
		printf("\n");
	}
}

static void populate_ths(struct sched *sched, int **th)
{
	int first, i, id, index, j;

	int n_cpus = yass_sched_get_ncpus(sched);

	for (i = 0; i < n_cpus; i++) {
		first = 1;

		for (j = 0; j < yass_list_n(stalled); j++) {

			id = yass_list_get(stalled, j);
			index = yass_task_get_from_id(sched, id);

			if (cpu_task[i][index] == -1)
				continue;

			if (first) {
				th[i][index] = get_th(sched, id);
				first = 0;
				continue;
			}

			th[i][index] = compute_th(sched, th, i, j);
		}
	}

	/* print_table(sched, th); */
}

static void compute_sleep_states(struct sched *sched, int **th, int **ss)
{
	int i, j, k, selected;
	double c, min, p;

	int n_cpus = yass_sched_get_ncpus(sched);
	int n_states = yass_cpu_get_nstates(sched);

	for (i = 0; i < n_cpus; i++) {
		for (j = 0; j < yass_list_n(stalled); j++) {

			min = INT_MAX;
			selected = -1;

			for (k = n_states - 1; k >= 0; k--) {
				c = yass_cpu_get_state_consumption(sched, k);
				p = yass_cpu_get_state_penalty(sched, k);

				if (th[i][j] * c + p < min) {
					min = th[i][j] * c + p;
					selected = k;
				}
			}

			ss[i][j] = selected;
		}
	}

	/* print_table(sched, ss); */
}

int offline(struct sched *sched)
{
	int i, j;
	int cpu;

	int **th, **ss;

	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);

	ready = yass_list_new(n_tasks);
	running = yass_list_new(n_tasks);
	stalled = yass_list_new(n_tasks);

	cpu_task = (int **)malloc(n_cpus * n_tasks * sizeof(int));

	th = (int **)malloc(n_cpus * n_tasks * sizeof(int));
	ss = (int **)malloc(n_cpus * n_tasks * sizeof(int));

	for (i = 0; i < n_tasks; i++)
		yass_list_add(stalled, yass_task_get_id(sched, i));

	for (i = 0; i < n_cpus; i++) {
		cpu_task[i] = (int *)malloc(n_tasks * sizeof(int));
		th[i] = (int *)malloc(n_tasks * sizeof(int));
		ss[i] = (int *)malloc(n_tasks * sizeof(int));

		for (j = 0; j < n_tasks; j++) {
			cpu_task[i][j] = -1;
			th[i][j] = -1;
			ss[i][j] = -1;
		}
	}

	/*
	 * Do not use nor LLED neither MM because all processors are
	 * identical. Just assign tasks to processors using the worst
	 * fit approach.
	 */

	yass_sort_list_int(sched, stalled, yass_task_time_to_deadline);

	for (i = n_tasks - 1; i >= 0; i--) {

		cpu = get_lowest_utilization_cpu(sched);

		if (cpu == -1)
			return -YASS_ERROR_NOT_SCHEDULABLE;

		cpu_task[cpu][i] = 1;
	}

	sort_list_desc_tc(sched);

	populate_ths(sched, th);

	compute_sleep_states(sched, th, ss);

	for (i = 0; i < n_cpus; i++) {
		free(th[i]);
		free(ss[i]);
	}

	free(th);
	free(ss);

	if (!is_schedulable(sched))
		return -YASS_ERROR_NOT_SCHEDULABLE;
	else
		return 0;
}

int schedule(struct sched *sched)
{
	int candidate, i, id, j;
	int ttd, ttd_candidate;

	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);

	yass_exec_inc(sched);

	yass_check_terminated_tasks(sched, running, stalled, YASS_OFFLINE);

	if (yass_deadline_miss(sched))
		return -YASS_ERROR_NOT_SCHEDULABLE;

	yass_check_ready_tasks(sched, stalled, ready);

	for (i = 0; i < n_cpus; i++) {
		id = -1;

		candidate = -1;
		ttd_candidate = INT_MAX;

		for (j = 0; j < n_tasks; j++) {
			if (cpu_task[i][j] == -1)
				continue;

			id = yass_task_get_id(sched, j);
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
