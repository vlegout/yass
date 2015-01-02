#include <stdio.h>
#include <stdlib.h>

#include "list.h"

#include "common.h"
#include "private.h"

struct yass_list {
	int n;			/* Number of tasks */
	int size;
	int *tasks;
};

YASS_EXPORT void yass_list_add(struct yass_list *q, int n)
{
	if (q == NULL)
		return;

	yass_warn(q->n < q->size);
	yass_warn(!yass_list_present(q, n));

	q->tasks[q->n] = n;

	q->n++;
}

YASS_EXPORT int yass_list_remove(struct yass_list *q, int n)
{
	int i;
	int found = -1;

	if (q == NULL)
		return -1;

	yass_warn(yass_list_present(q, n));

	for (i = 0; i < q->size; i++) {
		if (q->tasks[i] == n)
			found = 0;

		if (found == 0 && i < q->size - 1) {
			q->tasks[i] = q->tasks[i + 1];
		}
	}

	q->tasks[q->size - 1] = -1;
	q->n--;

	return found;
}

YASS_EXPORT int yass_list_present(struct yass_list *q, int n)
{
	int i;

	for (i = 0; i < q->size; i++) {
		if (q->tasks[i] == n)
			return 1;
	}

	return 0;
}

YASS_EXPORT int yass_list_size(struct yass_list *q)
{
	return q->size;
}

YASS_EXPORT int yass_list_n(struct yass_list *q)
{
	return q->n;
}

YASS_EXPORT int yass_list_get(struct yass_list *q, int index)
{
	return q->tasks[index];
}

YASS_EXPORT void yass_list_set(struct yass_list *q, int index, int value)
{
	q->tasks[index] = value;
}

YASS_EXPORT struct yass_list *yass_list_new(int size)
{
	int i;

	struct yass_list *q =
	    (struct yass_list *)malloc((2 + size) * sizeof(int));

	if (q == NULL)
		return NULL;

	q->tasks = (int *)malloc(size * sizeof(int));

	if (q->tasks == NULL)
		return NULL;

	for (i = 0; i < size; i++)
		q->tasks[i] = -1;

	q->n = 0;
	q->size = size;

	return q;
}

YASS_EXPORT void yass_list_free(struct yass_list *q)
{
	free(q->tasks);
	free(q);
}

YASS_EXPORT int yass_list_index(struct yass_list *q, int n)
{
	int i;

	for (i = 0; i < q->size; i++) {
		if (q->tasks[i] == n)
			return i;
	}

	return -1;
}

YASS_EXPORT void yass_list_print(struct yass_list *q)
{
	int i;

	for (i = 0; i < q->n; i++)
		printf(" %d", q->tasks[i]);

	printf("\n");
}
