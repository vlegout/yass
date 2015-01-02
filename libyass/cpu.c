#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"

#include "common.h"
#include "log.h"
#include "private.h"
#include "scheduler.h"

enum {
	DISCRETE,
	CONTINUOUS
};

struct yass_cpu {
	int task;

	int type;
	char *name;

	double speed;
	double average_speed;

	double consumption;

	int n_discrete;
	double *discrete;
	double *discrete_cons;

	int n_states;
	double *states_penalty;
	double *states_consumption;
	int *states_usage;

	int n_active;
	int *discrete_n_active;

	int idle_time;
	int idle_periods;
	int *idle_lengths;

	int ctx;
};

static void cpu_fill_default(struct yass_cpu *cpu)
{
	int i;

	cpu->task = -1;

	cpu->speed = 1;
	cpu->average_speed = 1;

	cpu->n_states = 0;

	cpu->states_penalty = NULL;
	cpu->states_consumption = NULL;
	cpu->states_usage = 0;

	cpu->consumption = 0;

	cpu->n_discrete = 0;
	cpu->n_active = 0;

	cpu->idle_time = 0;
	cpu->idle_periods = 0;

	cpu->idle_lengths = (int *)malloc(YASS_MAX_IDLE_PERIODS * sizeof(int));

	for (i = 0; i < YASS_MAX_IDLE_PERIODS; i++)
		cpu->idle_lengths[i] = -1;

	cpu->ctx = 0;
}

static int cpu_fill_discrete(struct yass_cpu *c, json_t * objects,
			     const char *filename)
{
	unsigned int i;

	json_t *object, *consumption, *speed;

	c->type = DISCRETE;

	c->n_discrete = json_array_size(objects);

	c->discrete = (double *)calloc(c->n_discrete, sizeof(double));

	if (c->discrete == NULL)
		return -YASS_ERROR_MALLOC;

	c->discrete_cons = (double *)calloc(c->n_discrete, sizeof(double));

	if (c->discrete_cons == NULL)
		return -YASS_ERROR_MALLOC;

	c->discrete_n_active = (int *)calloc(c->n_discrete, sizeof(int));

	if (c->discrete_n_active == NULL)
		return -YASS_ERROR_MALLOC;

	for (i = 0; i < json_array_size(objects); i++) {
		object = json_array_get(objects, i);

		if (!json_is_object(object)) {
			fprintf(stderr,
				"%s: error: speed %d is not an object\n",
				filename, i + 1);
			return -YASS_ERROR_CPU_FILE_JSON;
		}

		speed = json_object_get(object, "speed");

		if (!json_is_real(speed)) {
			fprintf(stderr,
				"error: speed %d: speed is not a double\n",
				i + 1);
			return -YASS_ERROR_CPU_FILE_JSON;
		}

		consumption = json_object_get(object, "consumption");

		if (!json_is_real(consumption)) {
			fprintf(stderr,
				"error: speed %d: consumption is not a double\n",
				i + 1);
			return -YASS_ERROR_CPU_FILE_JSON;
		}

		c->discrete[i] = json_real_value(speed);
		c->discrete_cons[i] = json_real_value(consumption);
		c->discrete_n_active[i] = 0;
	}

	return 0;
}

static int cpu_fill_states(struct yass_cpu *c, json_t * objects,
			   const char *filename)
{
	unsigned int i;

	json_t *object, *consumption, *penalty;

	c->n_states = json_array_size(objects);

	c->states_penalty = (double *)calloc(c->n_states, sizeof(double));

	if (c->states_penalty == NULL)
		return -YASS_ERROR_MALLOC;

	c->states_consumption = (double *)calloc(c->n_states, sizeof(double));

	if (c->states_consumption == NULL)
		return -YASS_ERROR_MALLOC;

	c->states_usage = (int *)calloc(c->n_states, sizeof(int));

	if (c->states_usage == NULL)
		return -YASS_ERROR_MALLOC;

	for (i = 0; i < json_array_size(objects); i++) {
		object = json_array_get(objects, i);

		if (!json_is_object(object)) {
			fprintf(stderr,
				"%s: error: state %d is not an object\n",
				filename, i + 1);
			return -YASS_ERROR_CPU_FILE_JSON;
		}

		consumption = json_object_get(object, "consumption");

		if (!json_is_real(consumption)) {
			fprintf(stderr,
				"error: state %d: consumption is not a double\n",
				i + 1);
			return -YASS_ERROR_CPU_FILE_JSON;
		}

		penalty = json_object_get(object, "penalty");

		if (!json_is_real(penalty)) {
			fprintf(stderr,
				"error: state %d: penalty is not a double\n",
				i + 1);
			return -YASS_ERROR_CPU_FILE_JSON;
		}

		c->states_penalty[i] = json_real_value(penalty);
		c->states_consumption[i] = json_real_value(consumption);
	}

	return 0;
}

