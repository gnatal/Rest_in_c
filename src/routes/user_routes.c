#include "user_routes.h"
#include "middleware/auth.h"
#include "common/json_util.h"
#include "models/types.h"

static void handle_get_current_user(const Request *req, Response *res) {
    User user;
    if (!auth_get_current_user(req, &user)) {
        send_error(res, 401, "unauthorized");
        return;
    }

    JsonWriter w;
    jw_init(&w);
    jw_object_begin(&w);
    jw_key(&w, "user");
    jw_object_begin(&w);
    jw_key(&w, "id");
    jw_int(&w, user.id);
    jw_key(&w, "email");
    jw_string(&w, user.email);
    jw_key(&w, "name");
    jw_string(&w, user.name);
    jw_key(&w, "role");
    jw_string(&w, role_to_string(user.role));
    jw_key(&w, "created_at");
    jw_string(&w, user.created_at);
    jw_object_end(&w);
    jw_object_end(&w);

    send_json(res, 200, &w);
}

void register_user_routes(App *app) {
    /* Protected route with mw_authenticate */
    app_get_mw(app, "/api/user", handle_get_current_user, (Middleware[]){ mw_authenticate }, 1);
}
