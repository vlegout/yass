#ifndef _YASS_CPU_H
#define _YASS_CPU_H

#include "yass.h"

#ifdef __cplusplus
extern "C" {
#endif

struct yass_cpu **yass_cpu_new(const char *cpu, int n_cpus, int *error);

void yass_cpu_free(struct sched *sched, int cpu);

int yass_cpu_get_type(struct sched *sched, int cpu);

double yass_cpu_get_speed(struct sched *sched, int cpu);

void yass_cpu_set_speed(struct sched *sched, int cpu, double speed);

double yass_cpu_get_average_speed(struct sched *sched, int cpu);

void yass_cpu_set_average_speed(struct sched *sched, int cpu,
				double average_speed);

double yass_cpu_get_consumption(struct sched *sched, int cpu);

void yass_cpu_set_consumption(struct sched *sched, int cpu, double consumption);

int yass_cpu_get_task(struct sched *sched, int cpu);

void yass_cpu_set_task(struct sched *sched, int cpu, int id);

int yass_cpu_is_active(struct sched *sched, int cpu);

void yass_cpu_remove_task(struct sched *sched, int cpu);

int yass_cpu_get_nstates(struct sched *sched);

double yass_cpu_get_state_consumption(struct sched *sched, int state);

double yass_cpu_get_state_penalty(struct sched *sched, int state);

int yass_cpu_get_state_usage(struct sched *sched, int cpu, int state);

int yass_cpu_get_nactive(struct sched *sched, int cpu);

void yass_cpu_set_nactive(struct sched *sched, int cpu, int n_active);

void yass_cpu_cons_inc(struct sched *sched, int cpu);

void yass_cpu_cons_add_penalty(struct sched *sched, int cpu, double penalty);

double yass_cpu_cons_get_total(struct sched *sched);

double yass_cpu_get_max_penalty(struct sched *sched);

void yass_cpu_cons_print(struct yass *yass);

void yass_cpu_print_consumption(struct yass *yass);

char *yass_cpu_get_name(struct sched *sched);

int yass_cpu_get_idle_time(struct sched *sched, int cpu);

void yass_cpu_reset_idle_time(struct sched *sched, int cpu);

void yass_cpu_increase_idle_time(struct sched *sched, int cpu);

int yass_cpu_get_idle_periods(struct sched *sched, int cpu);

void yass_cpu_increase_idle_periods(struct sched *sched, int cpu);

int yass_cpu_get_idle_length(struct sched *sched, int cpu, int i);

void yass_cpu_idle_print(struct sched *sched);

int yass_cpu_get_context_switches(struct sched *sched, int cpu);

void yass_cpu_add_context_switches(struct sched *sched, int cpu);

void yass_cpu_print_context_switches(struct sched *sched);

double yass_cpu_get_bet(struct sched *sched);

double yass_cpu_get_lowest_speed(struct sched *sched);

#ifdef __cplusplus
}
#endif

#endif				/* _YASS_CPU_H */
