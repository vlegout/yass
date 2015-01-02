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

int *priorities;

int idle_b;
int idle_e;

int idle_cpu;

const char *name()
{
	return "LPDPM1";
}

void create_rows(struct sched *sched, IloModel model, IloNumVarArray x,
		 int index, int subtask, int interval)
{
	IloEnv env = model.getEnv();

	int n_tasks = yass_sched_get_ntasks(sched);

	int id = yass_task_get_id(sched, index);

	add_variable(env, 'w', id, subtask, interval, ILOFLOAT, 0, 1, x);

	if (index == n_tasks - 1) {
		add_variable(env, 'f', id, subtask, interval, ILOBOOL, 0, 1, x);
		add_variable(env, 'e', id, subtask, interval, ILOBOOL, 0, 1, x);

		add_variable(env, 'F', id, subtask, interval, ILOBOOL, 0, 1, x);
		add_variable(env, 'E', id, subtask, interval, ILOBOOL, 0, 1, x);
	}

}

void add_constraints(struct lpdpm_functions *lf, struct sched *sched,
		     IloModel model, IloNumVarArray x, IloRangeArray con,
		     int *I, int n_intervals, int row, int inter, IloExpr obj,
		     int subtask)
{
	IloEnv env = model.getEnv();

	int row_idle = row + 4 * inter;

	int ww = row_idle;
	int f = row_idle + 1;
	int e = row_idle + 2;
	int F = row_idle + 3;
	int E = row_idle + 4;

	int f1 = row_idle + 5 + 1;
	int e1 = row_idle + 5 + 2;

	obj += x[f];
	obj += x[e];
	obj += x[F];
	obj += x[E];

	/*
	 * Utilization cannot be greater than M
	 */
	con[inter].setLinearCoef(x[ww], 1);

	/*
	 * sum_{k} w_{j, k} * |I_k| = C
	 */
	con[n_intervals + subtask].setLinearCoef(x[ww], I[inter]);

	/*
	 * f_k
	 */
	model.add(IloIfThen(env, x[ww] == 1, x[f] == 0));
	model.add(IloIfThen(env, x[ww] != 1, x[f] == 1));

	/*
	 * e_k
	 */
	model.add(IloIfThen(env, x[ww] == 0, x[e] == 0));
	model.add(IloIfThen(env, x[ww] != 0, x[e] == 1));

	if (inter == n_intervals - 1)
		return;

	/*
	 * F_k
	 */
	model.add(IloIfThen(env, x[f] == 1 && x[f1] == 0, x[F] == 1));
	model.add(IloIfThen(env, !(x[f] == 1 && x[f1] == 0), x[F] == 0));

	/*
	 * E_k
	 */
	model.add(IloIfThen(env, x[e] == 1 && x[e1] == 0, x[E] == 1));
	model.add(IloIfThen(env, !(x[e] == 1 && x[e1] == 0), x[E] == 0));
}

int offline(struct sched *sched)
{
	int error;

	struct lpdpm_functions lf = {
		create_rows,
		add_constraints,
	};

	error = yass_sched_add_idle_task(sched);

	if (error)
		return error;

	error = lpdpm_offline(&lf, sched, &w, &rem, &priorities, NULL);

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
			      &idle_cpu, NULL);
}

int sched_close(struct sched *sched __attribute__ ((unused)))
{
	return lpdpm_close(sched, rem, priorities, w, NULL);
}
