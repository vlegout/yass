#ifndef _YASS_RUN_H
#define _YASS_RUN_H

int run(int opts, const char *data, int n_cpus, int n_ticks, int n_hyperperiods,
	const char *cpu, int n_schedulers, char **scheduler, char *output,
	int jobs, char *tests_output);

#endif				/* _YASS_TESTS_H */
