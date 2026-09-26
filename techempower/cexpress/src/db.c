#include "db.h"
#include <libpq-fe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define INT4_OID 23
#define UPDATE_SQL_CAP (64 + MAX_QUERIES * 32)

static PGconn *conn;                                  /* per worker; closed by process exit */
static unsigned int rng_state;                        /* per worker, seeded in db_worker_init */
static unsigned char update_prepared[MAX_QUERIES + 1]; /* update_prepared[n]: "upd<n>" exists */

static const char *conninfo(void) {
    const char *env = getenv("DB_CONNINFO");
    return env ? env
               : "host=tfb-database dbname=hello_world user=benchmarkdbuser password=benchmarkdbpass "
                 "sslmode=disable";
}

static void prepare_or_die(const char *name, const char *sql, int nparams, const Oid *types) {
    PGresult *r = PQprepare(conn, name, sql, nparams, types);
    if (PQresultStatus(r) != PGRES_COMMAND_OK) {
        fprintf(stderr, "prepare %s failed: %s\n", name, PQerrorMessage(conn));
        exit(1);
    }
    PQclear(r);
}

void db_worker_init(void) {
    rng_state = (unsigned int)getpid() * 2654435761u ^ (unsigned int)time(NULL);
    if (rng_state == 0) rng_state = 1;

    conn = PQconnectdb(conninfo());
    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "Postgres connection failed: %s\n", PQerrorMessage(conn));
        exit(1);
    }
    const Oid int4[1] = {INT4_OID};
    prepare_or_die("world", "SELECT id, randomnumber FROM world WHERE id = $1", 1, int4);
    prepare_or_die("fortune", "SELECT id, message FROM fortune", 0, NULL);
    if (PQenterPipelineMode(conn) != 1) {
        fprintf(stderr, "libpq pipeline mode unavailable: %s\n", PQerrorMessage(conn));
        exit(1);
    }
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

/* Every query in a pipeline yields one result then a NULL separator; the batch ends with
 * PGRES_PIPELINE_SYNC. Reads one query's result (NULL if the connection is gone). */
static PGresult *next_result(void) {
    PGresult *r = PQgetResult(conn);
    PGresult *sep = PQgetResult(conn);
    if (sep) PQclear(sep); /* not expected; keeps the stream aligned either way */
    return r;
}

static int read_sync(void) {
    PGresult *r = PQgetResult(conn);
    const int ok = r && PQresultStatus(r) == PGRES_PIPELINE_SYNC;
    if (r) PQclear(r);
    if (!ok) fprintf(stderr, "pipeline out of sync: %s\n", PQerrorMessage(conn));
    return ok ? 0 : -1;
}

int db_fetch_worlds(const int *ids, int n, World *out) {
    char id_text[MAX_QUERIES][12];
    for (int i = 0; i < n; i++) {
        snprintf(id_text[i], sizeof(id_text[i]), "%d", ids[i]);
        const char *params[1] = {id_text[i]};
        if (!PQsendQueryPrepared(conn, "world", 1, params, NULL, NULL, 0)) return -1;
    }
    if (!PQpipelineSync(conn)) return -1;

    int failed = 0;
    for (int i = 0; i < n; i++) {
        PGresult *r = next_result();
        if (r && PQresultStatus(r) == PGRES_TUPLES_OK && PQntuples(r) == 1) {
            out[i].id = atoi(PQgetvalue(r, 0, 0));
            out[i].random_number = atoi(PQgetvalue(r, 0, 1));
        } else {
            failed = 1;
        }
        if (r) PQclear(r);
    }
    return (read_sync() == 0 && !failed) ? 0 : -1;
}

static int cmp_world_id(const void *a, const void *b) {
    const int x = ((const World *)a)->id, y = ((const World *)b)->id;
    return (x > y) - (x < y);
}

int db_update_worlds(const World *worlds, int n) {
    /* Sorted ids make concurrent batches lock rows in the same order (no deadlocks). */
    World sorted[MAX_QUERIES];
    memcpy(sorted, worlds, (size_t)n * sizeof(World));
    qsort(sorted, (size_t)n, sizeof(World), cmp_world_id);

    char name[16];
    snprintf(name, sizeof(name), "upd%d", n);
    if (!update_prepared[n]) {
        static char sql[UPDATE_SQL_CAP];
        size_t off = (size_t)snprintf(sql, sizeof(sql), "UPDATE world SET randomnumber = v.r FROM (VALUES ");
        for (int i = 0; i < n; i++)
            off += (size_t)snprintf(sql + off, sizeof(sql) - off, "%s($%d::int,$%d::int)", i ? "," : "",
                                    2 * i + 1, 2 * i + 2);
        snprintf(sql + off, sizeof(sql) - off, ") AS v(id, r) WHERE world.id = v.id");
        if (!PQsendPrepare(conn, name, sql, 0, NULL)) return -1;
    }

    char text[2 * MAX_QUERIES][12];
    const char *params[2 * MAX_QUERIES];
    for (int i = 0; i < n; i++) {
        snprintf(text[2 * i], sizeof(text[0]), "%d", sorted[i].id);
        snprintf(text[2 * i + 1], sizeof(text[0]), "%d", sorted[i].random_number);
        params[2 * i] = text[2 * i];
        params[2 * i + 1] = text[2 * i + 1];
    }
    if (!PQsendQueryPrepared(conn, name, 2 * n, params, NULL, NULL, 0)) return -1;
    if (!PQpipelineSync(conn)) return -1;

    int failed = 0;
    if (!update_prepared[n]) {
        PGresult *p = next_result();
        if (p && PQresultStatus(p) == PGRES_COMMAND_OK) update_prepared[n] = 1;
        else failed = 1;
        if (p) PQclear(p);
    }
    PGresult *r = next_result();
    if (!r || PQresultStatus(r) != PGRES_COMMAND_OK) failed = 1;
    if (r) PQclear(r);
    return (read_sync() == 0 && !failed) ? 0 : -1;
}

int db_fetch_fortunes(Fortune *out, int cap, void **result_out) {
    *result_out = NULL;
    if (!PQsendQueryPrepared(conn, "fortune", 0, NULL, NULL, NULL, 0)) return -1;
    if (!PQpipelineSync(conn)) return -1;

    PGresult *r = next_result();
    const int sync_ok = read_sync() == 0;
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK || !sync_ok) {
        if (r) PQclear(r);
        return -1;
    }
    int rows = PQntuples(r);
    if (rows > cap) rows = cap;
    for (int i = 0; i < rows; i++) {
        out[i].id = atoi(PQgetvalue(r, i, 0));
        out[i].message = PQgetvalue(r, i, 1);
        out[i].message_len = (size_t)PQgetlength(r, i, 1);
    }
    *result_out = r;
    return rows;
}

void db_release(void *result) {
    if (result) PQclear((PGresult *)result);
}
