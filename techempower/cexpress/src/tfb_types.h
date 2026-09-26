#ifndef TFB_TYPES_H
#define TFB_TYPES_H

#include <stddef.h>
#include "app_types.h"

/* Limits fixed by the TechEmpower spec. */
#define WORLD_ROWS 10000          /* World ids are 1..10000 */
#define MAX_QUERIES 500           /* ?queries= is clamped to 1..500 */
#define MAX_FORTUNES 64           /* the Fortune table has 12 rows; one more is added per request */

typedef struct {
    int id;
    int random_number;
} World;

/* `message` points into a PGresult (or a string literal) owned by the job that holds it. */
typedef struct {
    int id;
    const char *message;
    size_t message_len;
} Fortune;

typedef enum {
    DB_JOB_WORLDS,        /* n SELECTs by id */
    DB_JOB_UPDATE_READ,   /* /updates, phase 1: the n SELECTs */
    DB_JOB_UPDATE_WRITE,  /* /updates, phase 2: one UPDATE ... FROM (VALUES ...), maybe preceded by its PREPARE */
    DB_JOB_FORTUNES       /* SELECT every fortune */
} DbJobKind;

struct DbJob;

/* Builds the response once a job's results are in. Called on the event loop with the resumed Response
 * (never NULL: a request whose client left is dropped before this). Errors are answered 500 by db.c. */
typedef void (*DbDone)(Response *res, const struct DbJob *job);

/*
 * One HTTP request's database work. Queued in send order (db.c's FIFO): the connection's pipeline answers
 * in that order, and each job's queries end with a sync, so every PGRES_PIPELINE_SYNC completes the job at
 * the head. Owned by db.c (a free list), never by the request: the client may leave while its queries are
 * still in the pipeline. Released after `done` ran (or the request was found gone).
 */
typedef struct DbJob {
    struct DbJob *next;
    DeferHandle handle;          /* the deferred HTTP request */
    DbDone done;
    DbJobKind kind;
    int n;                       /* worlds wanted */
    int got;                     /* rows received in the current phase */
    int failed;                  /* any query of the current phase failed */
    int prepare_pending;         /* phase 2: the lazily prepared UPDATE's own result comes first */
    int as_array;                /* /queries and /updates answer an array, /db one object */
    World worlds[MAX_QUERIES];
    Fortune fortunes[MAX_FORTUNES + 1];
    int fortune_count;
    void *pg_result;             /* the fortunes PGresult the Fortune messages point into; cleared at release */
} DbJob;

#endif
