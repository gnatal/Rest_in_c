#include "db.h"
#include "cexpress.h"
#include <libpq-fe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define INT4_OID 23
#define UPDATE_SQL_CAP (64 + MAX_QUERIES * 32)

static App *g_app;                                    /* set by db_setup, before any fork */
static PGconn *g_conn;                                /* per worker; closed by process exit */
static int g_fd = -1;                                 /* PQsocket(g_conn), watched with app_watch_fd */
static unsigned g_watching;                           /* WATCH_* currently asked for on g_fd */
static int g_broken;                                  /* the connection failed: every request gets 500 */
static int g_unsent;                                  /* queries queued in libpq since the last flush */
static int g_flush_each;                              /* CEXPRESS_PG_FLUSH_EACH=1: flush per request (A/B baseline) */
static unsigned int rng_state;                        /* per worker, seeded in db_worker_init */
static unsigned char update_prepared[MAX_QUERIES + 1]; /* update_prepared[n]: "upd<n>" prepared or in flight */

/* Jobs in send order: g_head's results are the next ones the pipeline returns. */
static DbJob *g_head;
static DbJob *g_tail;
static DbJob *g_free;                                 /* released jobs, reused (malloc'd once, never freed) */

/* ---- jobs ---- */

static DbJob *job_take(void) {
    DbJob *job = g_free;
    if (job != NULL) {
        g_free = job->next;
    } else {
        job = malloc(sizeof(DbJob)); /* kept on the free list for the life of the worker */
    }
    return job;
}

static void job_release(DbJob *job) {
    if (job->pg_result != NULL) {
        PQclear((PGresult *)job->pg_result);
        job->pg_result = NULL;
    }
    job->next = g_free;
    g_free = job;
}

static void enqueue(DbJob *job) {
    job->next = NULL;
    if (g_tail != NULL) {
        g_tail->next = job;
    } else {
        g_head = job;
    }
    g_tail = job;
}

static DbJob *dequeue(void) {
    DbJob *job = g_head;
    g_head = job->next;
    if (g_head == NULL) {
        g_tail = NULL;
    }
    return job;
}

static void answer_500(Response *res) {
    res_status(res, 500);
    res_send(res, "Internal Server Error");
}

/* The job's work is over: answer its request (if the client is still there) and recycle it. */
static void finish(DbJob *job, const int ok) {
    Response *res = res_resume(g_app, job->handle);
    if (res != NULL) {
        if (ok) {
            job->done(res, job);
        } else {
            answer_500(res);
        }
    }
    job_release(job);
}

/* ---- the connection ---- */

static void on_pg_ready(App *app, int fd, unsigned events, void *udata);

static void watch(const unsigned want) {
    if (want != g_watching && app_watch_fd(g_app, g_fd, want, on_pg_ready, NULL) == 0) {
        g_watching = want;
    }
}

/* The connection is unusable: every queued request gets 500 (written after this callback), later ones too. */
static void connection_lost(void) {
    fprintf(stderr, "cexpress db: connection lost: %s", PQerrorMessage(g_conn));
    g_broken = 1;
    if (g_watching != 0) {
        app_unwatch_fd(g_app, g_fd);
        g_watching = 0;
    }
    while (g_head != NULL) {
        finish(dequeue(), 0);
    }
}

/* Sends what libpq has buffered; asks for write readiness only while some of it is still unsent. */
static void flush_output(void) {
    const int rc = PQflush(g_conn);
    if (rc < 0) {
        connection_lost();
        return;
    }
    watch(rc == 1 ? (WATCH_READ | WATCH_WRITE) : WATCH_READ);
}

/* Ends a request's queries (or a phase of them). With libpq 17+ the sync is only buffered and the turn-end
 * hook sends everything this event-loop turn queued in one flush; libpq 16's PQpipelineSync flushes itself. */
