#include <limits.h>
#include <math.h>
#include <stdio.h>

#include "helpers.h"

#include "common.h"
#include "cpu.h"
#include "log.h"
#include "list.h"
#include "private.h"
#include "task.h"
#include "scheduler.h"

YASS_EXPORT int yass_edf_choose_next_task(struct sched *sched,
					  struct yass_list *q)
{
	int i, id;
	int n = -1;
	int min = INT_MAX;

	for (i = 0; i < yass_list_n(q); i++) {
		id = yass_list_get(q, i);

		if (id != -1 && yass_task_time_to_deadline(sched, id) < min) {
			n = id;
			min = yass_task_time_to_deadline(sched, id);
		}
	}

	if (!yass_cpu_is_active(sched, 0))
		return n;

	id = yass_cpu_get_task(sched, 0);

	if (min < yass_task_time_to_deadline(sched, id))
		return n;
	else
		return -1;
}

YASS_EXPORT void yass_exec_inc(struct sched *sched)
{
	int i, id, speed;

	int n_cpus = yass_sched_get_ncpus(sched);

	for (i = 0; i < n_cpus; i++) {
		if (yass_cpu_is_active(sched, i)) {
			id = yass_cpu_get_task(sched, i);
			speed = yass_cpu_get_speed(sched, i);

			yass_task_exec_inc(sched, id, speed);
		}
	}
}

YASS_EXPORT int yass_check_terminated_tasks(struct sched *sched,
					    struct yass_list *running,
					    struct yass_list *stalled,
					    int execution_class)
{
	int i, id, n, r = 0;
	int n_cpus = yass_sched_get_ncpus(sched);

	double exec, wcet;

	for (i = 0; i < n_cpus; i++) {
		id = yass_cpu_get_task(sched, i);

		if (!yass_cpu_is_active(sched, i))
			continue;

		exec = yass_task_get_exec(sched, id);

		switch (execution_class) {
		case YASS_ONLINE:
			n = yass_task_get_n_exec(sched, id);
			wcet = yass_sched_get_exec_time(sched, id, n);
			break;
		case YASS_OFFLINE:
		default:
			wcet = yass_task_get_wcet(sched, id);
			break;
		}

		if (exec >= wcet) {
			yass_terminate_task(sched, i, id, running, stalled);

			r = 1;
		}
	}

	return r;
}

YASS_EXPORT int yass_deadline_miss(struct sched *sched)
{
	int i, id, exec, period;

	int tick = yass_sched_get_tick(sched);

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		id = yass_task_get_id(sched, i);
		exec = yass_task_get_exec(sched, id);
		period = yass_task_get_period(sched, id);

		if (tick % period == 0 && exec != 0)
			return 1;
	}

	return 0;
}

YASS_EXPORT int yass_check_ready_tasks(struct sched *sched,
				       struct yass_list *stalled,
				       struct yass_list *ready)
{
	int delay, i, id, period, r = 0, tick_delay;
	int tick = yass_sched_get_tick(sched);

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		id = yass_task_get_id(sched, i);
		period = yass_task_get_period(sched, id);
		delay = yass_task_get_delay(sched, id);

		tick_delay = tick - delay;

		if (tick_delay < 0)
			continue;

		if (tick_delay % period == 0 && yass_list_present(stalled, id)) {
			yass_list_remove(stalled, id);
			yass_list_add(ready, id);

			yass_task_set_release(sched, id, tick + period);

			r = 1;
		}
	}

	return r;
}

YASS_EXPORT void yass_terminate_task(struct sched *sched, int cpu, int id,
				     struct yass_list *running,
				     struct yass_list *stalled)
{
	int tick = yass_sched_get_tick(sched);

	yass_task_set_exec(sched, id, 0);

	yass_cpu_remove_task(sched, cpu);

	if (running != NULL)
		yass_list_remove(running, id);

	if (stalled != NULL)
		yass_list_add(stalled, id);

