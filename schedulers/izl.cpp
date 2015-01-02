#include <assert.h>
#include <math.h>
#include <stdio.h>

#include <libyass/common.h>
#include <libyass/cpu.h>
#include <libyass/log.h>
#include <libyass/list.h>
#include <libyass/helpers.h>
#include <libyass/scheduler.h>
#include <libyass/task.h>
#include <libyass/yass.h>

#include "lpdpm.hpp"

#define N_IZL 3

double *rem;
double **w;

int *priorities;

const char *name()
{
	return "IZL";
}

void create_rows(struct sched *sched, IloModel model, IloNumVarArray x,
		 int index, int subtask, int interval)
{
	IloEnv env = model.getEnv();

	int id = yass_task_get_id(sched, index);

	add_variable(env, 'w', id, subtask, interval, ILOFLOAT, 0, 1, x);
	add_variable(env, 'x', id, subtask, interval, ILOBOOL, 0, 1, x);
	add_variable(env, 'y', id, subtask, interval, ILOBOOL, 0, 1, x);
}

void add_constraints(struct lpdpm_functions *lf, struct sched *sched,
		     IloModel model, IloNumVarArray x, IloRangeArray con,
		     int *I, int n_intervals, int row, int inter, IloExpr obj,
		     int subtask)
{
	IloEnv env = model.getEnv();

	int index = (row - inter) / n_intervals;

	int ww = index * (n_intervals * 3) + inter * 3;
	int xx = ww + 1;
	int yy = xx + 1;
	int xx1 = yy + 1;

	/*
	 * Utilization cannot be greater than M
	 */
	con[inter].setLinearCoef(x[ww], 1.0);

	/*
	 * sum_{k} w_{j, k} * |I_k| = C
	 */
	con[n_intervals + subtask].setLinearCoef(x[ww], I[inter]);

	switch (lf->int_field) {
	case 0:
		obj += x[xx];
		break;
	case 1:
		obj += x[yy];
		break;
	case 2:
		obj += x[xx];
		obj += x[yy];
		break;
	}

	/*
	 * x
	 */
	model.add(x[xx] >= x[ww]);

	if (inter != n_intervals - 1) {
		/*
		 * y
		 */
		model.add(IloIfThen(env, x[xx] == 1 && x[xx1] == 0,
				    x[yy] == 1));
		model.add(IloIfThen(env, !(x[xx] == 1 && x[xx1] == 0),
				    x[yy] == 0));
	} else {
		model.add(x[yy] == 0);
	}
}

static int izl_run(struct yass *yass, double **w_izl)
{
	int i, j, r = 0;

	int n_ticks = yass_get_nticks(yass);

	struct sched *sched = yass_get_sched(yass, 0);

	double *rem_izl;
	int *priorities_izl;

	r = lpdpm_offline(NULL, sched, NULL, &rem_izl, &priorities_izl, NULL);

	if (r)
		return r;

	yass_warn(n_ticks >= YASS_DEFAULT_MIN_TICKS);

	for (i = 0; i < n_ticks; i++) {

		r = lpdpm_schedule(sched, rem_izl, w_izl, priorities_izl, NULL,
				   NULL, NULL, NULL);

		if (r) {
			lpdpm_close(sched, rem_izl, priorities_izl, NULL, NULL);
			return r;
		}

		for (j = 0; j < yass_sched_get_ncpus(sched); j++)
			yass_cpu_cons_inc(sched, j);

		yass_sched_update_idle(sched);

		yass_sched_tick_inc(sched);
	}

	r = lpdpm_close(sched, rem_izl, priorities_izl, NULL, NULL);

	return r;
}

static int compute_ctx(struct sched *sched, double **w_izl, int **exec_time)
{
	int ctx = 0, i, r;

	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);

	unsigned int h = yass_sched_get_hyperperiod(sched);

	struct yass *yass;
	struct sched *sched_tmp;

	yass = yass_new();

	r = yass_init(yass, 0, n_cpus, h, -1, "", 0, 1, NULL, n_tasks, 0, 0);

	if (r)
		return r;

	r = yass_init_tasks(yass, yass_sched_get_tasks(sched), exec_time);

	if (r)
		return r;

	r = izl_run(yass, w_izl);

	sched_tmp = yass_get_sched(yass, 0);

	if (r)
		return r;

	for (i = 0; i < yass_sched_get_ncpus(sched_tmp); i++)
		ctx += yass_cpu_get_context_switches(sched_tmp, i);

	yass_free(yass);

	return ctx;
}

static int choose(struct sched *sched, double ***w_izl)
{
	int ctx, i, r = -1;
	int min_ctx = INT_MAX;

	int n_tasks = yass_sched_get_ntasks(sched);

	int **exec_time;

	exec_time = yass_tasks_generate_exec(yass_sched_get_tasks(sched),
					     n_tasks);

	for (i = 0; i < N_IZL; i++) {
		ctx = compute_ctx(sched, w_izl[i], exec_time);

		if (ctx < 0)
			return ctx;

		if (ctx < min_ctx) {
			r = i;
			ctx = min_ctx;
		}
	}

	yass_task_free_exec_time(exec_time, n_tasks);

	return r;
}

static int init_izl(struct sched *sched, double ****w_izl)
{
	int i, j, k;

	int n_tasks = yass_sched_get_ntasks(sched);

	lpdpm_offline(NULL, sched, &w, &rem, &priorities, NULL);

	(*w_izl) = (double ***)calloc(N_IZL * n_tasks * MAX_N_INTER,
				      sizeof(double));

	if (!(*w_izl))
		return -YASS_ERROR_MALLOC;

	for (i = 0; i < N_IZL; i++) {
		(*w_izl)[i] = (double **)calloc(n_tasks * MAX_N_INTER,
						sizeof(double));

		if (!(*w_izl)[i])
			return -YASS_ERROR_MALLOC;

		for (j = 0; j < n_tasks; j++) {
			(*w_izl)[i][j] = (double *)calloc(MAX_N_INTER,
							  sizeof(double));

			if (!(*w_izl)[i][j])
				return -YASS_ERROR_MALLOC;

			for (k = 0; k < MAX_N_INTER; k++)
				(*w_izl)[i][j][k] = 0;
		}
	}

	return 0;
}

int offline(struct sched *sched)
{
	int i, j, r;

	int n_tasks = yass_sched_get_ntasks(sched);

	double ***w_izl;

	struct lpdpm_functions lf = {
		create_rows,
		add_constraints,
		0,
	};

	r = init_izl(sched, &w_izl);

	if (r)
		return r;

	for (i = 0; i < N_IZL; i++) {
		lf.int_field = i;

		if (!compute_weights(sched, w_izl[i], &lf))
			return -YASS_ERROR_NOT_SCHEDULABLE;
	}

	/*
	 * Now, we must choose the most appropriate weights. The only
	 * way is to schedule all the solutions ...
	 */
	r = choose(sched, w_izl);

	if (r < 0)
		return r;

	for (i = 0; i < n_tasks; i++)
		for (j = 0; j < MAX_N_INTER; j++)
			w[i][j] = w_izl[r][i][j];

	for (i = 0; i < N_IZL; i++) {
		for (j = 0; j < n_tasks; j++)
			free(w_izl[i][j]);

		free(w_izl[i]);
	}

	free(w_izl);

	return 0;
}

int schedule(struct sched *sched)
{
	return lpdpm_schedule(sched, rem, w, priorities,
			      NULL, NULL, NULL, NULL);
}

int sched_close(struct sched *sched __attribute__ ((unused)))
{
	return lpdpm_close(sched, rem, priorities, w, NULL);
}
