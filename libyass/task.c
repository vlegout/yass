#include <jansson.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "task.h"

#include "cpu.h"
#include "private.h"
#include "scheduler.h"

YASS_EXPORT int yass_task_exist(struct sched *sched, int id)
{
	int i;

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		if (yass_task_get_id(sched, i) == id)
			return 1;
	}

	return 0;
}

YASS_EXPORT struct yass_task *yass_task_new(int id)
{
	struct yass_task *task;

	task = (struct yass_task *)malloc(sizeof(struct yass_task));

	if (!task)
		return NULL;

	task->id = id;
	task->period = 0;
	task->deadline = 0;
	task->wcet = 0;
	task->criticality = 1;
	task->delay = 0;
	task->threads = 0;
	task->s = -1;
	task->parallel = -1;

	return task;
}

YASS_EXPORT struct yass_task_sched *yass_task_sched_new(int id)
{
	struct yass_task_sched *task;

	task = (struct yass_task_sched *)malloc(sizeof(struct yass_task_sched));

	if (!task)
		return NULL;

	task->id = id;
	task->priority = -1;
	task->release = 0;
	task->exec = 0;

	return task;
}

YASS_EXPORT struct yass_task **yass_tasks_new(int n)
{
	int i;
	struct yass_task **tasks;

	tasks = (struct yass_task **)calloc(n, sizeof(struct yass_task));

	if (tasks == NULL)
		return NULL;

	for (i = 0; i < n; i++) {
		tasks[i] = yass_task_new(i + 1);

		if (tasks[i] == NULL)
			return NULL;
	}

	return tasks;
}

static int get_int(json_t * object, const char *s)
{
	json_t *j = json_object_get(object, s);

	if (!json_is_integer(j)) {
		/* fprintf(stderr, "error while parsing tasks\n"); */
		if (!strcmp(s, "threads"))
			return 1;
		else
			return -1;
	}

	return json_integer_value(j);
}

YASS_EXPORT struct yass_task **yass_tasks_create(const char *data, int *n_tasks,
						 int *error)
{
	int i;
	unsigned int j;

	char filename[256];
	char tmp[128];
	char s[1024 * 1024] = "";

	json_t *root;
	json_t *objects, *object, *segments;
	json_error_t json_error;

	FILE *fp;

	struct yass_task **tasks;

	*error = 0;

	if (yass_find_file(filename, data, DATA)) {
		*error = -YASS_ERROR_DATA_FILE;
		return NULL;
	}

	if ((fp = fopen(filename, "r")) == NULL) {
		*error = -YASS_ERROR_DATA_FILE;
		return NULL;
	}

	while (fgets(tmp, 1024, fp) != NULL)
		strcat(s, tmp);

	root = json_loads(s, 0, &json_error);

	fclose(fp);

	if (!root) {
		fprintf(stderr, "%s: error on line %d: %s\n", filename,
			json_error.line, json_error.text);
		*error = -YASS_ERROR_DATA_FILE_JSON;
		return NULL;
	}

	objects = json_object_get(root, "tasks");

	if (!json_is_array(objects) || json_array_size(objects) <= 0) {
		*error = -YASS_ERROR_DATA_FILE_JSON;
		return NULL;
	}

	*n_tasks = json_array_size(objects);

	tasks = yass_tasks_new(*n_tasks);

	if (tasks == NULL) {
		*error = -YASS_ERROR_MALLOC;
		return NULL;
	}

	for (i = 0; i < *n_tasks; i++) {
		object = json_array_get(objects, i);

		if (!json_is_object(object)) {
			fprintf(stderr,
				"%s: error: task %d is not an object\n",
				filename, i + 1);
			*error = -YASS_ERROR_DATA_FILE_JSON;
			return NULL;
		}

		tasks[i]->id = get_int(object, "id");
		tasks[i]->vm = get_int(object, "vm");
		tasks[i]->threads = get_int(object, "threads");
		tasks[i]->deadline = get_int(object, "deadline");
		tasks[i]->period = get_int(object, "period");
		tasks[i]->delay = get_int(object, "delay");
		tasks[i]->criticality = get_int(object, "criticality");

		if (tasks[i]->delay == -1)
			tasks[i]->delay = 0;

		if (tasks[i]->deadline == -1)
			tasks[i]->deadline = tasks[i]->period;

		if (tasks[i]->criticality == -1)
			tasks[i]->criticality = 1;

		segments = json_object_get(object, "segments");

		if (!json_is_array(segments)) {
			tasks[i]->wcet = get_int(object, "wcet");

			tasks[i]->s = -1;
			tasks[i]->parallel = -1;

			yass_warn(tasks[i]->wcet <= tasks[i]->deadline);
			yass_warn(tasks[i]->wcet <= tasks[i]->period);

			yass_warn(tasks[i]->wcet >= YASS_MIN_WCET);
		} else {
			tasks[i]->parallel = get_int(object, "parallel");
			tasks[i]->s = json_array_size(segments);

			for (j = 0; j < json_array_size(segments); j++) {
				object = json_array_get(segments, j);

				if (!json_is_object(object)) {
					*error = -YASS_ERROR_DATA_FILE_JSON;
					return NULL;
				}

				tasks[i]->segments[j] = get_int(object, "wcet");
			}
		}
	}

	json_decref(root);

	return tasks;
}

