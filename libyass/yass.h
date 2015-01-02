#ifndef _YASS_YASS_H
#define _YASS_YASS_H

#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

struct yass {
	int n_schedulers;
	struct sched **sched;

	int n_ticks;
	int n_hyperperiods;

	int energy;
};

struct yass *yass_new(void);

int yass_init(struct yass *yass, int energy, int n_cpus, int n_ticks,
	      int n_hyperperiods, const char *cpu, int verbose,
	      int n_schedulers, char **schedulers, int n_tasks, int online,
	      int debug);

int yass_init_tasks(struct yass *yass, struct yass_task **tasks,
		    int **exec_time);

int yass_run(struct yass *yass, int jobs);

void yass_free(struct yass *yass);

struct sched *yass_get_sched(struct yass *y, int index);

int yass_get_nschedulers(struct yass *y);

int yass_get_energy(struct yass *y);

int yass_get_nticks(struct yass *y);

void yass_set_nticks(struct yass *y, int ticks);

int yass_get_nhyperperiods(struct yass *y);

void yass_set_nhyperperiods(struct yass *y, int hyperiods);

int yass_find_file(char *filename, const char *s, int type);

void yass_handle_error(int error_code);

#ifdef __cplusplus
}
#endif

#endif				/* _YASS_YASS_H */
