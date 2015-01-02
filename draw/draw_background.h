#ifndef _YASS_DRAW_BACKGROUND_H
#define _YASS_DRAW_BACKGROUND_H

void draw_lines(cairo_t * cr[10], int n_tasks, int n_cpus, int n_ticks,
		int n_sched, int scale, int disable_frequency, int cpu,
		int legend, int h);

void draw_line_above_cpu(cairo_t * cr[10], int n_sched, int n_tasks,
			 int n_ticks, int h);

void draw_line_between_schedulers(cairo_t * cr[10], int n_sched, int n_ticks);

#endif				/* _YASS_DRAW_BACKGROUND_H */
