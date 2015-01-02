
#include <math.h>
#include <stdio.h>

#include <libyass/cpu.h>
#include <libyass/helpers.h>
#include <libyass/scheduler.h>

#include "lpdpm.hpp"

int get_ntasks(struct sched *sched)
{
	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);

	if (!strcmp(name(), "IZL"))
		return n_tasks;

	if (!strcmp(name(), "LPDPM3"))
		return n_tasks + n_cpus + 1;

	return n_tasks + 1;
}

int get_task_id(struct sched *sched, int index)
{
	int n_tasks = yass_sched_get_ntasks(sched);

	if (strcmp(name(), "IZL") && index >= n_tasks - 1)
		return YASS_IDLE_TASK_ID;

	return yass_task_get_id(sched, index);
}

static int get_task_cpu(struct sched *sched, int id, int index, int *idle_b,
			int *idle_e)
{
	int n_tasks = yass_sched_get_ntasks(sched);
	int cpu = yass_task_get_cpu(sched, id);

	if (!strcmp(name(), "IZL"))
		return cpu;

	if (index == n_tasks - 1 && *idle_b)
		cpu = 0;
	else if (index == n_tasks - 1)
		cpu = -1;

	if (index == n_tasks && *idle_e)
		cpu = 0;
	else if (index == n_tasks)
		cpu = -1;

	return cpu;
}

static void set_active(struct sched *sched, int index, int n, int *idle_b,
		       int *idle_e)
{
	int n_tasks = yass_sched_get_ntasks(sched);

	if (!idle_b || !idle_e)
		return;

	if (index == n_tasks - 1)
		*idle_b = n;
	else if (index == n_tasks)
		*idle_e = n;
}

static void yield_rem_weight(struct sched *sched, double *rem, double rem_w,
			     double rem_ticks)
{
	int n_tasks = yass_sched_get_ntasks(sched);

	rem[n_tasks] += rem_w;

	if (rem[n_tasks - 1] + rem[n_tasks] > rem_ticks)
		rem[n_tasks] = rem_ticks - rem[n_tasks - 1];
}

static int is_runnable(struct sched *sched, int index, double *rem)
{
	int aet;
	double exec;

	int id = get_task_id(sched, index);

	if (yass_sched_task_is_idle_task(sched, id))
		return rem[index] > EPSILON;

	exec = yass_task_get_exec(sched, id);
	aet = yass_task_get_aet(sched, id);

	return rem[index] > EPSILON && exec < aet - EPSILON;
}

static void terminate_tasks(struct sched *sched, int *idle_b, int *idle_e,
			    double *rem, double rem_ticks, double *rem_nc)
{
	int aet, cpu, criticality, id, i;
	double exec;

	int n_tasks = yass_sched_get_ntasks(sched);

	for (i = 0; i < get_ntasks(sched); i++) {

		id = get_task_id(sched, i);
		cpu = yass_task_get_cpu(sched, id);

		if (id != YASS_IDLE_TASK_ID && cpu == -1) {
			continue;
		} else if (id == YASS_IDLE_TASK_ID && i == n_tasks - 1
			   && (!*idle_b || cpu == -1)) {
			continue;
		} else if (id == YASS_IDLE_TASK_ID && i == n_tasks
			   && (!*idle_e || cpu == -1)) {
			continue;
		}

		set_active(sched, i, 1, idle_b, idle_e);

		aet = yass_task_get_aet(sched, id);
		criticality = yass_task_get_criticality(sched, id);
		exec = yass_task_get_exec(sched, id);

		/*
		 * For critical tasks and tau'
		 */
		if (((!sched_is_mc(name())) || criticality == 1) &&
		    (rem[i] <= EPSILON || (!yass_sched_task_is_idle_task(sched, id) && exec > aet - EPSILON))) {

			if (rem[i] > EPSILON)
				yield_rem_weight(sched, rem, rem[i], rem_ticks);

			rem[i] = 0;

			set_active(sched, i, 0, idle_b, idle_e);

			yass_preempt_task(sched, cpu, NULL, NULL);
		}

		/*
		 * Non critical tasks
		 */
		if (rem_nc != NULL && criticality == 0 &&
		    ((rem[i] <= EPSILON && rem_nc[i] <= EPSILON) || exec > aet - EPSILON)) {

			if (rem[i] > EPSILON)
				yield_rem_weight(sched, rem, rem[i], rem_ticks);

			rem[i] = 0;

			set_active(sched, i, 0, idle_b, idle_e);

			yass_preempt_task(sched, cpu, NULL, NULL);
		}
	}
}

