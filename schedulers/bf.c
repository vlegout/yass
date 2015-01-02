#include <math.h>
#include <stdlib.h>

#include <libyass/common.h>
#include <libyass/cpu.h>
#include <libyass/helpers.h>
#include <libyass/list.h>
#include <libyass/scheduler.h>
#include <libyass/task.h>

struct yass_list *stalled;
struct yass_list *ready;
struct yass_list *running;

double *rw;
double *pw;
double *a;
double *uf;
int *m;
int *o;

int **assign;

int k;

const char *name()
{
	return "Boundary Fair";
}

int offline(struct sched *sched)
{
	int i, id, n_tasks, t;

	int n_cpus = yass_sched_get_ncpus(sched);

	if (!yass_dpm_schedulability_test(sched))
		return -YASS_ERROR_NOT_SCHEDULABLE;

	t = yass_sched_get_total_exec(sched);

	/*
	 * With BF, the global utilization *must* be equal to the
	 * number of processors. Thus we need to add an idle task if
	 * necessary.
	 *
	 * Note that U < m - 1 is not possible because of
	 * yass_dpm_schedulability_test().
	 */
	if (n_cpus * yass_sched_get_hyperperiod(sched) - t > 0) {
		if (yass_sched_add_idle_task(sched))
			return -YASS_ERROR_NOT_SCHEDULABLE;
	}

	n_tasks = yass_sched_get_ntasks(sched);

	ready = yass_list_new(n_tasks);
	running = yass_list_new(n_tasks);
	stalled = yass_list_new(n_tasks);

	rw = (double *)malloc(n_tasks * sizeof(double));
	pw = (double *)malloc(n_tasks * sizeof(double));
	a = (double *)malloc(n_tasks * sizeof(double));
	uf = (double *)malloc(n_tasks * sizeof(double));
	m = (int *)malloc(n_tasks * sizeof(int));
	o = (int *)malloc(n_tasks * sizeof(int));

	for (i = 0; i < n_tasks; i++) {
		id = yass_task_get_id(sched, i);

		yass_list_add(stalled, id);

		rw[i] = 0;
	}

	assign = (int **)malloc(n_cpus * 2 * sizeof(int));

	for (i = 0; i < n_cpus; i++) {
		assign[i] = (int *)malloc(2 * sizeof(int));
		assign[i][0] = 0;
		assign[i][1] = 0;
	}

	k = 0;

	return 0;
}

/*
 * We assume that this function is only called on a boundary, thus
 * only the next boundary should be computed.
 */
static int get_period_length(struct sched *sched)
{
	int tick = yass_sched_get_tick(sched);
	int next_release = yass_sched_get_next_release(sched);

	return (next_release - tick);
}

static double compute_ru(struct sched *sched)
{
	int i;
	double sum = 0;

	int length = get_period_length(sched);
	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);

	/*
	 * TODO: Line 6 Algorithm 1
	 */
	for (i = 0; i < n_tasks; i++)
		sum += m[i];

	return length * n_cpus - sum;
}

static void sort_tab(struct yass_list *q)
{
	int i, j, n, index, tmp;
	double a1, a2, uf1, uf2;

	for (i = 0; i < yass_list_n(q); i++) {

		index = yass_list_get(q, i);

		a1 = a[index];
		uf1 = uf[index];

		n = -1;

		for (j = i + 1; j < yass_list_n(q); j++) {

			index = yass_list_get(q, j);

			a2 = a[index];
			uf2 = uf[index];

			if (a1 == 0 && a2 == 0)
				continue;
			else if (a1 >= 0 && a2 <= 0)
				continue;
			else if (a1 <= 0 && a2 >= 0) ;
			else if (uf1 > uf2) ;
			else
				continue;

			n = j;
			a1 = a2;
			uf1 = uf2;
		}

		if (n != -1) {
			tmp = yass_list_get(q, i);
			yass_list_set(q, i, yass_list_get(q, n));
			yass_list_set(q, n, tmp);
		}
	}

}

/*
 * TODO: Must be updated to use some sort of recursivity, see last
 * paragraph, page 3 of zhu's paper.
 */
static void compute_a_uf(struct sched *sched)
{
	int i, id, s;
	double ak, uf1, w;
	double bk, bk1;

	int n_tasks = yass_sched_get_ntasks(sched);
	int tick = yass_sched_get_tick(sched);

	/*
	 * Compute a & UF
	 */
	for (i = 0; i < n_tasks; i++) {
		id = yass_task_get_id(sched, i);
		w = yass_task_get_utilization(sched, id);

		ak = 1;
		s = 0;
		uf1 = 0;

		bk = yass_sched_get_tick(sched);
		bk1 = bk;

		while (ak > 0) {
			if (s == 0)
				bk = tick;
			else
				bk = bk1;

			bk1 = yass_sched_get_next_boundary(sched, bk);

			if (k > 0)
				ak = bk1 * w - floorf(bk * w) - (bk1 - bk);
			else
				ak = -1;

			if (k >= 1 && ak < 0)
				uf1 = (1 - bk1 * w + floorf(bk1 * w)) / w;
			else
				uf1 = 0;

			a[i] = ak;
			uf[i] = uf1;

			s++;
		}
	}

}

