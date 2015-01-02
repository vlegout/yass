#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <libyass/common.h>
#include <libyass/cpu.h>
#include <libyass/helpers.h>
#include <libyass/list.h>
#include <libyass/scheduler.h>
#include <libyass/task.h>

#define MAX_LEVEL 20

enum server_type {
	DUAL_SERVER,
	EDF_SERVER,
	ROOT_SERVER
};

enum reduce_operation {
	PACK = 1,
	DUAL = 2
};

struct yass_list *stalled;
struct yass_list *ready;
struct yass_list *running;

struct server {

	int id;

	double u;

	int deadline;
	double exec;
	double wcet;

	/*
	 * If the server is a leaf server, it has a task
	 * associated. This field contains the id of this task. If the
	 * server is not a leaf server, task_id = -1.
	 */
	int task_id;

	int active;

	int level;

	enum server_type type;

	int n;
	int *period;
	struct server **previous;

	struct server *next;
};

struct server ***servers;

const char *name()
{
	return "RUN";
}

static struct server *server_new(int id)
{
	int i;

	struct server *s = (struct server *)malloc(sizeof(struct server));

	if (!s)
		return NULL;

	s->id = id;
	s->u = 0;
	s->exec = 0;
	s->deadline = 0;
	s->wcet = 0;
	s->task_id = -1;
	s->type = EDF_SERVER;
	s->active = 0;
	s->level = -1;

	s->n = 0;

	s->period = (int *)malloc(YASS_MAX_N_TASKS * sizeof(int));

	if (!s->period)
		return NULL;

	s->previous = (struct server **)
	    malloc(YASS_MAX_N_TASKS * sizeof(struct server));

	if (!s->previous)
		return NULL;

	for (i = 0; i < YASS_MAX_N_TASKS; i++) {
		s->period[i] = -1;
		s->previous[i] = NULL;
	}

	s->next = NULL;

	return s;
}

static void server_free(struct server *s)
{
	free(s->period);
	free(s->previous);
	free(s);
}

static int init_servers(struct sched *sched)
{
	int i, id, j;

	int n_tasks = yass_sched_get_ntasks(sched);
	int n_servers = n_tasks * 2 + 1;

	struct server *s;

	servers = (struct server ***)
	    malloc(MAX_LEVEL * n_servers * sizeof(struct server *));

	if (!servers)
		return -YASS_ERROR_MALLOC;

	for (i = 0; i < MAX_LEVEL; i++) {

		servers[i] = (struct server **)
		    malloc(n_servers * sizeof(struct server *));

		if (!servers[i])
			return -YASS_ERROR_MALLOC;

		for (j = 0; j < n_servers; j++)
			servers[i][j] = NULL;
	}

	for (i = 0; i < n_tasks; i++) {
		id = yass_task_get_id(sched, i);

		servers[0][i] = server_new(id);

		if (!servers[0][i])
			return -YASS_ERROR_MALLOC;

		s = servers[0][i];

		s->id = id;
		s->u = yass_task_get_utilization(sched, id);
		s->task_id = id;
		s->level = 0;
		s->n = 1;
		s->period[0] = id;
	}

	return 0;
}

/*
 * The reduction is done once all servers are unit servers
 */
static int reduction_done(int level)
{
	int i = 0;

	if (level == 0)
		return 0;

	while (servers[level][i] != NULL) {
		if (servers[level][i]->u < 1 - 0.0001)
			return 0;

		i++;
	}

	return 1;
}

static void add_period(struct server *dst, struct server *src)
{
	int d = 0, s = 0;

	while (dst->period[d] != -1)
		d++;

	while (src->period[s] != -1) {
		dst->period[d] = src->period[s];
		s++;
		d++;
	}
}

static void add_previous(struct server *dst, struct server *src)
{
	int d = 0;

	while (dst->previous[d] != NULL)
		d++;

	dst->previous[d] = src;
}