static int *izl_update_priorities(struct sched *sched, int *priorities,
				  double *rem)
{
	int i, j, k;

	int n_tasks = yass_sched_get_ntasks(sched);

	for (i = 0; i < n_tasks; i++)
		priorities[i] = -1;

	for (i = 0; i < n_tasks; i++) {
		j = 0;

		while (priorities[j] != -1 && rem[priorities[j]] <= rem[i]
		       && j < n_tasks - 1)
			j++;

		if (j != n_tasks - 1) {
			k = n_tasks - 1;

			while (k >= j && k > 0) {
				priorities[k] = priorities[k - 1];
				k--;
			}
		}

		priorities[j] = i;
	}

	return priorities;
}

static int *update_priorities(struct sched *sched, int *priorities, double *rem)
{
	int i, j, k;

	int n_tasks = yass_sched_get_ntasks(sched);

	if (!strcmp(name(), "IZL"))
		return izl_update_priorities(sched, priorities, rem);

	for (i = 0; i < n_tasks + 1; i++)
		priorities[i] = -1;

	for (i = 0; i < n_tasks - 1; i++) {
		j = 0;

		while (priorities[j] != -1 && rem[priorities[j]] >= rem[i]
		       && j < n_tasks - 1)
			j++;

		if (j != n_tasks - 2) {
			k = n_tasks - 2;

			while (k >= j && k > 0) {
				priorities[k] = priorities[k - 1];
				k--;
			}
		}

		priorities[j] = i;
	}

	for (i = n_tasks - 1; i >= 1; i--)
		priorities[i] = priorities[i - 1];

	priorities[0] = n_tasks - 1;

	priorities[n_tasks] = n_tasks;

	return priorities;
}

/*
 * Schedule tasks according using priorities
 */
static int schedule_tasks(struct sched *sched, int *priorities, double *rem,
			  int *idle_b, int *idle_e, double *rem_nc)
{
	int cpu, criticality, i, id, index, j;

	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);

	for (i = 0; i < n_cpus; i++) {

		id = yass_cpu_get_task(sched, i);

		if (id >= 0) {
			criticality = yass_task_get_criticality(sched, id);
			index = yass_task_get_from_id(sched, id);

			/*
			 * Means the running task is not beeing ran
			 * using its rem_nc.
			 */
			if (criticality == 0 && rem[index] > EPSILON)
				criticality = 1;
		} else {
			criticality = 0;
		}

		if (id >= 0 && criticality == 1)
			continue;

		j = 0;

		do {
			index = priorities[j];
			id = get_task_id(sched, index);
			cpu = get_task_cpu(sched, id, index, idle_b, idle_e);

			j++;

		} while (j < get_ntasks(sched) && (!is_runnable(sched, index, rem) || cpu != -1));

		if (cpus_always_on(sched) &&
		    (!is_runnable(sched, index, rem) || cpu != -1)) {
			return -YASS_ERROR_NOT_SCHEDULABLE;
		}

		if (index == n_tasks && *idle_b)
			continue;

		if (cpu == -1 && rem[index] > EPSILON) {
			if (yass_cpu_get_task(sched, i) >= 0)
				yass_preempt_task(sched, i, NULL, NULL);

			yass_run_task(sched, i, id, NULL, NULL);
			set_active(sched, index, 1, idle_b, idle_e);
		}
	}

	if (cpus_always_on(sched)) {
		for (i = 0; i < n_cpus; i++)
			if (!yass_cpu_is_active(sched, i))
				return -YASS_ERROR_NOT_SCHEDULABLE;
	}

	return 0;
}

/*
 * Schedule tasks with a negative laxity. Preempt active tasks if
 * necessary.
 */
