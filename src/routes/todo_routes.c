#include "todo_routes.h"
#include "middleware/auth.h"
#include "common/json_util.h"
#include "models/types.h"
#include "models/todo_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void serialize_todo(JsonWriter *w, const Todo *t) {
    jw_object_begin(w);
    jw_key(w, "id");
    jw_int(w, t->id);
    jw_key(w, "user_id");
    jw_int(w, t->user_id);
    jw_key(w, "title");
    jw_string(w, t->title);
    jw_key(w, "description");
    jw_string(w, t->description);
    jw_key(w, "completed");
    jw_bool(w, t->completed);
    jw_key(w, "created_at");
    jw_string(w, t->created_at);
    jw_object_end(w);
}

/* GET /api/todos - List todos (scoped to caller unless admin queries all) */
static void handle_list_todos(const Request *req, Response *res) {
    User user;
    if (!auth_get_current_user(req, &user)) {
        send_error(res, 401, "unauthorized");
        return;
    }

    int user_id_filter = user.id;
    if (user.role == ROLE_ADMIN) {
        const char *uid_param = req_get_query(req, "user_id");
        if (uid_param != NULL) {
            if (strcmp(uid_param, "all") == 0) {
                user_id_filter = 0;
            } else {
                user_id_filter = atoi(uid_param);
            }
        } else {
            user_id_filter = 0; /* Admin sees all by default */
        }
    }

    int completed_filter = -1;
    const char *completed_param = req_get_query(req, "completed");
    if (completed_param != NULL) {
        if (strcmp(completed_param, "true") == 0 || strcmp(completed_param, "1") == 0) {
            completed_filter = 1;
        } else if (strcmp(completed_param, "false") == 0 || strcmp(completed_param, "0") == 0) {
            completed_filter = 0;
        }
    }

    Todo items[MAX_TODOS];
    size_t count = 0;
    todo_store_list(user_id_filter, completed_filter, items, MAX_TODOS, &count);

    JsonWriter w;
    jw_init(&w);
    jw_array_begin(&w);
    for (size_t i = 0; i < count; i++) {
        serialize_todo(&w, &items[i]);
    }
    jw_array_end(&w);

    send_json(res, 200, &w);
}

/* POST /api/todos - Create new todo attributed to caller */
static void handle_create_todo(const Request *req, Response *res) {
    User user;
    if (!auth_get_current_user(req, &user)) {
        send_error(res, 401, "unauthorized");
        return;
    }

    if (!require_json_content_type(req, res)) return;

    char err[128];
    JsonValue *root = json_parse(req->body, err, sizeof(err));
    if (!root) {
        send_error(res, 400, err);
        return;
    }

    const char *title_str = json_as_string(json_object_get(root, "title"), "");
    const char *desc_str  = json_as_string(json_object_get(root, "description"), "");

    char title[128];
    char desc[512];
    strncpy(title, title_str, sizeof(title) - 1); title[sizeof(title) - 1] = '\0';
    strncpy(desc, desc_str, sizeof(desc) - 1);   desc[sizeof(desc) - 1] = '\0';

    json_free(root);

    if (strlen(title) == 0) {
        send_error(res, 400, "title is required");
        return;
    }

    Todo todo;
    if (!todo_store_create(user.id, title, desc, &todo)) {
        send_error(res, 500, "failed to create todo item");
        return;
    }

    char location[64];
    snprintf(location, sizeof(location), "/api/todos/%d", todo.id);
    res_set_header(res, "Location", location);

    JsonWriter w;
    jw_init(&w);
    serialize_todo(&w, &todo);

    send_json(res, 201, &w);
}

/* GET /api/todos/:id - Get specific todo */
static void handle_get_todo(const Request *req, Response *res) {
    User user;
    if (!auth_get_current_user(req, &user)) {
        send_error(res, 401, "unauthorized");
        return;
    }

    const char *id_str = req_get_param(req, "id");
    int id = atoi(id_str ? id_str : "0");
    if (id <= 0) {
        send_error(res, 400, "invalid todo id");
        return;
    }

    Todo todo;
    if (!todo_store_get(id, &todo)) {
        send_error(res, 404, "todo not found");
        return;
    }

    if (user.role != ROLE_ADMIN && todo.user_id != user.id) {
        send_error(res, 403, "forbidden: you do not have permission to access this todo");
        return;
    }

    JsonWriter w;
    jw_init(&w);
    serialize_todo(&w, &todo);

    send_json(res, 200, &w);
}

