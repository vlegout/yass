#ifndef _YASS_LOG_H
#define _YASS_LOG_H

#include "yass.h"

#ifdef __cplusplus
extern "C" {
#endif

FILE *yass_log_new(struct yass *yass, const char *output);

void yass_log_sched(struct sched *sched,
		    int i1, int i2, int i3, int i4, int i5, int i6);

void yass_log_free(void);

#ifdef __cplusplus
}
#endif

#endif				/* _YASS_LOG_H */
