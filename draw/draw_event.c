#include <assert.h>
#include <cairo.h>
#include <stdio.h>
#include <stdlib.h>

#include <libyass/common.h>

#include "draw.h"
#include "draw_event.h"

struct event {
	int tick;
	int task;
};

int *id;
int ***boundaries;
int **speed;
double ***cons;
struct event **run;

cairo_pattern_t **colors;
cairo_pattern_t *colors_blank;

static int task_add_index(int n)
{
	int i = 0;

	while (id[i] != -1)
		i++;

	id[i] = n;

	return i;
}

static int task_get_index(int n)
{
	int i;

	for (i = 0; i < YASS_MAX_N_TASKS; i++) {
		if (id[i] == n)
			return i;
	}

	return -1;
}

int task_get_id(int index)
{
	return id[index];
}

void draw_init(int n_tasks, int n_sched, int n_cpus, int n_ticks)
{
	int i, j;

	boundaries =
	    (int ***)calloc(n_tasks * 2 * (n_ticks / 100 + 2), sizeof(int));
	speed = (int **)calloc(n_sched * n_cpus, sizeof(int));
	run = (struct event **)calloc(n_sched * n_cpus, sizeof(struct event));
	cons = (double ***)calloc(n_sched * n_cpus * 2, sizeof(double));

	id = (int *)calloc(YASS_MAX_N_TASKS, sizeof(int));

	for (i = 0; i < YASS_MAX_N_TASKS; i++)
		id[i] = -1;

	for (i = 0; i < n_tasks; i++) {
		boundaries[i] =
		    (int **)calloc(2 * (n_ticks / 100 + 2), sizeof(int));
		boundaries[i][0] =
		    (int *)calloc(n_ticks / 100 + 2, sizeof(int));
		boundaries[i][1] =
		    (int *)calloc(n_ticks / 100 + 2, sizeof(int));

		for (j = 0; j < n_ticks / 100 + 2; j++) {
			boundaries[i][0][j] = -1;
			boundaries[i][1][j] = -1;
		}
	}

	for (i = 0; i < n_sched; i++) {
		speed[i] = (int *)calloc(n_cpus, sizeof(int));
		run[i] = (struct event *)calloc(n_cpus, sizeof(struct event));
		cons[i] = (double **)calloc(n_cpus * 2, sizeof(double));

		for (j = 0; j < n_cpus; j++) {
			speed[i][j] = 100;

			run[i][j].tick = -1;
			run[i][j].task = -1;

			cons[i][j] = (double *)calloc(2, sizeof(double));
			cons[i][j][0] = 100;
			cons[i][j][1] = 200 + j * 225;
		}
	}
}

void draw_free(int n_tasks, int n_sched, int n_cpus)
{
	int i, j;

	free(id);

	for (i = 0; i < n_tasks; i++) {
		free(boundaries[i][0]);
		free(boundaries[i][1]);
		free(boundaries[i]);
	}

	for (i = 0; i < n_sched; i++) {
		free(speed[i]);
		free(run[i]);

		for (j = 0; j < n_cpus; j++)
			free(cons[i][j]);
		free(cons[i]);
	}

	free(boundaries);
	free(speed);
	free(run);
	free(cons);

	free(colors);
}

void set_cpu_speed(int scheduler, int cpu, int new_speed)
{
	speed[scheduler][cpu] = new_speed;
}

void add_boundary(int type, int task, int tick)
{
	int i = 0;
	int index = task_get_index(task);

	if (index == -1)
		index = task_add_index(task);

	while (boundaries[index][type][i] != -1)
		i++;

	boundaries[index][type][i] = tick;
}

static void draw_arrows(cairo_t * cr[10], int sched, int task, int type, int h)
{
	int i = 0, width;

	int y1 = (type == 0) ? h - 15 : h;
	int y2 = (type == 0) ? h - 12 : h - 3;

	while (boundaries[task][type][i] != -1) {
		cairo_set_source_rgb(cr[sched], 0, 0, 0);

		width = WIDTH + boundaries[task][type][i];

		cairo_move_to(cr[sched], width - 35, h + task * h);
		cairo_line_to(cr[sched], width - 35, h - 15 + task * h);

		cairo_move_to(cr[sched], width - 35, y1 + task * h);
		cairo_line_to(cr[sched], width - 38, y2 + task * h);

		cairo_move_to(cr[sched], width - 35, y1 + task * h);
		cairo_line_to(cr[sched], width - 32, y2 + task * h);

		cairo_set_line_width(cr[sched], 1.2);
		cairo_stroke(cr[sched]);

		i++;
	}
}