	if (!yass_sched_task_is_idle_task(sched, id)) {
		yass_log_sched(sched,
			       YASS_EVENT_TASK_TERMINATE,
			       yass_sched_get_index(sched), id, tick, cpu, 0);
	}
}

YASS_EXPORT void yass_terminate_task_tick(struct sched *sched, int cpu,
					  int id, int tick,
					  struct yass_list *running,
					  struct yass_list *stalled)
{
	yass_task_set_exec(sched, id, 0);

	yass_list_remove(running, id);
	if (stalled != NULL)
		yass_list_add(stalled, id);

	if (!yass_sched_task_is_idle_task(sched, id)) {
		yass_log_sched(sched,
			       YASS_EVENT_TASK_TERMINATE,
			       yass_sched_get_index(sched), id, tick, cpu, 0);
	}

	yass_cpu_remove_task(sched, cpu);
}

YASS_EXPORT void yass_preempt_task(struct sched *sched, int cpu,
				   struct yass_list *running,
				   struct yass_list *ready)
{
	int tick = yass_sched_get_tick(sched);
	int id = yass_cpu_get_task(sched, cpu);

	yass_warn(yass_cpu_is_active(sched, cpu));

	if (!yass_sched_task_is_idle_task(sched, id)) {
		yass_log_sched(sched,
			       YASS_EVENT_TASK_TERMINATE,
			       yass_sched_get_index(sched), id, tick, cpu, 0);
	}

	yass_list_remove(running, id);
	yass_list_add(ready, id);

	yass_cpu_remove_task(sched, cpu);
}

YASS_EXPORT void yass_run_task(struct sched *sched, int cpu, int id,
			       struct yass_list *ready,
			       struct yass_list *running)
{
	int tick = yass_sched_get_tick(sched);

	if (!yass_sched_task_is_idle_task(sched, id)) {
		yass_log_sched(sched,
			       YASS_EVENT_TASK_RUN,
			       yass_sched_get_index(sched), id, tick, cpu, 0);
	}

	yass_list_remove(ready, id);
	yass_list_add(running, id);

	yass_cpu_set_task(sched, cpu, id);
}

YASS_EXPORT void yass_sort_list_int(struct sched *sched, struct yass_list *q,
				    int (*f) (struct sched *, int))
{
	int i, j;
	int n, tmp;

	int id1, id2;
	int r1, r2;

	if (yass_list_n(q) <= 1)
		return;

	for (i = 0; i < yass_list_n(q); i++) {
		id1 = yass_list_get(q, i);
		r1 = f(sched, id1);

		n = -1;

		for (j = i + 1; j < yass_list_n(q); j++) {
			id2 = yass_list_get(q, j);

			if (id2 == -1)
				continue;

			r2 = f(sched, id2);

			if (r2 < r1 || (r2 == r1 && id2 < id1)) {
				n = j;
				r1 = r2;
				id1 = id2;
			}
		}

		if (n != -1) {
			tmp = yass_list_get(q, i);
			yass_list_set(q, i, yass_list_get(q, n));
			yass_list_set(q, n, tmp);
		}
	}
}

YASS_EXPORT void yass_sort_list_double(struct sched *sched,
				       struct yass_list *q,
				       double (*f) (struct sched *, int))
{
	int i, j;
	int n, tmp;

	int id1, id2;
	double r1, r2;

	if (yass_list_n(q) <= 1)
		return;

	for (i = 0; i < yass_list_n(q); i++) {
		id1 = yass_list_get(q, i);
		r1 = f(sched, id1);

		n = -1;

		for (j = i + 1; j < yass_list_n(q); j++) {
			id2 = yass_list_get(q, j);

			if (id2 == -1)
				continue;

			r2 = f(sched, id2);

			if (r2 < r1 || (r2 == r1 && id2 < id1)) {
				n = j;
				r1 = r2;
				id1 = id2;
			}
		}

		if (n != -1) {
			tmp = yass_list_get(q, i);
			yass_list_set(q, i, yass_list_get(q, n));
			yass_list_set(q, n, tmp);
		}
	}
}

