#ifndef REST_TODO_STORE_H
#define REST_TODO_STORE_H

#include "types.h"

/* Initialize todo store */
void todo_store_init(void);

/* Destroy todo store */
void todo_store_destroy(void);

/*
 * Lists todos matching filters.
 * user_id_filter: > 0 for specific user, 0 for all users (admin view).
 * completed_filter: -1 for all, 0 for incomplete, 1 for completed.
 */
int todo_store_list(int user_id_filter, int completed_filter, Todo *out_items, size_t max_items, size_t *out_count);

/* Find todo by ID. Returns 1 if found, 0 if not found. */
int todo_store_get(int id, Todo *out_item);

/*
 * Create new todo assigned to user_id.
 * Returns 1 on success, 0 on failure.
 */
int todo_store_create(int user_id, const char *title, const char *description, Todo *out_item);

/*
 * Updates todo by ID.
 * Returns 1 on success, 0 if not found.
 */
int todo_store_update(int id, const char *title, const char *description, int completed, Todo *out_item);

/* Deletes todo by ID. Returns 1 on success, 0 if not found. */
int todo_store_delete(int id);

/* Total count of todos in store */
size_t todo_store_count(void);

#endif /* REST_TODO_STORE_H */
