#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <libyass/common.h>
#include <libyass/yass.h>

#include "main.h"

#include "config.h"

#include "run.h"

void version()
{
	printf("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
}

int main(int argc, char **argv)
{
	int c, i;
	int r;
	int opts = 0;

	int n_hyperperiods = YASS_DEFAULT_N_HYPERPERIODS;
	int n_ticks = -1;
	int jobs = YASS_DEFAULT_N_JOBS;

	int n_cpus = YASS_DEFAULT_N_CPU;
	char cpu[128] = "";

	char data[128] = "";
	char output[128] = "";
	char tests_output[128] = "";

	int n_schedulers = 0;
	char **scheduler = (char **)malloc(YASS_MAX_N_CPU * 128);

	srand(time(NULL));
	srand48(time(NULL));

	for (i = 0; i < YASS_MAX_N_CPU; i++) {
		scheduler[i] = (char *)malloc(128);
		strcpy(scheduler[i], "");
	}

	while (1) {
		static struct option long_options[] = {
			{"context-switches", no_argument, 0, OPTS_CTX},
			{"cpu", required_argument, 0, 'c'},
			{"data", required_argument, 0, 'd'},
			{"deadline-misses", no_argument, 0, OPTS_DEADLINE},
			{"debug", no_argument, 0, OPTS_DEBUG},
			{"energy", no_argument, 0, 'e'},
			{"hyperperiods", required_argument, 0, 'h'},
			{"idle", no_argument, 0, 'i'},
			{"jobs", required_argument, 0, 'j'},
			{"n-cpus", required_argument, 0, 'n'},
			{"online", no_argument, 0, OPTS_ONLINE},
			{"output", required_argument, 0, 'o'},
			{"scheduler", required_argument, 0, 's'},
			{"verbose", no_argument, 0, 'v'},
			{"version", no_argument, 0, 'V'},
			{"tests", no_argument, 0, OPTS_TESTS},
			{"tests-output", required_argument, 0, OPTS_TESTS_OUTPUT},
			{"ticks", required_argument, 0, 't'},

			{0, 0, 0, 0}
		};
		/* getopt_long stores the option index here. */
		int option_index = 0;

		c = getopt_long(argc, argv, "eivVc:d:h:j:n:o:s:t:",
				long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
			break;

		switch (c) {
		case OPTS_CTX:
			opts |= OPTS_CTX;
			break;

		case 'c':
			strcpy(cpu, optarg);
			break;

		case 'd':
			strcpy(data, optarg);
			break;

		case OPTS_DEADLINE:
			opts |= OPTS_DEADLINE;
			break;

		case OPTS_DEBUG:
			opts |= OPTS_DEBUG;
			break;

		case 'e':
			opts |= OPTS_ENERGY;
			break;

		case 'h':
			n_hyperperiods = atoi(optarg);

			if (n_hyperperiods <= 0) {
				yass_handle_error(-YASS_ERROR_N_TICKS);
				exit(1);
			}

			break;
		case 'i':
			opts |= OPTS_IDLE;
			break;

		case 'j':
			jobs = atoi(optarg);

			if (jobs <= 0 || jobs > 32) {
				yass_handle_error(-YASS_ERROR_N_JOBS);
				exit(1);
			}

			break;

		case 'n':
			n_cpus = atoi(optarg);

			if (n_cpus <= 0 || n_cpus > 32) {
				yass_handle_error(-YASS_ERROR_N_CPUS);
				exit(1);
			}

			break;

		case OPTS_ONLINE:
			opts |= OPTS_ONLINE;
			break;

		case 'o':
			strcpy(output, optarg);
			break;

		case 's':
			strcpy(scheduler[n_schedulers++], optarg);
			break;

		case OPTS_TESTS:
			opts |= OPTS_TESTS;
			break;

		case OPTS_TESTS_OUTPUT:
			strcpy(tests_output, optarg);
			break;

		case 't':
			n_ticks = atoi(optarg);

			if (n_ticks < YASS_DEFAULT_MIN_TICKS) {
				yass_handle_error(-YASS_ERROR_N_TICKS);
				exit(1);
			}

			break;

		case 'v':
			opts |= OPTS_VERBOSE;
			break;

		case 'V':
			version();
			exit(0);
			break;

		case '?':
			/* getopt_long already printed an error message. */
			break;

		default:
			abort();
		}
	}

	if (n_ticks != -1 && n_hyperperiods != YASS_DEFAULT_N_HYPERPERIODS) {
		yass_handle_error(-YASS_ERROR_TICKS_HYPERPERIOD);
		exit(1);
	}

	r = run(opts, (char *)data, n_cpus, n_ticks, n_hyperperiods,
		(char *)cpu, n_schedulers, scheduler, output, jobs,
		tests_output);

	for (i = 0; i < YASS_MAX_N_CPU; i++)
		free(scheduler[i]);
	free(scheduler);

	/*
	 * To please valgrind
	 */
	/* pthread_exit(NULL); */

	return r;
}