static int pipeline_sync(void) {
    g_unsent = 1;
#ifdef LIBPQ_HAS_SEND_PIPELINE_SYNC
    if (!g_flush_each) return PQsendPipelineSync(g_conn);
#endif
    return PQpipelineSync(g_conn);
}

/* app_on_turn_end hook: every handler and result callback of this turn has queued its queries; send them. */
static void on_turn_end(App *app, void *udata) {
    (void)app;
    (void)udata;
    if (g_unsent && !g_broken) {
        g_unsent = 0;
        flush_output();
    }
}

static int cmp_world_id(const void *a, const void *b) {
    const int x = ((const World *)a)->id, y = ((const World *)b)->id;
    return (x > y) - (x < y);
}

/* Queues the n SELECTs of `job` and its sync. 0, or -1 (the connection refused: it is broken). */
static int send_selects(const DbJob *job, const int *ids) {
    for (int i = 0; i < job->n; i++) {
        char id_text[12];
        snprintf(id_text, sizeof(id_text), "%d", ids[i]);
        const char *params[1] = {id_text};
        if (!PQsendQueryPrepared(g_conn, "world", 1, params, NULL, NULL, 0)) return -1;
    }
    return pipeline_sync() ? 0 : -1;
}

/* Queues one UPDATE ... FROM (VALUES ...) of job->worlds (prepared lazily per row count) and its sync. */
static int send_update(DbJob *job) {
    /* Sorted ids make concurrent batches lock rows in the same order (no deadlocks). */
    World sorted[MAX_QUERIES];
    memcpy(sorted, job->worlds, (size_t)job->n * sizeof(World));
    qsort(sorted, (size_t)job->n, sizeof(World), cmp_world_id);

    char name[16];
    snprintf(name, sizeof(name), "upd%d", job->n);
    job->prepare_pending = 0;
    if (!update_prepared[job->n]) {
        static char sql[UPDATE_SQL_CAP];
        size_t off = (size_t)snprintf(sql, sizeof(sql), "UPDATE world SET randomnumber = v.r FROM (VALUES ");
        for (int i = 0; i < job->n; i++)
            off += (size_t)snprintf(sql + off, sizeof(sql) - off, "%s($%d::int,$%d::int)", i ? "," : "",
                                    2 * i + 1, 2 * i + 2);
        snprintf(sql + off, sizeof(sql) - off, ") AS v(id, r) WHERE world.id = v.id");
        if (!PQsendPrepare(g_conn, name, sql, 0, NULL)) return -1;
        update_prepared[job->n] = 1; /* in flight: later jobs are behind it in the pipeline */
        job->prepare_pending = 1;
    }

    static char text[2 * MAX_QUERIES][12];
    const char *params[2 * MAX_QUERIES];
    for (int i = 0; i < job->n; i++) {
        snprintf(text[2 * i], sizeof(text[0]), "%d", sorted[i].id);
        snprintf(text[2 * i + 1], sizeof(text[0]), "%d", sorted[i].random_number);
        params[2 * i] = text[2 * i];
        params[2 * i + 1] = text[2 * i + 1];
    }
    if (!PQsendQueryPrepared(g_conn, name, 2 * job->n, params, NULL, NULL, 0)) return -1;
    return pipeline_sync() ? 0 : -1;
}

/* The head job's sync arrived: its current phase is complete. */
static void phase_done(DbJob *job) {
    if (job->kind == DB_JOB_UPDATE_READ && !job->failed && job->got == job->n) {
        for (int i = 0; i < job->n; i++) {
            int r;
            do r = db_random_id(); while (r == job->worlds[i].random_number);
            job->worlds[i].random_number = r;
        }
        job->kind = DB_JOB_UPDATE_WRITE;
        job->got = 0;
        if (send_update(job) != 0) {
            enqueue(job);
            connection_lost();
            return;
        }
        enqueue(job); /* its results come after everything already in the pipeline */
        return;
    }
    const int complete = job->kind == DB_JOB_FORTUNES ? job->pg_result != NULL
                         : job->kind == DB_JOB_UPDATE_WRITE ? job->got == 1
                                                            : job->got == job->n;
    finish(job, !job->failed && complete);
}

