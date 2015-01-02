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

double **assignment;

const char *name()
{
	return "U-EDF";
}

static double double_min(double a, double b)
{
	if (a < b)
		return a;
	else
		return b;
}

static double double_max(double a, double b)
{
	if (a > b)
		return a;
	else
		return b;
}

int offline(struct sched *sched)
{
	int i, id, j;

	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);

	if (!yass_dpm_schedulability_test(sched))
		return -YASS_ERROR_NOT_SCHEDULABLE;

	ready = yass_list_new(n_tasks);
	running = yass_list_new(n_tasks);
	stalled = yass_list_new(n_tasks);

	assignment = (double **)calloc(n_tasks * n_cpus, sizeof(double));

	for (i = 0; i < n_tasks; i++) {
		id = yass_task_get_id(sched, i);
		yass_list_add(stalled, id);

		assignment[i] = (double *)calloc(n_cpus, sizeof(double));

		for (j = 0; j < n_cpus; j++)
			assignment[i][j] = 0;
	}

	return 0;
}

static double get_u(struct sched *sched, struct yass_list *candidate, int n,
		    int cpu)
{
	int i, id;
	double u1 = 0, u2 = 0;

	for (i = 0; i < n; i++) {
		id = yass_list_get(candidate, i);

		u1 += yass_task_get_utilization(sched, id);
		u2 += yass_task_get_utilization(sched, id);
	}

	id = yass_list_get(candidate, n);

	u1 += yass_task_get_utilization(sched, id);

	u1 -= cpu;
	u2 -= cpu;

	u1 = double_min(1, double_max(0, u1));
	u2 = double_min(1, double_max(0, u2));

	return u1 - u2;
}

static double get_cpu_free_time(struct sched *sched, int cpu, double rem, int n,
				struct yass_list *candidate)
{
	int d2, i, j;
	double previous = 0, u;

	int id = yass_list_get(candidate, n);
	int index = yass_task_get_from_id(sched, id);
	int d1 = yass_task_get_next_release(sched, id);

	int tick = yass_sched_get_tick(sched);

	double free_time = d1 - tick;

	/*
	 * Assignments of this task on previous cpus
	 */
	for (j = 0; j < cpu; j++)
		previous += assignment[index][j];

	free_time -= previous;

	/*
	 * Assignments of previous tasks in this cpus
	 */
	for (i = 0; i < n; i++) {
		id = yass_list_get(candidate, i);
		index = yass_task_get_from_id(sched, id);
		d2 = yass_task_get_next_release(sched, id);

		free_time -= assignment[index][cpu];

		u = get_u(sched, candidate, i, cpu);

		free_time -= (d1 - d2) * u;
	}

	return double_min(rem - previous, free_time);
}

/*
 * Assign a cpu to each task sorted by increasing time to deadline
 */
static int assign(struct sched *sched, struct yass_list *candidate)
{
	int cpu, i, id, index, j, wcet;
	double exec, rem, t;

	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);

	for (i = 0; i < n_tasks; i++) {
		for (j = 0; j < n_cpus; j++)
			assignment[i][j] = 0;
	}

	for (i = 0; i < yass_list_n(candidate); i++) {
		id = yass_list_get(candidate, i);

		index = yass_task_get_from_id(sched, id);
		exec = yass_task_get_exec(sched, id);
		wcet = yass_task_get_wcet(sched, id);
		rem = wcet - exec;

		if (yass_list_present(stalled, id))
			rem = 0;

		cpu = 0;

		while (rem > 0) {
			t = get_cpu_free_time(sched, cpu, rem, i, candidate);

			if (t < -0.0001)
				return 0;

			assignment[index][cpu] += t;

			cpu++;

			if (cpu >= n_cpus)
				break;
		}
	}

	return 1;
}

static void terminate_tasks(struct sched *sched)
{
	int i, id, index;

	int wcet;
	double exec;

	int n_cpus = yass_sched_get_ncpus(sched);

	for (i = 0; i < n_cpus; i++) {

		if (!yass_cpu_is_active(sched, i))
			continue;

		id = yass_cpu_get_task(sched, i);
		index = yass_task_get_from_id(sched, id);
		exec = yass_task_get_exec(sched, id);
		wcet = yass_task_get_wcet(sched, id);

		if (assignment[index][i] <= 0.0001) {
			if (exec >= wcet - 0.0001) {
				yass_terminate_task(sched, i, id, running,
						    stalled);
			} else {
				yass_terminate_task(sched, i, id, running,
						    ready);

				/*
				 * Needed because yass_terminate_task set the
				 * execution time to 0, what we do not want.
				 */
				yass_task_set_exec(sched, id, exec);
			}
		}
	}
}

static int check_ready_tasks(struct sched *sched, struct yass_list *stalled,
			     struct yass_list *ready)
{
	int i, id, period;
	int tick = yass_sched_get_tick(sched);

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		id = yass_task_get_id(sched, i);
		period = yass_task_get_period(sched, id);

