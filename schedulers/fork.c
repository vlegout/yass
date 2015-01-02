#include <stdlib.h>

#include <libyass/common.h>
#include <libyass/cpu.h>
#include <libyass/helpers.h>
#include <libyass/list.h>
#include <libyass/log.h>
#include <libyass/scheduler.h>
#include <libyass/task.h>

struct yass_list *stalled;
struct yass_list *ready;
struct yass_list *running;

struct yass_task_extra {
	int id;

	int active_s;

	double exec[YASS_MAX_SEGMENTS][YASS_MAX_THREADS + 1];
};

struct yass_task_extra *task_extra;

int threads_done[YASS_MAX_N_TASKS];

const char *name()
{
	return "Fork-join";
}

int offline(struct sched *sched)
{
	int i, id, j, k;
	int n_tasks = yass_sched_get_ntasks(sched);

	ready = yass_list_new(n_tasks * YASS_MAX_THREADS);
	running = yass_list_new(n_tasks * YASS_MAX_THREADS);
	stalled = yass_list_new(n_tasks * YASS_MAX_THREADS);

	for (i = 0; i < n_tasks; i++)
		yass_list_add(stalled, yass_task_get_id(sched, i));

	task_extra = (struct yass_task_extra *)
	    malloc(n_tasks * sizeof(struct yass_task_extra));

	for (i = 0; i < n_tasks; i++) {
		id = yass_task_get_id(sched, i);

		task_extra[i].id = id;
		task_extra[i].active_s = 0;

		for (j = 0; j < YASS_MAX_SEGMENTS; j++)
			for (k = 0; k < YASS_MAX_THREADS + 1; k++)
				task_extra[i].exec[j][k] = 0;

		threads_done[i] = 0;
	}

	return 0;
}

static int fork_get_task_id(struct sched *sched, int id)
{
	if (id == -1 || yass_task_exist(sched, id))
		return id;

	return id / 10;
}

static int fork_get_thread(int id)
{
	return id % 10;
}

static int fork_get_active_s(int task_index)
{
	return task_extra[task_index].active_s;
}

static void fork_reset_active_s(int task_index)
{
	task_extra[task_index].active_s = 0;
}

static void fork_inc_active_s(int task_index)
{
	task_extra[task_index].active_s++;
}

static void fork_exec_inc(int task_index, int thread)
{
	int active_s = fork_get_active_s(task_index);

	task_extra[task_index].exec[active_s][thread]++;
}

static int fork_get_exec(int task_index, int thread)
{
	int active_s = fork_get_active_s(task_index);

	return task_extra[task_index].exec[active_s][thread];
}

static void fork_set_exec(int task_index, int thread, double exec)
{
	int active_s = fork_get_active_s(task_index);

	task_extra[task_index].exec[active_s][thread] = exec;
}

static int fork_task_is_finished(struct sched *sched, int id_corrected,
				 int thread)
{
	int task_index = yass_task_get_from_id(sched, id_corrected);

	double exec = fork_get_exec(task_index, thread);

	int active_s = fork_get_active_s(task_index);
	int s = yass_task_get_segments(sched, id_corrected)[active_s];

	return exec >= s;
}

static void fork_sort_list_time_to_deadline(struct sched *sched,
					    struct yass_list *q)
{
	int i, j, id, id2, n;
	int min, tmp;

	for (i = 0; i < yass_list_n(q); i++) {
		id = fork_get_task_id(sched, yass_list_get(q, i));
		min = yass_task_time_to_deadline(sched, id);

		n = -1;

		for (j = i + 1; j < yass_list_n(q); j++) {
			id2 = fork_get_task_id(sched, yass_list_get(q, j));

			if (id2 != -1 &&
			    ((yass_task_time_to_deadline(sched, id2) < min) ||
			     (yass_task_time_to_deadline(sched, id2) == min &&
			      yass_list_get(q, i) < yass_list_get(q, j)))) {

				n = j;
				min = yass_task_time_to_deadline(sched, id2);
			}
		}

		if (n != -1) {
			tmp = yass_list_get(q, i);
			yass_list_set(q, i, yass_list_get(q, n));
			yass_list_set(q, n, tmp);
		}
	}
}

static void handle_next_tasks(struct sched *sched, int task_index, int id)
{
	int i;

	int active_s = fork_get_active_s(task_index);
	int parallel = yass_task_get_parallel(sched, id);

	if (active_s < yass_task_get_s(sched, id) - 1) {

		if (active_s % 2 == 1) {
			threads_done[task_index]++;

			if (threads_done[task_index] == parallel) {
				threads_done[task_index] = 0;
				yass_list_add(ready, id * 10);
				fork_inc_active_s(task_index);
			}
		} else {
			for (i = 0; i < yass_task_get_parallel(sched, id); i++)
				yass_list_add(ready, id * 10 + i + 1);

			fork_inc_active_s(task_index);
		}

	} else {
		yass_list_add(stalled, id);
		fork_reset_active_s(task_index);
	}
}