/* One result for the head job (a query's result or its sync). Takes ownership of r. */
static void on_result(PGresult *r) {
    DbJob *job = g_head;
    const ExecStatusType status = PQresultStatus(r);
    if (status == PGRES_PIPELINE_SYNC) {
        PQclear(r);
        phase_done(dequeue());
        return;
    }
    switch (job->kind) {
    case DB_JOB_WORLDS:
    case DB_JOB_UPDATE_READ:
        if (status == PGRES_TUPLES_OK && PQntuples(r) == 1 && job->got < job->n) {
            job->worlds[job->got].id = atoi(PQgetvalue(r, 0, 0));
            job->worlds[job->got].random_number = atoi(PQgetvalue(r, 0, 1));
            job->got++;
        } else {
            job->failed = 1;
        }
        break;
    case DB_JOB_UPDATE_WRITE:
        if (job->prepare_pending) {
            job->prepare_pending = 0;
            if (status != PGRES_COMMAND_OK) {
                job->failed = 1;
                update_prepared[job->n] = 0; /* the next job of this size prepares it again */
            }
        } else if (status == PGRES_COMMAND_OK) {
            job->got++;
        } else {
            job->failed = 1;
        }
        break;
    case DB_JOB_FORTUNES:
        if (status == PGRES_TUPLES_OK && job->pg_result == NULL) {
            int rows = PQntuples(r);
            if (rows > MAX_FORTUNES) rows = MAX_FORTUNES;
            for (int i = 0; i < rows; i++) {
                job->fortunes[i].id = atoi(PQgetvalue(r, i, 0));
                job->fortunes[i].message = PQgetvalue(r, i, 1);
                job->fortunes[i].message_len = (size_t)PQgetlength(r, i, 1);
            }
            job->fortune_count = rows;
            job->pg_result = r; /* the messages point into it; cleared by job_release */
            return;
        }
        job->failed = 1;
        break;
    }
    PQclear(r);
}

/* Reads every result already received, without blocking. Each query yields its result then a NULL; each
 * sync yields PGRES_PIPELINE_SYNC. Two NULLs in a row mean nothing more is buffered. */
static void drain_results(void) {
    int nulls = 0;
    while (g_head != NULL && !g_broken && !PQisBusy(g_conn)) {
        PGresult *r = PQgetResult(g_conn);
        if (r == NULL) {
            if (++nulls == 2) break;
            continue;
        }
        nulls = 0;
        on_result(r);
    }
}

static void on_pg_ready(App *app, int fd, unsigned events, void *udata) {
    (void)app;
    (void)fd;
    (void)udata;
    if (g_broken) return;
    if (events & (WATCH_READ | WATCH_ERROR)) {
        if (!PQconsumeInput(g_conn)) {
            connection_lost();
            return;
        }
        drain_results();
    }
    if (!g_broken && ((events & WATCH_WRITE) || g_flush_each)) {
        flush_output(); /* the rest of a partial send; an /updates phase 2 waits for the turn-end flush */
    }
}

/* ---- setup ---- */

static const char *conninfo(void) {
    const char *env = getenv("DB_CONNINFO");
    return env ? env
               : "host=tfb-database dbname=hello_world user=benchmarkdbuser password=benchmarkdbpass "
                 "sslmode=disable";
}

static void prepare_or_die(const char *name, const char *sql, int nparams, const Oid *types) {
    PGresult *r = PQprepare(g_conn, name, sql, nparams, types);
    if (PQresultStatus(r) != PGRES_COMMAND_OK) {
        fprintf(stderr, "prepare %s failed: %s\n", name, PQerrorMessage(g_conn));
        exit(1);
    }
    PQclear(r);
}

/* app_on_worker_start hook: runs in each worker after fork, before its event loop starts (app_watch_fd
 * stores the watch until then). */