static int schedule_negative_laxity_tasks(struct sched *sched, double rem_ticks,
					  int *priorities, double *rem,
					  int *idle_b, int *idle_e)
{
	int cpu, cpu2, i, id, id2, index, j;

	for (i = 0; i < get_ntasks(sched); i++) {

		id = get_task_id(sched, i);
		cpu2 = get_task_cpu(sched, id, i, idle_b, idle_e);

		/*
		 * Only consider tasks with a negative laxity and not
		 * yet scheduled
		 */
		if (rem[i] < rem_ticks - EPSILON || cpu2 >= 0)
			continue;

		if (!is_runnable(sched, i, rem))
			continue;

		/*
		 * Find an available cpu. At this stage, every cpu
		 * should be associated with a task, thus run through
		 * tasks by increasing priority and preempt the
		 * scheduled task with the lowest priority.
		 */

		j = get_ntasks(sched) - 1;

		do {
			index = priorities[j];

			if (index == i) {
				j--;
				continue;
			}

			id2 = get_task_id(sched, index);
			cpu = get_task_cpu(sched, id2, index, idle_b, idle_e);

			j--;
		} while (j >= 0 &&
			 !(rem[index] < rem_ticks - EPSILON && cpu != -1));

		if (id2 == -1 || rem[index] >= rem_ticks + EPSILON || cpu == -1)
			return -YASS_ERROR_NOT_SCHEDULABLE;

		yass_preempt_task(sched, cpu, NULL, NULL);
		yass_run_task(sched, cpu, id, NULL, NULL);

		set_active(sched, i, 1, idle_b, idle_e);
	}

	return 0;
}

static int choose_non_critical_task(struct sched *sched, double *rem_nc,
				    double *rem)
{
	int cpu, i, id, choice = -1, criticality, min = INT_MAX, ttd;
	int aet, exec;

	int n_tasks = yass_sched_get_ntasks(sched);

	for (i = 0; i < n_tasks; i++) {
		id = yass_task_get_id(sched, id);
		criticality = yass_task_get_criticality(sched, id);
		ttd = yass_task_time_to_deadline(sched, id);
		cpu = yass_task_get_cpu(sched, id);

		if (criticality == 1 || cpu != -1)
			continue;

		aet = yass_task_get_aet(sched, id);
		exec = yass_task_get_exec(sched, id);

		if (ttd < min && exec < aet && rem[i] < EPSILON
		    && rem_nc[i] > EPSILON) {
			choice = id;
			min = ttd;
		}
	}

	return choice;
}

static int schedule_non_critical(struct sched *sched, int *idle_b, int *idle_e,
				 int *old_cpu, double *rem_nc, double *rem)
{
	int i, id, index, j;

	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);

	if (!sched_is_mc(name()))
		return 0;

	for (i = 0; i < n_cpus; i++) {
		if (yass_cpu_is_active(sched, i))
			continue;

		index = -1;

		for (j = 0; j < n_tasks; j++) {
			if (old_cpu[i] == i)
				index = i;
		}

		id = choose_non_critical_task(sched, rem_nc, rem);

		if (id != -1 && (index == -1 || index >= n_tasks - 1)) {
			index = yass_task_get_from_id(sched, id);

			yass_run_task(sched, i, id, NULL, NULL);
			set_active(sched, index, 1, idle_b, idle_e);
		}
	}

	return 0;
}

static double get_rem_nc(struct sched *sched, int id, double **w, int index,
			 int interval)
{
	int length = 0;
	double total_task = 0;

	int period = yass_task_get_period(sched, id);
	int wcet = yass_task_get_wcet(sched, id);

	int t = yass_sched_get_tick(sched);

	do {
		t++;
		length++;

		if (yass_tick_is_interval_boundary(sched, t)) {
			total_task += w[index][interval] * length;
			interval++;
			length = 0;
		}

	} while (t % period != 0);

	return wcet - total_task;
}

/*
 * Called at teach interval boundary to fill rem.
 */