static int cpu_alloc_discrete(struct yass_cpu *c1, struct yass_cpu *c2)
{
	c1->n_discrete = c2->n_discrete;

	c1->discrete = (double *)calloc(c2->n_discrete, sizeof(double));

	if (c1->discrete == NULL)
		return -YASS_ERROR_MALLOC;

	memcpy(c1->discrete, c2->discrete, c2->n_discrete * sizeof(double));

	c1->discrete_cons = (double *)calloc(c2->n_discrete, sizeof(double));

	if (c1->discrete_cons == NULL)
		return -YASS_ERROR_MALLOC;

	memcpy(c1->discrete_cons, c2->discrete_cons,
	       c2->n_discrete * sizeof(double));

	c1->discrete_n_active = (int *)calloc(c2->n_discrete, sizeof(int));

	if (c1->discrete_n_active == NULL)
		return -YASS_ERROR_MALLOC;

	memcpy(c1->discrete_n_active, c2->discrete_n_active,
	       c2->n_discrete * sizeof(int));

	return 0;
}

static int cpu_alloc_states(struct yass_cpu *c1, struct yass_cpu *c2)
{
	c1->n_states = c2->n_states;

	c1->states_penalty = (double *)calloc(c2->n_states, sizeof(double));

	if (c1->states_penalty == NULL)
		return -YASS_ERROR_MALLOC;

	memcpy(c1->states_penalty, c2->states_penalty,
	       c2->n_states * sizeof(double));

	c1->states_consumption = (double *)calloc(c2->n_states, sizeof(double));

	if (c1->states_consumption == NULL)
		return -YASS_ERROR_MALLOC;

	memcpy(c1->states_consumption, c2->states_consumption,
	       c2->n_states * sizeof(double));

	c1->states_usage = (int *)calloc(c2->n_states, sizeof(int));

	if (c1->states_usage == NULL)
		return -YASS_ERROR_MALLOC;

	memcpy(c1->states_usage, c2->states_usage, c2->n_states * sizeof(int));

	return 0;
}

YASS_EXPORT struct yass_cpu **yass_cpu_new(const char *cpu, int n_cpus,
					   int *error)
{
	unsigned int i;

	FILE *fp;

	char filename[128];
	char tmp[128];
	char s[1024 * 1024] = "";

	struct yass_cpu *c;
	struct yass_cpu **cpus;

	json_t *objects, *object, *root;
	json_error_t json_error;

	if (yass_find_file(filename, cpu, PROCESSORS)) {
		*error = -YASS_ERROR_CPU_FILE;
		return NULL;
	}

	if ((fp = fopen(filename, "r")) == NULL) {
		*error = -YASS_ERROR_CPU_FILE;
		return NULL;
	}

	while (fgets(tmp, 1024, fp) != NULL)
		strcat(s, tmp);

	root = json_loads(s, 0, &json_error);

	fclose(fp);

	if (!root) {
		fprintf(stderr, "%s: error on line %d: %s\n", filename,
			json_error.line, json_error.text);
		*error = -YASS_ERROR_CPU_FILE_JSON;
		return NULL;
	}

	c = (struct yass_cpu *)malloc(sizeof(struct yass_cpu));

	if (c == NULL) {
		*error = -YASS_ERROR_MALLOC;
		return NULL;
	}

	cpu_fill_default(c);

	object = json_object_get(root, "name");
	if (!json_is_string(object)) {
		fprintf(stderr, "%s: error with cpu name\n", filename);
		*error = -YASS_ERROR_CPU_FILE_JSON;
		return NULL;
	}

	c->name = (char *)malloc(strlen(json_string_value(object)) + 1);

	if (c->name == NULL) {
		*error = -YASS_ERROR_MALLOC;
		return NULL;
	}

	strcpy(c->name, json_string_value(object));

	objects = json_object_get(root, "speeds");

	if (json_is_array(objects) && json_array_size(objects) > 0) {
		if ((*error = cpu_fill_discrete(c, objects, filename)) != 0)
			return NULL;
	} else {
		c->type = CONTINUOUS;
	}

	objects = json_object_get(root, "states");

	if (json_is_array(objects) && json_array_size(objects) > 0) {
		if ((*error = cpu_fill_states(c, objects, filename)) != 0)
			return NULL;
	}

	cpus = (struct yass_cpu **)calloc(n_cpus, sizeof(struct yass_cpu));

	if (cpus == NULL) {
		*error = -YASS_ERROR_MALLOC;
		return NULL;
	}

	for (i = 0; i < (unsigned int)n_cpus; i++) {
		cpus[i] = (struct yass_cpu *)malloc(sizeof(struct yass_cpu));

		if (cpus[i] == NULL) {
			*error = -YASS_ERROR_MALLOC;
			return NULL;
		}

		cpu_fill_default(cpus[i]);

		cpus[i]->name = (char *)malloc(strlen(c->name) + 1);

		if (cpus[i]->name == NULL) {
			*error = -YASS_ERROR_MALLOC;
			return NULL;
		}

		strcpy(cpus[i]->name, c->name);

		cpus[i]->type = c->type;

		if (c->type == DISCRETE) {
			if ((*error = cpu_alloc_discrete(cpus[i], c)) != 0)
				return NULL;
		}

		if (c->n_states > 0) {
			if ((*error = cpu_alloc_states(cpus[i], c)) != 0)
				return NULL;
		}
	}

	json_decref(root);

	if (c->type == DISCRETE) {
		free(c->discrete);
		free(c->discrete_cons);
		free(c->discrete_n_active);
	}

	if (c->states_penalty) {
		free(c->states_penalty);
		free(c->states_consumption);
	}

	if (c->states_usage)
		free(c->states_usage);

	free(c->name);
	free(c->idle_lengths);
	free(c);

	return cpus;
}

