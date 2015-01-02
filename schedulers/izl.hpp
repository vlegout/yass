
#include <libyass/yass.h>

#ifdef __cplusplus
extern "C" {
#endif

	const char *name();
	int offline(struct sched *sched);
	int schedule(struct sched *sched);
	int sched_close(struct sched *sched);

#ifdef __cplusplus
}
#endif

int izl_offline(struct sched *sched, int objective);

int izl_schedule(struct sched *sched);

int izl_close(struct sched *sched __attribute__ ((unused)));
