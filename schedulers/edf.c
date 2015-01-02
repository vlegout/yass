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
	return "EDF";
}

int offline(struct sched *sched)
{
	int i, n_tasks;

	if (yass_sched_get_ncpus(sched) != 1)
		return -YASS_ERROR_MORE_THAN_ONE_CPU;
	else if (!yass_optimal_schedulability_test(sched))
		return -YASS_ERROR_NOT_SCHEDULABLE;

	n_tasks = yass_sched_get_ntasks(sched);

	ready = yass_list_new(n_tasks);
	running = yass_list_new(n_tasks);
	stalled = yass_list_new(n_tasks);

	for (i = 0; i < n_tasks; i++)
		yass_list_add(stalled, yass_task_get_id(sched, i));

	return 0;
}

int schedule(struct sched *sched)
{
	int id, cpu = 0;

	yass_exec_inc(sched);

	yass_check_terminated_tasks(sched, running, stalled, YASS_OFFLINE);

	if (yass_deadline_miss(sched))
		return -YASS_ERROR_NOT_SCHEDULABLE;

	yass_check_ready_tasks(sched, stalled, ready);

	if (yass_list_n(ready) > 0) {
		yass_sort_list_int(sched, ready, yass_task_time_to_deadline);

		id = yass_edf_choose_next_task(sched, ready);

		if (id != -1) {
			if (yass_cpu_is_active(sched, cpu))
				yass_preempt_task(sched, cpu, running, ready);

			yass_run_task(sched, cpu, id, ready, running);
		}
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
