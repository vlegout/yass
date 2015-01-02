#include <assert.h>
#include <math.h>
#include <stdio.h>

#include <libyass/common.h>
#include <libyass/cpu.h>
#include <libyass/log.h>
#include <libyass/helpers.h>
#include <libyass/task.h>
#include <libyass/yass.h>

#include "lpdpm.hpp"

double *rem;
double **w;

double *rem_nc;

int *priorities;

int idle_b;
int idle_e;

int idle_cpu;

const char *name()
{
	return "LPDPMMC1";
}

int offline(struct sched *sched)
{
	int error;

	struct lpdpm_functions lf = {
		create_rows_lpdpm2,
		add_constraints_lpdpm2,
		0,
		0.2,
	};

	error = yass_sched_add_idle_task(sched);

	if (error)
		return error;

	error = lpdpm_offline(&lf, sched, &w, &rem, &priorities, &rem_nc);

	if (error)
		return error;

	if (!compute_weights(sched, w, &lf))
		return -YASS_ERROR_NOT_SCHEDULABLE;

	idle_cpu = 0;

	idle_b = 0;
	idle_e = 0;

	return 0;
}

int schedule(struct sched *sched)
{
	return lpdpm_schedule(sched, rem, w, priorities, &idle_b, &idle_e,
			      &idle_cpu, rem_nc);
}

int sched_close(struct sched *sched __attribute__ ((unused)))
{
	return lpdpm_close(sched, rem, priorities, w, rem_nc);
}