static int lp_reset(struct sched *sched, double **w, double *rem,
		    double rem_ticks, int idle_was_active, double *rem_nc)
{
	int i, id;
	int aet, criticality, period;
	double exec;

	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);
	int tick = yass_sched_get_tick(sched);

	int interval = yass_get_current_interval(sched);

	double total = 0;

	/*
	 * Schedulability checks.
	 */
	for (i = 0; i < get_ntasks(sched); i++) {

		if (tick == 0)
			continue;

		if (rem[i] > EPSILON || rem[i] < -EPSILON)
			return -YASS_ERROR_NOT_SCHEDULABLE;
	}

	for (i = 0; i < get_ntasks(sched); i++) {

		rem[i] = w[i][interval] * rem_ticks;

		total += rem[i];

		id = get_task_id(sched, i);

		criticality = yass_task_get_criticality(sched, id);
		period = yass_task_get_period(sched, id);

		/*
		 * New job, need to recompute rem_nc. Only for
		 * non-critical tasks.
		 */
		if (rem_nc != NULL && tick % period == 0 && criticality == 0)
			rem_nc[i] = get_rem_nc(sched, id, w, i, interval);

		if (!yass_sched_task_is_idle_task(sched, id)) {
			aet = yass_task_get_aet(sched, id);
			exec = yass_task_get_exec(sched, id);

			if (exec >= aet - EPSILON && rem[i] > EPSILON) {
				yield_rem_weight(sched, rem, rem[i], rem_ticks);
				rem[i] = 0;
			}
		}

	}

	/*
	 * For LPDPM, if the processor is not active at the beginning
	 * of the interval, all the weight of b is given to e.
	 */
	if (!strcmp(name(), "LPDPM1") && !idle_was_active) {
		if (rem[n_tasks - 1] > EPSILON && rem[n_tasks] < 1) {
			rem[n_tasks] = rem[n_tasks - 1];
			rem[n_tasks - 1] = 0;
		}
	}

	/*
	 * I don't remember why I wrote this so comment. Yes, this is
	 * quite sad ...
	 */
	/*
	double tmp;
	if (strcmp(name(), "LPDPM1") && idle_was_active && rem[n_tasks] > 0.99) {
		tmp = rem[n_tasks];
		rem[n_tasks] = rem[n_tasks - 1];
		rem[n_tasks - 1] = tmp;
	}
	*/

	if (total > rem_ticks * n_cpus + EPSILON)
		return -YASS_ERROR_NOT_SCHEDULABLE;

	return 0;
}

static void check_idle_cpu(struct sched *sched, int idle_cpu)
{
	int i, id, id2;

	int n_cpus = yass_sched_get_ncpus(sched);

	for (i = 0; i < n_cpus; i++) {
		id = yass_cpu_get_task(sched, i);

		if (id >= YASS_IDLE_TASK_ID && i != idle_cpu) {

			id2 = yass_cpu_get_task(sched, idle_cpu);

			yass_preempt_task(sched, i, NULL, NULL);

			if (id2 != -1) {
				yass_preempt_task(sched, idle_cpu, NULL, NULL);
				yass_run_task(sched, i, id2, NULL, NULL);
			}

			yass_run_task(sched, idle_cpu, id, NULL, NULL);
		}
	}
}

static int increase_inc(struct sched *sched, double inc, double rem_tick,
			double *rem, int *idle_b, int *idle_e, double *rem_nc)
{
	int cpu, i, id;

	double total = 0;

	for (i = 0; i < get_ntasks(sched); i++) {

		total += rem[i];

		id = get_task_id(sched, i);
		cpu = get_task_cpu(sched, id, i, idle_b, idle_e);

		if (cpu != -1) {
			yass_task_exec_inc(sched, id, inc);

			if (rem[i] > EPSILON)
				rem[i] -= inc;
			else if (rem_nc != NULL)
				rem_nc[i] -= inc;
		}

		if (rem[i] > rem_tick - inc + EPSILON)
			return -YASS_ERROR_NOT_SCHEDULABLE;
	}

	if (total > rem_tick * yass_sched_get_ncpus(sched) + EPSILON)
		return -YASS_ERROR_NOT_SCHEDULABLE;

	return 0;
}

static double compute_inc(struct sched *sched, double rem_ticks, double time,
			  double *rem, int *idle_b, int *idle_e, double *rem_nc)
{
	int cpu, criticality, i, id;
	double min = 1 - time;

	for (i = 0; i < get_ntasks(sched); i++) {

		id = get_task_id(sched, i);
		criticality = yass_task_get_criticality(sched, id);

		cpu = get_task_cpu(sched, id, i, idle_b, idle_e);

		/*
		 * Execution finishes before the end of the interval
		 */
		if (cpu != -1 && rem[i] < min && rem[i] > EPSILON)
			min = rem[i];

		/*
		 * Laxity becomes null before the end of the interval
		 */
		if (cpu == -1 && rem_ticks - rem[i] + EPSILON < min && rem_ticks - rem[i] > EPSILON)
			min = rem_ticks - rem[i];

		/*
		 * For non critical tasks running using their rem_nc
		 */
		if (rem_nc != NULL && cpu != -1 && criticality == 0 &&
		    rem[i] < EPSILON && rem_nc[i] > EPSILON && rem_nc[i] < min) {
			min = rem_nc[i];
		}
	}

	return min;
}

