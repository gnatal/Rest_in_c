#include "auth.h"
#include "models/user_store.h"
#include "models/token_store.h"
#include "common/json_util.h"
#include <string.h>
#include <strings.h>

int auth_get_current_user(const Request *req, User *out_user) {
    const char *token = NULL;

    /* 1. Check Authorization: Bearer <token> */
    const char *auth_hdr = req_get_header(req, "Authorization");
    if (auth_hdr != NULL) {
        if (strncasecmp(auth_hdr, "Bearer ", 7) == 0) {
            token = auth_hdr + 7;
            while (*token == ' ') token++;
        }
    }

    /* 2. Fallback to session cookie */
    if (token == NULL || token[0] == '\0') {
        token = req_get_cookie(req, "session");
    }

    if (token == NULL || token[0] == '\0') {
        return 0;
    }

    int user_id = 0;
    if (!session_validate(token, &user_id)) {
        return 0;
    }

    return user_store_find_by_id(user_id, out_user);
}

void mw_authenticate(const Request *req, Response *res, MiddlewareChain *chain) {
    User user;
    if (!auth_get_current_user(req, &user)) {
        send_error(res, 401, "Unauthorized: missing or invalid authentication token");
        return;
    }
    chain_next(chain);
}

void mw_require_admin(const Request *req, Response *res, MiddlewareChain *chain) {
    User user;
    if (!auth_get_current_user(req, &user)) {
        send_error(res, 401, "Unauthorized: missing or invalid authentication token");
        return;
    }

    if (user.role != ROLE_ADMIN) {
        send_error(res, 403, "Forbidden: administrator role required");
        return;
    }

    chain_next(chain);
}