static int task_selection(struct sched *sched)
{
	int i;

	int n_tasks = yass_sched_get_ntasks(sched);
	int tick = yass_sched_get_tick(sched);

	struct yass_list *t = yass_list_new(n_tasks);

	/*
	 * Remainings units
	 */
	int ru = compute_ru(sched);

	double bk = tick;
	double bk1 = yass_sched_get_next_boundary(sched, tick);

	for (i = 0; i < n_tasks; i++) {
		o[i] = 0;

		if (pw[i] > 0.0001 && m[i] < (bk1 - bk))
			yass_list_add(t, i);
	}

	if (k != 0) {
		compute_a_uf(sched);

		sort_tab(t);
	}

	if (ru > n_tasks)
		return -YASS_ERROR_NOT_SCHEDULABLE;

	for (i = 0; i < ru; i++) {
		o[yass_list_get(t, i)] = 1;
		m[yass_list_get(t, i)]++;
	}

	for (i = 0; i < n_tasks; i++) {
		rw[i] = pw[i] - o[i];
	}

	yass_list_free(t);

	return 0;
}

static int generate_schedule(struct sched *sched)
{
	int i, j;
	int cpu = 0;
	int t = 0;

	int length = get_period_length(sched);
	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);

	for (i = 0; i < n_tasks; i++) {
		for (j = 0; j < m[i]; j++) {
			if (t % length == 0) {
				assign[cpu][0] = i;
				assign[cpu][1] = j;

				cpu++;
			}

			t++;
		}
	}

	if (cpu != n_cpus)
		return -YASS_ERROR_NOT_SCHEDULABLE;

	return 0;
}

int schedule(struct sched *sched)
{
	int cpu, i, id;
	double tmp, utilization;

	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);

	yass_exec_inc(sched);

	yass_check_terminated_tasks(sched, running, stalled, YASS_OFFLINE);

	/*
	 * The algorithm is only ran on job boundaries, thus only we a
	 * task becomes ready.
	 */
	if (yass_check_ready_tasks(sched, stalled, ready)) {

		for (i = 0; i < n_tasks; i++) {
			id = yass_task_get_id(sched, i);
			utilization = yass_task_get_utilization(sched, id);

			tmp = get_period_length(sched);
			tmp *= utilization;
			tmp += rw[i];

			if (tmp <= 0)
				m[i] = 0;
			else
				m[i] = (int)floorf(tmp);

			pw[i] = tmp - m[i];
		}

		if (task_selection(sched))
			return -YASS_ERROR_NOT_SCHEDULABLE;

		if (generate_schedule(sched))
			return -YASS_ERROR_NOT_SCHEDULABLE;

		k++;
	}

	for (i = 0; i < n_cpus; i++) {
		id = yass_task_get_id(sched, assign[i][0]);

		if (id != yass_cpu_get_task(sched, i)) {

			/*
			 * Preempt the task running on the cpu
			 */
			if (yass_cpu_is_active(sched, i))
				yass_preempt_task(sched, i, running, ready);

			/*
			 * The task to schedule can already be active
			 * on another cpu, so we need to preempt it
			 * first.
			 */
			if (yass_task_get_cpu(sched, id) != -1) {
				cpu = yass_task_get_cpu(sched, id);
				yass_preempt_task(sched, cpu, running, ready);
			}

			yass_run_task(sched, i, id, ready, running);
		}

		assign[i][1]++;

		/*
		 * The current task has finished its execution, find
		 * the next one.
		 */
		if (assign[i][1] == m[assign[i][0]]) {

			do {
				assign[i][0]++;
			} while (assign[i][0] < n_tasks &&
				 m[assign[i][0]] == 0);

			assign[i][1] = 0;
		}
	}

	return 0;
}

int sched_close(struct sched *sched __attribute__ ((__unused__)))
{
	int i;

	int n_cpus = yass_sched_get_ncpus(sched);

	yass_list_free(stalled);
	yass_list_free(ready);
	yass_list_free(running);

	free(rw);
	free(m);
	free(pw);
	free(a);
	free(uf);
	free(o);

	for (i = 0; i < n_cpus; i++)
		free(assign[i]);

	free(assign);

	return 0;
}
