#include "cors.h"

void mw_cors(const Request *req, Response *res, MiddlewareChain *chain) {
    (void)req;
    res_set_header(res, "Access-Control-Allow-Origin", "*");
    res_set_header(res, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res_set_header(res, "Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
    chain_next(chain);
}
