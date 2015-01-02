#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "draw.h"

#define DEFAULT_HEIGHT 60

int main(int argc, char **argv)
{
	int c, option_index;
	int opts = 0;

	char input[128] = "";
	char output[128] = "";

	int choice;
	int height = DEFAULT_HEIGHT;
	int n_ticks = -1;
	int scale = 0;

	while (1) {
		static struct option long_options[] = {
			{"cpu", no_argument, 0, 'c'},
			{"disable-dpm", no_argument, 0, 'd'},
			{"disable-frequency", no_argument, 0, 'f'},
			{"height", required_argument, 0, 'h'},
			{"input", required_argument, 0, 'i'},
			{"legend", no_argument, 0, 'l'},
			{"line-above-cpus", no_argument, 0, OPTS_SEP_TASK_CPU},
			{"output", required_argument, 0, 'o'},
			{"one-page", no_argument, 0, 'p'},
			{"scale", required_argument, 0, 's'},
			{"ticks", required_argument, 0, 't'},
			{"pdf", no_argument, 0, OPTS_PDF},
			{"png", no_argument, 0, OPTS_PNG},
			{"ps", no_argument, 0, OPTS_PS},
			{"svg", no_argument, 0, OPTS_SVG},
			{0, 0, 0, 0}
		};

		/* getopt_long stores the option index here. */
		option_index = 0;

		c = getopt_long(argc, argv, "cdflph:i:o:s:t:", long_options,
				&option_index);

		/* Detect the end of the options. */
		if (c == -1)
			break;

		switch (c) {
		case OPTS_SEP_TASK_CPU:
			opts |= OPTS_SEP_TASK_CPU;
			break;

		case 'c':
			opts |= OPTS_CPU;
			break;

		case 'd':
			opts |= OPTS_DISABLE_DPM;
			break;

		case 'f':
			opts |= OPTS_DISABLE_FREQUENCY;
			break;

		case 'h':
			height = atoi(optarg);
			if (height < 40 || height > 200) {
				fprintf(stderr, "yass-draw: unvalid height.\n");
				exit(1);
			}
			break;

		case 'l':
			opts |= OPTS_LEGEND;
			break;

		case 'i':
			strcpy(input, optarg);
			break;

		case 'o':
			strcpy(output, optarg);
			break;

		case 'p':
			opts |= OPTS_ONE_PAGE;
			break;

		case 's':
			scale = atoi(optarg);
			break;

		case 't':
			n_ticks = atoi(optarg);
			break;

		case OPTS_PDF:
			opts |= OPTS_PDF;
			break;

		case OPTS_PNG:
			opts |= OPTS_PNG;
			break;

		case OPTS_PS:
			opts |= OPTS_PS;
			break;

		case OPTS_SVG:
			opts |= OPTS_SVG;
			break;

		case '?':
			/* getopt_long already printed an error message. */
			break;

		default:
			abort();
		}
	}

	if (((opts & OPTS_PDF) > 0) + ((opts & OPTS_PNG) > 0) +
	    ((opts & OPTS_PS) > 0) + ((opts & OPTS_SVG) > 0) > 1) {
		fprintf(stderr,
			"yass-draw: Please choose only one output format.\n");
		exit(1);
	}

	if (opts & OPTS_PNG)
		choice = OPTS_PNG;
	else if (opts & OPTS_PS)
		choice = OPTS_PS;
	else if (opts & OPTS_SVG)
		choice = OPTS_SVG;
	else
		choice = OPTS_PDF;

	if (scale != 0 && scale != 1 && scale != 2 && scale != 3) {
		fprintf(stderr, "yass-draw: scale must be 0, 1, 2 or 3.\n");
		exit(1);
	}

	/* Print any remaining command line arguments (not options). */
	if (optind < argc) {
		printf("non-option ARGV-elements: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		putchar('\n');
	}

	draw(opts, input, choice, n_ticks, scale, output, height);

	return 0;
}
