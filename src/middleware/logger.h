#ifndef REST_LOGGER_H
#define REST_LOGGER_H

#include "cexpress.h"

/* Logs incoming request method and path to stdout */
void mw_logger(const Request *req, Response *res, MiddlewareChain *chain);

/* Global JSON error handler for CExpress app_use_error */
void error_handler_json(int status, const char *message, const Request *req, Response *res);

#endif /* REST_LOGGER_H */
