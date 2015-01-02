#ifndef _YASS_DRAW_EVENT_H
#define _YASS_DRAW_EVENT_H

#include <cairo.h>

int task_get_id(int n);

void draw_init(int n_tasks, int n_sched, int n_cpus, int n_ticks);

void draw_free(int n_tasks, int n_sched, int n_cpus);

void set_cpu_speed(int scheduler, int cpu, int new_speed);

void add_boundary(int type, int task, int tick);

void draw_boundaries(cairo_t * cr[10], int n_tasks, int n_schedulers, int h);

void save_run(int scheduler, int task, int tick, int cpu);

void draw_execution(int sched, int task, int tick, int cpu, int n_tasks,
		    int ticks, cairo_t * cr[10], int only_cpu, int h);

void draw_consumption(int scheduler, int cpu, int consumption, int tick,
		      int n_ticks);

void colors_init(int disable_dpm, int n_tasks);

void colors_destroy(int n_tasks);

#endif				/* _YASS_DRAW_EVENT_H */