static void print_level(struct sched *sched, int l)
{
	int i = 0, id, j;

	int n_tasks = yass_sched_get_ntasks(sched);

	struct server *s;

	printf("=== Level %d ===\n", l);

	while (i < n_tasks && servers[l][i] != NULL) {

		s = servers[l][i];

		printf(" -- server %d --\n", i);

		printf("  id : %d,", s->id);
		printf("  u : %.4lf,", s->u);
		printf("  exec : %.2lf,", s->exec);
		printf("  wcet : %.2lf,", s->wcet);
		printf("  deadline : %d,", s->deadline);

		printf("\n");

		printf("  task_id : %d,", s->task_id);
		printf("  type : %d,", s->type);
		printf("  level : %d,", s->level);
		printf("  active : %d,", s->active);
		printf("  n : %d\n", s->n);

		j = 0;

		printf("  period:");

		while (s->period[j] != -1) {
			id = s->period[j];
			printf(" %d (%d)",
			       yass_task_get_period(sched, id),
			       yass_task_get_wcet(sched, id));
			j++;
		}

		printf("\n");

		j = 0;

		printf("  previous:");

		while (s->previous[j] != NULL) {
			printf(" %d", s->previous[j]->id);
			j++;
		}

		printf("  next:");

		if (s->next != NULL)
			printf(" %d", s->next->id);

		printf("\n");

		printf(" --------------\n");

		i++;
	}
}

__attribute__ ((__unused__))
static void print_levels(struct sched *sched)
{
	int l = 0;

	while (servers[l][0] != NULL) {
		print_level(sched, l);
		l++;
	}
}

static int can_add_dummy_task(struct sched *sched, struct server *next)
{
	int i, id, wcet, period;

	int n_cpus = yass_sched_get_ncpus(sched);

	unsigned long long exec, exec_t, h = 1, needed;
	unsigned long long h_t = yass_sched_get_hyperperiod(sched);

	struct server *s;

	i = 0;

	/*
	 * Compute h
	 */
	while (servers[0][i] != NULL) {

		s = servers[0][i];

		id = s->task_id;

		if (s != NULL && s->next == next)
			h = yass_lcm(h, yass_task_get_period(sched, id));

		i++;
	}

	exec = 0;
	exec_t = 0;
	i = 0;

	while (servers[0][i] != NULL) {

		s = servers[0][i];

		id = s->task_id;

		wcet = yass_task_get_wcet(sched, id);
		period = yass_task_get_period(sched, id);

		if (s != NULL && s->next == next)
			exec += wcet * (h / period);

		exec_t += wcet * (h_t / period);

		i++;
	}

	if (next != NULL) {
		needed = (h - exec) * (h_t / h);

		return needed > 0 && exec_t + needed <= h_t * n_cpus;
	}

	if (next == NULL && h_t * n_cpus - exec_t > 0)
		return 1;

	return 0;
}

static int add_dummy_task(struct sched *sched, double u, struct server *next)
{
	int id, index, n_tasks, r;

	struct server *s;
	struct yass_task *t;

	if (u < 0 || u > 1)
		return -YASS_ERROR_NOT_SCHEDULABLE;

	r = yass_sched_add_idle_task(sched);

	if (r)
		return r;

	n_tasks = yass_sched_get_ntasks(sched);

	id = yass_task_get_id(sched, n_tasks - 1);
	t = yass_sched_get_task(sched, id);

	t->wcet = (int)round(u * t->period);

	if (yass_task_get_wcet(sched, id) <= 0)
		return -YASS_ERROR_NOT_SCHEDULABLE;

	index = 0;

	while (index < n_tasks && servers[0][index] != NULL)
		index++;

	servers[0][index] = server_new(id);

	if (!servers[0][index])
		return -YASS_ERROR_NOT_SCHEDULABLE;

	s = servers[0][index];

	s->u = u;
	s->task_id = s->id;
	s->level = 0;
	s->n = 1;
	s->period[0] = id;

	if (next != NULL) {
		s->next = next;

		next->u = 1;

		add_period(next, s);
		add_previous(next, s);
	}

	return 0;
}

