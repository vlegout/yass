
#include <math.h>
#include <stdio.h>

#include <libyass/cpu.h>
#include <libyass/helpers.h>
#include <libyass/scheduler.h>

#include "lpdpm.hpp"

int sched_is_mc(const char *name)
{
	if (!strstr(name, "LPDPMMC"))
		return 0;

	return 1;
}

int cpus_always_on(struct sched *sched)
{
	if (!strcmp(name(), "IZL"))
		return 0;

	if (yass_sched_get_online(sched))
		return 0;

	return 1;
}

static void cplex_set_parameter(struct sched *sched, IloCplex cplex)
{
	if (!yass_sched_get_debug(sched)) {
		cplex.setParam(IloCplex::SimDisplay, 0);
		cplex.setParam(IloCplex::MIPDisplay, 0);
	}

	cplex.setParam(IloCplex::TiLim, CPLEX_TIME);
	cplex.setParam(IloCplex::EpRHS, 1e-9);
}

static int is_solution_valid(struct sched *sched, int *I, int n_intervals, double **w)
{
	int i, j;
	double b, e, total;

	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched) + 1;

	if (!strcmp(name(), "IZL"))
		n_tasks--;

	for (i = 0; i < n_intervals; i++) {
		total = 0;

		for (j = 0; j < n_tasks; j++) {
			total += w[j][i] * I[i];

			if (w[j][i] < 0)
				return 0;
		}

		if (strcmp(name(), "IZL")) {
			b = w[n_tasks - 2][i];
			e = w[n_tasks - 1][i];

			if (b + e > 1 + EPSILON)
				return 0;
		}

		if (total - n_cpus * I[i] > EPSILON)
			return 0;

		if (cpus_always_on(sched) && total - n_cpus * I[i] < -EPSILON)
			return 0;
	}

	return 1;
}

static void populate_w(struct sched *sched, double **w, IloNumArray val,
		       IloNumVarArray x)
{
	int i, index;
	int id, subtask, interval;

	long double ld;

	char c, tmp[128];

	for (i = 0; i < val.getSize(); i++) {

		strcpy(tmp, x[i].getName());
		sscanf(tmp, "%c_%d_%d_%d", &c, &id, &subtask, &interval);

		interval--;

		ld = val[i];

		if ((c == 'w' || c == 'b' || c == 'e') &&
		    ld - round(ld) < EPSILON) {
			ld *= 1e9;
			ld = round(ld);
			ld /= 1e9;
		}

		index = yass_task_get_from_id(sched, id);

		if (!strcmp(name(), "LPDPM1")) {
			if (c == 'w')
				w[index][interval] = ld;
		} else if (!strcmp(name(), "IZL")) {
			if (c == 'w')
				w[index][interval] = ld;
		} else {
			if (c == 'w')
				w[index][interval] = ld;
			else if (c == 'b')
				w[index][interval] = ld;
			else if (c == 'e')
				w[index + 1][interval] = ld;
		}
	}
}

void print_matrix(IloNumVarArray x, IloRangeArray con)
{
	int i;

	for (i = 0; i < con.getSize(); i++) {
		printf("Row %d (%s): ", i, con[i].getName());

		IloExpr::LinearIterator it = con[i].getLinearIterator();

		while (it.ok()) {
			printf(" %s %.2lf", it.getVar().getName(),
			       it.getCoef());
			++it;
		}

		printf(" -- %lf %lf", con[i].getLB(), con[i].getUB());

		printf("\n");
	}

	printf("\n");

	for (i = 0; i < x.getSize(); i++) {
		printf("Col %d", i);

		printf(" %s", x[i].getName());

		printf(" (%.2lf %.2lf):", x[i].getLB(), x[i].getUB());

		printf("\n");
	}

	printf("\n");
}

static void print_results(IloNumArray val, IloNumVarArray x, int status)
{
	int i;

	printf("Solution status = %d\n\n", status);

	for (i = 0; i < val.getSize(); i++) {
		printf("%s: %lf\n", x[i].getName(), val[i]);
	}
}

void add_variable(IloModel model, char c, int id, int subtask, int interval,
		  IloNumVar::Type type, double min, double max,
		  IloNumVarArray x)
{
	char tmp[128];

	IloEnv env = model.getEnv();

	sprintf(tmp, "%c_%d_%d_%d", c, id, subtask, interval);
	x.add(IloNumVar(env, min, max, type, tmp));
}