YASS_EXPORT void yass_task_free_tasks(struct yass_task **tasks, int n)
{
	int i;

	for (i = 0; i < n; i++)
		free(tasks[i]);

	free(tasks);
}

YASS_EXPORT void yass_task_free_exec_time(int **exec_time, int n)
{
	int i;

	for (i = 0; i < n; i++)
		free(exec_time[i]);

	free(exec_time);
}

YASS_EXPORT struct yass_task_sched **yass_tasks_sched_new(struct sched *sched,
							  int n_tasks)
{
	int i, id;

	struct yass_task_sched **tasks_sched;

	tasks_sched = (struct yass_task_sched **)
	    calloc(n_tasks, sizeof(struct yass_task_sched));

	if (tasks_sched == NULL)
		return NULL;

	for (i = 0; i < n_tasks; i++) {
		id = yass_task_get_id(sched, i);

		tasks_sched[i] = yass_task_sched_new(id);

		if (tasks_sched[i] == NULL)
			return NULL;
	}

	return tasks_sched;
}

YASS_EXPORT int yass_task_get_from_id(struct sched *sched, int id)
{
	int i;

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		if (yass_task_get_id(sched, i) == id)
			return i;
	}

	return -1;
}

YASS_EXPORT int yass_task_time_to_deadline(struct sched *sched, int id)
{
	yass_warn(yass_task_exist(sched, id));

	int tick = yass_sched_get_tick(sched);
	int deadline = yass_task_get_deadline(sched, id);
	int next_release = yass_task_get_next_release(sched, id);
	int period = yass_task_get_period(sched, id);

	return next_release - period + deadline - tick;
}

YASS_EXPORT int yass_task_get_id(struct sched *sched, int index)
{
	return (yass_sched_get_task_index(sched, index))->id;
}

YASS_EXPORT int yass_task_get_vm(struct sched *sched, int id)
{
	yass_warn(yass_task_exist(sched, id));

	return (yass_sched_get_task(sched, id))->vm;
}

YASS_EXPORT int yass_task_get_threads(struct sched *sched, int id)
{
	yass_warn(yass_task_exist(sched, id));

	return (yass_sched_get_task(sched, id))->threads;
}

YASS_EXPORT int yass_task_get_wcet(struct sched *sched, int id)
{
	yass_warn(yass_task_exist(sched, id));

	return (yass_sched_get_task(sched, id))->wcet;
}

/*
 * The name is misleading because it does not always return the actual
 * execution time ...
 */
YASS_EXPORT int yass_task_get_aet(struct sched *sched, int id)
{
	int n, aet;

	yass_warn(yass_task_exist(sched, id));

	if (yass_sched_task_is_idle_task(sched, id)) {
		aet = yass_task_get_wcet(sched, id);
	} else if (yass_sched_get_online(sched)) {
		n = yass_task_get_n_exec(sched, id);
		aet = yass_sched_get_exec_time(sched, id, n);
	} else if (yass_task_get_criticality(sched, id) == 1) {
		aet = yass_task_get_wcet(sched, id);
	} else {
		aet = yass_task_get_wcet(sched, id);
	}

	return aet;
}

