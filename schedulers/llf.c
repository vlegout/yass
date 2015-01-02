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
	return "LLF";
}

int offline(struct sched *sched)
{
	int i;

	int n_tasks = yass_sched_get_ntasks(sched);

	ready = yass_list_new(n_tasks);
	running = yass_list_new(n_tasks);
	stalled = yass_list_new(n_tasks);

	for (i = 0; i < n_tasks; i++)
		yass_list_add(stalled, yass_task_get_id(sched, i));

	return 0;
}

int schedule(struct sched *sched)
{
	int i, id;

	int n_cpus = yass_sched_get_ncpus(sched);

	struct yass_list *candidate = yass_list_new(YASS_MAX_N_TASKS);

	yass_exec_inc(sched);

	yass_check_terminated_tasks(sched, running, stalled, YASS_OFFLINE);

	if (yass_deadline_miss(sched))
		return -YASS_ERROR_NOT_SCHEDULABLE;

	yass_check_ready_tasks(sched, stalled, ready);

	for (i = 0; i < yass_list_n(running); i++)
		yass_list_add(candidate, yass_list_get(running, i));
	for (i = 0; i < yass_list_n(ready); i++)
		yass_list_add(candidate, yass_list_get(ready, i));

	yass_sort_list_double(sched, candidate, yass_task_get_laxity);

	while (yass_list_n(candidate) > n_cpus) {
		for (i = n_cpus; i < n_cpus + yass_list_n(candidate) - 1; i++) {
			id = yass_list_get(candidate, i);
			yass_list_remove(candidate, id);
			break;
		}
	}

	if (n_cpus < yass_list_n(candidate))
		return -YASS_ERROR_NOT_SCHEDULABLE;

	for (i = 0; i < n_cpus; i++) {
		id = yass_cpu_get_task(sched, i);

		if (id != -1 && !yass_list_present(candidate, id))
			yass_preempt_task(sched, i, running, ready);

		if (id != -1 && yass_list_present(candidate, id))
			yass_list_remove(candidate, id);
	}

	for (i = 0; i < n_cpus; i++) {
		id = yass_list_get(candidate, 0);

		if (!yass_cpu_is_active(sched, i) && id != -1) {
			yass_run_task(sched, i, id, ready, running);

			yass_list_remove(candidate, id);
		}
	}

	if (yass_list_n(candidate) != 0)
		return -YASS_ERROR_NOT_SCHEDULABLE;

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