void wcet_constraint(struct lpdpm_functions *lf, struct sched *sched, int id,
		     IloRangeArray con, IloEnv env)
{
	int index = yass_task_get_from_id(sched, id);
	int criticality = yass_task_get_criticality(sched, id);
	int period = yass_task_get_period(sched, id);
	int wcet = yass_task_get_wcet(sched, id);

	int n_tasks = yass_sched_get_ntasks(sched);

	if (sched_is_mc(name())) {
		if (yass_sched_task_is_idle_task(sched, id))
			con.add(IloRange(env, wcet, period));
		else if (criticality == 0)
			con.add(IloRange(env, lf->double_field * wcet, wcet));
		else
			con.add(IloRange(env, wcet, wcet));
	} else if (strcmp(name(), "LPDPM3") || index <= n_tasks - 1) {
		con.add(IloRange(env, wcet, wcet));
	}
}

void create_rows_lpdpm2(struct sched *sched, IloModel model, IloNumVarArray x,
			int index, int subtask, int interval)
{
	int j;

	IloEnv env = model.getEnv();

	int n_tasks = yass_sched_get_ntasks(sched);
	int n_states = yass_cpu_get_nstates(sched);

	int id = yass_task_get_id(sched, index);

	if (index != n_tasks - 1) {
		add_variable(env, 'w', id, subtask, interval, ILOFLOAT, 0, 1, x);
	} else {
		add_variable(env, 'b', id, subtask, interval, ILOFLOAT, 0, 1, x);
		add_variable(env, 'e', id, subtask, interval, ILOFLOAT, 0, 1, x);

		add_variable(env, 'l', id, subtask, interval, ILOFLOAT, 0, IloInfinity, x);
		add_variable(env, 'L', id, subtask, interval, ILOFLOAT, 0, IloInfinity, x);

		for (j = 0; j < n_states + 1; j++)
			add_variable(env, 'p', id, subtask, interval, ILOBOOL, 0, 1, x);

		add_variable(env, 'P', id, subtask, interval, ILOFLOAT, 0, IloInfinity, x);
	}
}

void add_variable_interval(struct lpdpm_functions *lf, struct sched *sched,
			   IloModel model, IloNumVarArray x, IloRangeArray con,
			   int *I)
{
	int i, id, interval, period, subtask;

	int n_tasks = yass_sched_get_ntasks(sched);
	int n_states = yass_cpu_get_nstates(sched);

	IloEnv env = model.getEnv();

	int idle = YASS_IDLE_TASK_ID;

	unsigned long long t;
	unsigned long long tick = yass_sched_get_tick(sched);
	unsigned long long h = yass_sched_get_hyperperiod(sched);

	/*
	 * Run through all intervals for each task and call
	 * create_rows to add variables.
	 */
	for (i = 0; i < n_tasks; i++) {
		id = yass_task_get_id(sched, i);
		period = yass_task_get_period(sched, id);

		subtask = 0;
		t = tick;
		interval = 0;

		wcet_constraint(lf, sched, id, con, env);

		while (t < tick + h) {
			t += I[interval];
			interval++;

			lf->create_rows(sched, model, x, i, subtask, interval);

			if (t % period == 0) {
				subtask++;

				if (t != tick + h)
					wcet_constraint(lf, sched, id, con,
							env);
			}
		}
	}

	/*
	 * For LPDPM2, we need additional variables for the idle
	 * starting a the beginning of interval 0, which just include
	 * b_0.
	 */
	if (!strcmp(name(), "LPDPM2")) {
		add_variable(env, 'l', idle, 0, 0, ILOFLOAT, 0, IloInfinity, x);
		add_variable(env, 'L', idle, 0, 0, ILOFLOAT, 0, IloInfinity, x);

		for (i = 0; i < n_states + 1; i++)
			add_variable(env, 'p', idle, 0, 0, ILOBOOL, 0, 1, x);

		add_variable(env, 'P', idle, 0, 0, ILOFLOAT, 0, IloInfinity, x);
	}
}

static void add_initial_constraints(struct sched *sched, IloEnv env,
				    IloRangeArray con, int n_intervals)
{
	int i, min;

	int n_cpus = yass_sched_get_ncpus(sched);

	if (cpus_always_on(sched))
		min = n_cpus;
	else
		min = 0;

	/*
	 * For u <= m
	 */
	for (i = 0; i < n_intervals; i++)
		con.add(IloRange(env, min, n_cpus));
}

