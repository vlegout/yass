#ifndef _YASS_LPDPM_H
#define _YASS_LPDPM_H

#include <libyass/yass.h>

using namespace std;

#include <ilcplex/ilocplex.h>

#ifdef __cplusplus
extern "C" {
#endif

const char *name();
int offline(struct sched *sched);
int schedule(struct sched *sched);
int sched_close(struct sched *sched);

struct lpdpm_functions {
	void (*create_rows)(struct sched *sched, IloModel model, IloNumVarArray x,
			    int index, int subtask, int interval);
	void (*add_constraints)(struct lpdpm_functions *lf, struct sched *sched,
				IloModel model, IloNumVarArray x,
				IloRangeArray con, int *I, int n_intervals,
				int row, int inter, IloExpr obj, int subtask);

	int int_field;
	double double_field;
};

#define MAX_N_INTER 32767

#define EPSILON 0.000001

#define MIN_WCET 0.2

#define CONSUMPTION_IMPROVEMENT 0.9

#define CPLEX_TIME 60

int sched_is_mc(const char *name);

int cpus_always_on(struct sched *sched);

int get_ntasks(struct sched *sched);

int get_task_id(struct sched *sched, int index);

void print_matrix(IloNumVarArray x, IloRangeArray con);

void add_variable(IloModel model, char c, int id, int subtask, int interval,
		  IloNumVar::Type type, double min, double max,
		  IloNumVarArray x);

void create_rows_lpdpm2(struct sched *sched, IloModel model, IloNumVarArray x,
			int index, int subtask, int interval);

void add_variable_interval(struct lpdpm_functions *lf, struct sched *sched,
			   IloModel model, IloNumVarArray x, IloRangeArray con,
			   int *I);

void add_constraints_lpdpm2(struct lpdpm_functions *lf, struct sched *sched,
			    IloModel model, IloNumVarArray x, IloRangeArray con,
			    int *I, int n_intervals, int row, int inter,
			    IloExpr obj, int subtask);

int compute_weights(struct sched *sched, double **w,
		    struct lpdpm_functions *lf);

int lpdpm_offline(struct lpdpm_functions *lf, struct sched *sched, double ***w,
		  double **rem, int **priorities, double **rem_nc);

int lpdpm_schedule(struct sched *sched, double *rem, double **w,
		   int *priorities, int *idle_b, int *idle_e, int *idle_cpu,
		   double *rem_nc);

int lpdpm_close(struct sched *sched, double *rem, int *priorities, double **w,
		double *rem_nc);

#ifdef __cplusplus
}
#endif

#endif				/* _YASS_LPDPM_H */
