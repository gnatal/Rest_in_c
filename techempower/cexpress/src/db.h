#ifndef TFB_DB_H
#define TFB_DB_H

#include "tfb_types.h"

/*
 * One non-blocking libpq connection per worker process, in pipeline mode, shared by every request on that
 * worker. A handler submits its queries and returns (res_defer); the event loop watches the connection's
 * socket (app_watch_fd) and answers each request when its results arrive (res_resume). Many requests'
 * queries are in flight on one connection at once, which is what keeps one worker per core busy.
 */

/* Remembers the App (the worker-start hook takes no argument) and registers the per-worker connect hook. */
void db_setup(App *app);

/* Uniform random World id in 1..WORLD_ROWS, per-worker generator seeded after fork. */
int db_random_id(void);

/*
 * Submit a request's work. Each defers `res` and returns; `done` builds the response later. On any
 * failure (no connection, a send error, a query error) the client gets 500 and `done` is not called;
 * over the engine's memory budget it gets 503.
 *   db_submit_worlds: n SELECTs for ids[0..n) (as_array: /queries; else /db, one object).
 *   db_submit_updates: n SELECTs, then new random numbers, then one UPDATE of all n rows; `done` sees the
 *     updated values. ids must be distinct.
 *   db_submit_fortunes: every fortune in job->fortunes[0..fortune_count), room for one more at the end.
 */
void db_submit_worlds(Response *res, const int *ids, int n, int as_array, DbDone done);
void db_submit_updates(Response *res, const int *ids, int n, DbDone done);
void db_submit_fortunes(Response *res, DbDone done);

#endif
