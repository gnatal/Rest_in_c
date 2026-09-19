#ifndef REST_JSON_UTIL_H
#define REST_JSON_UTIL_H

#include "cexpress.h"

/*
 * Sends response with status code and serialized JsonWriter data.
 * Guarantees that jw_free(w) is called in all cases.
 */
void send_json(Response *res, int status, JsonWriter *w);

/*
 * Sends a standardized {"error": "<message>"} JSON response.
 */
void send_error(Response *res, int status, const char *message);

/*
 * Validates that the request has Content-Type: application/json.
 * If not, sends a 415 Unsupported Media Type error and returns 0.
 * Returns 1 if valid.
 */
int require_json_content_type(const Request *req, Response *res);

#endif /* REST_JSON_UTIL_H */
