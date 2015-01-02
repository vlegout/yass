#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scheduler.h"

#include "cpu.h"
#include "helpers.h"
#include "private.h"

YASS_EXPORT int yass_sched_get_index(struct sched *sched)
{
	return sched->index;
}

YASS_EXPORT FILE *yass_sched_get_fp(struct sched * sched)
{
	return sched->fp;
}

YASS_EXPORT void yass_sched_set_fp(struct sched *sched, FILE * fp)
{
	sched->fp = fp;
}

YASS_EXPORT int yass_sched_get_verbose(struct sched *sched)
{
	return sched->verbose;
}

YASS_EXPORT int yass_sched_get_debug(struct sched *sched)
{
	return sched->debug;
}

YASS_EXPORT int yass_sched_get_online(struct sched *sched)
{
	return sched->online;
}

YASS_EXPORT int yass_sched_get_tick(struct sched *sched)
{
	return sched->tick;
}

YASS_EXPORT void yass_sched_tick_inc(struct sched *sched)
{
	sched->tick++;
}

YASS_EXPORT int yass_sched_get_stat(struct sched *sched)
{
	return sched->stat;
}

YASS_EXPORT void yass_sched_set_stat(struct sched *sched, int stat)
{
	sched->stat = stat;
}

/*
 * If choice == 0, return the number of deadline misses. Else, return
 * the percentage of deadline misses from the total number of jobs
 * from non-critical tasks.
 *
 * FIXME: only works if the task set is scheduled for one hyperperiod.
 */
YASS_EXPORT double yass_sched_get_deadline_misses(struct sched *sched, int choice)
{
	int i, n_jobs = 0;
	int id, period;

	int hyperperiod = yass_sched_get_hyperperiod(sched);
	int n_tasks = yass_sched_get_ntasks(sched);

	if (choice == 0)
		return sched->deadline_misses;

	for (i = 0; i < n_tasks; i++) {
		id = yass_task_get_id(sched, i);
		period = yass_task_get_period(sched, id);

		n_jobs += hyperperiod / period;
	}

	if (n_jobs == 0)
		return 0;

	return sched->deadline_misses / ((double) n_jobs);
}

YASS_EXPORT int yass_sched_check_deadline_misses(struct sched *sched)
{
	int i, id, deadline, n, period, aet;
	double exec;

	int r = 0;

	int n_tasks = yass_sched_get_ntasks(sched);
	int tick = yass_sched_get_tick(sched);

	for (i = 0; i < n_tasks; i++) {
		id = yass_task_get_id(sched, i);
		exec = yass_task_get_exec(sched, id);
		deadline = yass_task_get_deadline(sched, id);
		n = yass_task_get_n_exec(sched, id);
		period = yass_task_get_period(sched, id);

		if (yass_sched_task_is_idle_task(sched, id))
			continue;

		aet = yass_task_get_aet(sched, id);

		if (tick != 0 && n * period + deadline == tick) {
			if (exec < aet - 0.001) {
				/* yass_sched_inc_deadline_misses(sched); */
				r = 1;
			}
		}
	}

	return r;
}

YASS_EXPORT void yass_sched_inc_deadline_misses(struct sched *sched)
{
	sched->deadline_misses++;
}

YASS_EXPORT int yass_sched_get_ncpus(struct sched *sched)
{
	return sched->n_cpus;
}

YASS_EXPORT int yass_sched_get_ntasks(struct sched *sched)
{
	return sched->n_tasks;
}

YASS_EXPORT void yass_sched_set_ntasks(struct sched *sched, int n_tasks)
{
	sched->n_tasks = n_tasks;
}

YASS_EXPORT struct yass_cpu *yass_sched_get_cpu(struct sched *sched, int cpu)
{
	return sched->cpus[cpu];
}

YASS_EXPORT struct yass_task *yass_sched_get_task(struct sched *sched, int id)
{
	int i;

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		if (yass_task_get_id(sched, i) == id)
			return sched->tasks[i];
	}

	return NULL;
}

YASS_EXPORT struct yass_task **yass_sched_get_tasks(struct sched *sched)
{
	return sched->tasks;
}

YASS_EXPORT struct yass_task *yass_sched_get_task_index(struct sched *sched,
							int index)
{
	return sched->tasks[index];
}

YASS_EXPORT struct yass_task_sched *yass_sched_get_task_sched(struct sched
							      *sched, int id)
{
	int i;

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		if (yass_task_get_id(sched, i) == id)
			return sched->tasks_sched[i];
	}

	return NULL;
}

static int sched_open(struct sched *sched, const char *filename)
{
	sched->handle = dlopen(filename, RTLD_LAZY);

	if (!sched->handle) {
		/* fprintf(stderr, "%s\n", dlerror()); */
		return -YASS_ERROR_DLOPEN;
	}

	return 0;
}

