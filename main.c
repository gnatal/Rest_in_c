#include "cexpress.h"
#include "db.h"
#include "controllers_user.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void handler_ping(const Request *req, Response *res) {
    (void)req;
    res_send(res, "pong");
}

int main(void) {
    App app;
    app_init(&app);

    if (db_open("realworld.db") != 0) {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;
    }
    app_on_worker_start(&app, db_worker_init);

    // RealWorld API base router
    Router api_router;
    router_init(&api_router);

    // Health check endpoint
    router_get(&api_router, "/ping", handler_ping);

    // User endpoints
    router_post(&api_router, "/users", handler_register_user);
    router_post(&api_router, "/users/login", handler_login_user);
    
    // Protected User endpoint
    Middleware mw_auth_arr[] = {mw_require_auth};
    router_get_mw(&api_router, "/user", handler_get_current_user, mw_auth_arr, 1);

    // Mount the /api router
    app_mount(&app, "/api", &api_router);

    app.config.port = 8080;
    
    printf("Starting RealWorld API on port %d...\n", app.config.port);
    app_listen(&app, app.config.port);
    
    db_close();
    app_destroy(&app);
    return 0;
}
