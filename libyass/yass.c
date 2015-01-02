#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "yass.h"

#include "common.h"
#include "cpu.h"
#include "log.h"
#include "private.h"
#include "scheduler.h"
#include "task.h"

struct thread_info {
	pthread_t id;
	struct sched *sched;
	int n_ticks;
	int error;
};

struct configuration_type {
	char directories[3][128];
	char file[128];
	char suffix[16];
};

YASS_EXPORT int yass_init_tasks(struct yass *yass, struct yass_task **tasks,
				int **exec_time)
{
	int error, h, i, id, j;
	int n_tasks = yass_sched_get_ntasks(yass_get_sched(yass, 0));

	struct sched *sched;

	for (i = 0; i < yass->n_schedulers; i++) {
		sched = yass_get_sched(yass, i);

		error = yass_sched_init_tasks(sched, tasks, exec_time, n_tasks);

		if (error)
			return error;
	}

	sched = yass_get_sched(yass, 0);

	if (yass_get_nticks(yass) != -1) {
		yass_set_nticks(yass, yass_get_nticks(yass));
	} else {
		h = yass_sched_get_hyperperiod(sched);
		yass_set_nticks(yass, yass_get_nhyperperiods(yass) * h + 1);
	}

	yass_sched_get_hyperperiod(sched);

	/* Check that each id is unique */
	for (i = 0; i < n_tasks - 1; i++) {
		id = yass_task_get_id(sched, i);

		for (j = i + 1; j < n_tasks; j++) {
			if (yass_task_get_id(sched, j) == id)
				return -YASS_ERROR_ID_NOT_UNIQUE;
		}
	}

	return 0;
}

YASS_EXPORT struct yass *yass_new(void)
{
	return (struct yass *)malloc(sizeof(struct yass));
}

YASS_EXPORT int yass_init(struct yass *yass, int energy, int n_cpus,
			  int n_ticks, int n_hyperperiods, const char *cpu,
			  int verbose, int n_schedulers, char **schedulers,
			  int n_tasks, int online, int debug)
{
	int error;

	yass->n_schedulers = n_schedulers;

	yass->sched = yass_sched_new(n_schedulers);

	if (yass->sched == NULL)
		return -YASS_ERROR_MALLOC;

	error = yass_sched_init(yass->sched, n_schedulers, schedulers,
				n_cpus, cpu, n_tasks, verbose, online, debug);

	if (error)
		return error;

	yass_set_nticks(yass, n_ticks);
	yass_set_nhyperperiods(yass, n_hyperperiods);

	yass->energy = energy;

	return 0;
}

static void yass_log_indep(struct sched *sched)
{
	int i, id, tick_delay;
	int delay, deadline, period;

	int tick = yass_sched_get_tick(sched);

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		id = yass_task_get_id(sched, i);

		if (yass_sched_task_is_idle_task(sched, id))
			continue;

		delay = yass_task_get_delay(sched, id);
		deadline = yass_task_get_deadline(sched, id);
		period = yass_task_get_period(sched, id);

		tick_delay = tick - delay;

		if (tick_delay < 0)
			continue;

		if (tick_delay % period == 0) {
			yass_log_sched(sched, YASS_EVENT_TASK_RELEASE, id,
				       tick, 0, 0, 0);

			if (period == deadline && tick != delay)
				yass_log_sched(sched, YASS_EVENT_TASK_DEADLINE,
					       id, tick, 0, 0, 0);
		}

		if ((tick_delay / period) * period + deadline == tick_delay) {
			yass_log_sched(sched, YASS_EVENT_TASK_DEADLINE, id,
				       tick, 0, 0, 0);
		}
	}
}

static void *routine(void *arg)
{
	int error = 0, j, k;

	int n_ticks = ((struct thread_info *)arg)->n_ticks;
	struct sched *sched = ((struct thread_info *)arg)->sched;

	error = yass_sched_offline(sched);

	if (error)
		goto error;

	yass_warn(n_ticks >= YASS_DEFAULT_MIN_TICKS);

	for (j = 0; j < n_ticks; j++) {
		if (yass_sched_get_index(sched) == 0)
			yass_log_indep(sched);

		yass_sched_check_deadline_misses(sched);

		error = yass_sched_schedule(sched);

		if (error) {
			yass_sched_close(sched);
			goto error;
		}

		for (k = 0; k < yass_sched_get_ncpus(sched); k++)
			yass_cpu_cons_inc(sched, k);

		yass_sched_update_idle(sched);

		yass_sched_tick_inc(sched);
	}

	yass_sched_end_idle_periods(sched);

	error = yass_sched_close(sched);

	if (error)
		goto error;

	return NULL;

 error:
	((struct thread_info *)arg)->error = error;

	return NULL;
}

YASS_EXPORT int yass_run(struct yass *yass, int jobs)
{
	int end, error, i, n, s;

	int n_ticks = yass_get_nticks(yass);
	int n_schedulers = yass_get_nschedulers(yass);

	struct sched *sched;

	struct thread_info *tinfo = (struct thread_info *)
	    calloc(n_schedulers, sizeof(struct thread_info));

	if (tinfo == NULL)
		return -YASS_ERROR_MALLOC;

	error = 0;
	n = 0;

	if (n_schedulers <= jobs)
		jobs = n_schedulers;

	while (n * jobs < n_schedulers) {
		if ((n + 1) * jobs > n_schedulers - 1)
			end = n_schedulers;
		else
			end = (n + 1) * jobs;

		for (i = n * jobs; i < end; i++) {
			sched = yass_get_sched(yass, i);

			tinfo[i].sched = sched;
			tinfo[i].n_ticks = n_ticks;
			tinfo[i].error = 0;

			if (yass_sched_get_debug(sched))
				printf("RUN: %s\n", yass_sched_get_name(sched));

			s = pthread_create(&tinfo[i].id, NULL, &routine,
					   &tinfo[i]);

			if (s != 0) {
				error = -YASS_ERROR_THREAD_CREATE;
				goto end;
			}
		}

		for (i = n * jobs; i < end; i++) {
			pthread_join(tinfo[i].id, NULL);

			if (tinfo[i].error < 0) {
				error = tinfo[i].error;
				goto end;
			}
		}

		n++;
	}

 end:
	free(tinfo);

	return error;
}