		if (tick % period == 0 && yass_list_present(stalled, id)) {
			yass_list_remove(stalled, id);
			yass_list_add(ready, id);

			yass_task_set_release(sched, id, tick + period);
		} else if (tick % period == 0) {
			return 0;
		}
	}

	return 1;
}

__attribute__ ((__unused__))
static void print_assignemnts(struct sched *sched)
{
	int i, j;

	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);
	int tick = yass_sched_get_tick(sched);

	printf("== tick %d ==\n", tick);

	for (i = 0; i < n_tasks; i++) {
		printf("%d:", yass_task_get_id(sched, i));
		for (j = 0; j < n_cpus; j++)
			printf(" %lf", assignment[i][j]);
		printf("\n");
	}

	printf("==\n");
}

static int choose_task(struct sched *sched, struct yass_list *candidate,
		       int cpu)
{
	int current_cpu, i, id, index;

	for (i = 0; i < yass_list_n(candidate); i++) {
		id = yass_list_get(candidate, i);
		index = yass_task_get_from_id(sched, id);

		current_cpu = yass_task_get_cpu(sched, id);

		if (yass_list_present(stalled, id))
			continue;

		/*
		 * We have found an available task if cpu time
		 * is assigned for the task to this cpu and
		 * the task is not already scheduled on a
		 * previous cpu.
		 */

		if (assignment[index][cpu] > 0.0001
		    && !(current_cpu >= 0 && current_cpu < cpu)) {
			return id;
		}
	}

	return -1;
}

static void execute(struct sched *sched, double time)
{
	int i, id, index;

	int n_cpus = yass_sched_get_ncpus(sched);

	for (i = 0; i < n_cpus; i++) {
		if (yass_cpu_is_active(sched, i)) {

			id = yass_cpu_get_task(sched, i);
			index = yass_task_get_from_id(sched, id);

			yass_task_exec_inc(sched, id, time);
			assignment[index][i] -= time;
		}
	}
}

int schedule(struct sched *sched)
{
	int cpu, i, id, index;
	double min, time;

	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);
	int tick = yass_sched_get_tick(sched);

	struct yass_list *candidate;

	/*
	 * Run tasks. As tasks can have an execution time shorter than
	 * one, run each cpu for the lowest period of time
	 */

	time = 0;

	while (time < 1 - 0.0001) {

		if (time > 0)
			yass_sched_update_idle(sched);

		terminate_tasks(sched);

		if (time == 0 && !check_ready_tasks(sched, stalled, ready))
			return -YASS_ERROR_NOT_SCHEDULABLE;

		/*
		 * Add all tasks to the candidate list
		 */
		candidate = yass_list_new(YASS_MAX_N_TASKS);

		for (i = 0; i < n_tasks; i++) {
			id = yass_task_get_id(sched, i);
			yass_list_add(candidate, id);
		}

		yass_sort_list_int(sched, candidate,
				   yass_task_time_to_deadline);

		/*
		 * Run the algorithm on interval boundaries, i.e. when a task
		 * is released
		 */
		if (time == 0 && yass_tick_is_interval_boundary(sched, tick)) {
			if (!assign(sched, candidate))
				return -YASS_ERROR_NOT_SCHEDULABLE;
		}

		for (i = 0; i < n_cpus; i++) {

			id = choose_task(sched, candidate, i);

			if (id == -1) {
				if (yass_cpu_is_active(sched, i))
					yass_preempt_task(sched, i, running,
							  ready);

			} else if (yass_cpu_get_task(sched, i) != id) {
				if (yass_cpu_is_active(sched, i))
					yass_preempt_task(sched, i, running,
							  ready);

				cpu = yass_task_get_cpu(sched, id);

				/*
				 * If the task is already scheduled on a
				 * subsequent cpu, preempt it.
				 */
				if (cpu != -1)
					yass_preempt_task(sched, cpu, running,
							  ready);

				yass_run_task(sched, i, id, ready, running);
			}
		}

		min = YASS_MAX_PERIOD;

		for (i = 0; i < n_cpus; i++) {
			id = yass_cpu_get_task(sched, i);

			if (id != -1) {
				index = yass_task_get_from_id(sched, id);

				if (assignment[index][i] < min)
					min = assignment[index][i];
			}
		}

		if (time + min > 1 - 0.0001)
			execute(sched, 1 - time);
		else
			execute(sched, min);

		time += min;

		yass_list_free(candidate);
	}

	return 0;
}

int sched_close(struct sched *sched __attribute__ ((__unused__)))
{
	int i;

	yass_list_free(stalled);
	yass_list_free(ready);
	yass_list_free(running);

	for (i = 0; i < yass_sched_get_ntasks(sched); i++)
		free(assignment[i]);

	free(assignment);

	return 0;
}