int schedule(struct sched *sched)
{
	int cpu, i, id, id_corrected, j;
	int task_index, thread;

	int tick = yass_sched_get_tick(sched);
	int n_cpus = yass_sched_get_ncpus(sched);
	int n_tasks = yass_sched_get_ntasks(sched);

	struct yass_list *candidate =
	    yass_list_new(YASS_MAX_N_TASKS * YASS_MAX_SEGMENTS);

	/*
	 * Incrémenter l'exécution de toutes les tâches en cours
	 * d'exécution
	 */
	for (i = 0; i < n_cpus; i++) {
		if (!yass_cpu_is_active(sched, i))
			continue;

		id = yass_cpu_get_task(sched, i);
		id_corrected = fork_get_task_id(sched, id);

		yass_warn(yass_task_exist(sched, id_corrected));

		if (yass_task_get_s(sched, id_corrected) == -1) {
			yass_task_exec_inc(sched, id_corrected, 1);
		} else {
			thread = fork_get_thread(id);
			task_index = yass_task_get_from_id(sched, id_corrected);

			fork_exec_inc(task_index, thread);
		}
	}

	/*
	 * Certaintes tâches ont fini leur exécution, elles sont
	 * placées dans la list stalled
	 */
	for (i = 0; i < n_cpus; i++) {
		if (!yass_cpu_is_active(sched, i))
			continue;

		id = yass_cpu_get_task(sched, i);
		id_corrected = fork_get_task_id(sched, id);
		thread = fork_get_thread(id);

		if (yass_task_get_s(sched, id_corrected) == -1 &&
		    yass_task_get_exec(sched, id) >= yass_task_get_wcet(sched,
									id)) {

			yass_terminate_task(sched, i, id, running, stalled);

		} else if (yass_task_exist(sched, id / 10) &&
			   fork_task_is_finished(sched, id_corrected, thread)) {

			task_index = yass_task_get_from_id(sched, id_corrected);

			yass_list_remove(running, id);

			yass_log_sched(sched, YASS_EVENT_TASK_TERMINATE,
				       yass_sched_get_index(sched),
				       id_corrected, tick, i, 0);

			yass_cpu_remove_task(sched, i);

			fork_set_exec(task_index, thread, 0);

			handle_next_tasks(sched, task_index, id_corrected);
		}
	}

	/*
	 * Certaintes tâches deviennent prêtes, elles sont placées
	 * dans la list ready.
	 *
	 * Pour les tâches fork-join, le premier segment est forcément
	 * non parallèle, donc il peut être placé dans la list ready
	 * (en n'oubliant pas de multiplier son id par 10)
	 */
	for (i = 0; i < n_tasks; i++) {
		id = yass_task_get_id(sched, i);

		if (tick % yass_task_get_period(sched, id) == 0 &&
		    yass_list_present(stalled, id)) {

			yass_list_remove(stalled, id);

			if (yass_task_get_s(sched, id) == -1)
				yass_list_add(ready, id);
			else
				yass_list_add(ready, id * 10);
		}
	}

	/*
	 * Combien de tâches sont candidates ?
	 *
	 * Ces tâches peuvent être dans les lists ready et
	 * running. De plus, certaines ont plusieurs threads donc ont
	 * besoin de plusieurs cpus.
	 */
	for (i = 0; i < yass_list_n(running); i++)
		yass_list_add(candidate, yass_list_get(running, i));
	for (i = 0; i < yass_list_n(ready); i++)
		yass_list_add(candidate, yass_list_get(ready, i));

	fork_sort_list_time_to_deadline(sched, candidate);

	/*
	 * Parmi toutes les tâches candidates, sélectionne celles qui
	 * seront ordonnancées
	 */
	while (yass_list_n(candidate) > n_cpus) {
		for (i = n_cpus; i < n_cpus + yass_list_n(candidate) - 1; i++) {
			yass_list_remove(candidate,
					 yass_list_get(candidate, i));
			break;
		}
	}
	yass_warn(n_cpus >= yass_list_n(candidate));

	/*
	 * Préempte les tâches en cours d'exécution qui n'ont pas été
	 * sélectionnnées
	 */
	for (i = 0; i < n_tasks; i++) {
		id = yass_task_get_id(sched, i);

		if (yass_task_get_s(sched, id) == -1 &&
		    yass_list_present(running, id) &&
		    !yass_list_present(candidate, id)) {

			cpu = yass_task_get_cpu(sched, id);
			yass_preempt_task(sched, cpu, running, ready);

		} else if (yass_task_get_s(sched, id) > 0) {

			for (j = 0; j < yass_task_get_parallel(sched, id) + 1;
			     j++) {

				id_corrected = id * 10 + j;
				cpu = yass_task_get_cpu(sched, id_corrected);

				if (yass_list_present(running, id_corrected) &&
				    !yass_list_present(candidate,
						       id_corrected)) {

					yass_log_sched(sched,
						       YASS_EVENT_TASK_TERMINATE,
						       yass_sched_get_index
						       (sched), id, tick, cpu,
						       0);

					yass_list_remove(running, id_corrected);
					yass_list_add(ready, id_corrected);

					yass_cpu_remove_task(sched, cpu);
				}
			}
		}
	}

	/*
	 * Affecte les tâches aux cpus. En prenant soin de ne pas
	 * migrer une tâche sans nécessité
	 */
	for (i = 0; i < n_cpus; i++) {
		id = yass_cpu_get_task(sched, i);

		if (id != -1 && yass_list_present(candidate, id))
			yass_list_remove(candidate, id);
	}

	for (i = 0; i < n_cpus; i++) {
		id = yass_list_get(candidate, 0);
		id_corrected = fork_get_task_id(sched, id);

		if (!yass_cpu_is_active(sched, i) && id != -1) {

			task_index = yass_task_get_from_id(sched, id_corrected);

			yass_log_sched(sched, YASS_EVENT_TASK_RUN,
				       yass_sched_get_index(sched),
				       id_corrected, tick, i, 0);

			yass_list_remove(ready, id);
			yass_list_add(running, id);

			yass_cpu_set_task(sched, i, id);

			yass_list_remove(candidate, id);
		}
	}

	yass_warn(yass_list_n(candidate) == 0);

	yass_list_free(candidate);

	return 0;
}

int sched_close(struct sched *sched __attribute__ ((__unused__)))
{
	yass_list_free(stalled);
	yass_list_free(ready);
	yass_list_free(running);

	return 0;
}
