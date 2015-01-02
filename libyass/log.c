#include <stdio.h>
#include <stdlib.h>

#include "log.h"

#include "common.h"
#include "private.h"
#include "scheduler.h"
#include "yass.h"

YASS_EXPORT FILE *yass_log_new(struct yass *yass, const char *output)
{
	FILE *fp;
	char tmp[128] = "";

	struct sched *sched = yass_get_sched(yass, 0);

	if ((fp = fopen(output, "r")) != NULL) {
		fclose(fp);

		sprintf(tmp, "rm -f %s\n", output);
		system(tmp);
	}

	if ((fp = fopen(output, "w+")) == NULL)
		return NULL;

	fprintf(fp, "%d %d %d %d\n",
		yass_sched_get_ntasks(sched),
		yass_sched_get_ncpus(sched),
		yass_get_nticks(yass), yass_get_nschedulers(yass));

	return fp;
}

YASS_EXPORT void yass_log_sched(struct sched *sched,
				int i1, int i2, int i3, int i4, int i5, int i6)
{
	char tmp[128] = "";

	if (yass_sched_get_verbose(sched)) {
		switch (i1) {
		case YASS_EVENT_TASK_RELEASE:
			printf("%d RELEASE task %d\n", i3, i2);
			break;
		case YASS_EVENT_TASK_DEADLINE:
			printf("%d DEADLINE task %d\n", i3, i2);
			break;

		case YASS_EVENT_TASK_RUN:
			printf("%d RUN sched %d task %d cpu %d\n",
			       i4, i2, i3, i5);
			break;
		case YASS_EVENT_TASK_TERMINATE:
			printf("%d TERMINATE sched %d task %d cpu %d\n",
			       i4, i2, i3, i5);
			break;

		case YASS_EVENT_CPU_SPEED:
			printf("%d SPEED sched %d cpu %d speed %d\n",
			       yass_sched_get_tick(sched), i2, i3, i4);
			break;
		case YASS_EVENT_CPU_MODE:
			printf("%d MODE sched %d cpu %d mode %d\n",
			       yass_sched_get_tick(sched), i2, i3, i4);
			break;
		case YASS_EVENT_CPU_CONSUMPTION:
			/* printf("%d CONS sched %d cpu %d consumption %d\n", */
			/*        yass_sched_get_tick(sched), i2, i3, i4); */
			break;
		}
	}

	if (yass_sched_get_fp(sched) == NULL)
		return;

	sprintf(tmp, "%d %d %d %d %d %d", i1, i2, i3, i4, i5, i6);

	fprintf(yass_sched_get_fp(sched), "%s\n", tmp);
}
