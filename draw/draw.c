#include <cairo.h>
#include <cairo-pdf.h>
#include <cairo-ps.h>
#include <cairo-svg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "draw.h"

#include "draw_background.h"
#include "draw_event.h"

#define DEFAULT_INPUT "output.txt"

cairo_surface_t *final_surface;;
cairo_t *final;

cairo_surface_t *surface[10];
cairo_t *cr[10];

static void handle_event(int d[6], int n_tasks, int ticks, int cpu, int h)
{
	if (d[1] == YASS_IDLE_TASK_ID)
		return;

	switch (d[0]) {
	case YASS_EVENT_TASK_RUN:
		save_run(d[1], d[2], d[3] / 10, d[4]);
		break;
	case YASS_EVENT_TASK_TERMINATE:
		draw_execution(d[1], d[2], d[3] / 10, d[4], n_tasks, ticks, cr,
			       cpu, h);
		break;

	case YASS_EVENT_TASK_RELEASE:
		add_boundary(YASS_EVENT_TASK_RELEASE, d[1], d[2] / 10);
		break;
	case YASS_EVENT_TASK_DEADLINE:
		add_boundary(YASS_EVENT_TASK_DEADLINE, d[1], d[2] / 10);
		break;

	case YASS_EVENT_CPU_SPEED:
		set_cpu_speed(d[1], d[2], d[3]);
		break;
	case YASS_EVENT_CPU_MODE:
		break;
	case YASS_EVENT_CPU_CONSUMPTION:
		break;

	default:
		fprintf(stderr, "yass-draw: cannot parse input file\n");
		exit(1);
		break;
	}
}

static void image_init(int n_tasks, int n_cpus, int n_ticks, int n_sched,
		       int choice, char output[128], int cpu, int one_page,
		       int legend, int h)
{
	int i;
	int width, height;

	if (cpu)
		n_tasks = 0;

	width = WIDTH - 5 + n_ticks / 10;
	height = 20 + (n_tasks + n_cpus) * h + TASK_CPU_MARGIN;

	for (i = 0; i < n_sched; i++) {
		switch (choice) {
		case OPTS_PDF:
		default:
			surface[i] = cairo_pdf_surface_create(NULL, width,
							      height);
			break;
		case OPTS_PS:
			surface[i] = cairo_ps_surface_create(NULL, width,
							     height);
			break;
		case OPTS_SVG:
			surface[i] = cairo_svg_surface_create(NULL, width,
							      height);
			break;
		}

		cr[i] = cairo_create(surface[i]);
	}

	if (!strcmp(output, "")) {
		switch (choice) {
		case OPTS_PDF:
		default:
			strcpy(output, "output.pdf");
			break;
		case OPTS_PNG:
			output = NULL;
			break;
		case OPTS_PS:
			strcpy(output, "output.ps");
			break;
		case OPTS_SVG:
			strcpy(output, "output.svg");
			break;
		}
	}

	if (one_page)
		height = n_sched * (height - 10);

	if (!legend)
		width -= WIDTH - 50;

	switch (choice) {
	case OPTS_PDF:
	default:
		final_surface = cairo_pdf_surface_create(output, width, height);
		break;
	case OPTS_PNG:
		final_surface = cairo_pdf_surface_create(NULL, width, height);
		break;
	case OPTS_PS:
		final_surface = cairo_ps_surface_create(output, width, height);
		break;
	case OPTS_SVG:
		final_surface = cairo_svg_surface_create(output, width, height);
		break;
	}

	final = cairo_create(final_surface);
}

static void destroy_surfaces(int n_sched)
{
	int i;

	for (i = 0; i < n_sched; i++) {
		cairo_destroy(cr[i]);
		cairo_surface_destroy(surface[i]);
	}

	cairo_destroy(final);
	cairo_surface_destroy(final_surface);
}

void draw(int opts, char input[128], int choice, int ticks, int scale,
	  char output[128], int h)
{
	int i, height = 0;
	int n_cpus, n_ticks, n_tasks, n_sched;
	int d[6];
	FILE *fp;

	int cpu = opts & OPTS_CPU;
	int disable_dpm = opts & OPTS_DISABLE_DPM;
	int disable_frequency = opts & OPTS_DISABLE_FREQUENCY;
	int legend = !(opts & OPTS_LEGEND);
	int line_above_cpus = opts & OPTS_SEP_TASK_CPU;
	int one_page = opts & OPTS_ONE_PAGE;

	if (!strcmp(input, ""))
		strcpy(input, DEFAULT_INPUT);

	if ((fp = fopen(input, "r")) == NULL) {
		fprintf(stderr, "yass-draw: cannot open input file\n");
		exit(1);
	}

	if (fscanf(fp, "%d %d %d %d",
		   &n_tasks, &n_cpus, &n_ticks, &n_sched) != 4) {
		fprintf(stderr, "yass-draw: cannot parse input file\n");
		exit(1);
	}

	image_init(n_tasks, n_cpus, n_ticks, n_sched, choice, output, cpu,
		   one_page, legend, h);

	colors_init(disable_dpm, n_tasks);
	draw_init(n_tasks, n_sched, n_cpus, n_ticks);

	while (fscanf(fp, "%d %d %d %d %d %d",
		      &d[0], &d[1], &d[2], &d[3], &d[4], &d[5]) == 6) {
		handle_event(d, n_tasks, ticks, cpu, h);
	}

	draw_lines(cr, n_tasks, n_cpus, n_ticks, n_sched, scale,
		   disable_frequency, cpu, legend, h);

	if (!cpu)
		draw_boundaries(cr, n_tasks, n_sched, h);

	if (line_above_cpus)
		draw_line_above_cpu(cr, n_sched, n_tasks, n_ticks, h);

	if (one_page)
		draw_line_between_schedulers(cr, n_sched, n_ticks);

	for (i = 0; i < n_sched; i++) {
		if (one_page && !cpu)
			height = h * (n_cpus + n_tasks) + TASK_CPU_MARGIN;
		else if (one_page && cpu)
			height = h * n_cpus + TASK_CPU_MARGIN;

		if (legend)
			cairo_set_source_surface(final, surface[i],
						 0, height * i);
		else
			cairo_set_source_surface(final, surface[i],
						 -30, height * i);

		cairo_paint(final);

		if (!one_page)
			cairo_show_page(final);

		if (choice == OPTS_PNG) {
			if (!strcmp(output, ""))
				strcpy(output, "output.png");

			cairo_surface_write_to_png(surface[i], output);
		}
	}

	colors_destroy(n_tasks);
	destroy_surfaces(n_sched);

	draw_free(n_tasks, n_sched, n_cpus);
}
