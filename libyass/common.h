#ifndef _YASS_COMMON_H
#define _YASS_COMMON_H

#include <limits.h>

#define YASS_MAX_N_CPU 64
#define YASS_MAX_N_TASKS 128
#define YASS_MAX_N_SCHEDULERS 10
#define YASS_MAX_N_TASK_EXECUTION 4096
#define YASS_MAX_N_VMS 64

#define YASS_DEFAULT_N_JOBS 1
#define YASS_DEFAULT_N_CPU 3
#define YASS_DEFAULT_N_HYPERPERIODS 1

#define YASS_DEFAULT_MIN_TICKS 100

#define YASS_MAX_SEGMENTS 10
#define YASS_MAX_THREADS 10

#define YASS_MAX_PERIOD INT_MAX

#define YASS_MIN_WCET 10

#define YASS_CPU_MODE_NORMAL -1
#define YASS_CPU_MODE_WAKEUP -2

#define YASS_IDLE_TASK_ID 987

#define YASS_MAX_IDLE_PERIODS 10000

#ifdef __cplusplus
extern "C" {
#endif

enum yass_events {
	YASS_EVENT_TASK_RELEASE,
	YASS_EVENT_TASK_DEADLINE,

	YASS_EVENT_TASK_RUN,
	YASS_EVENT_TASK_TERMINATE,

	YASS_EVENT_CPU_SPEED,
	YASS_EVENT_CPU_MODE,
	YASS_EVENT_CPU_CONSUMPTION,
};

enum yass_execution_class {
	YASS_OFFLINE,
	YASS_ONLINE,
};

#define yass_warn(exp)							\
	do {								\
		if ((exp) == 0) {					\
			fprintf(stderr,					\
				"Warning: %s:%d: %s: `%s' failed\n",	\
				__FILE__, __LINE__, __func__, #exp);	\
		}							\
	} while (0)

#define yass_info(arg...) fprintf(stderr, ## arg)

enum yass_error_code {
	YASS_ERROR_0,
	YASS_ERROR_CPU_FILE,
	YASS_ERROR_CPU_FILE_JSON,
	YASS_ERROR_DATA_FILE,
	YASS_ERROR_DATA_FILE_JSON,
	YASS_ERROR_DEFAULT,
	YASS_ERROR_DLOPEN,
	YASS_ERROR_DLSYM,
	YASS_ERROR_FILE,
	YASS_ERROR_ID_NOT_UNIQUE,
	YASS_ERROR_MALLOC,
	YASS_ERROR_MORE_THAN_ONE_CPU,
	YASS_ERROR_N_CPUS,
	YASS_ERROR_N_JOBS,
	YASS_ERROR_N_TICKS,
	YASS_ERROR_NOT_SCHEDULABLE,
	YASS_ERROR_SCHEDULE,
	YASS_ERROR_SCHEDULER_NOT_UNIQUE,
	YASS_ERROR_SCHEDULER_NAME_TOO_SHORT,
	YASS_ERROR_THREAD_CREATE,
	YASS_ERROR_TICKS_HYPERPERIOD
};

#ifdef __cplusplus
}
#endif

#endif				/* _YASS_COMMON_H */