static int keep_tasks_same_cpu(struct sched *sched, int *old_cpu, int *idle_b,
			       int *idle_e)
{
	int cpu, i, id, id2;

	for (i = 0; i < get_ntasks(sched); i++) {
		id = get_task_id(sched, i);

		cpu = get_task_cpu(sched, id, i, idle_b, idle_e);

		if (cpu >= 0 && old_cpu[i] >= 0 && cpu != old_cpu[i]) {

			id2 = yass_cpu_get_task(sched, old_cpu[i]);

			if (id2 < YASS_IDLE_TASK_ID) {
				yass_preempt_task(sched, cpu, NULL, NULL);

				if (id2 != -1) {
					yass_preempt_task(sched, old_cpu[i],
							  NULL, NULL);
					yass_run_task(sched, cpu, id2, NULL,
						      NULL);
				}

				yass_run_task(sched, old_cpu[i], id, NULL,
					      NULL);
			}
		}
	}

	return 0;
}

static int set_old_cpu(struct sched *sched, int *old_cpu, int *idle_b,
		       int *idle_e)
{
	int i, id, r = 0;

	for (i = 0; i < get_ntasks(sched); i++) {
		id = get_task_id(sched, i);
		old_cpu[i] = get_task_cpu(sched, id, i, idle_b, idle_e);

		if (old_cpu[i] >= 0 && yass_sched_task_is_idle_task(sched, id))
			r = 1;
	}

	return r;
}

int lpdpm_schedule(struct sched *sched, double *rem, double **w,
		   int *priorities, int *idle_b, int *idle_e, int *idle_cpu,
		   double *rem_nc)
{
	int all_active = 1, check = 0, i, id, idle_was_active, period, r;

	double inc, time = 0;

	int n_cpus = yass_sched_get_ncpus(sched);
	int tick = yass_sched_get_tick(sched);

	double rem_ticks = yass_sched_get_next_boundary(sched, tick) - tick;

	int *old_cpu = (int *)malloc(get_ntasks(sched) * sizeof(int));

	while (time < 1 - EPSILON) {

		if (time > EPSILON)
			yass_sched_update_idle(sched);

		idle_was_active = set_old_cpu(sched, old_cpu, idle_b, idle_e);

		terminate_tasks(sched, idle_b, idle_e, rem, rem_ticks, rem_nc);

		for (i = 0; i < get_ntasks(sched); i++) {

			id = get_task_id(sched, i);

			period = yass_task_get_period(sched, id);

			if (tick % period == 0)
				yass_task_set_exec(sched, id, 0);

			if (rem[i] > rem_ticks + EPSILON)
				return -YASS_ERROR_NOT_SCHEDULABLE;

			if (tick != 0 && rem[i] < -EPSILON)
				return -YASS_ERROR_NOT_SCHEDULABLE;
		}

		if (time == 0 && yass_tick_is_interval_boundary(sched, tick)) {

			r = lp_reset(sched, w, rem, rem_ticks, idle_was_active,
				     rem_nc);

			if (r)
				return r;

			update_priorities(sched, priorities, rem);
		}

		r = schedule_tasks(sched, priorities, rem, idle_b, idle_e,
				   rem_nc);

		if (r)
			return r;

		r = schedule_negative_laxity_tasks(sched, rem_ticks, priorities,
						   rem, idle_b, idle_e);

		if (r)
			return r;

		if (strcmp(name(), "IZL"))
			check_idle_cpu(sched, *idle_cpu);

		schedule_non_critical(sched, idle_b, idle_e, old_cpu, rem_nc,
				      rem);

		r = keep_tasks_same_cpu(sched, old_cpu, idle_b, idle_e);

		if (r)
			return r;

		inc = compute_inc(sched, rem_ticks, time, rem,
				  idle_b, idle_e, rem_nc);

		if (time + inc > 1)
			inc = 1 - time;

		r = increase_inc(sched, inc, rem_ticks, rem,
				 idle_b, idle_e, rem_nc);

		if (r)
			return r;

		for (i = 0; i < n_cpus; i++) {
			id = yass_cpu_get_task(sched, i);

			if (cpus_always_on(sched) && id <= 0)
				return -YASS_ERROR_NOT_SCHEDULABLE;

			if (id <= 0 || id == YASS_IDLE_TASK_ID)
				all_active = 0;
		}

		if (idle_cpu && idle_was_active && all_active) {
			(*idle_cpu)++;
			*idle_cpu = *idle_cpu % n_cpus;
		}

		time += inc;
		rem_ticks -= inc;

		if (check++ > 1000)
			return -YASS_ERROR_NOT_SCHEDULABLE;
	}

	free(old_cpu);

	return 0;
}