static int add_idle_tasks(struct sched *sched)
{
	int i, index, r;
	double u, u_rem;

	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);

	i = 0;

	while (i < n_tasks && servers[1][i] != NULL) {
		u = servers[1][i]->u;

		if (can_add_dummy_task(sched, servers[1][i])) {
			r = add_dummy_task(sched, 1 - u, servers[1][i]);

			if (r)
				return r;

			n_tasks = yass_sched_get_ntasks(sched);
		}

		i++;
	}

	/*
	 * We have added idle tasks to create unit servers. However,
	 * idle time could have been left because no other unit server
	 * can be created. Allocate this idle time to another idle
	 * task.
	 */
	u_rem = n_cpus - yass_sched_get_global_utilization(sched);

	if (can_add_dummy_task(sched, NULL)) {
		r = add_dummy_task(sched, u_rem, NULL);

		if (r)
			return r;

		index = 0;

		while (index < n_tasks && servers[0][index] != NULL)
			index++;

		i = 0;

		while (i < n_tasks && servers[1][i] != NULL) {

			u = servers[1][i]->u;

			if (u < 1 && u + u_rem <= 1) {

				servers[0][index]->next = servers[1][i];

				servers[1][i]->u += u_rem;
				servers[1][i]->n++;

				add_period(servers[1][i], servers[0][index]);
				add_previous(servers[1][i], servers[0][index]);

				break;
			}

			i++;
		}
	}

	return 0;
}

static int pack(int l)
{
	int i = 0, n;

	struct server *s, *next;

	while (servers[l][i] != NULL) {

		s = servers[l][i];

		n = 0;

		/*
		 * Find a server to pack the server s. Create a new
		 * server if s does not fit in an existing one.
		 */

		while (servers[l + 1][n] != NULL &&
		       servers[l + 1][n]->u + s->u > 1 + 0.000001) {
			n++;
		}

		if (servers[l + 1][n] == NULL) {
			servers[l + 1][n] = server_new((l + 1) * 100 + n);

			if (!servers[l + 1][n])
				return -YASS_ERROR_MALLOC;
		}

		next = servers[l + 1][n];

		s->next = next;

		next->u += s->u;
		next->type = DUAL_SERVER;
		next->level = l + 1;
		next->n++;

		add_period(next, s);
		add_previous(next, s);

		i++;
	}

	return 0;
}

static int dual(int l)
{
	int i = 0;

	struct server *s, *next;

	while (servers[l][i] != NULL) {

		s = servers[l][i];

		servers[l + 1][i] = server_new((l + 1) * 100 + i);

		if (!servers[l + 1][i])
			return -YASS_ERROR_MALLOC;

		next = servers[l + 1][i];

		next->u = 1 - s->u;
		next->type = EDF_SERVER;
		next->level = l + 1;
		next->n++;

		add_period(next, s);
		add_previous(next, s);

		s->next = next;

		i++;
	}

	return 0;
}

static int reduce(struct sched *sched)
{
	int i, level = 0, r;

	int n_tasks = yass_sched_get_ntasks(sched);

	do {
		/*
		 * PACK
		 */
		r = pack(level);

		if (r)
			return r;

		level++;

		if (level == 1) {
			if (add_idle_tasks(sched))
				return -YASS_ERROR_MALLOC;

			n_tasks = yass_sched_get_ntasks(sched);
		}

		if (reduction_done(level))
			break;

		/*
		 * DUAL
		 */
		r = dual(level);

		if (r)
			return r;

		level++;

	} while (level < MAX_LEVEL - 2 && !reduction_done(level));

	if (level >= MAX_LEVEL - 2)
		return -YASS_ERROR_NOT_SCHEDULABLE;

	i = 0;

	while (i < n_tasks && servers[level][i] != NULL) {
		servers[level][i]->type = ROOT_SERVER;
		i++;
	}

	return 0;
}

int offline(struct sched *sched)
{
	int i, id, n_tasks, r;

	if (!yass_dpm_schedulability_test(sched))
		return -YASS_ERROR_NOT_SCHEDULABLE;

	init_servers(sched);

	r = reduce(sched);

	if (r)
		return r;

	n_tasks = yass_sched_get_ntasks(sched);

	ready = yass_list_new(n_tasks);
	running = yass_list_new(n_tasks);
	stalled = yass_list_new(n_tasks);

	for (i = 0; i < n_tasks; i++) {
		id = yass_task_get_id(sched, i);
		yass_list_add(stalled, id);
	}

	return 0;
}

static int server_get_deadline(struct sched *sched, struct server *s)
{
	int i = 0, min = YASS_MAX_PERIOD;
	int id, release;

	int tick = yass_sched_get_tick(sched);

	while (s->period[i] != -1) {

		id = s->period[i];
		release = yass_task_get_next_release(sched, id);

		if (release < min)
			min = release;

		i++;
	}

	if (min <= tick)
		return -1;

	return min;
}

static int server_is_ready(struct server *s)
{
	return s->exec < s->wcet - 0.001;
}