YASS_EXPORT void yass_cpu_free(struct sched *sched, int cpu)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	if (c->type == DISCRETE) {
		free(c->discrete);
		free(c->discrete_cons);
		free(c->discrete_n_active);
	}

	if (c->states_penalty) {
		free(c->states_penalty);
		free(c->states_consumption);
	}

	if (c->states_usage)
		free(c->states_usage);

	free(c->idle_lengths);

	free(c->name);
	free(c);
}

YASS_EXPORT int yass_cpu_get_type(struct sched *sched, int cpu)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	return c->type;
}

YASS_EXPORT double yass_cpu_get_speed(struct sched *sched, int cpu)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	return c->speed;
}

YASS_EXPORT void yass_cpu_set_speed(struct sched *sched, int cpu, double speed)
{
	int i, n;
	double processor_speed = 0;

	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	if (speed == 0) {
		processor_speed = 0;
	} else if (speed >= 1) {
		processor_speed = 1;
	} else {
		switch (c->type) {
		case DISCRETE:
			n = c->n_discrete;
			processor_speed = c->discrete[n - 1];
			for (i = 0; i < n; i++) {
				if (c->discrete[i] >= speed)
					processor_speed = c->discrete[i];
			}
			break;
		case CONTINUOUS:
			processor_speed = speed + 0.001;
			break;
		}
	}

	c->speed = processor_speed;

	yass_log_sched(sched, YASS_EVENT_CPU_SPEED, yass_sched_get_index(sched),
		       cpu, (int)(processor_speed * 100), 0, 0);
}

YASS_EXPORT double yass_cpu_get_average_speed(struct sched *sched, int cpu)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	return c->average_speed;
}

YASS_EXPORT void yass_cpu_set_average_speed(struct sched *sched, int cpu,
					    double average_speed)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	c->average_speed = average_speed;
}

YASS_EXPORT double yass_cpu_get_consumption(struct sched *sched, int cpu)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	return c->consumption;
}

YASS_EXPORT void yass_cpu_set_consumption(struct sched *sched, int cpu,
					  double consumption)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	c->consumption = consumption;
}

YASS_EXPORT int yass_cpu_get_task(struct sched *sched, int cpu)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	return c->task;
}

YASS_EXPORT void yass_cpu_set_task(struct sched *sched, int cpu, int id)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	c->task = id;
}

YASS_EXPORT int yass_cpu_is_active(struct sched *sched, int cpu)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	return c->task != -1;
}

YASS_EXPORT void yass_cpu_remove_task(struct sched *sched, int cpu)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	c->task = -1;
}

YASS_EXPORT int yass_cpu_get_nstates(struct sched *sched)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, 0);

	return c->n_states;
}