static void *assign(struct sched *sched, const char *s)
{
	return dlsym(sched->handle, s);
}

YASS_EXPORT struct sched **yass_sched_new(int n_schedulers)
{
	return (struct sched **)calloc(n_schedulers, sizeof(struct sched));
}

YASS_EXPORT int yass_sched_init(struct sched **sched, int n_schedulers,
				char **schedulers, int n_cpus, const char *cpu,
				int n_tasks, int verbose, int online, int debug)
{
	int i, j, error;
	char filename[256];

	/*
	 * This test is not enough to guarantee that a scheduler is
	 * not used twice because a scheduler can be called with two
	 * differente strings, for example "edf" and
	 * "schedulers/.libs/edf.so".
	 */
	for (i = 0; i < n_schedulers; i++) {
		for (j = i + 1; j < n_schedulers; j++) {
			if (!strcmp(schedulers[i], schedulers[j]))
				return -YASS_ERROR_SCHEDULER_NOT_UNIQUE;
		}
	}

	for (i = 0; i < n_schedulers; i++) {
		sched[i] = (struct sched *)malloc(sizeof(struct sched));

		if (sched[i] == NULL)
			return -YASS_ERROR_MALLOC;

		sched[i]->id = i + 1;
		sched[i]->index = i;
		sched[i]->fp = NULL;
		sched[i]->verbose = verbose;
		sched[i]->debug = debug;
		sched[i]->tick = 0;
		sched[i]->stat = -1;
		sched[i]->deadline_misses = 0;
		sched[i]->n_cpus = n_cpus;
		sched[i]->n_tasks = n_tasks;
		sched[i]->online = online;

		sched[i]->offline = NULL;
		sched[i]->schedule = NULL;
		sched[i]->close = NULL;

		sched[i]->tasks = NULL;
		sched[i]->tasks_sched = NULL;
		sched[i]->exec_time = NULL;

		sched[i]->last_tasks = (int **)calloc(2 * n_cpus, sizeof(int));

		for (j = 0; j < n_cpus; j++) {
			sched[i]->last_tasks[j] = (int *)calloc(2, sizeof(int));

			sched[i]->last_tasks[j][0] = -1;
			sched[i]->last_tasks[j][1] = -1;
		}

		sched[i]->cpus = NULL;
		sched[i]->handle = NULL;

		if (schedulers != NULL) {
			error = yass_find_file(filename, schedulers[i], SCHED);

			if (error)
				return error;

			error = sched_open(sched[i], filename);

			if (error)
				return error;

			if ((sched[i]->offline = (int (*)(struct sched *))
			     assign(sched[i], "offline")) == NULL) {
				return -YASS_ERROR_DLSYM;
			}
			if ((sched[i]->schedule = (int (*)(struct sched *))
			     assign(sched[i], "schedule")) == NULL) {
				return -YASS_ERROR_DLSYM;
			}
			if ((sched[i]->close = (int (*)(struct sched *))
			     assign(sched[i], "sched_close")) == NULL) {
				return -YASS_ERROR_DLSYM;
			}

			if ((sched[i]->name = (const char *(*)())
			     assign(sched[i], "name")) == NULL) {
				return -YASS_ERROR_DLSYM;
			}

			if (sched[i]->name && strlen(sched[i]->name()) < 2)
				return -YASS_ERROR_SCHEDULER_NAME_TOO_SHORT;
		}

		sched[i]->cpus = yass_cpu_new(cpu, n_cpus, &error);

		if (error)
			return error;
	}

	return 0;
}

YASS_EXPORT int yass_sched_init_tasks(struct sched *sched,
				      struct yass_task **tasks,
				      int **exec_time, int n_tasks)
{
	int i, j;

	struct yass_task **t;
	int **e;

	t = yass_tasks_new(n_tasks);

	if (!t)
		return -YASS_ERROR_MALLOC;

	for (i = 0; i < n_tasks; i++)
		memcpy(t[i], tasks[i], sizeof(struct yass_task));

	e = yass_tasks_exec_new(n_tasks);

	if (!e)
		return -YASS_ERROR_MALLOC;

	for (i = 0; i < n_tasks; i++) {
		for (j = 0; j < YASS_MAX_N_TASK_EXECUTION; j++)
			e[i][j] = exec_time[i][j];
	}

	sched->tasks = t;
	sched->exec_time = e;

	sched->tasks_sched = yass_tasks_sched_new(sched, n_tasks);

	if (!sched->tasks_sched)
		return -YASS_ERROR_MALLOC;

	return 0;
}