static int server_get_min_period(struct sched *sched, struct server *ser)
{
	int i = 0, id, period;
	int min = YASS_MAX_PERIOD;

	while (ser->period[i] != -1) {
		id = ser->period[i];
		period = yass_task_get_period(sched, id);

		if (period < min)
			min = period;

		i++;
	}

	return min;
}

static void sort_servers(struct sched *sched, int f, struct server *next)
{
	int i = 0, j, sub;
	int min1, min2, p1, p2;

	int n_tasks = yass_sched_get_ntasks(sched);

	struct server *s1, *s2, *tmp;

	while (servers[f][i] != NULL) {

		s1 = servers[f][i];

		if (next != NULL && s1->next != next) {
			i++;
			continue;
		}

		min1 = server_get_min_period(sched, s1);
		p1 = s1->deadline;

		if (!server_is_ready(s1))
			p1 = YASS_MAX_PERIOD;

		j = i + 1;
		sub = -1;

		while (j < n_tasks && servers[f][j] != NULL) {

			s2 = servers[f][j];

			if (next != NULL && s2->next != next) {
				j++;
				continue;
			}

			if (!server_is_ready(s2)) {
				j++;
				continue;
			}

			min2 = server_get_min_period(sched, s2);
			p2 = s2->deadline;

			if (p2 < p1 || (p2 == p1 && min2 < min1)) {
				sub = j;
				p1 = p2;
			}

			j++;
		}

		if (sub != -1) {
			tmp = servers[f][i];
			servers[f][i] = servers[f][sub];
			servers[f][sub] = tmp;
		}

		i++;
	}
}

static int set_active(struct server *s, int edf[MAX_LEVEL * YASS_MAX_N_TASKS])
{
	int t = s->type;

	struct server *n = s->next;

	switch (t) {
	case ROOT_SERVER:
		s->active = server_is_ready(s);
		break;
	case DUAL_SERVER:
		if (!n->active && !server_is_ready(s))
			return -YASS_ERROR_NOT_SCHEDULABLE;

		s->active = !n->active && server_is_ready(s);

		if (!n->active && !s->active)
			return -YASS_ERROR_NOT_SCHEDULABLE;

		break;
	case EDF_SERVER:
		if (n->active && edf[n->id] == 0 && server_is_ready(s)) {
			edf[n->id] = 1;
			s->active = 1;
		}
		break;
	}

	return 0;
}

static int update_active_servers(struct sched *sched)
{
	int i, j, l, t, r;

	/*
	 * To know whether or not a child server has been activated
	 * for edf servers. Obviously, this is not the prettiest way
	 * to achieve this ...
	 */
	int edf[MAX_LEVEL * YASS_MAX_N_TASKS];

	int n_tasks = yass_sched_get_ntasks(sched);

	struct server *s;

	/*
	 * Compute the root server(s) level.
	 */
	l = MAX_LEVEL - 1;

	while (servers[l][0] == NULL)
		l--;

	if (l >= MAX_LEVEL - 1 || l <= 0)
		return -YASS_ERROR_NOT_SCHEDULABLE;

	sort_servers(sched, l, NULL);

	for (i = 0; i < MAX_LEVEL * YASS_MAX_N_TASKS; i++)
		edf[i] = 0;

	while (l >= 0) {

		sort_servers(sched, l, NULL);

		j = 0;

		while (j < n_tasks && servers[l][j] != NULL) {

			s = servers[l][j];

			s->active = 0;

			if (server_is_ready(s)) {
				r = set_active(s, edf);

				if (r)
					return r;
			}

			j++;
		}

		j = 0;

		while (j < n_tasks && servers[l][j] != NULL) {
			s = servers[l][j];
			t = s->type;

			/*
			 * Be sure that an edf server has at least one
			 * child server active.
			 */
			if (t == EDF_SERVER && s->next->active &&
			    !edf[s->next->id]) {
				return -YASS_ERROR_NOT_SCHEDULABLE;
			}

			j++;
		}

		l--;
	}

	return 0;
}

static int select_active_tasks(struct sched *sched, struct yass_list *candidate)
{
	int id, j = 0;

	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);

	struct server *s;

	while (j < n_tasks && servers[0][j] != NULL) {

		s = servers[0][j];
		id = s->task_id;

		if (s->active == 1 && !yass_list_present(stalled, id))
			yass_list_add(candidate, id);

		j++;
	}

	if (yass_list_n(candidate) != n_cpus)
		return 0;

	return 1;
}