YASS_EXPORT int yass_optimal_schedulability_test(struct sched *sched)
{
	int i, id;

	unsigned long long e = 0;
	unsigned long long h = yass_sched_get_hyperperiod(sched);

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		id = yass_task_get_id(sched, i);
		e += yass_task_get_exec_hyperperiod(sched, id);
	}

	return e <= h * yass_sched_get_ncpus(sched);
}

YASS_EXPORT int yass_rm_schedulability_test(struct sched *sched)
{
	int i, id;
	double u = 0;

	int n_tasks = yass_sched_get_ntasks(sched);

	for (i = 0; i < n_tasks; i++) {
		id = yass_task_get_id(sched, i);
		u += yass_task_get_utilization(sched, id);
	}

	return u <= n_tasks * (pow(2, 1 / (double)n_tasks) - 1);
}

YASS_EXPORT int yass_dpm_schedulability_test(struct sched *sched)
{
	int i, id;

	unsigned long long total = 0;
	unsigned long long h = yass_sched_get_hyperperiod(sched);

	int n_cpus = yass_sched_get_ncpus(sched);

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		id = yass_task_get_id(sched, i);
		total += yass_task_get_exec_hyperperiod(sched, id);
	}

	return (total <= h * n_cpus && total > h * (n_cpus - 1));
}

YASS_EXPORT void yass_print_task_set(struct sched *sched)
{
	int i, id;

	printf("------------------\n");
	printf("  tick %d\n", yass_sched_get_tick(sched));

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		id = yass_task_get_id(sched, i);

		printf("id %d wcet %d exec %lf period %d\n",
		       id,
		       yass_task_get_wcet(sched, id),
		       yass_task_get_exec(sched, id),
		       yass_task_get_period(sched, id));
	}

	printf("------------------\n");
}

/*
 * There must be another way to write this function, it seems so
 * ugly. And it may not work in some situations.
 */
YASS_EXPORT unsigned long long yass_gcd(unsigned long long a,
					unsigned long long b)
{
	float div = (double)b / (double)a;
	unsigned long long d = b / a;

	if (div - d < 0 || div - d > 0)
		return yass_gcd(b - d * a, a);
	else
		return a;
}

YASS_EXPORT unsigned long long yass_lcm(unsigned long long a,
					unsigned long long b)
{
	unsigned long long g = yass_gcd(a, b);

	return a * b / g;
}

YASS_EXPORT int yass_compute_intervals(struct sched *sched, int *I)
{
	unsigned long long t, tick, tmp;
	int n_intervals = 0;

	unsigned long long h = yass_sched_get_hyperperiod(sched);

	tick = yass_sched_get_tick(sched);

	t = tick;

	while (t < tick + h) {
		tmp = yass_sched_get_next_boundary(sched, t);

		I[n_intervals] = tmp - t;
		n_intervals++;

		t = tmp;

		yass_warn(n_intervals < INT_MAX);
	}

	return n_intervals;
}

YASS_EXPORT int yass_get_current_interval(struct sched *sched)
{
	int i, r = -1, t = 0;
	int id, period;

	int n_tasks = yass_sched_get_ntasks(sched);

	int tick = yass_sched_get_tick(sched);;
	int h = yass_sched_get_hyperperiod(sched);

	while (tick >= h)
		tick -= h;

	while (t <= tick) {
		for (i = 0; i < n_tasks; i++) {
			id = yass_task_get_id(sched, i);
			period = yass_task_get_period(sched, id);

			if (t % period == 0) {
				r++;
				break;
			}
		}

		t++;
	}

	return r;
}

YASS_EXPORT int yass_tick_is_interval_boundary(struct sched *sched, int tick)
{
	int i;
	int id, period;

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		id = yass_task_get_id(sched, i);
		period = yass_task_get_period(sched, id);

		if (tick % period == 0)
			return 1;
	}

	return 0;
}
