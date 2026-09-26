/* TechEmpower Framework Benchmarks implementation for CExpress.
 * Spec: https://github.com/TechEmpower/FrameworkBenchmarks/wiki/Project-Information-Framework-Tests-Overview */
#include "cexpress.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SERVER_NAME "cexpress"
#define HELLO "Hello, World!"
#define EXTRA_FORTUNE "Additional fortune added at request time."
#define HTML_TYPE "text/html; charset=utf-8"

static void send_error(Response *res) {
    res_status(res, 500);
    res_send(res, "Internal Server Error");
}

/* ---- /plaintext, /json ---- */

static void handler_plaintext(const Request *req, Response *res) {
    (void)req;
    res_set_header(res, "Server", SERVER_NAME);
    res_send(res, HELLO);
}

static void handler_json(const Request *req, Response *res) {
    (void)req;
    res_set_header(res, "Server", SERVER_NAME);
    /* The spec requires serializing a fresh object on every request. */
    const yyjson_alc alc = arena_yyjson_alc(res->conn->arena);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_str(doc, root, "message", HELLO);
    const char *json = yyjson_mut_write_opts(doc, 0, &alc, NULL, NULL);
    if (!json) {
        send_error(res);
        return;
    }
    res_json(res, json);
}

/* ---- /db, /queries, /updates ---- */

/* ?queries=: missing, non-numeric or < 1 -> 1; > 500 -> 500. */
static int query_count(const Request *req) {
    const char *q = req_get_query(req, "queries");
    if (!q) return 1;
    char *end;
    const long n = strtol(q, &end, 10);
    if (end == q || n < 1) return 1;
    return n > MAX_QUERIES ? MAX_QUERIES : (int)n;
}

/* n distinct random ids (distinct so an UPDATE batch touches n rows). */
static void random_ids(int *ids, int n) {
    for (int i = 0; i < n; i++) {
        int id, dup;
        do {
            id = db_random_id();
            dup = 0;
            for (int j = 0; j < i && !dup; j++) dup = ids[j] == id;
        } while (dup);
        ids[i] = id;
    }
}

static yyjson_mut_val *world_json(yyjson_mut_doc *doc, const World *w) {
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_int(doc, obj, "id", w->id);
    yyjson_mut_obj_add_int(doc, obj, "randomNumber", w->random_number);
    return obj;
}

static void send_worlds(Response *res, const World *worlds, int n, int as_array) {
    const yyjson_alc alc = arena_yyjson_alc(res->conn->arena);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    if (as_array) {
        yyjson_mut_val *arr = yyjson_mut_arr(doc);
        for (int i = 0; i < n; i++) yyjson_mut_arr_append(arr, world_json(doc, &worlds[i]));
        yyjson_mut_doc_set_root(doc, arr);
    } else {
        yyjson_mut_doc_set_root(doc, world_json(doc, &worlds[0]));
    }
    const char *json = yyjson_mut_write_opts(doc, 0, &alc, NULL, NULL);
    if (!json) {
        send_error(res);
        return;
    }
    res_json(res, json);
}

/* The database answers on the event loop (db.c): each handler submits its queries and returns; the
 * matching done_* callback builds the response once the results are in. */

static void done_worlds(Response *res, const DbJob *job) {
    send_worlds(res, job->worlds, job->n, job->as_array);
}

static void handler_db(const Request *req, Response *res) {
    (void)req;
    res_set_header(res, "Server", SERVER_NAME); /* kept on the deferred response */
    const int id = db_random_id();
    db_submit_worlds(res, &id, 1, 0, done_worlds);
}

static void handler_queries(const Request *req, Response *res) {
    res_set_header(res, "Server", SERVER_NAME);
    const int n = query_count(req);
    int ids[MAX_QUERIES];
    random_ids(ids, n);
    db_submit_worlds(res, ids, n, 1, done_worlds);
}

static void handler_updates(const Request *req, Response *res) {
    res_set_header(res, "Server", SERVER_NAME);
    const int n = query_count(req);
    int ids[MAX_QUERIES];
    random_ids(ids, n);
    db_submit_updates(res, ids, n, done_worlds);
}

