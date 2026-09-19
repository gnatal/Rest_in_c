#include "logger.h"
#include "common/json_util.h"
#include <stdio.h>
#include <time.h>

void mw_logger(const Request *req, Response *res, MiddlewareChain *chain) {
    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_buf);

    printf("[%s] %s %s\n", time_str, req->method, req->path);
    fflush(stdout);

    res_set_header(res, "X-Powered-By", "CExpress/1.0");
    chain_next(chain);
}

void error_handler_json(int status, const char *message, const Request *req, Response *res) {
    (void)req;
    send_error(res, status, message);
}