static void db_worker_init(void) {
    const char *flush_each = getenv("CEXPRESS_PG_FLUSH_EACH");
    g_flush_each = flush_each != NULL && strcmp(flush_each, "1") == 0;
    rng_state = (unsigned int)getpid() * 2654435761u ^ (unsigned int)time(NULL);
    if (rng_state == 0) rng_state = 1;

    g_conn = PQconnectdb(conninfo()); /* blocking: once per worker, before any request */
    if (PQstatus(g_conn) != CONNECTION_OK) {
        fprintf(stderr, "Postgres connection failed: %s\n", PQerrorMessage(g_conn));
        exit(1);
    }
    const Oid int4[1] = {INT4_OID};
    prepare_or_die("world", "SELECT id, randomnumber FROM world WHERE id = $1", 1, int4);
    prepare_or_die("fortune", "SELECT id, message FROM fortune", 0, NULL);
    if (PQsetnonblocking(g_conn, 1) != 0 || PQenterPipelineMode(g_conn) != 1) {
        fprintf(stderr, "libpq non-blocking pipeline mode unavailable: %s\n", PQerrorMessage(g_conn));
        exit(1);
    }
    g_fd = PQsocket(g_conn);
    if (app_watch_fd(g_app, g_fd, WATCH_READ, on_pg_ready, NULL) != 0) {
        fprintf(stderr, "cannot watch the Postgres socket\n");
        exit(1);
    }
    g_watching = WATCH_READ;
}

void db_setup(App *app) {
    g_app = app;
    app_on_worker_start(app, db_worker_init);
    app_on_turn_end(app, on_turn_end, NULL);
}

int db_random_id(void) {
    /* xorshift32: fast, and good enough for picking rows */
    unsigned int x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return (int)(x % WORLD_ROWS) + 1;
}

/* ---- submitting ---- */

/* Defers res and prepares a job, or answers res itself (500 with no connection, 503 over budget) and
 * returns NULL. */
static DbJob *job_begin(Response *res, const DbJobKind kind, const int n, const int as_array, DbDone done) {
    if (g_conn == NULL || g_broken) {
        answer_500(res);
        return NULL;
    }
    DbJob *job = job_take();
    if (job == NULL) {
        answer_500(res);
        return NULL;
    }
    const DeferHandle h = res_defer(res);
    if (h == 0) {
        job_release(job); /* the engine already built a 503 */
        return NULL;
    }
    job->handle = h;
    job->done = done;
    job->kind = kind;
    job->n = n;
    job->got = 0;
    job->failed = 0;
    job->prepare_pending = 0;
    job->as_array = as_array;
    job->fortune_count = 0;
    job->pg_result = NULL;
    return job;
}

/* The job's queries are queued in libpq (sent == 0): it joins the FIFO; the turn-end hook flushes. A send
 * that failed means the connection is broken: every queued request, this one included, gets 500. */
static void job_commit(DbJob *job, const int sent) {
    enqueue(job);
    if (sent != 0) {
        connection_lost();
        return;
    }
    if (g_flush_each) flush_output();
}

void db_submit_worlds(Response *res, const int *ids, const int n, const int as_array, DbDone done) {
    DbJob *job = job_begin(res, DB_JOB_WORLDS, n, as_array, done);
    if (job != NULL) {
        job_commit(job, send_selects(job, ids));
    }
}

void db_submit_updates(Response *res, const int *ids, const int n, DbDone done) {
    DbJob *job = job_begin(res, DB_JOB_UPDATE_READ, n, 1, done);
    if (job != NULL) {
        job_commit(job, send_selects(job, ids));
    }
}

void db_submit_fortunes(Response *res, DbDone done) {
    DbJob *job = job_begin(res, DB_JOB_FORTUNES, 0, 0, done);
    if (job != NULL) {
        const int sent = PQsendQueryPrepared(g_conn, "fortune", 0, NULL, NULL, NULL, 0) && pipeline_sync();
        job_commit(job, sent ? 0 : -1);
    }
}