/* ---- /fortunes ---- */

static int cmp_fortune(const void *a, const void *b) {
    const Fortune *x = a, *y = b;
    const size_t len = x->message_len < y->message_len ? x->message_len : y->message_len;
    const int c = memcmp(x->message, y->message, len);
    if (c) return c;
    return (x->message_len > y->message_len) - (x->message_len < y->message_len);
}

typedef struct {
    char *buf;
    size_t len;
} HtmlOut;

static void html_raw(HtmlOut *o, const char *s, size_t n) {
    memcpy(o->buf + o->len, s, n);
    o->len += n;
}

static void html_escaped(HtmlOut *o, const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        switch (s[i]) {
        case '&': html_raw(o, "&amp;", 5); break;
        case '<': html_raw(o, "&lt;", 4); break;
        case '>': html_raw(o, "&gt;", 4); break;
        case '"': html_raw(o, "&quot;", 6); break;
        case '\'': html_raw(o, "&#39;", 5); break;
        default: o->buf[o->len++] = s[i];
        }
    }
}

static void done_fortunes(Response *res, const DbJob *job) {
    Fortune fortunes[MAX_FORTUNES + 1];
    int n = job->fortune_count;
    memcpy(fortunes, job->fortunes, (size_t)n * sizeof(Fortune)); /* messages still point into the job's result */
    fortunes[n++] = (Fortune){0, EXTRA_FORTUNE, sizeof(EXTRA_FORTUNE) - 1};
    qsort(fortunes, (size_t)n, sizeof(Fortune), cmp_fortune);

    static const char head[] = "<!DOCTYPE html><html><head><title>Fortunes</title></head><body><table>"
                               "<tr><th>id</th><th>message</th></tr>";
    static const char tail[] = "</table></body></html>";
    size_t cap = sizeof(head) + sizeof(tail);
    for (int i = 0; i < n; i++) cap += 32 + 6 * fortunes[i].message_len; /* worst case: every byte escaped */

    HtmlOut o = {arena_alloc(res->conn->arena, cap), 0};
    if (!o.buf) {
        send_error(res);
        return;
    }
    html_raw(&o, head, sizeof(head) - 1);
    for (int i = 0; i < n; i++) {
        char id[16];
        html_raw(&o, "<tr><td>", 8);
        html_raw(&o, id, (size_t)snprintf(id, sizeof(id), "%d", fortunes[i].id));
        html_raw(&o, "</td><td>", 9);
        html_escaped(&o, fortunes[i].message, fortunes[i].message_len);
        html_raw(&o, "</td></tr>", 10);
    }
    html_raw(&o, tail, sizeof(tail) - 1);
    res_send_bytes(res, HTML_TYPE, (const unsigned char *)o.buf, o.len);
}

static void handler_fortunes(const Request *req, Response *res) {
    (void)req;
    res_set_header(res, "Server", SERVER_NAME);
    db_submit_fortunes(res, done_fortunes);
}

int main(void) {
    static App app;
    app_init(&app);

    app_get(&app, "/plaintext", handler_plaintext);
    app_get(&app, "/json", handler_json);

    /* The DB routes (and their per-worker connection) only exist in the postgres variant, so the
     * json/plaintext container runs without a database. */
    const char *db_env = getenv("CEXPRESS_DB");
    if (db_env && strcmp(db_env, "1") == 0) {
        db_setup(&app); /* per-worker connection, opened after fork */
        app_get(&app, "/db", handler_db);
        app_get(&app, "/queries", handler_queries);
        app_get(&app, "/updates", handler_updates);
        app_get(&app, "/fortunes", handler_fortunes);
    }

    const char *workers_env = getenv("CEXPRESS_WORKERS");
    app.config.workers = workers_env ? atoi(workers_env) : 0; /* 0 = one per core */
    app.config.max_connections = 0;                           /* TFB drives up to 16,384 connections */

    printf("cexpress TFB listening on 8080 (workers=%d, db=%s)\n", app.config.workers, db_env ? db_env : "0");
    fflush(stdout);
    app_listen(&app, 8080);
    app_destroy(&app);
    return 0;
}
