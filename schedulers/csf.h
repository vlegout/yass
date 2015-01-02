#ifndef _YASS_CSF_H
#define _YASS_CSF_H

#include <libyass/yass.h>
#include <libyass/list.h>

#define INC 1
#define PERIOD 100.0

struct server {
	int budget_init;
	int budget;
	int period;

	int exec;
	int deadline;
};

double compute_sbf(struct server *servers, double time, int vm);

int is_sched_edf(struct server *servers, struct sched *sched, int vm, double prob);

int get_lowest_utilization_cpu(struct sched *sched, struct server *servers, int **cpu_task);

void exec_inc(struct sched *sched, int *server_running, struct server *servers);

void check_ready_tasks(struct sched *sched,
		       struct yass_list *stalled,
		       struct yass_list *ready,
		       struct yass_list *stalled_tasks,
		       struct yass_list *ready_tasks,
		       struct server *servers);

int check_terminated_tasks(struct sched *sched,
			   struct yass_list *running,
			   struct yass_list *stalled,
			   struct yass_list *running_tasks,
			   struct yass_list *stalled_tasks,
			   struct yass_list *ready_tasks,
			   struct server *servers,
			   int *server_running);

int server_time_to_deadline(struct sched *sched, int index, struct server *servers);

void deadline_miss(struct sched *sched,
		   struct yass_list *running,
		   struct yass_list *stalled,
		   struct yass_list *running_tasks,
		   struct yass_list *stalled_tasks,
		   struct yass_list *ready_tasks,
		   int *server_running);

#endif
