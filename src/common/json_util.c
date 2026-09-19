#include "json_util.h"
#include <strings.h>

void send_json(Response *res, int status, JsonWriter *w) {
    if (!jw_ok(w)) {
        res_status(res, 500);
        res_json(res, "{\"error\":\"json serialization failed\"}");
    } else {
        res_status(res, status);
        res_json(res, jw_data(w));
    }
    jw_free(w);
}

void send_error(Response *res, int status, const char *message) {
    JsonWriter w;
    jw_init(&w);
    jw_object_begin(&w);
    jw_key(&w, "error");
    jw_string(&w, message ? message : "unknown error");
    jw_object_end(&w);
    send_json(res, status, &w);
}

int require_json_content_type(const Request *req, Response *res) {
    const char *ct = req_get_header(req, "Content-Type");
    if (ct == NULL || strncasecmp(ct, "application/json", 16) != 0) {
        send_error(res, 415, "Content-Type must be application/json");
        return 0;
    }
    return 1;
}
