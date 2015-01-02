#include <math.h>

#include <libyass/common.h>
#include <libyass/cpu.h>
#include <libyass/helpers.h>
#include <libyass/list.h>
#include <libyass/scheduler.h>
#include <libyass/task.h>

#define SLOT_SIZE 1

struct yass_list *stalled;
struct yass_list *ready;
struct yass_list *running;

const char *name()
{
	return "PF";
}

int offline(struct sched *sched)
{
	int i, id;

	int n_tasks = yass_sched_get_ntasks(sched);

	if (!yass_dpm_schedulability_test(sched))
		return -YASS_ERROR_NOT_SCHEDULABLE;

	ready = yass_list_new(n_tasks);
	running = yass_list_new(n_tasks);
	stalled = yass_list_new(n_tasks);

	for (i = 0; i < n_tasks; i++) {
		id = yass_task_get_id(sched, i);

		yass_list_add(stalled, id);
	}

	return 0;
}

static int get_subtask(struct sched *sched, int id)
{
	return (yass_task_get_exec(sched, id) / SLOT_SIZE) + 1;
}

static int get_deadline(struct sched *sched, int id)
{
	int next_release = yass_task_get_next_release(sched, id);
	int period = yass_task_get_period(sched, id);
	int subtask = get_subtask(sched, id);
	int wcet = yass_task_get_wcet(sched, id);

	int tmp = ceil(subtask * period / wcet);

	return next_release - period + (tmp * SLOT_SIZE);
}

/*
 * The number of slots by which Ti's window overlaps Ti+1's window.
 */
static int get_successor_bit(struct sched *sched, int id)
{
	int b;

	int subtask = get_subtask(sched, id);

	int period = yass_task_get_period(sched, id);
	int wcet = yass_task_get_wcet(sched, id);

	int last_subtask = yass_task_get_wcet(sched, id) / SLOT_SIZE;

	if (subtask == last_subtask)
		return 0;

	b = get_deadline(sched, id);

	b -= floor(subtask * period / wcet) * SLOT_SIZE;

	return b;
}

/*
 * Return 1 if T2 has higher priority than T1, and 0 otherwise.
 */
static int has_priority(int d1, int d2, int b1, int b2)
{
	if (d2 < d1)
		return 1;

	if (d2 == d1 && b2 > b1)
		return 1;

	/*
	 * TODO: This is not exactly like in anderson05, there should
	 * be a recursive call to has_priority for the next subtasks.
	 */
	if (d2 == d1 && b2 == b1)
		return 1;

	return 0;
}

/*
 * Sort the q list according to PF.
 */
static void sort_list(struct sched *sched, struct yass_list *q)
{
	int i, j, id, n, tmp;
	int d1, d2;
	int b1, b2;

	for (i = 0; i < yass_list_n(q); i++) {
		id = yass_list_get(q, i);

		d1 = get_deadline(sched, id);
		b1 = get_successor_bit(sched, id);

		n = -1;

		for (j = i + 1; j < yass_list_n(q); j++) {
			id = yass_list_get(q, j);

			d2 = get_deadline(sched, id);
			b2 = get_successor_bit(sched, id);

			if (id != -1 && has_priority(d1, d2, b1, b2)) {
				n = j;
				d1 = d2;
				b1 = b2;
			}
		}

		if (n != -1) {
			tmp = yass_list_get(q, i);
			yass_list_set(q, i, yass_list_get(q, n));
			yass_list_set(q, n, tmp);
		}
	}
}

int schedule(struct sched *sched)
{
	int i, id;
	double lag;

	int n_cpus = yass_sched_get_ncpus(sched);
	int tick = yass_sched_get_tick(sched);

	struct yass_list *candidate = yass_list_new(YASS_MAX_N_TASKS);

	yass_exec_inc(sched);

	yass_check_terminated_tasks(sched, running, stalled, YASS_OFFLINE);

	/*
	 * If we are inside a slot, execute current tasks
	 */
	if (tick % SLOT_SIZE != 0)
		goto out;

	yass_check_ready_tasks(sched, stalled, ready);

	/*
	 * Put in the candidate list tasks from the running and ready
	 * lists
	 */
	for (i = 0; i < yass_list_n(running); i++)
		yass_list_add(candidate, yass_list_get(running, i));
	for (i = 0; i < yass_list_n(ready); i++)
		yass_list_add(candidate, yass_list_get(ready, i));

	sort_list(sched, candidate);

	for (i = 0; i < yass_list_n(candidate); i++) {
		id = yass_list_get(candidate, i);
		lag = yass_task_get_lag(sched, id);

		/*
		 * This is necessary if we do not want to miss a
		 * deadline.
		 */
		if (lag > SLOT_SIZE)
			return -YASS_ERROR_NOT_SCHEDULABLE;

		/*
		 * Enable only if global utilization is equal to the
		 * number of processors. Else, a task could be
		 * scheduled early and its lag might become lower than
		 * SLOT_SIZE.
		 */
		/* if (lag <= -SLOT_SIZE) */
		/*      return -YASS_ERROR_NOT_SCHEDULABLE; */
	}

	/*
	 * Need to keep at most n_cpus tasks in the candidate list
	 */
	while (yass_list_n(candidate) > n_cpus) {
		id = yass_list_get(candidate, n_cpus);
		yass_list_remove(candidate, id);
	}

	if (yass_list_n(candidate) > n_cpus)
		return -YASS_ERROR_NOT_SCHEDULABLE;

	/*
	 * Preempt running tasks not in the candidate list
	 */
	for (i = 0; i < n_cpus; i++) {
		id = yass_cpu_get_task(sched, i);

		if (id != -1 && !yass_list_present(candidate, id))
			yass_preempt_task(sched, i, running, ready);
	}

	/*
	 * Keep active tasks on their processor
	 */
	for (i = 0; i < n_cpus; i++) {
		id = yass_cpu_get_task(sched, i);

		if (id != -1 && yass_list_present(candidate, id))
			yass_list_remove(candidate, id);
	}

	/*
	 * Schedule the remaining tasks in the candidate list. Those
	 * tasks were not active, thus the processor is chosen
	 * randomly.
	 */
	for (i = 0; i < n_cpus; i++) {
		id = yass_list_get(candidate, 0);

		if (id != -1 && !yass_cpu_is_active(sched, i)) {
			yass_run_task(sched, i, id, ready, running);
			yass_list_remove(candidate, id);
		}
	}

	if (yass_list_n(candidate) != 0)
		return -YASS_ERROR_NOT_SCHEDULABLE;

 out:
	yass_list_free(candidate);

	return 0;
}

int sched_close(struct sched *sched __attribute__ ((__unused__)))
{
	yass_list_free(stalled);
	yass_list_free(ready);
	yass_list_free(running);

	return 0;
}
