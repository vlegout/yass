#ifndef _YASS_SCHED_H
#define _YASS_SCHED_H

#include <stdio.h>

#include "common.h"

#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sched {
	int id;
	int index;

	FILE *fp;

	void *handle;

	int verbose;
	int debug;

	int online;

	int tick;

	int stat;

	int deadline_misses;

	int n_cpus;
	struct yass_cpu **cpus;

	int n_tasks;
	struct yass_task **tasks;
	struct yass_task_sched **tasks_sched;
	int **exec_time;

	int **last_tasks;

	int (*offline) (struct sched * sched);
	int (*schedule) (struct sched * sched);
	int (*close) (struct sched * sched);

	const char *(*name) (void);
};

struct yass_task;

struct sched **yass_sched_new(int n_schedulers);

int yass_sched_init(struct sched **sched, int n_schedulers,
		    char **schedulers, int n_cpus, const char *cpu,
		    int n_tasks, int verbose, int online, int debug);

void yass_sched_free(struct sched *sched);

int yass_sched_init_tasks(struct sched *sched, struct yass_task **tasks,
			  int **exec_time, int n_tasks);

int yass_sched_get_index(struct sched *sched);

FILE *yass_sched_get_fp(struct sched *sched);

void yass_sched_set_fp(struct sched *sched, FILE * fp);

int yass_sched_get_verbose(struct sched *sched);

int yass_sched_get_debug(struct sched *sched);

int yass_sched_get_online(struct sched *sched);

int yass_sched_get_tick(struct sched *sched);

void yass_sched_tick_inc(struct sched *sched);

int yass_sched_get_stat(struct sched *sched);

void yass_sched_set_stat(struct sched *sched, int stat);

double yass_sched_get_deadline_misses(struct sched *sched, int choice);

int yass_sched_check_deadline_misses(struct sched *sched);

void yass_sched_inc_deadline_misses(struct sched *sched);

int yass_sched_get_ncpus(struct sched *sched);

int yass_sched_get_ntasks(struct sched *sched);

void yass_sched_set_ntasks(struct sched *sched, int n_tasks);

struct yass_cpu *yass_sched_get_cpu(struct sched *sched, int cpu);

struct yass_task *yass_sched_get_task(struct sched *sched, int id);

struct yass_task **yass_sched_get_tasks(struct sched *sched);

struct yass_task *yass_sched_get_task_index(struct sched *sched, int index);

struct yass_task_sched *yass_sched_get_task_sched(struct sched *sched, int id);

int yass_sched_offline(struct sched *sched);

int yass_sched_schedule(struct sched *sched);

int yass_sched_close(struct sched *sched);

double yass_sched_get_exec_time(struct sched *sched, int id, int n_exec);

int **yass_sched_get_exec_times(struct sched *sched);

int yass_sched_get_next_boundary(struct sched *sched, int tick);

int yass_sched_all_cpus_active(struct sched *sched);

const char *yass_sched_get_name(struct sched *sched);

double yass_sched_get_global_utilization(struct sched *sched);

int yass_sched_get_next_release(struct sched *sched);

unsigned long long yass_sched_get_hyperperiod(struct sched *sched);

unsigned long long yass_sched_get_hyperperiod_vm(struct sched *sched, int vm);

int yass_sched_get_total_exec(struct sched *sched);

int yass_sched_add_idle_task(struct sched *sched);

void yass_sched_update_idle(struct sched *sched);

int yass_sched_task_is_idle_task(struct sched *sched, int id);

void yass_sched_end_idle_periods(struct sched *sched);

int yass_sched_get_nvms(struct sched *sched);

#ifdef __cplusplus
}
#endif

#endif				/* _YASS_SCHED_H */