YASS_EXPORT void yass_sched_free(struct sched *sched)
{
	int i;

	for (i = 0; i < sched->n_cpus; i++)
		free(sched->last_tasks[i]);
	free(sched->last_tasks);

	if (sched->cpus != NULL) {
		for (i = 0; i < sched->n_cpus; i++) {
			if (sched->cpus[i] != NULL)
				yass_cpu_free(sched, i);
		}

		free(sched->cpus);
	}

	if (sched->tasks != NULL) {
		for (i = 0; i < sched->n_tasks; i++)
			free(sched->tasks[i]);

		free(sched->tasks);
	}

	if (sched->exec_time != NULL) {
		for (i = 0; i < sched->n_tasks; i++)
			free(sched->exec_time[i]);

		free(sched->exec_time);
	}

	if (sched->tasks_sched) {
		for (i = 0; i < sched->n_tasks; i++)
			free(sched->tasks_sched[i]);

		free(sched->tasks_sched);
	}

	if (sched->handle)
		dlclose(sched->handle);

	free(sched);
}

YASS_EXPORT int yass_sched_offline(struct sched *sched)
{
	return sched->offline(sched);
}

YASS_EXPORT int yass_sched_schedule(struct sched *sched)
{
	return sched->schedule(sched);
}

YASS_EXPORT int yass_sched_close(struct sched *sched)
{
	return sched->close(sched);
}

YASS_EXPORT double yass_sched_get_exec_time(struct sched *sched, int id,
					    int n_exec)
{
	int index = yass_task_get_from_id(sched, id);

	return sched->exec_time[index][n_exec];
}

YASS_EXPORT int **yass_sched_get_exec_times(struct sched *sched)
{
	return sched->exec_time;
}

/*
 * TODO: There must be another way to do that, a way which does not
 * require a check for all tasks in every tick.
 */
YASS_EXPORT int yass_sched_get_next_boundary(struct sched *sched, int tick)
{
	int i, id, period, t;

	int n_tasks = yass_sched_get_ntasks(sched);

	t = tick;

	while (1) {
		t++;

		for (i = 0; i < n_tasks; i++) {
			id = yass_task_get_id(sched, i);
			period = yass_task_get_period(sched, id);

			if (t % period == 0)
				return t;
		}
	}

	return -1;
}

YASS_EXPORT int yass_sched_all_cpus_active(struct sched *sched)
{
	int i;

	for (i = 0; i < yass_sched_get_ncpus(sched); i++) {
		if (!yass_cpu_is_active(sched, i))
			return 0;
	}

	return 1;
}

YASS_EXPORT const char *yass_sched_get_name(struct sched *sched)
{
	if (!sched->name)
		return "__";

	return sched->name();
}

YASS_EXPORT double yass_sched_get_global_utilization(struct sched *sched)
{
	int i, id;
	double u = 0;

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		id = yass_task_get_id(sched, i);
		u += yass_task_get_utilization(sched, id);
	}

	return u;
}

YASS_EXPORT int yass_sched_get_next_release(struct sched *sched)
{
	int i, id, next;

	int tick = yass_sched_get_tick(sched);
	int next_release = YASS_MAX_PERIOD;

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		id = yass_task_get_id(sched, i);
		next = yass_task_get_next_release(sched, id);

		if (next == tick) {
			if (next_release > next + tick)
				next_release = next + tick;
		} else if (next_release > next) {
			next_release = next;
		}
	}

	return next_release;
}

YASS_EXPORT unsigned long long yass_sched_get_hyperperiod(struct sched *sched)
{
	int i, id;
	unsigned long long h;

	int n_tasks = yass_sched_get_ntasks(sched);

	h = 1;

	for (i = 0; i < n_tasks; i++) {
		id = yass_task_get_id(sched, i);

		h = yass_lcm(h, yass_task_get_period(sched, id));
	}

	return h;
}

YASS_EXPORT unsigned long long yass_sched_get_hyperperiod_vm(struct sched *sched, int vm)
{
	int i, id;
	unsigned long long h;

	int n_tasks = yass_sched_get_ntasks(sched);

	h = 1;

	for (i = 0; i < n_tasks; i++) {
		id = yass_task_get_id(sched, i);

		if (yass_task_get_vm(sched, id) == vm)
			h = yass_lcm(h, yass_task_get_period(sched, id));
	}

	return h;
}

YASS_EXPORT int yass_sched_get_total_exec(struct sched *sched)
{
	int i, id, period, t, wcet;

	unsigned long long h = yass_sched_get_hyperperiod(sched);

	int n_tasks = yass_sched_get_ntasks(sched);

	t = 0;

	for (i = 0; i < n_tasks; i++) {
		id = yass_task_get_id(sched, i);
		period = yass_task_get_period(sched, id);
		wcet = yass_task_get_wcet(sched, id);

		t += (h / period) * wcet;
	}

	return t;
}