YASS_EXPORT int yass_task_get_deadline(struct sched *sched, int id)
{
	yass_warn(yass_task_exist(sched, id));

	return (yass_sched_get_task(sched, id))->deadline;
}

YASS_EXPORT int yass_task_get_period(struct sched *sched, int id)
{
	yass_warn(yass_task_exist(sched, id));

	return (yass_sched_get_task(sched, id))->period;
}

YASS_EXPORT int yass_task_get_delay(struct sched *sched, int id)
{
	yass_warn(yass_task_exist(sched, id));

	return (yass_sched_get_task(sched, id))->delay;
}

YASS_EXPORT int yass_task_get_criticality(struct sched *sched, int id)
{
	yass_warn(yass_task_exist(sched, id));

	return (yass_sched_get_task(sched, id))->criticality;
}

YASS_EXPORT int yass_task_get_priority(struct sched *sched, int id)
{
	yass_warn(yass_task_exist(sched, id));

	return (yass_sched_get_task_sched(sched, id))->priority;
}

YASS_EXPORT void yass_task_set_priority(struct sched *sched, int id,
					int priority)
{
	yass_warn(yass_task_exist(sched, id));

	(yass_sched_get_task_sched(sched, id))->priority = priority;
}

YASS_EXPORT double yass_task_get_exec(struct sched *sched, int id)
{
	yass_warn(yass_task_exist(sched, id));

	return (yass_sched_get_task_sched(sched, id))->exec;
}

YASS_EXPORT void yass_task_set_exec(struct sched *sched, int id, double exec)
{
	yass_warn(yass_task_exist(sched, id));

	(yass_sched_get_task_sched(sched, id))->exec = exec;
}

YASS_EXPORT void yass_task_exec_inc(struct sched *sched, int id, double exec)
{
	int i;
	double tmp;

	yass_warn(yass_task_exist(sched, id));

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		if (yass_task_get_id(sched, i) == id) {
			tmp = yass_task_get_exec(sched, id);
			yass_task_set_exec(sched, id, tmp + exec);
		}
	}
}

/* Does not work if a task has more than one thread */
YASS_EXPORT int yass_task_get_cpu(struct sched *sched, int id)
{
	int i;

	/* yass_warn(yass_task_exist(sched, id)); */

	for (i = 0; i < yass_sched_get_ncpus(sched); i++) {
		if (yass_cpu_get_task(sched, i) == id)
			return i;
	}

	return -1;
}

YASS_EXPORT double yass_task_get_utilization(struct sched *sched, int id)
{
	double period = yass_task_get_period(sched, id);
	double wcet = yass_task_get_wcet(sched, id);

	yass_warn(yass_task_exist(sched, id));

	return wcet / period;
}

YASS_EXPORT double yass_task_get_remaining_exec(struct sched *sched, int id)
{
	yass_warn(yass_task_exist(sched, id));

	return yass_task_get_wcet(sched, id) - yass_task_get_exec(sched, id);
}

YASS_EXPORT int yass_task_get_parallel(struct sched *sched, int id)
{
	yass_warn(yass_task_exist(sched, id));

	return (yass_sched_get_task(sched, id))->parallel;
}

YASS_EXPORT int yass_task_get_s(struct sched *sched, int id)
{
	yass_warn(yass_task_exist(sched, id));

	return (yass_sched_get_task(sched, id))->s;
}

YASS_EXPORT int *yass_task_get_segments(struct sched *sched, int id)
{
	yass_warn(yass_task_exist(sched, id));

	return (yass_sched_get_task(sched, id))->segments;
}

YASS_EXPORT double yass_task_get_laxity(struct sched *sched, int id)
{
	yass_warn(yass_task_exist(sched, id));

	double remaining =
	    yass_task_get_wcet(sched, id) - yass_task_get_exec(sched, id);

	return yass_task_time_to_deadline(sched, id) - remaining;
}

