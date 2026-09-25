#ifndef CONTROLLERS_USER_H
#define CONTROLLERS_USER_H

#include "cexpress.h"

// Middleware
void mw_require_auth(const Request *req, Response *res, MiddlewareChain *chain);

// Handlers
void handler_register_user(const Request *req, Response *res);
void handler_login_user(const Request *req, Response *res);
void handler_get_current_user(const Request *req, Response *res);

// Helpers
int get_current_user_id(const Request *req);

#endif // CONTROLLERS_USER_H