YASS_EXPORT double yass_cpu_get_state_consumption(struct sched *sched,
						  int state)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, 0);

	return c->states_consumption[state];
}

YASS_EXPORT double yass_cpu_get_state_penalty(struct sched *sched, int state)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, 0);

	return c->states_penalty[state];
}

YASS_EXPORT int yass_cpu_get_state_usage(struct sched *sched, int cpu,
					 int state)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	return c->states_usage[state];
}

YASS_EXPORT int yass_cpu_get_nactive(struct sched *sched, int cpu)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	return c->n_active;
}

YASS_EXPORT void yass_cpu_set_nactive(struct sched *sched, int cpu,
				      int n_active)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	c->n_active = n_active;
}

YASS_EXPORT void yass_cpu_cons_inc(struct sched *sched, int cpu)
{
	int criticality, i, id;

	double average_speed = yass_cpu_get_average_speed(sched, cpu);
	double consumption = yass_cpu_get_consumption(sched, cpu);
	int n_active = yass_cpu_get_nactive(sched, cpu);
	double speed = yass_cpu_get_speed(sched, cpu);

	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	id = yass_cpu_get_task(sched, cpu);

	if (id == -1 || yass_sched_task_is_idle_task(sched, id))
		return;

	/*
	 * Do not add the consumption of tasks with a criticality of
	 * 1.
	 */
	criticality = yass_task_get_criticality(sched, id);

	if (criticality == 1)
		return;

	if (speed != 0) {
		n_active++;

		average_speed = (average_speed * (n_active - 1) +
				 speed) / n_active;

		yass_cpu_set_nactive(sched, cpu, n_active);
		yass_cpu_set_average_speed(sched, cpu, average_speed);
	}

	switch (yass_cpu_get_type(sched, cpu)) {
	case DISCRETE:
		for (i = 0; i < c->n_discrete; i++) {
			if (speed == c->discrete[i]) {
				c->discrete_n_active[i]++;
				consumption += c->discrete_cons[i];
			}
		}
		break;
	case CONTINUOUS:
		/* consumption += pow(speed, 2.5); */
		consumption += speed;

		break;
	}

	yass_cpu_set_consumption(sched, cpu, consumption);
}

YASS_EXPORT void yass_cpu_cons_add_penalty(struct sched *sched, int cpu,
					   double penalty)
{
	double consumption = yass_cpu_get_consumption(sched, cpu);

	consumption += penalty;

	yass_cpu_set_consumption(sched, cpu, consumption);
}

YASS_EXPORT double yass_cpu_cons_get_total(struct sched *sched)
{
	int i;
	double consumption = 0;

	for (i = 0; i < yass_sched_get_ncpus(sched); i++)
		consumption += yass_cpu_get_consumption(sched, i);

	return consumption;
}

YASS_EXPORT double yass_cpu_get_max_penalty(struct sched *sched)
{
	int i, n;
	double max = 0;

	for (i = 0; i < yass_sched_get_ncpus(sched); i++) {
		n = yass_cpu_get_nstates(sched);

		if (max < yass_cpu_get_state_penalty(sched, n - 1)) {
			max = yass_cpu_get_state_penalty(sched, n - 1);
		}
	}

	return max;
}

static double get_percentage(double n, double max)
{
	return (n * 100.0) / max;
}

YASS_EXPORT void yass_cpu_cons_print(struct yass *yass)
{
	int i, j;
	double total = 0;

	struct sched *sched = yass_get_sched(yass, 0);
	struct yass_cpu *c = yass_sched_get_cpu(sched, 0);

	int tick = yass_sched_get_tick(sched);

	printf("\n");
	printf("Processor: %s\n", c->name);
	printf("=============\n\n");

	printf("Statistics:\n");
	printf("-------------\n");
	printf("Number of ticks: %d\n", tick);
	printf("\n");

	for (i = 0; i < yass_sched_get_ncpus(sched); i++) {
		c = yass_sched_get_cpu(sched, i);

		printf("== Processor %d ==\n", i + 1);
		printf("Activity: %d (%2.2lf %%)\n", c->n_active,
		       get_percentage(c->n_active, tick));
		printf("Average speed (when active): %1.2lf\n",
		       c->average_speed);
		printf("\n");

		switch (c->type) {
		case DISCRETE:
			printf("Number of speeds %d\n", c->n_discrete);
			for (j = 0; j < c->n_discrete; j++) {
				printf("- %1.2lf: ", c->discrete[j]);
				printf("%d (%2.2lf %%)\n",
				       c->discrete_n_active[j],
				       get_percentage(c->discrete_n_active[j],
						      tick));
			}
			break;
		case CONTINUOUS:
			break;
		}
		printf("\n");

		printf("Consumption %1.2lf %%\n",
		       get_percentage(c->consumption, tick));

		printf("-------------\n");
	}

	for (i = 0; i < yass_sched_get_ncpus(sched); i++)
		total += yass_cpu_get_consumption(sched, i);

	printf("\nTotal consumption: %lf\n", total);
}

