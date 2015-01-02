#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "run.h"

#include "main.h"

#include <libyass/common.h>
#include <libyass/cpu.h>
#include <libyass/log.h>
#include <libyass/scheduler.h>
#include <libyass/task.h>
#include <libyass/yass.h>

#define DEFAULT_OUTPUT "output.txt"

#define SUB_SCHED "sched"
#define SUB_STATS "stats"
#define SUB_OUTPUT "out"
#define SUB_USAGE "usage"

static int output_stats_sched(struct sched *sched, int index, char *output_file,
			      int n_hyperperiods)
{
	int i, j, length, n, r = 0;
	int *use;
	FILE *fp;

	char s[1024];

	double consumption = yass_cpu_cons_get_total(sched);
	double deadline_misses = yass_sched_get_deadline_misses(sched, 1);

	double ctx = 0;
	double idle = 0;

	for (i = 0; i < yass_sched_get_ncpus(sched); i++) {
		ctx += yass_cpu_get_idle_periods(sched, i);
		idle += yass_cpu_get_context_switches(sched, i);
	}

	deadline_misses /= n_hyperperiods;

	sprintf(s, "%s/%s", SUB_OUTPUT, output_file);

	fp = fopen(s, "a+");

	if (!fp)
		return -YASS_ERROR_FILE;

	fprintf(fp, "%lf %lf %lf %lf\n", idle, ctx, consumption,
		deadline_misses);

	fclose(fp);

	/*
	 * Idle periods length
	 */

	sprintf(s, "%s/%d", SUB_SCHED, index);

	fp = fopen(s, "a+");

	if (!fp)
		return -YASS_ERROR_FILE;

	for (i = 0; i < yass_sched_get_ncpus(sched); i++) {

		n = 0;
		length = yass_cpu_get_idle_length(sched, i, n);

		while (length != -1) {
			fprintf(fp, " %d", length);

			n++;
			length = yass_cpu_get_idle_length(sched, i, n);
		}
	}

	fprintf(fp, "\n");

	fclose(fp);

	/*
	 * Low-power states usage
	 */
	sprintf(s, "%s/%d", SUB_USAGE, index);

	fp = fopen(s, "a+");

	if (!fp)
		return -YASS_ERROR_FILE;

	use = (int *)malloc(yass_cpu_get_nstates(sched) * sizeof(int));

	if (!use)
		return -YASS_ERROR_MALLOC;

	for (i = 0; i < yass_cpu_get_nstates(sched); i++)
		use[i] = 0;

	for (i = 0; i < yass_sched_get_ncpus(sched); i++)
		for (j = 0; j < yass_cpu_get_nstates(sched); j++)
			use[j] += yass_cpu_get_state_usage(sched, i, j);

	for (i = 0; i < yass_cpu_get_nstates(sched); i++)
		fprintf(fp, " %d", use[i]);

	fprintf(fp, "\n");

	free(use);

	fclose(fp);

	/*
	 * stats
	 */
	sprintf(s, "%s/%s.%d", SUB_STATS, output_file, index);

	fp = fopen(s, "a+");

	if (!fp)
		return -YASS_ERROR_FILE;

	fprintf(fp, "%d\n", yass_sched_get_stat(sched));

	fclose(fp);

	return r;
}

static int output_stats(struct yass *yass, char *output_file, int error)
{
	int i, r;
	char tmp[1024];
	FILE *fp;

	struct sched *sched = yass_get_sched(yass, 0);

	int n_hyperperiods = yass_get_nhyperperiods(yass);

	sprintf(tmp, "%s/%s", SUB_OUTPUT, output_file);

	fp = fopen(tmp, "w+");

	if (!fp)
		return -YASS_ERROR_FILE;

	if (error) {
		fclose(fp);
		return 0;
	}

	fprintf(fp, "%lld\n", yass_sched_get_hyperperiod(sched));

	fclose(fp);

	for (i = 0; i < yass_get_nschedulers(yass); i++) {
		sched = yass_get_sched(yass, i);
		r = output_stats_sched(sched, i, output_file, n_hyperperiods);

		if (r)
			return r;
	}

	return 0;
}

int run(int opts, const char *data, int n_cpus, int n_ticks, int n_hyperperiods,
	const char *cpu, int n_schedulers, char **scheduler, char *output,
	int jobs, char *tests_output)
{
	int c, error, n_tasks;

	int ctx = opts & OPTS_CTX;
	int deadline = opts & OPTS_DEADLINE;
	int debug = opts & OPTS_DEBUG;
	int energy = opts & OPTS_ENERGY;
	int idle = opts & OPTS_IDLE;
	int online = opts & OPTS_ONLINE;
	int verbose = opts & OPTS_VERBOSE;
	int tests = opts & OPTS_TESTS;

	FILE *fp;

	int **exec_time;
	struct yass_task **tasks;

	struct sched *sched;

	struct yass *yass;

	if (n_schedulers == 0)
		n_schedulers = 1;

	tasks = yass_tasks_create(data, &n_tasks, &error);

	if (tasks == NULL) {
		yass_handle_error(error);
		goto end;
	}

	exec_time = yass_tasks_generate_exec(tasks, n_tasks);

	if (exec_time == NULL) {
		fprintf(stderr, "Error while generating execution times\n");
		goto end_tasks;
	}

	yass = yass_new();

	if (yass == NULL)
		yass_handle_error(-YASS_ERROR_MALLOC);

	error = yass_init(yass, energy, n_cpus, n_ticks, n_hyperperiods, cpu,
			  verbose, n_schedulers, scheduler, n_tasks, online,
			  debug);

	if (error) {
		yass_handle_error(error);
		goto end_exec;
	}

	error = yass_init_tasks(yass, tasks, exec_time);

	if (error) {
		yass_handle_error(error);
		goto end_yass;
	}

	if (!strcmp(output, ""))
		strcpy(output, DEFAULT_OUTPUT);

	fp = yass_log_new(yass, output);

	if (fp == NULL) {
		fprintf(stderr, "Error while creating log file\n");
		goto end_yass;
	}

	for (c = 0; c < n_schedulers; c++)
		yass_sched_set_fp(yass_get_sched(yass, c), fp);

	error = yass_run(yass, jobs);

	if (error) {
		yass_handle_error(error);
		goto end_fp;
	}

	if (yass_get_energy(yass))
		yass_cpu_print_consumption(yass);

	if (idle) {
		for (c = 0; c < n_schedulers; c++) {
			sched = yass_get_sched(yass, c);
			yass_cpu_idle_print(sched);
		}
	}

	if (ctx) {
		for (c = 0; c < n_schedulers; c++) {
			sched = yass_get_sched(yass, c);
			yass_cpu_print_context_switches(sched);
		}
	}

	if (deadline) {
		for (c = 0; c < n_schedulers; c++) {
			sched = yass_get_sched(yass, c);
			printf("%s: %lf\n",
			       yass_sched_get_name(sched),
			       yass_sched_get_deadline_misses(sched, 1));
		}
	}

 end_fp:
	if (tests) {
		error = output_stats(yass, tests_output, error);

		if (error)
			yass_handle_error(error);
	}

	fclose(fp);

 end_yass:
	yass_free(yass);

 end_exec:
	yass_task_free_exec_time(exec_time, n_tasks);

 end_tasks:
	yass_task_free_tasks(tasks, n_tasks);

 end:
	if (error)
		return 1;

	return 0;
}