static int schedule_candidate_tasks(struct sched *sched,
				    struct yass_list *candidate)
{
	int i, id;

	int n_cpus = yass_sched_get_ncpus(sched);

	/*
	 * Preempt running tasks not in the candidate list
	 */
	for (i = 0; i < n_cpus; i++) {
		id = yass_cpu_get_task(sched, i);

		if (id != -1 && !yass_list_present(candidate, id))
			yass_preempt_task(sched, i, running, ready);
	}

	/*
	 * Keep already scheduled tasks on the same processor
	 */
	for (i = 0; i < n_cpus; i++) {
		id = yass_cpu_get_task(sched, i);

		if (id != -1 && yass_list_present(candidate, id))
			yass_list_remove(candidate, id);
	}

	/*
	 * Assign a processor to the remaining tasks in the candidate
	 * list.
	 */
	for (i = 0; i < n_cpus; i++) {
		id = yass_list_get(candidate, 0);

		if (!yass_cpu_is_active(sched, i) && id != -1) {
			yass_run_task(sched, i, id, ready, running);

			yass_list_remove(candidate, id);
		}
	}

	if (yass_list_n(candidate) != 0 || yass_list_n(running) != n_cpus)
		return 0;

	return 1;
}

static void terminate_task(struct sched *sched, int id)
{
	int cpu = yass_task_get_cpu(sched, id);
	int wcet = yass_task_get_wcet(sched, id);
	int tick = yass_sched_get_tick(sched);

	double exec = yass_task_get_exec(sched, id);

	if (exec >= wcet - 0.001) {
		yass_terminate_task_tick(sched, cpu, id, tick,
					 running, stalled);
	}
}

static int check_terminated_tasks(struct sched *sched)
{
	int i, id, j, l = 0, r = 0;

	int n_tasks = yass_sched_get_ntasks(sched);

	struct server *s;

	while (servers[l][0] != NULL) {

		i = 0;

		while (i < n_tasks && servers[l][i] != NULL) {

			s = servers[l][i];

			if (s->active && !server_is_ready(s)) {

				s->active = 0;
				r = 1;
				j = 0;

				while (s->period[j] != -1) {
					id = s->period[j];

					if (yass_task_is_active(sched, id))
						terminate_task(sched, id);

					j++;
				}
			}

			i++;
		}

		l++;
	}

	return r;
}

static int update_wcet(struct sched *sched)
{
	int d, i, l = 0;

	int n_tasks = yass_sched_get_ntasks(sched);
	int tick = yass_sched_get_tick(sched);

	struct server *s;

	while (servers[l][0] != NULL) {

		i = 0;

		while (i < n_tasks && servers[l][i] != NULL) {

			s = servers[l][i];

			if (s->deadline == tick) {

				d = server_get_deadline(sched, s);

				if (d == -1)
					return -YASS_ERROR_NOT_SCHEDULABLE;

				if (s->exec <= s->wcet - 0.0001)
					return -YASS_ERROR_NOT_SCHEDULABLE;

				if (s->exec >= s->wcet + 0.0001)
					return -YASS_ERROR_NOT_SCHEDULABLE;

				s->exec = 0;
				s->deadline = d;
				s->wcet = s->u * (d - tick);

				if (s->deadline < tick + s->wcet - 0.001)
					return -YASS_ERROR_NOT_SCHEDULABLE;
			}

			i++;
		}

		l++;
	}

	return 0;
}

static int schedulability_test(struct sched *sched, double time)
{
	int i, l = 0;

	double tick = yass_sched_get_tick(sched) + time;
	int n_tasks = yass_sched_get_ntasks(sched);

	struct server *s;

	while (servers[l][0] != NULL) {

		i = 0;

		while (i < n_tasks && servers[l][i] != NULL) {

			s = servers[l][i];

			if (tick + s->wcet - s->exec > s->deadline + 0.001)
				return 1;

			i++;
		}

		l++;
	}

	return 0;
}