void add_constraints_lpdpm2(struct lpdpm_functions *lf, struct sched *sched,
			    IloModel model, IloNumVarArray x, IloRangeArray con,
			    int *I, int n_intervals, int row, int inter,
			    IloExpr obj, int subtask)
{
	int i;
	int n_states = yass_cpu_get_nstates(sched);

	double cons, penalty;

	IloEnv env = model.getEnv();
	IloExpr obj_p(env);

	int row_idle = row + (5 + n_states) * (inter);

	int b = row_idle;
	int e = row_idle + 1;
	int l = row_idle + 2;
	int L = row_idle + 3;
	int p = row_idle + 4;
	int P = row_idle + 5 + n_states;

	int b1 = row_idle + 6 + n_states;
	int e1 = row_idle + 6 + n_states + 1;
	int l1 = row_idle + 6 + n_states + 2;

	static int b0 = -1;
	static int e0 = -1;
	static int l0 = -1;

	if (b0 == -1)
		b0 = b;

	if (e0 == -1)
		e0 = e;

	if (l0 == -1)
		l0 = l;

	/*
	 * Utilization cannot be greater than M
	 */

	con[inter].setLinearCoef(x[b], 1.0);
	con[inter].setLinearCoef(x[e], 1.0);

	/*
	 * sum_{k} w_{j, k} * |I_k| = C
	 */

	con[n_intervals + subtask].setLinearCoef(x[b], I[inter]);
	con[n_intervals + subtask].setLinearCoef(x[e], I[inter]);

	/*
	 * b_k + e_k <= 1
	 */
	model.add(x[b] + x[e] <= 1);

	/*
	 * l_k
	 */
	if (inter != n_intervals - 1) {
		model.add(IloIfThen(env, x[b1] + x[e1] == 1,
				    x[l] == x[e] * I[inter] + x[b1] * I[inter + 1] + x[l1]));
		model.add(IloIfThen(env, x[b1] + x[e1] != 1,
				    x[l] == x[e] * I[inter] + x[b1] * I[inter + 1]));
	} else {
		model.add(IloIfThen(env, x[b0] + x[e0] == 1,
				    x[l] == x[e] * I[inter] + x[b0] * I[0] + x[l0]));
		model.add(IloIfThen(env, x[b0] + x[e0] != 1,
				    x[l] == x[e] * I[inter] + x[b0] * I[0]));
	}

	/*
	 * L_k
	 */
	model.add(IloIfThen(env, x[b] + x[e] != 1, x[L] == x[l]));
	model.add(IloIfThen(env, x[b] + x[e] == 1, x[L] == 0));

	/*
	 * p_k
	 */

	// Fake idle low-power state, BET is 0
	model.add(IloIfThen(env, x[L] <= 0, x[p] == 0));

	for (i = 0; i < n_states; i++) {
		penalty = yass_cpu_get_state_penalty(sched, i);
		model.add(IloIfThen(env, x[L] <= penalty, x[p + 1 + i] == 0));
	}

	for (i = 0; i < n_states; i++)
		obj_p += x[p + i];

	model.add(IloIfThen(env, x[L] == 0, obj_p == 0));
	model.add(IloIfThen(env, x[L] != 0, obj_p == 1));

	/*
	 * P_k
	 */

	// Fake idle low-power state
	model.add(IloIfThen(env, x[L + 1] == 1, x[P] == x[L]));

	for (i = 1; i < n_states; i++) {
		cons = yass_cpu_get_state_consumption(sched, i - 1);
		penalty = 0.5 * yass_cpu_get_state_penalty(sched, i - 1);

		model.add(IloIfThen(env, x[L + 1 + i] == 1,
				    x[P] == cons * x[L] + penalty));
	}

	obj += x[P];
}

