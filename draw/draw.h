#ifndef _YASS_DRAW_H
#define _YASS_DRAW_H

#include <libyass/common.h>

#define WIDTH 80

#define TASK_CPU_MARGIN 10

#define OPTS_CPU               1
#define OPTS_LEGEND            2
#define OPTS_DISABLE_DPM       4
#define OPTS_DISABLE_FREQUENCY 8
#define OPTS_SEP_TASK_CPU      16
#define OPTS_ONE_PAGE          32
#define OPTS_PDF               64
#define OPTS_PNG               128
#define OPTS_PS                256
#define OPTS_SVG               512

void draw(int opts, char input[128], int choice, int ticks, int scale,
	  char output[128], int h);

#endif				/* _YASS_DRAW_H */