/* PUT /api/todos/:id - Update specific todo */
static void handle_update_todo(const Request *req, Response *res) {
    User user;
    if (!auth_get_current_user(req, &user)) {
        send_error(res, 401, "unauthorized");
        return;
    }

    const char *id_str = req_get_param(req, "id");
    int id = atoi(id_str ? id_str : "0");
    if (id <= 0) {
        send_error(res, 400, "invalid todo id");
        return;
    }

    Todo existing;
    if (!todo_store_get(id, &existing)) {
        send_error(res, 404, "todo not found");
        return;
    }

    if (user.role != ROLE_ADMIN && existing.user_id != user.id) {
        send_error(res, 403, "forbidden: you do not have permission to update this todo");
        return;
    }

    if (!require_json_content_type(req, res)) return;

    char err[128];
    JsonValue *root = json_parse(req->body, err, sizeof(err));
    if (!root) {
        send_error(res, 400, err);
        return;
    }

    const JsonValue *title_val = json_object_get(root, "title");
    const JsonValue *desc_val  = json_object_get(root, "description");
    const JsonValue *comp_val  = json_object_get(root, "completed");

    char title_buf[128];
    char desc_buf[512];
    char *p_title = NULL;
    char *p_desc = NULL;
    int completed_flag = -1;

    if (title_val != NULL) {
        const char *s = json_as_string(title_val, NULL);
        if (s != NULL) {
            strncpy(title_buf, s, sizeof(title_buf) - 1);
            title_buf[sizeof(title_buf) - 1] = '\0';
            p_title = title_buf;
        }
    }
    if (desc_val != NULL) {
        const char *s = json_as_string(desc_val, NULL);
        if (s != NULL) {
            strncpy(desc_buf, s, sizeof(desc_buf) - 1);
            desc_buf[sizeof(desc_buf) - 1] = '\0';
            p_desc = desc_buf;
        }
    }
    if (comp_val != NULL) {
        completed_flag = json_as_bool(comp_val, existing.completed);
    }

    json_free(root);

    Todo updated;
    if (!todo_store_update(id, p_title, p_desc, completed_flag, &updated)) {
        send_error(res, 500, "failed to update todo");
        return;
    }

    JsonWriter w;
    jw_init(&w);
    serialize_todo(&w, &updated);

    send_json(res, 200, &w);
}

/* DELETE /api/todos/:id - Delete specific todo */
static void handle_delete_todo(const Request *req, Response *res) {
    User user;
    if (!auth_get_current_user(req, &user)) {
        send_error(res, 401, "unauthorized");
        return;
    }

    const char *id_str = req_get_param(req, "id");
    int id = atoi(id_str ? id_str : "0");
    if (id <= 0) {
        send_error(res, 400, "invalid todo id");
        return;
    }

    Todo existing;
    if (!todo_store_get(id, &existing)) {
        send_error(res, 404, "todo not found");
        return;
    }

    if (user.role != ROLE_ADMIN && existing.user_id != user.id) {
        send_error(res, 403, "forbidden: you do not have permission to delete this todo");
        return;
    }

    if (!todo_store_delete(id)) {
        send_error(res, 500, "failed to delete todo");
        return;
    }

    JsonWriter w;
    jw_init(&w);
    jw_object_begin(&w);
    jw_key(&w, "message");
    jw_string(&w, "todo deleted successfully");
    jw_key(&w, "id");
    jw_int(&w, id);
    jw_object_end(&w);

    send_json(res, 200, &w);
}

void register_todo_routes(App *app) {
    app_get_mw(app, "/api/todos", handle_list_todos, (Middleware[]){ mw_authenticate }, 1);
    app_post_mw(app, "/api/todos", handle_create_todo, (Middleware[]){ mw_authenticate }, 1);
    app_get_mw(app, "/api/todos/:id", handle_get_todo, (Middleware[]){ mw_authenticate }, 1);
    app_put_mw(app, "/api/todos/:id", handle_update_todo, (Middleware[]){ mw_authenticate }, 1);
    app_delete_mw(app, "/api/todos/:id", handle_delete_todo, (Middleware[]){ mw_authenticate }, 1);
}