void add_lpdpm2(struct sched *sched, IloModel model, IloNumVarArray x,
		IloRangeArray con, int *I, int n_intervals, IloExpr obj)
{
	int i;

	int n_states = yass_cpu_get_nstates(sched);
	int n_tasks = yass_sched_get_ntasks(sched);

	double cons, penalty;

	IloEnv env = model.getEnv();
	IloExpr obj_p(env);

	int inter = 0;

	int b = n_intervals * (n_tasks - 1);
	int e = b + 1;
	int l1 = e + 1;

	int l = n_intervals * (n_tasks - 1) + n_intervals * (5 + n_states + 1);
	int L = l + 1;
	int p = l + 2;
	int P = l + 3 + n_states;

	/*
	 * l_k
	 */
	model.add(IloIfThen(env, x[b] + x[e] == 1, x[l] == x[b] * I[inter] + x[l1]));
	model.add(IloIfThen(env, x[b] + x[e] != 1, x[l] == x[b] * I[inter]));

	/*
	 * L_k
	 */
	model.add(x[L] == x[l]);

	/*
	 * p_k
	 */

	// Fake idle low-power state, BET is 0
	model.add(IloIfThen(env, x[L] <= 0, x[p] == 0));

	for (i = 0; i < n_states; i++) {
		penalty = yass_cpu_get_state_penalty(sched, i);
		model.add(IloIfThen(env, x[L] <= penalty, x[p + 1 + i] == 0));
	}

	for (i = 0; i < n_states; i++)
		obj_p += x[p + i];

	model.add(IloIfThen(env, x[L] == 0, obj_p == 0));
	model.add(IloIfThen(env, x[L] != 0, obj_p == 1));

	/*
	 * P_k
	 */

	// Fake idle low-power state
	model.add(IloIfThen(env, x[L + 1] == 1, x[P] == x[L]));

	for (i = 1; i < n_states; i++) {
		cons = yass_cpu_get_state_consumption(sched, i - 1);
		penalty = 0.5 * yass_cpu_get_state_penalty(sched, i - 1);

		model.add(IloIfThen(env, x[L + 1 + i] == 1,
				    x[P] == cons * x[L] + penalty));
	}

	obj += x[P];
}

static void add_constraints(struct sched *sched, struct lpdpm_functions *lf,
			    IloModel model, IloNumVarArray x, IloRangeArray con,
			    int *I, int n_intervals)
{
	int i, row;
	int interval, subtask;
	int id, period;

	unsigned long long t;
	unsigned long long tick = yass_sched_get_tick(sched);
	unsigned long long h = yass_sched_get_hyperperiod(sched);

	int n_tasks = yass_sched_get_ntasks(sched);

	IloEnv env = model.getEnv();
	IloExpr obj(env);

	IloExpr obj2(env);
	IloExpr obj_jobs(env);

	subtask = -1;

	for (i = 0; i < n_tasks; i++) {
		id = yass_task_get_id(sched, i);
		period = yass_task_get_period(sched, id);

		interval = 0;
		t = tick;

		subtask++;

		obj_jobs = IloExpr(env);

		while (t < tick + h) {
			t += I[interval];
			interval++;

			row = n_intervals * i + interval - 1;

			if (!strcmp(name(), "IZL")) {
				lf->add_constraints(lf, sched, model, x, con, I, n_intervals,
						    row, interval - 1, obj, subtask);
			} else {
				if (i != n_tasks - 1) {
					con[interval - 1].setLinearCoef(x[row], 1.0);
					con[n_intervals + subtask].setLinearCoef(x[row],
										 I[interval - 1]);

					/*
					 * Add the consumption while
					 * execution tasks.
					 */
					if (sched_is_mc(name()))
						obj += x[row] * I[interval - 1];

				} else {
					lf->add_constraints(lf, sched, model, x, con, I,
							    n_intervals, row, interval - 1, obj,
							    subtask);
				}
			}

			obj_jobs += x[row];

			if (t % period == 0 && t != tick + h)
				subtask++;
		}
	}

	if (!strcmp(name(), "LPDPM2"))
		add_lpdpm2(sched, model, x, con, I, n_intervals, obj);

	model.add(IloMinimize(env, obj));

	model.add(con);
}