YASS_EXPORT int yass_sched_add_idle_task(struct sched *sched)
{
	int id, n_tasks;

	int n_cpus = yass_sched_get_ncpus(sched);

	unsigned long long exec = yass_sched_get_total_exec(sched);
	unsigned long h = yass_sched_get_hyperperiod(sched);

	struct yass_task *task;
	struct yass_task_sched *task_sched;

	id = YASS_IDLE_TASK_ID;

	while (yass_task_exist(sched, id))
		id++;

	task = yass_task_new(id);

	if (task == NULL)
		return -YASS_ERROR_MALLOC;

	task->wcet = (h * n_cpus) - exec;
	task->period = h;
	task->deadline = h;

	task->criticality = 1;

	task_sched = yass_task_sched_new(id);

	if (task_sched == NULL)
		return -YASS_ERROR_MALLOC;

	sched->n_tasks++;

	n_tasks = sched->n_tasks;

	sched->tasks = (struct yass_task **)
	    realloc(sched->tasks, n_tasks * sizeof(struct yass_task));

	if (!sched->tasks)
		return -YASS_ERROR_MALLOC;

	sched->tasks[n_tasks - 1] = task;

	sched->tasks_sched = (struct yass_task_sched **)
	    realloc(sched->tasks_sched,
		    n_tasks * sizeof(struct yass_task_sched));

	if (!sched->tasks_sched)
		return -YASS_ERROR_MALLOC;

	sched->tasks_sched[n_tasks - 1] = task_sched;

	return 0;
}

YASS_EXPORT void yass_sched_update_idle(struct sched *sched)
{
	int i;
	int id, id_is_idle = 0;
	int last, last_is_idle = 0, last_last;

	int n_cpus = yass_sched_get_ncpus(sched);
	int tick = yass_sched_get_tick(sched);

	for (i = 0; i < n_cpus; i++) {
		id = yass_cpu_get_task(sched, i);

		if (id != -1)
			id_is_idle = yass_sched_task_is_idle_task(sched, id);

		last = sched->last_tasks[i][0];

		if (last != -1) {
			last_is_idle =
			    yass_sched_task_is_idle_task(sched, last);
		}

		last_last = sched->last_tasks[i][1];

		if (id == -1 || id_is_idle) {
			/*
			 * Processor currently idle, no context switch
			 */
		} else if (last == -1 || last_is_idle) {
			/*
			 * Processor was idle, a context switch only
			 * if the current task is different from the
			 * last active task
			 */
			if (id != -1 && !id_is_idle && id != last_last)
				yass_cpu_add_context_switches(sched, i);
		} else if (id != -1 && !id_is_idle) {
			/*
			 * Processor active, a context switch if the
			 * current task is different from the previous
			 * one
			 */
			if (id != last)
				yass_cpu_add_context_switches(sched, i);
		}

		if (tick != 0 && id != -1 && !id_is_idle &&
		    (last == -1 || last_is_idle)) {

			if (yass_cpu_get_idle_time(sched, i) != 1)
				yass_cpu_increase_idle_periods(sched, i);

			yass_cpu_reset_idle_time(sched, i);
		}

		if (id == -1 || id_is_idle)
			yass_cpu_increase_idle_time(sched, i);

		if (id != sched->last_tasks[i][0])
			sched->last_tasks[i][1] = sched->last_tasks[i][0];

		sched->last_tasks[i][0] = id;
	}
}

YASS_EXPORT int yass_sched_task_is_idle_task(struct sched *sched
					     __attribute__ ((unused)), int id)
{
	return id >= YASS_IDLE_TASK_ID;
}

YASS_EXPORT void yass_sched_end_idle_periods(struct sched *sched)
{
	int i, id;
	double idle_time;

	for (i = 0; i < yass_sched_get_ncpus(sched); i++) {
		id = yass_cpu_get_task(sched, i);

		idle_time = yass_cpu_get_idle_time(sched, i);

		if ((id < 0 || id == YASS_IDLE_TASK_ID) && idle_time > 1.1)
			yass_cpu_increase_idle_periods(sched, i);
	}
}

YASS_EXPORT int yass_sched_get_nvms(struct sched *sched)
{
	int i, id, j, n = 0;

	int vms[YASS_MAX_N_VMS];

	for (i = 0; i < YASS_MAX_N_VMS; i++)
		vms[i] = -1;

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		id = yass_task_get_id(sched, i);

		for (j = 0; j < YASS_MAX_N_VMS; j++) {
			if (vms[j] == yass_task_get_vm(sched, id))
				break;

			if (vms[j] == -1) {
				vms[j] = yass_task_get_vm(sched, id);
				n++;
				break;
			}
		}
	}

	return n;
}
