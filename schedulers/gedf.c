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
	return "Global EDF";
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
	int cpu, i, id;

	int n_cpus = yass_sched_get_ncpus(sched);
	int online = yass_sched_get_online(sched);

	struct yass_list *candidate = yass_list_new(YASS_MAX_N_TASKS);

	yass_exec_inc(sched);

	if (online)
		yass_check_terminated_tasks(sched, running, stalled, YASS_ONLINE);
	else
		yass_check_terminated_tasks(sched, running, stalled, YASS_OFFLINE);

	deadline_miss(sched);

	yass_check_ready_tasks(sched, stalled, ready);

	/*
	 * Add in the candidate list all tasks ready to be scheduled,
	 * i.e. tasks from the ready and running list.
	 */
	for (i = 0; i < n_cpus; i++) {
		if (yass_cpu_is_active(sched, i))
			yass_list_add(candidate, yass_cpu_get_task(sched, i));
	}
	for (i = 0; i < yass_list_n(ready); i++) {
		if (yass_list_get(ready, i) != -1)
			yass_list_add(candidate, yass_list_get(ready, i));
	}

	yass_sort_list_int(sched, candidate, yass_task_time_to_deadline);

	/*
	 * Remove from the candidate list tasks which are not going to
	 * be scheduled.
	 */
	while (yass_list_n(candidate) > n_cpus) {
		for (i = n_cpus; i < n_cpus + yass_list_n(candidate) - 1; i++) {
			yass_list_remove(candidate,
					 yass_list_get(candidate, i));
			break;
		}
	}
	yass_warn(n_cpus >= yass_list_n(candidate));

	/*
	 * Preempt running tasks
	 */
	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		id = yass_task_get_id(sched, i);
		cpu = yass_task_get_cpu(sched, id);

		if (yass_list_present(running, id)
		    && !yass_list_present(candidate, id)) {
			yass_preempt_task(sched, cpu, running, ready);
		}
	}

	/*
	 * Keep already scheduled tasks on the same processor
	 */
	for (i = 0; i < n_cpus; i++) {
		id = yass_cpu_get_task(sched, i);

		if (id != -1 && yass_list_present(candidate, id))
			yass_list_remove(candidate, id);
	}

	/*
	 * Assign a processor to the remaining tasks in the candidate
	 * list.
	 */
	for (i = 0; i < n_cpus; i++) {
		id = yass_list_get(candidate, 0);

		if (!yass_cpu_is_active(sched, i) && id != -1) {
			yass_run_task(sched, i, id, ready, running);

			yass_list_remove(candidate, id);
		}
	}

	yass_warn(yass_list_n(candidate) == 0);

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
