#include <libyass/common.h>
#include <libyass/cpu.h>
#include <libyass/helpers.h>
#include <libyass/list.h>
#include <libyass/log.h>
#include <libyass/scheduler.h>
#include <libyass/task.h>

/*
 * Chaque tâche possède un nombre fixe de thread. Tous ces threads ont
 * le même WCET et doivent être exécutés en parallèle.
 *
 * C'est un ordonnancement en-ligne. Pour ce simulateur, l'algorithme
 * regarde si la tâche prête ayant la priorité la plus importante peut
 * être ordonnancée, et l'ordonnance si possible. Sinon, la tâche
 * suivante est testée et ainsi de suite pour toutes les tâches.
 *
 * Par rapport à global, cela rajoute quelques tests en plus.
 */

struct yass_list *stalled;
struct yass_list *ready;
struct yass_list *running;

const char *name()
{
	return "Gang EDF";
}

int offline(struct sched *sched)
{
	int i;

	ready = yass_list_new(yass_sched_get_ntasks(sched));
	running = yass_list_new(yass_sched_get_ntasks(sched));
	stalled = yass_list_new(yass_sched_get_ntasks(sched));

	for (i = 0; i < yass_sched_get_ntasks(sched); i++)
		yass_list_add(stalled, yass_task_get_id(sched, i));

	return 0;
}

int schedule(struct sched *sched)
{
	int c, i, id, remaining_threads;
	int tick = yass_sched_get_tick(sched);
	int n_cpus = yass_sched_get_ncpus(sched);

	struct yass_list *candidate = yass_list_new(YASS_MAX_N_TASKS);

	for (i = 0; i < yass_list_n(running); i++) {
		id = yass_list_get(running, i);
		c = yass_task_get_cpu(sched, id);

		yass_task_exec_inc(sched, id, yass_cpu_get_speed(sched, c));
	}

	/*
	 * Une tâche courante vient de terminer son exécution, elle est
	 * placée dans la list stalled.
	 */
	for (i = 0; i < n_cpus; i++) {
		id = yass_cpu_get_task(sched, i);

		if (id != -1 &&
		    yass_task_get_exec(sched, id) >= yass_task_get_wcet(sched,
									id)) {
			yass_task_set_exec(sched, id, 1000000);

			if (yass_list_present(running, id))
				yass_list_remove(running, id);
			if (!yass_list_present(stalled, id))
				yass_list_add(stalled, id);

			yass_log_sched(sched, YASS_EVENT_TASK_TERMINATE,
				       yass_sched_get_index(sched),
				       id, tick, i, 0);

			yass_cpu_set_task(sched, i, -1);
		}
	}

	for (i = 0; i < yass_sched_get_ntasks(sched); i++) {
		id = yass_task_get_id(sched, i);

		if (yass_task_get_exec(sched, id) == 1000000)
			yass_task_set_exec(sched, id, 0);
	}

	yass_check_ready_tasks(sched, stalled, ready);

	/*
	 * Place dans la list candidate toutes les tâches pouvant
	 * potentiellement être ordonnancées. Ce sont les tâches des
	 * lists running et ready.
	 */
	for (i = 0; i < n_cpus; i++) {
		id = yass_cpu_get_task(sched, i);

		if (id != -1 && !yass_list_present(candidate, id))
			yass_list_add(candidate, id);
	}
	for (i = 0; i < yass_list_n(ready); i++) {
		if (yass_list_get(ready, i) != -1)
			yass_list_add(candidate, yass_list_get(ready, i));
	}

	yass_sort_list_int(sched, candidate, yass_task_time_to_deadline);

	/*
	 * Il n'y qu'un nombre limité de cpu. Supprime les tâches qui
	 * ne seront pas exécutées de la list candidate.
	 */
	int free_cpus = n_cpus;

	for (i = 0; i < yass_list_n(candidate); i++) {
		id = yass_list_get(candidate, i);

		if (yass_task_get_threads(sched, id) <= free_cpus) {
			free_cpus -= yass_task_get_threads(sched, id);
		} else {
			yass_list_remove(candidate, id);
			i--;
		}
	}

	/*
	 * Préempte les tâche en cours d'exécution qui n'ont pas été
	 * sélectionnées
	 */
	for (i = 0; i < n_cpus; i++) {
		id = yass_cpu_get_task(sched, i);

		if (!yass_list_present(candidate, id)) {
			yass_log_sched(sched, YASS_EVENT_TASK_TERMINATE,
				       yass_sched_get_index(sched),
				       id, tick, i, 0);

			if (yass_list_present(running, id))
				yass_list_remove(running, id);
			if (!yass_list_present(ready, id))
				yass_list_add(ready, id);

			yass_cpu_set_task(sched, i, -1);
		}
	}

	/* Laisse les tâches déjà actives sur le même cpu */
	for (i = 0; i < n_cpus; i++) {
		id = yass_cpu_get_task(sched, i);

		if (id != -1 && yass_list_present(candidate, id))
			yass_list_remove(candidate, id);
	}

	/*
	 * Il ne reste plus que les nouvelles tâches, on leur assigne
	 * un processeur de façon aléatoire
	 */
	if (yass_list_n(candidate) == 0)
		goto out;

	id = yass_list_get(candidate, 0);
	remaining_threads = yass_task_get_threads(sched, id);

	for (i = 0; i < n_cpus; i++) {
		if (yass_cpu_get_task(sched, i) == -1 && id != -1) {
			yass_log_sched(sched, YASS_EVENT_TASK_RUN,
				       yass_sched_get_index(sched),
				       id, tick, i, 0);

			yass_cpu_set_task(sched, i, id);

			remaining_threads--;
			if (remaining_threads == 0) {
				yass_list_remove(candidate, id);

				yass_list_remove(ready, id);
				yass_list_add(running, id);

				if (yass_list_n(candidate) == 0)
					goto out;

				id = yass_list_get(candidate, 0);
				remaining_threads =
				    yass_task_get_threads(sched, id);
			}
		}
	}

 out:
	yass_warn(yass_list_n(candidate) == 0);

	/*
	 * Toutes les tâche actives doivent être dans la list running
	 * et toutes les tâches dans la list running doivent être
	 * actives.
	 */
	for (i = 0; i < n_cpus; i++)
		yass_warn(yass_list_present
			  (running, yass_cpu_get_task(sched, i)));

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