YASS_EXPORT void yass_cpu_print_consumption(struct yass *yass)
{
	int i, j, n;
	double c;

	struct sched *sched;

	for (i = 0; i < yass_get_nschedulers(yass); i++) {
		sched = yass_get_sched(yass, i);

		c = 0;
		n = 0;

		for (j = 0; j < yass_sched_get_ncpus(sched); j++) {
			c += yass_cpu_get_consumption(sched, j);
			n += yass_cpu_get_idle_periods(sched, j);
		}

		printf("%s: energy consumption %lf n idle periods %d\n",
		       yass_sched_get_name(sched), c, n);
	}
}

YASS_EXPORT char *yass_cpu_get_name(struct sched *sched)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, 0);

	return c->name;
}

YASS_EXPORT int yass_cpu_get_idle_time(struct sched *sched, int cpu)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	return c->idle_time;
}

YASS_EXPORT void yass_cpu_increase_idle_time(struct sched *sched, int cpu)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	c->idle_time++;
}

YASS_EXPORT void yass_cpu_reset_idle_time(struct sched *sched, int cpu)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	c->idle_time = 0;
}

YASS_EXPORT int yass_cpu_get_idle_periods(struct sched *sched, int cpu)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	return c->idle_periods;
}

YASS_EXPORT void yass_cpu_increase_idle_periods(struct sched *sched, int cpu)
{
	int choice_i = -1, i = 0;
	double cons, choice_c = INT_MAX;

	double penalty, consumption;

	double idle_time = yass_cpu_get_idle_time(sched, cpu);

	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	while (c->idle_lengths[i] != -1)
		i++;

	c->idle_lengths[i] = idle_time;

	for (i = c->n_states - 1; i >= 0; i--) {
		penalty = c->states_penalty[i];
		consumption = c->states_consumption[i];

		cons = (penalty - consumption) / 2 + idle_time * consumption;

		if (idle_time >= penalty && cons < choice_c) {
			choice_c = cons;
			choice_i = i;
		}
	}

	if (choice_i != -1 && choice_c < idle_time) {
		penalty = c->states_penalty[choice_i];
		consumption = c->states_consumption[choice_i];

		c->consumption += (penalty - consumption) / 2;
		c->consumption += idle_time * consumption;
		c->states_usage[choice_i]++;
	} else {
		/*
		 * No low-power state can be used, increase consumption as is
		 * the processor was at full power during the whole interval
		 */
		c->consumption = idle_time * yass_cpu_get_speed(sched, cpu);
	}

	c->idle_periods++;
}

YASS_EXPORT int yass_cpu_get_idle_length(struct sched *sched, int cpu, int i)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	return c->idle_lengths[i];
}

YASS_EXPORT void yass_cpu_idle_print(struct sched *sched)
{
	int i;

	for (i = 0; i < yass_sched_get_ncpus(sched); i++)
		printf("%d ", yass_cpu_get_idle_periods(sched, i));

	printf("\n");
}

YASS_EXPORT int yass_cpu_get_context_switches(struct sched *sched, int cpu)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	return c->ctx;
}

YASS_EXPORT void yass_cpu_add_context_switches(struct sched *sched, int cpu)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, cpu);

	c->ctx++;
}

YASS_EXPORT void yass_cpu_print_context_switches(struct sched *sched)
{
	int i;

	for (i = 0; i < yass_sched_get_ncpus(sched); i++)
		printf("%d ", yass_cpu_get_context_switches(sched, i));

	printf("\n");
}

YASS_EXPORT double yass_cpu_get_bet(struct sched *sched)
{
	return yass_cpu_get_state_penalty(sched, 0);
}

YASS_EXPORT double yass_cpu_get_lowest_speed(struct sched *sched)
{
	struct yass_cpu *c = yass_sched_get_cpu(sched, 0);

	return c->discrete[c->n_discrete - 1];
}
