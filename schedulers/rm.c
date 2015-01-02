#include <libyass/common.h>
#include <libyass/cpu.h>
#include <libyass/helpers.h>
#include <libyass/list.h>
#include <libyass/scheduler.h>
#include <libyass/task.h>

struct yass_list *stalled;
struct yass_list *ready;
struct yass_list *running;

const char *name()
{
	return "RM";
}

int offline(struct sched *sched)
{
	int i, id;

	int n_tasks = yass_sched_get_ntasks(sched);

	if (yass_sched_get_ncpus(sched) != 1)
		return -YASS_ERROR_MORE_THAN_ONE_CPU;

	ready = yass_list_new(n_tasks);
	running = yass_list_new(n_tasks);
	stalled = yass_list_new(n_tasks);

	for (i = 0; i < n_tasks; i++)
		yass_list_add(stalled, yass_task_get_id(sched, i));

	yass_sort_list_int(sched, stalled, yass_task_get_deadline);

	/* Assign priority to tasks */
	for (i = 0; i < yass_list_n(stalled); i++) {
		id = yass_list_get(stalled, i);
		yass_task_set_priority(sched, id, i + 1);
	}

	if (yass_rm_schedulability_test(sched))
		return 0;
	else
		return -YASS_ERROR_NOT_SCHEDULABLE;
}

static int task_compare_priority(struct sched *sched, int id1, int id2)
{
	int p1 = yass_task_get_priority(sched, id1);
	int p2 = yass_task_get_priority(sched, id2);

	return p1 > p2;
}

int schedule(struct sched *sched)
{
	int id, cpu_task;

	yass_exec_inc(sched);

	yass_check_ready_tasks(sched, stalled, ready);

	yass_check_terminated_tasks(sched, running, stalled, YASS_OFFLINE);

	yass_sort_list_int(sched, ready, yass_task_get_priority);

	id = yass_list_get(ready, 0);
	cpu_task = yass_cpu_get_task(sched, 0);

	if (id == -1)
		return 0;

	if (!yass_cpu_is_active(sched, 0) ||
	    task_compare_priority(sched, cpu_task, id)) {

		if (yass_list_get(running, 0) != -1)
			yass_preempt_task(sched, 0, running, ready);

		yass_run_task(sched, 0, yass_list_get(ready, 0), ready,
			      running);
	}

	return 0;
}

int sched_close(struct sched *sched __attribute__ ((__unused__)))
{
	yass_list_free(stalled);
	yass_list_free(ready);
	yass_list_free(running);

	return 0;
}
