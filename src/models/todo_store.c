#include "todo_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

static Todo s_todos[MAX_TODOS];
static size_t s_todo_count = 0;
static int s_next_todo_id = 1;
static pthread_mutex_t s_todo_mutex = PTHREAD_MUTEX_INITIALIZER;

static void get_iso_timestamp(char *out_buf, size_t size) {
    time_t now = time(NULL);
    struct tm tm_buf;
    gmtime_r(&now, &tm_buf);
    strftime(out_buf, size, "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
}

void todo_store_init(void) {
    pthread_mutex_lock(&s_todo_mutex);
    s_todo_count = 0;
    s_next_todo_id = 1;
    pthread_mutex_unlock(&s_todo_mutex);
}

void todo_store_destroy(void) {
    pthread_mutex_lock(&s_todo_mutex);
    s_todo_count = 0;
    pthread_mutex_unlock(&s_todo_mutex);
}

int todo_store_list(int user_id_filter, int completed_filter, Todo *out_items, size_t max_items, size_t *out_count) {
    pthread_mutex_lock(&s_todo_mutex);

    size_t count = 0;
    for (size_t i = 0; i < s_todo_count && count < max_items; i++) {
        if (user_id_filter > 0 && s_todos[i].user_id != user_id_filter) {
            continue;
        }
        if (completed_filter != -1 && s_todos[i].completed != completed_filter) {
            continue;
        }
        if (out_items) {
            out_items[count] = s_todos[i];
        }
        count++;
    }

    if (out_count) {
        *out_count = count;
    }

    pthread_mutex_unlock(&s_todo_mutex);
    return 1;
}

int todo_store_get(int id, Todo *out_item) {
    pthread_mutex_lock(&s_todo_mutex);
    for (size_t i = 0; i < s_todo_count; i++) {
        if (s_todos[i].id == id) {
            if (out_item) {
                *out_item = s_todos[i];
            }
            pthread_mutex_unlock(&s_todo_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&s_todo_mutex);
    return 0;
}

int todo_store_create(int user_id, const char *title, const char *description, Todo *out_item) {
    if (!title || strlen(title) == 0) return 0;

    pthread_mutex_lock(&s_todo_mutex);

    if (s_todo_count >= MAX_TODOS) {
        pthread_mutex_unlock(&s_todo_mutex);
        return 0;
    }

    Todo item;
    memset(&item, 0, sizeof(item));
    item.id = s_next_todo_id++;
    item.user_id = user_id;
    strncpy(item.title, title, sizeof(item.title) - 1);
    strncpy(item.description, description ? description : "", sizeof(item.description) - 1);
    item.completed = 0;
    get_iso_timestamp(item.created_at, sizeof(item.created_at));

    s_todos[s_todo_count++] = item;

    if (out_item) {
        *out_item = item;
    }

    pthread_mutex_unlock(&s_todo_mutex);
    return 1;
}

int todo_store_update(int id, const char *title, const char *description, int completed, Todo *out_item) {
    pthread_mutex_lock(&s_todo_mutex);

    for (size_t i = 0; i < s_todo_count; i++) {
        if (s_todos[i].id == id) {
            if (title != NULL && strlen(title) > 0) {
                strncpy(s_todos[i].title, title, sizeof(s_todos[i].title) - 1);
            }
            if (description != NULL) {
                strncpy(s_todos[i].description, description, sizeof(s_todos[i].description) - 1);
            }
            if (completed == 0 || completed == 1) {
                s_todos[i].completed = completed;
            }

            if (out_item) {
                *out_item = s_todos[i];
            }

            pthread_mutex_unlock(&s_todo_mutex);
            return 1;
        }
    }

    pthread_mutex_unlock(&s_todo_mutex);
    return 0;
}

int todo_store_delete(int id) {
    pthread_mutex_lock(&s_todo_mutex);

    for (size_t i = 0; i < s_todo_count; i++) {
        if (s_todos[i].id == id) {
            s_todos[i] = s_todos[--s_todo_count];
            pthread_mutex_unlock(&s_todo_mutex);
            return 1;
        }
    }

    pthread_mutex_unlock(&s_todo_mutex);
    return 0;
}

size_t todo_store_count(void) {
    pthread_mutex_lock(&s_todo_mutex);
    size_t count = s_todo_count;
    pthread_mutex_unlock(&s_todo_mutex);
    return count;
}
