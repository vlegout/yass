#ifndef _YASS_SCHEDULER_COMMON_H
#define _YASS_SCHEDULER_COMMON_H

#include "list.h"
#include "yass.h"

#ifdef __cplusplus
extern "C" {
#endif

int yass_edf_choose_next_task(struct sched *sched, struct yass_list *q);

void yass_exec_inc(struct sched *sched);

int yass_check_terminated_tasks(struct sched *sched,
				struct yass_list *running,
				struct yass_list *stalled, int execution_class);

int yass_deadline_miss(struct sched *sched);

int yass_check_ready_tasks(struct sched *sched, struct yass_list *stalled,
			   struct yass_list *ready);

void yass_terminate_task(struct sched *sched, int cpu, int id,
			 struct yass_list *running,
			 struct yass_list *stalled);

void yass_terminate_task_tick(struct sched *sched, int cpu,
			      int id, int tick,
			      struct yass_list *running,
			      struct yass_list *stalled);

void yass_preempt_task(struct sched *sched, int cpu,
		       struct yass_list *running, struct yass_list *ready);

void yass_run_task(struct sched *sched, int cpu, int id,
		   struct yass_list *ready, struct yass_list *running);

void yass_sort_list_int(struct sched *sched, struct yass_list *q,
			 int (*f) (struct sched *, int));

void yass_sort_list_double(struct sched *sched, struct yass_list *q,
			    double (*f) (struct sched *, int));

int yass_optimal_schedulability_test(struct sched *sched);

int yass_rm_schedulability_test(struct sched *sched);

int yass_dpm_schedulability_test(struct sched *sched);

void yass_print_task_set(struct sched *sched);

unsigned long long yass_gcd(unsigned long long a, unsigned long long b);

unsigned long long yass_lcm(unsigned long long a, unsigned long long b);

int yass_compute_intervals(struct sched *sched, int *I);

int yass_get_current_interval(struct sched *sched);

int yass_tick_is_interval_boundary(struct sched *sched, int tick);

#ifdef __cplusplus
}
#endif

#endif				/* _YASS_SCHEDULER_COMMON_H */
