#ifndef _YASS_TASK_H
#define _YASS_TASK_H

#include "common.h"

#include "scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sched;

struct yass_task {
	int id;
	int vm;
	int threads;
	int wcet;
	int deadline;
	int period;
	int criticality;

	int delay;

	/* Fork-join task model */
	int parallel;
	int s;
	int segments[YASS_MAX_SEGMENTS];
};

struct yass_task_sched {
	int id;
	int priority;
	int release;

	double exec;
};

int yass_task_exist(struct sched *sched, int id);

int yass_task_get_from_id(struct sched *sched, int id);

struct yass_task *yass_task_new(int id);

struct yass_task_sched *yass_task_sched_new(int id);

struct yass_task **yass_tasks_new(int n);

struct yass_task **yass_tasks_create(const char *data, int *n_tasks,
				     int *error);

void yass_task_free_tasks(struct yass_task **tasks, int n);

void yass_task_free_exec_time(int **exec_time, int n);

struct yass_task_sched **yass_tasks_sched_new(struct sched *sched, int n_tasks);

int yass_task_time_to_deadline(struct sched *sched, int id);

int yass_task_get_id(struct sched *sched, int index);

int yass_task_get_vm(struct sched *sched, int id);

int yass_task_get_threads(struct sched *sched, int id);

int yass_task_get_wcet(struct sched *sched, int id);

int yass_task_get_aet(struct sched *sched, int id);

int yass_task_get_deadline(struct sched *sched, int id);

int yass_task_get_period(struct sched *sched, int id);

int yass_task_get_delay(struct sched *sched, int id);

int yass_task_get_criticality(struct sched *sched, int id);

int yass_task_get_priority(struct sched *sched, int id);

void yass_task_set_priority(struct sched *sched, int id, int priority);

double yass_task_get_exec(struct sched *sched, int id);

void yass_task_set_exec(struct sched *sched, int id, double exec);

void yass_task_exec_inc(struct sched *sched, int id, double exec);

int yass_task_get_cpu(struct sched *sched, int id);

double yass_task_get_utilization(struct sched *sched, int id);

double yass_task_get_remaining_exec(struct sched *sched, int id);

int yass_task_get_parallel(struct sched *sched, int id);

int yass_task_get_s(struct sched *sched, int id);

int *yass_task_get_segments(struct sched *sched, int id);

double yass_task_get_laxity(struct sched *sched, int id);

double yass_task_get_lag(struct sched *sched, int id);

void yass_task_set_release(struct sched *sched, int id, int release);

int yass_task_get_next_release(struct sched *sched, int id);

int **yass_tasks_exec_new(int n_tasks);

int **yass_tasks_generate_exec(struct yass_task **tasks, int n_tasks);

int yass_task_is_active(struct sched *sched, int id);

int yass_task_get_exec_hyperperiod(struct sched *sched, int id);

int yass_task_get_n_exec(struct sched *sched, int id);

#ifdef __cplusplus
}
#endif

#endif				/* _YASS_TASK_H */