YASS_EXPORT double yass_task_get_lag(struct sched *sched, int id)
{
	double w = yass_task_get_utilization(sched, id);
	int wcet = yass_task_get_wcet(sched, id);

	double exec = yass_task_get_n_exec(sched, id) * wcet;

	exec += yass_task_get_exec(sched, id);

	return w * yass_sched_get_tick(sched) - exec;
}

YASS_EXPORT void yass_task_set_release(struct sched *sched, int id, int release)
{
	yass_warn(yass_task_exist(sched, id));

	(yass_sched_get_task_sched(sched, id))->release = release;
}

YASS_EXPORT int yass_task_get_next_release(struct sched *sched, int id)
{
	yass_warn(yass_task_exist(sched, id));

	return (yass_sched_get_task_sched(sched, id))->release;
}

YASS_EXPORT int **yass_tasks_exec_new(int n_tasks)
{
	int i, j;
	int **e;

	e = (int **)calloc(n_tasks * YASS_MAX_N_TASK_EXECUTION, sizeof(int));

	if (e == NULL)
		return NULL;

	for (i = 0; i < n_tasks; i++) {
		e[i] = (int *)calloc(YASS_MAX_N_TASK_EXECUTION, sizeof(int));

		if (e[i] == NULL)
			return NULL;

		for (j = 0; j < YASS_MAX_N_TASK_EXECUTION; j++)
			e[i][j] = 0;
	}

	return e;
}

/*
 * Gumbel distribution. See :
 *  http://en.wikipedia.org/wiki/Gumbel_distribution
 */
__attribute__ ((__unused__))
static int generate_gumbel(int min, int max)
{
	double r = -10, u;

	while (r < -5 || r > 20) {

		u = ((double) rand()) / RAND_MAX;

		r = 2 - 4 * log(- log(u));
	}

	r += 5;
	r *= (max - min) / 25;
	r += min;

	return (int)r;
}

/*
 * Normal distribution, using Box–Muller method. See :
 *  http://en.wikipedia.org/wiki/Normal_distribution
 */
__attribute__ ((__unused__))
static int generate_normal(int min, int max)
{
	double r = 10, u, v;

	while (r < -4 || r > 4) {

		u = ((double) rand()) / RAND_MAX;
		v = ((double) rand()) / RAND_MAX;

		r = sqrt(-2 * log(u)) * cos(2 * M_PI * v);
	}

	r += 4;
	r *= (max - min) / 8;
	r += min;

	return (int)r;
}

YASS_EXPORT int **yass_tasks_generate_exec(struct yass_task **tasks,
					   int n_tasks)
{
	int i, j;
	int min, max;

	int **exec_time = yass_tasks_exec_new(n_tasks);

	if (exec_time == NULL)
		return NULL;

	for (i = 0; i < n_tasks; i++) {

		/* min = round(0.5 * tasks[i]->wcet); */
		min = 1;
		max = tasks[i]->wcet;

		for (j = 0; j < YASS_MAX_N_TASK_EXECUTION; j++)
			exec_time[i][j] = generate_normal(min, max);
	}

	return exec_time;
}

YASS_EXPORT int yass_task_is_active(struct sched *sched, int id)
{
	yass_warn(yass_task_exist(sched, id));

	return yass_task_get_cpu(sched, id) > -1;
}

YASS_EXPORT int yass_task_get_exec_hyperperiod(struct sched *sched, int id)
{
	int period = yass_task_get_period(sched, id);
	int wcet = yass_task_get_wcet(sched, id);

	unsigned long long h = yass_sched_get_hyperperiod(sched);

	return wcet * (h / period);
}

YASS_EXPORT int yass_task_get_n_exec(struct sched *sched, int id)
{
	yass_warn(yass_task_exist(sched, id));

	int tick = yass_sched_get_tick(sched);
	int deadline = yass_task_get_deadline(sched, id);
	int period = yass_task_get_period(sched, id);

	int n = -1, t = 0;

	while (t <= tick) {
		if (t % period == 0)
			n++;

		t++;
	}

	if (tick != 0 && tick % period == 0 && deadline == period)
		n--;

 	return n;
}
