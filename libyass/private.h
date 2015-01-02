#ifndef _YASS_PRIVATE_H
#define _YASS_PRIVATE_H

enum {
	SCHED,
	DATA,
	PROCESSORS,
	TESTS,
};

#define YASS_EXPORT __attribute__ ((visibility("default")))

#endif				/* _YASS_PRIVATE_H */
