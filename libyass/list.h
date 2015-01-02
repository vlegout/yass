#ifndef _YASS_QUEUE_H
#define _YASS_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

struct yass_list;

void yass_list_add(struct yass_list *q, int n);

int yass_list_remove(struct yass_list *q, int n);

int yass_list_present(struct yass_list *q, int n);

int yass_list_size(struct yass_list *q);

int yass_list_n(struct yass_list *q);

int yass_list_get(struct yass_list *q, int index);

void yass_list_set(struct yass_list *q, int index, int value);

struct yass_list *yass_list_new(int size);

void yass_list_free(struct yass_list *q);

int yass_list_index(struct yass_list *q, int n);

void yass_list_print(struct yass_list *q);

#ifdef __cplusplus
}
#endif

#endif				/* _YASS_QUEUE_H */
