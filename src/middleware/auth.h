#ifndef REST_AUTH_MIDDLEWARE_H
#define REST_AUTH_MIDDLEWARE_H

#include "cexpress.h"
#include "models/types.h"

/*
 * Retrieves the currently authenticated user from the Request.
 * Looks for 'Authorization: Bearer <token>' header or 'session' cookie.
 * Returns 1 if valid and copies into out_user; returns 0 otherwise.
 */
int auth_get_current_user(const Request *req, User *out_user);

/*
 * Guard middleware: Rejects request with 401 Unauthorized if no valid
 * session token is present.
 */
void mw_authenticate(const Request *req, Response *res, MiddlewareChain *chain);

/*
 * Guard middleware: Rejects request with 403 Forbidden if the user
 * does not have ROLE_ADMIN.
 */
void mw_require_admin(const Request *req, Response *res, MiddlewareChain *chain);

#endif /* REST_AUTH_MIDDLEWARE_H */
