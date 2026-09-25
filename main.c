#include "cexpress.h"
#include "db.h"
#include "controllers_user.h"
#include "controllers_article.h"
#include "controllers_profile.h"
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
    router_put_mw(&api_router, "/user", handler_update_user, mw_auth_arr, 1);

    // Article endpoints
    router_get(&api_router, "/articles", handler_get_articles);
    router_get(&api_router, "/articles/feed", handler_get_feed);
    router_get(&api_router, "/articles/:slug", handler_get_article);
    router_post_mw(&api_router, "/articles", handler_create_article, mw_auth_arr, 1);
    router_put_mw(&api_router, "/articles/:slug", handler_update_article, mw_auth_arr, 1);
    router_delete_mw(&api_router, "/articles/:slug", handler_delete_article, mw_auth_arr, 1);

    // Profile and Follow endpoints
    router_get(&api_router, "/profiles/:username", handler_get_profile);
    router_post_mw(&api_router, "/profiles/:username/follow", handler_follow_user, mw_auth_arr, 1);
    router_delete_mw(&api_router, "/profiles/:username/follow", handler_unfollow_user, mw_auth_arr, 1);

    // Favorite endpoints
    router_post_mw(&api_router, "/articles/:slug/favorite", handler_favorite_article, mw_auth_arr, 1);
    router_delete_mw(&api_router, "/articles/:slug/favorite", handler_unfavorite_article, mw_auth_arr, 1);

    // Comment endpoints
    router_get(&api_router, "/articles/:slug/comments", handler_get_comments);
    router_post_mw(&api_router, "/articles/:slug/comments", handler_add_comment, mw_auth_arr, 1);
    router_delete_mw(&api_router, "/articles/:slug/comments/:id", handler_delete_comment, mw_auth_arr, 1);

    // Tags
    router_get(&api_router, "/tags", handler_get_tags);

    // Mount the /api router
    app_mount(&app, "/api", &api_router);

    app.config.port = 8080;
    
    printf("Starting RealWorld API on port %d with 4 workers...\n", app.config.port);
    app_listen_cluster(&app, app.config.port, 4);
    
    db_close();
    app_destroy(&app);
    return 0;
}
