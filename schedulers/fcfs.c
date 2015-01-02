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
	return "FCFS";
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

	yass_exec_inc(sched);

	yass_check_ready_tasks(sched, stalled, ready);

	yass_check_terminated_tasks(sched, running, stalled, YASS_OFFLINE);

	for (i = 0; i < n_cpus; i++) {
		if (yass_cpu_get_task(sched, i) == -1) {
			if (yass_list_n(ready) > 0) {
				id = yass_list_get(ready, 0);
				yass_run_task(sched, i, id, ready, running);
			}
		}
	}

	return 0;
}

int sched_close(struct sched *sched __attribute__ ((unused)))
{
	yass_list_free(stalled);
	yass_list_free(ready);
	yass_list_free(running);

	return 0;
}
