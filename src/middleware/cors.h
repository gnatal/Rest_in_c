#ifndef REST_CORS_H
#define REST_CORS_H

#include "cexpress.h"

/* Middleware to attach permissive CORS headers to every response */
void mw_cors(const Request *req, Response *res, MiddlewareChain *chain);

#endif /* REST_CORS_H */