int compute_weights(struct sched *sched, double **w, struct lpdpm_functions *lf)
{
	int *I, r = 0, status, n_intervals, n_try = 0;

	IloEnv env;
	IloModel model;
	IloCplex cplex;
	IloNumVarArray x;
	IloRangeArray con;
	IloNumArray res;

	I = (int *)calloc(MAX_N_INTER, sizeof(int));

	if (I == NULL)
		return -1;

	n_intervals = yass_compute_intervals(sched, I);

	try {
		do {
			n_try++;

			model = IloModel(env);
			cplex = IloCplex(env);
			x = IloNumVarArray(env);
			con = IloRangeArray(env);

			cplex_set_parameter(sched, cplex);

			add_initial_constraints(sched, env, con, n_intervals);

			add_variable_interval(lf, sched, env, x, con, I);

			add_constraints(sched, lf, model, x, con, I,
					n_intervals);

			// print_matrix(x, con);

			cplex.extract(model);

			if (!cplex.solve())
				continue;

			status = cplex.getStatus();

			yass_sched_set_stat(sched, status);

			if (status == 0)
				continue;

			res = IloNumArray(env);

			cplex.getValues(res, x);

			if (yass_sched_get_debug(sched))
				print_results(res, x, status);

			populate_w(sched, w, res, x);

			r = is_solution_valid(sched, I, n_intervals, w);

		} while (n_try < 3 && r != 1);
	}
	catch(IloException & e) {
		fprintf(stderr, "Concert exception caught: %s\n",
			e.getMessage());
		r = -1;
		goto end;
	}
	catch( ...) {
		fprintf(stderr, "Unknown exception caught\n");
		r = -1;
		goto end;
	}

 end:
	free(I);

	env.end();

	return r;
}

int dpm_mc_schedulability_test(struct sched *sched, double alpha)
{
	int criticality, i, id;
	int exec;

	double total_mc = 0;
	int total = 0;

	int h = yass_sched_get_hyperperiod(sched);

	int n_cpus = yass_sched_get_ncpus(sched);

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		id = yass_task_get_id(sched, i);
		criticality = yass_task_get_criticality(sched, id);

		exec = yass_task_get_exec_hyperperiod(sched, id);

		total += exec;

		if (criticality == 1)
			total_mc += exec;
		else
			total_mc += alpha * (double)exec;
	}

	if (total > h * n_cpus)
		return 0;

	if (total_mc < h * (n_cpus - 2))
		sched->n_cpus -= 2;
	else if (total_mc < h * (n_cpus - 1))
		sched->n_cpus -= 1;

	return 1;
}

int lpdpm_offline(struct lpdpm_functions *lf, struct sched *sched, double ***w,
		  double **rem, int **priorities, double **rem_nc)
{
	int i, j;

	int n_tasks = get_ntasks(sched);

	if (!strcmp(name(),"LPDPM1") || !strcmp(name(), "LPDPM2") ||
	    !strcmp(name(), "LPDPM3")) {
		if (!yass_dpm_schedulability_test(sched))
			return -YASS_ERROR_NOT_SCHEDULABLE;
	} else {
		if (lf == NULL)
			return -YASS_ERROR_NOT_SCHEDULABLE;

		if (!dpm_mc_schedulability_test(sched, lf->double_field))
			return -YASS_ERROR_NOT_SCHEDULABLE;
	}

	(*rem) = (double *)calloc(n_tasks, sizeof(double));

	if (!*(rem))
		return -YASS_ERROR_MALLOC;

	(*priorities) = (int *)malloc(n_tasks * sizeof(int));

	if (!*(priorities))
		return -YASS_ERROR_MALLOC;

	if (w != NULL) {
		(*w) = (double **)calloc(n_tasks * MAX_N_INTER, sizeof(double));

		if (!(*w))
			return -YASS_ERROR_MALLOC;

		for (i = 0; i < n_tasks; i++) {
			(*w)[i] = (double *)calloc(MAX_N_INTER, sizeof(double));

			if (!(*w)[i])
				return -YASS_ERROR_MALLOC;

			for (j = 0; j < MAX_N_INTER; j++)
				(*w)[i][j] = 0;
		}
	}

	if (rem_nc != NULL) {
		(*rem_nc) = (double *)calloc(n_tasks, sizeof(double));

		if (!*(rem_nc))
			return -YASS_ERROR_MALLOC;
	}

	for (i = 0; i < n_tasks; i++) {
		(*priorities)[i] = -1;
		(*rem)[i] = -1;

		if (rem_nc != NULL)
			(*rem_nc)[i] = -1;
	}

	return 0;
}

int lpdpm_close(struct sched *sched, double *rem, int *priorities, double **w,
		double *rem_nc)
{
	int i;

	if (rem)
		free(rem);

	if (priorities)
		free(priorities);

	if (rem_nc)
		free(rem_nc);

	if (w) {
		for (i = 0; i < yass_sched_get_ntasks(sched) + 1; i++)
			free(w[i]);

		free(w);
	}

	return 0;
}
