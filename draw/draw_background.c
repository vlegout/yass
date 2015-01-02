#include <cairo.h>
#include <stdio.h>
#include <stdlib.h>

#include "draw.h"
#include "draw_background.h"
#include "draw_event.h"

static void draw_scale(cairo_t * cr[10], int sched, int y, int n_ticks,
		       int scale)
{
	int i, t;
	char tmp[128];

	cairo_select_font_face(cr[sched], "serif", CAIRO_FONT_SLANT_NORMAL,
			       CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr[sched], 11.0);

	for (i = 100 + WIDTH - 35; i < n_ticks / 10 + WIDTH; i += 100) {
		t = i - WIDTH + 35;

		switch (scale) {
		default:
		case 0:
			sprintf(tmp, "%4d", t * 10);
			break;
		case 1:
			sprintf(tmp, "%4d", t);
			break;
		case 2:
			sprintf(tmp, "%4d", t / 10);
			break;
		case 3:
			sprintf(tmp, "%4d", t / 100);
			break;
		}

		cairo_move_to(cr[sched], i - 15, y - 2);
		cairo_show_text(cr[sched], tmp);

		cairo_move_to(cr[sched], i, y - 15);
		cairo_line_to(cr[sched], i, y - 25);
	}

	cairo_set_line_width(cr[sched], 1.5);
	cairo_stroke(cr[sched]);

	for (i = WIDTH - 35; i < n_ticks / 10 + WIDTH - 20; i += 10) {
		cairo_move_to(cr[sched], i, y - 15);

		if ((i - WIDTH + 35) % 50 == 0)
			cairo_line_to(cr[sched], i, y - 25);
		else
			cairo_line_to(cr[sched], i, y - 20);
	}

	cairo_set_line_width(cr[sched], 1);
	cairo_stroke(cr[sched]);
}

static void draw_cpu_speed(cairo_t * cr[10], int sched, int y, int h)
{
	int i;
	char tmp[128];

	double height = 2 * (h - 10) / 5;

	cairo_select_font_face(cr[sched], "serif", CAIRO_FONT_SLANT_NORMAL,
			       CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr[sched], 7.0);

	for (i = 0; i < 2; i++) {
		sprintf(tmp, "%2.1f", 0.5 * (i + 1));

		cairo_move_to(cr[sched], WIDTH - 52, y + 3 - height * (i + 1));
		cairo_show_text(cr[sched], tmp);

		cairo_move_to(cr[sched], WIDTH - 35, y - height * (i + 1));
		cairo_line_to(cr[sched], WIDTH - 30, y - height * (i + 1));
	}

	cairo_set_line_width(cr[sched], 1);
	cairo_stroke(cr[sched]);
}

static void draw_line_sched(cairo_t * cr[10], int sched, int n, int cpu,
			    int n_cpus, int width, int n_tasks, int legend,
			    int h)
{
	int margin = cpu * TASK_CPU_MARGIN;
	int id = task_get_id(n);
	char tmp[128];

	cairo_set_source_rgb(cr[sched], 0, 0, 0);

	cairo_move_to(cr[sched], 2, h / 2 + 5 + n * h + margin);

	cairo_select_font_face(cr[sched], "serif", CAIRO_FONT_SLANT_NORMAL,
			       CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr[sched], 12.0);

	if (n_cpus == 1 && cpu)
		sprintf(tmp, "  C");
	else
		sprintf(tmp, "%c %d", cpu ? 'C' : 'T',
			cpu ? n - n_tasks + 1 : id);

	if (legend)
		cairo_show_text(cr[sched], tmp);

	/* Horizontal arrow */
	cairo_move_to(cr[sched], WIDTH - 37, (n + 1) * h + margin);
	cairo_line_to(cr[sched], width, (n + 1) * h + margin);

	cairo_move_to(cr[sched], width + 1, (n + 1) * h + margin - 1);
	cairo_line_to(cr[sched], width - 6, (n + 1) * h + margin + 6);

	cairo_move_to(cr[sched], width + 1, (n + 1) * h + margin + 1);
	cairo_line_to(cr[sched], width - 6, (n + 1) * h + margin - 6);

	/* Vertical arrow */
	cairo_move_to(cr[sched], WIDTH - 35, (n + 1) * h + margin);
	cairo_line_to(cr[sched], WIDTH - 35, 10 + n * h + margin);

	cairo_move_to(cr[sched], WIDTH - 34, 10 + n * h + margin - 1);
	cairo_line_to(cr[sched], WIDTH - 41, 10 + n * h + margin + 6);

	cairo_move_to(cr[sched], WIDTH - 36, 10 + n * h + margin - 1);
	cairo_line_to(cr[sched], WIDTH - 29, 10 + n * h + margin + 6);

	cairo_set_line_width(cr[sched], 2.5);
	cairo_stroke(cr[sched]);
}

void draw_lines(cairo_t * cr[10], int n_tasks, int n_cpus, int n_ticks,
		int n_sched, int scale, int disable_frequency, int cpu,
		int legend, int h)
{
	int i, j, margin;
	int width = n_ticks / 10 + WIDTH - 10;

	if (cpu)
		n_tasks = 0;

	for (j = 0; j < n_sched; j++) {
		for (i = 0; i < n_tasks + n_cpus; i++) {
			margin = (i >= n_tasks) * TASK_CPU_MARGIN;

			draw_line_sched(cr, j, i, (i >= n_tasks), n_cpus, width,
					n_tasks, legend, h);

			draw_scale(cr, j, h + 15 + i * h + margin,
				   n_ticks, scale);

			if (!disable_frequency)
				draw_cpu_speed(cr, j, (1 + i) * h + margin, h);
		}
	}
}

void draw_line_above_cpu(cairo_t * cr[10], int n_sched, int n_tasks,
			 int n_ticks, int h)
{
	int i;

	int width = n_ticks / 10 + WIDTH;
	int height = n_tasks * h + 16;

	static const double dashed[] = { 5.0, 5.0 };

	for (i = 0; i < n_sched; i++) {
		cairo_move_to(cr[i], 0, height);
		cairo_line_to(cr[i], width, height);

		cairo_set_dash(cr[i], dashed, 1, 0);

		cairo_set_line_width(cr[i], 0.5);
		cairo_stroke(cr[i]);
	}
}

void draw_line_between_schedulers(cairo_t * cr[10], int n_sched, int n_ticks)
{
	int i;

	int width = n_ticks / 10 + WIDTH;
	int height = TASK_CPU_MARGIN + 5;

	static const double dashed[] = { 5.0, 5.0 };

	for (i = 1; i < n_sched; i++) {
		cairo_move_to(cr[i], 0, height);
		cairo_line_to(cr[i], width, height);

		cairo_set_dash(cr[i], dashed, 1, 0);

		cairo_set_line_width(cr[i], 0.5);
		cairo_stroke(cr[i]);
	}
}