static int execute(struct sched *sched, double inc)
{
	int i, id, l = 0;

	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);

	double total_execution = 0;

	struct server *s;

	for (i = 0; i < n_cpus; i++) {

		if (!yass_cpu_is_active(sched, i))
			return 1;

		if (yass_cpu_is_active(sched, i)) {
			id = yass_cpu_get_task(sched, i);

			yass_task_exec_inc(sched, id, inc);
			total_execution += inc;
		}
	}

	if (total_execution - inc * n_cpus > 0.0001 ||
	    total_execution - inc * n_cpus < -0.0001) {
		return 1;
	}

	total_execution = 0;

	while (servers[l][0] != NULL) {

		i = 0;

		while (i < n_tasks && servers[l][i] != NULL) {

			s = servers[l][i];

			if (s->active) {
				s->exec += inc;

				if (l == 0)
					total_execution += inc;
			}

			i++;
		}

		l++;
	}

	if (total_execution - inc * n_cpus > 0.0001 ||
	    total_execution - inc * n_cpus < -0.0001) {
		return 1;
	}

	return 0;
}

static double min_execution(struct sched *sched)
{
	int i, l = 0;
	double min = YASS_MAX_PERIOD;

	int n_tasks = yass_sched_get_ntasks(sched);

	struct server *s;

	while (servers[l][0] != NULL) {

		i = 0;

		while (i < n_tasks && servers[l][i] != NULL) {

			s = servers[l][i];

			if (s->active && s->wcet - s->exec < min)
				min = s->wcet - s->exec;

			i++;
		}

		l++;
	}

	return min;
}

static int check_ready_tasks(struct sched *sched, struct yass_list *stalled,
			     struct yass_list *ready)
{
	int i, id, period, l, r = 0;

	int n_tasks = yass_sched_get_ntasks(sched);
	int tick = yass_sched_get_tick(sched);

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		id = yass_task_get_id(sched, i);
		period = yass_task_get_period(sched, id);

		if (tick % period == 0 && !yass_list_present(stalled, id))
			return -1;

		if (tick % period == 0 && yass_list_present(stalled, id)) {
			yass_list_remove(stalled, id);
			yass_list_add(ready, id);

			yass_task_set_release(sched, id, tick + period);

			r = 1;
		}
	}

	l = 0;

	while (servers[l][0] != NULL) {
		i = 0;

		while (i < n_tasks && servers[l][i] != NULL) {
			if (servers[l][i]->deadline == tick)
				r = 1;

			i++;
		}

		l++;
	}

	return r;
}

int schedule(struct sched *sched)
{
	int r;

	double min, time = 0;

	struct yass_list *candidate;

	while (time < 1 - 0.0001) {

		if (time > 0)
			yass_sched_update_idle(sched);

		if (schedulability_test(sched, time))
			return -YASS_ERROR_NOT_SCHEDULABLE;

		if (check_terminated_tasks(sched)) {
			r = update_active_servers(sched);
			if (r)
				return r;
		}

		if (time == 0) {
			r = check_ready_tasks(sched, stalled, ready);

			if (r == 1) {
				r = update_wcet(sched);
				if (r)
					return r;

				r = update_active_servers(sched);
				if (r)
					return r;
			} else if (r == -1) {
				return -YASS_ERROR_NOT_SCHEDULABLE;
			}
		}

		candidate = yass_list_new(YASS_MAX_N_TASKS);

		if (!select_active_tasks(sched, candidate))
			return -YASS_ERROR_NOT_SCHEDULABLE;

		if (!schedule_candidate_tasks(sched, candidate))
			return -YASS_ERROR_NOT_SCHEDULABLE;

		min = min_execution(sched);

		if (time + min > 1 - 0.0001)
			min = 1 - time;

		if (execute(sched, min))
			return -YASS_ERROR_NOT_SCHEDULABLE;

		time += min;

		yass_list_free(candidate);
	}

	if (time > 1.0001 || time < 1 - 0.0001)
		return -YASS_ERROR_NOT_SCHEDULABLE;

	return 0;
}

int sched_close(struct sched *sched __attribute__ ((__unused__)))
{
	int i, j;

	int n_tasks = yass_sched_get_ntasks(sched);

	yass_list_free(stalled);
	yass_list_free(ready);
	yass_list_free(running);

	for (i = 0; i < MAX_LEVEL; i++) {
		for (j = 0; j < n_tasks; j++) {
			if (servers[i][j] != NULL)
				server_free(servers[i][j]);
		}

		free(servers[i]);
	}

	free(servers);

	return 0;
}
