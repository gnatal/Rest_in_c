#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "cexpress.h"
#include "common/json_util.h"
#include "middleware/cors.h"
#include "middleware/logger.h"
#include "models/user_store.h"
#include "models/token_store.h"
#include "models/todo_store.h"
#include "routes/auth_routes.h"
#include "routes/user_routes.h"
#include "routes/todo_routes.h"

static time_t server_start_time = 0;

/* GET /health - Service health and status */
static void handle_health(const Request *req, Response *res) {
    (void)req;
    time_t now = time(NULL);
    long uptime = (long)(now - server_start_time);

    JsonWriter w;
    jw_init(&w);
    jw_object_begin(&w);
    jw_key(&w, "status");
    jw_string(&w, "ok");
    jw_key(&w, "app");
    jw_string(&w, "Rest_in_c");
    jw_key(&w, "uptime_seconds");
    jw_int(&w, uptime);
    jw_key(&w, "users_count");
    jw_int(&w, (long long)user_store_count());
    jw_key(&w, "todos_count");
    jw_int(&w, (long long)todo_store_count());
    jw_object_end(&w);

    send_json(res, 200, &w);
}

int main(void) {
    server_start_time = time(NULL);

    int port = 8080;
    const char *port_env = getenv("PORT");
    if (port_env != NULL && port_env[0] != '\0') {
        int parsed_port = atoi(port_env);
        if (parsed_port > 0 && parsed_port <= 65535) {
            port = parsed_port;
        }
    }

    /* Initialize in-memory repositories */
    user_store_init();
    token_store_init();
    todo_store_init();

    App app;
    app_init(&app);

    /* Register global middlewares */
    app_use(&app, mw_logger);
    app_use(&app, mw_cors);
    app_use_error(&app, error_handler_json);

    /* Core healthcheck route */
    app_get(&app, "/health", handle_health);

    /* Register REST route modules */
    register_auth_routes(&app);
    register_user_routes(&app);
    register_todo_routes(&app);

    printf("\n=======================================================\n");
    printf("   Rest_in_c API Server (powered by CExpress)\n");
    printf("   Listening on http://localhost:%d\n", port);
    printf("   Healthcheck: http://localhost:%d/health\n", port);
    printf("   Default Admin: admin@example.com / Admin123!\n");
    printf("=======================================================\n\n");
    fflush(stdout);

    /* Start event loop (blocks until SIGINT / SIGTERM) */
    app_listen(&app, port);

    /* Graceful cleanup */
    app_destroy(&app);
    user_store_destroy();
    token_store_destroy();
    todo_store_destroy();

    printf("Server stopped cleanly.\n");
    return 0;
}