void draw_boundaries(cairo_t * cr[10], int n_tasks, int n_schedulers, int h)
{
	int i, k;

	for (k = 0; k < n_schedulers; k++) {
		for (i = 0; i < n_tasks; i++) {
			draw_arrows(cr, k, i, 0, h);
			draw_arrows(cr, k, i, 1, h);
		}
	}
}

void save_run(int sched, int task, int tick, int cpu)
{
	if (task_get_index(task) == -1)
		task_add_index(task);

	run[sched][cpu].tick = tick;
	run[sched][cpu].task = task;
}

void draw_execution(int sched, int task, int tick, int cpu, int n_tasks,
		    int ticks, cairo_t * cr[10], int only_cpu, int h)
{
	int run_tick = run[sched][cpu].tick;
	int run_task = run[sched][cpu].task;

	int index = task_get_index(task);

	double x = WIDTH - 35 + run_tick;
	double y;

	double width = tick - run_tick;
	double height = speed[sched][cpu] * ((4 * ((double)h) / 5 - 7) / 100);

	assert(tick != -1 && task != -1);
	assert(task == run_task);

	if (ticks > -1 && tick * 10 > ticks)
		return;

	if (task == -2)
		cairo_set_source(cr[sched], colors_blank);
	else
		cairo_set_source(cr[sched], colors[index]);

	cairo_set_line_width(cr[sched], 1.5);

	if (only_cpu) {
		y = h + cpu * h + TASK_CPU_MARGIN - height;

		cairo_rectangle(cr[sched], x, y, width, height);

		cairo_fill(cr[sched]);

		return;
	}

	if (task != -2) {
		y = h + index * h - height;
		cairo_rectangle(cr[sched], x, y, width, height);
	}

	cairo_set_line_width(cr[sched], 1.5);
	cairo_fill(cr[sched]);

	y = (1 + n_tasks + cpu) * h + TASK_CPU_MARGIN - height;
	cairo_rectangle(cr[sched], x, y, width, height);

	cairo_set_line_width(cr[sched], 1.5);
	cairo_fill(cr[sched]);
}

void colors_init(int disable_dpm, int n_tasks)
{
	int i, index;

	double red, green, blue;

	double color_table[6][6][3] = {
		{
			{1, 1, 1}
		},
		{
			{0.96862745, 0.98823529, 0.7254902},
			{0.19215686, 0.63921569, 0.32941176}
		},
		{
			{0.96862745, 0.98823529, 0.7254902},
			{0.67843137, 0.86666667, 0.55686275},
			{0.19215686, 0.63921569, 0.32941176}
		},
		{
			{1, 1, 0.8},
			{0.76078431, 0.90196078, 0.6},
			{0.47058824, 0.77647059, 0.47058824},
			{0.1372549, 0.51764706, 0.2627451}
		},
		{
			{1, 1, 0.8},
			{0.76078431, 0.90196078, 0.6},
			{0.47058824, 0.77647059, 0.47058824},
			{0.19215686, 0.63921569, 0.32941176},
			{0, 0.40784314, 0.21568627}
		},
		{
			{1, 1, 0.8},
			{0.85098039, 0.94117647, 0.63921569},
			{0.67843137, 0.86666667, 0.55686275},
			{0.47058824, 0.77647059, 0.47058824},
			{0.19215686, 0.63921569, 0.32941176},
			{0, 0.40784314, 0.21568627}
		}
	};

	colors =
	    (cairo_pattern_t **) calloc(n_tasks, sizeof(cairo_pattern_t *));

	if (n_tasks > 6)
		index = 6;
	else
		index = n_tasks;

	for (i = 0; i < n_tasks; i++) {

		if (i < 6) {
			red = color_table[index - 1][i][0];
			green = color_table[index - 1][i][1];
			blue = color_table[index - 1][i][2];
		} else {
			red = 0 + 0.08 * (i % 12);
			green = 0.4 + 0.5 * (i % 2);
			blue = 0 + 0.45 * (i % 3);
		}

		colors[i] = cairo_pattern_create_rgb(red, green, blue);
	}

	if (disable_dpm)
		colors_blank = cairo_pattern_create_rgba(0, 0, 0, 0);
	else
		colors_blank = cairo_pattern_create_rgba(0, 0.2, 0.2, 0.4);
}

void colors_destroy(int n_tasks)
{
	int i;

	for (i = 0; i < n_tasks; i++)
		cairo_pattern_destroy(colors[i]);

}