YASS_EXPORT void yass_free(struct yass *yass)
{
	int i;

	struct sched *sched;

	for (i = 0; i < yass->n_schedulers; i++) {
		sched = yass_get_sched(yass, i);

		if (sched != NULL)
			yass_sched_free(sched);
	}

	free(yass->sched);

	free(yass);
}

YASS_EXPORT struct sched *yass_get_sched(struct yass *y, int index)
{
	yass_warn(index >= 0 && index < yass_get_nschedulers(y));

	if (!y->sched)
		return NULL;

	return y->sched[index];
}

YASS_EXPORT int yass_get_nschedulers(struct yass *y)
{
	return y->n_schedulers;
}

YASS_EXPORT int yass_get_energy(struct yass *y)
{
	return y->energy;
}

YASS_EXPORT int yass_get_nticks(struct yass *y)
{
	return y->n_ticks;
}

YASS_EXPORT void yass_set_nticks(struct yass *y, int ticks)
{
	y->n_ticks = ticks;
}

YASS_EXPORT int yass_get_nhyperperiods(struct yass *y)
{
	return y->n_hyperperiods;
}

YASS_EXPORT void yass_set_nhyperperiods(struct yass *y, int hyperiods)
{
	y->n_hyperperiods = hyperiods;
}

YASS_EXPORT int yass_find_file(char *filename, const char *s, int type)
{
	int i;

	struct stat tmp;

	struct configuration_type configuration_array[4] = {
		{{"", "schedulers/.libs/", SCHED_DIR}, "fcfs", ".so"},
		{{"", "data/", DATA_DIR}, "default", ""},
		{{"", "processors/", PROCESSORS_DIR}, "stm32l", ""},
		{{"", "tests/", TESTS_DIR}, "default", ""},
	};

	if (!s)
		return -YASS_ERROR_FILE;

	for (i = 0; i < 3; i++) {
		strcpy(filename, configuration_array[type].directories[i]);

		if (s == NULL || !strcmp(s, ""))
			strcat(filename, configuration_array[type].file);
		else
			strcat(filename, s);

		strcat(filename, configuration_array[type].suffix);

		if (!stat(filename, &tmp))
			break;
	}

	/*
	 * If we did not found the file in one of the three predefined
	 * directories, we just return the original string.
	 */
	if (stat(filename, &tmp))
		strcpy(filename, s);

	return 0;
}

YASS_EXPORT void yass_handle_error(int error_code)
{
	fprintf(stderr, "Error: ");

	switch (error_code) {
	case -YASS_ERROR_CPU_FILE:
		fprintf(stderr, "error while parsing processor file\n");
		break;
	case -YASS_ERROR_CPU_FILE_JSON:
		fprintf(stderr, "processor file is not a valid json file\n");
		break;
	case -YASS_ERROR_DATA_FILE:
		fprintf(stderr, "error while parsing data file\n");
		break;
	case -YASS_ERROR_DATA_FILE_JSON:
		fprintf(stderr, "data file is not a valid json file\n");
		break;
	case -YASS_ERROR_DLOPEN:
		fprintf(stderr, "cannot open scheduler file\n");
		break;
	case -YASS_ERROR_DLSYM:
		fprintf(stderr, "missing symbol in scheduler file\n");
		break;
	case -YASS_ERROR_FILE:
		fprintf(stderr, "error while searching a file\n");
		break;
	case -YASS_ERROR_ID_NOT_UNIQUE:
		fprintf(stderr, "task id must be unique\n");
		break;
	case -YASS_ERROR_MALLOC:
		fprintf(stderr, "cannot allocate memory\n");
		break;
	case -YASS_ERROR_MORE_THAN_ONE_CPU:
		fprintf(stderr, "a scheduler needs a uniprocessor system\n");
		break;
	case -YASS_ERROR_N_CPUS:
		fprintf(stderr, "too few or too many cpus\n");
		break;
	case -YASS_ERROR_N_JOBS:
		fprintf(stderr, "too few or too many jobs\n");
		break;
	case -YASS_ERROR_N_TICKS:
		fprintf(stderr, "too few ticks\n");
		break;
	case -YASS_ERROR_NOT_SCHEDULABLE:
		fprintf(stderr, "task set not schedulable\n");
		break;
	case -YASS_ERROR_SCHEDULE:
		fprintf(stderr, "cannot schedule the task set\n");
		break;
	case -YASS_ERROR_SCHEDULER_NOT_UNIQUE:
		fprintf(stderr, "a scheduler can only be used once\n");
		break;
	case -YASS_ERROR_SCHEDULER_NAME_TOO_SHORT:
		fprintf(stderr, "scheduler name must be greater than 1\n");
		break;
	case -YASS_ERROR_THREAD_CREATE:
		fprintf(stderr, "cannot create a thread\n");
		break;
	case -YASS_ERROR_TICKS_HYPERPERIOD:
		fprintf(stderr, "cannot set both ticks and hyperperiods\n");
		break;
	case -YASS_ERROR_DEFAULT:
	default:
		fprintf(stderr, "error while running yass\n");
		break;
	}
}
