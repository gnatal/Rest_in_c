#ifndef TFB_DB_H
#define TFB_DB_H

#include "tfb_types.h"
#include <stddef.h>

/* One libpq connection per worker process, opened by db_worker_init (app_on_worker_start hook)
 * and kept in pipeline mode for the life of the worker: every request's queries are sent in one
 * batch and read back after a single round trip. Calls block the worker's event loop while they
 * wait for Postgres, so the DB variant runs more workers than cores (see cexpress-postgres.dockerfile). */

void db_worker_init(void);

/* Uniform random World id in 1..WORLD_ROWS, per-worker generator seeded after fork. */
int db_random_id(void);

/* Fill `out[0..n)` with the rows for `ids[0..n)`: n separate SELECTs in one pipeline. 0 / -1. */
int db_fetch_worlds(const int *ids, int n, World *out);

/* Write `worlds[i].random_number` for every row in one UPDATE ... FROM (VALUES ...) statement,
 * prepared lazily per row count. `worlds` must hold distinct ids. 0 / -1. */
int db_update_worlds(const World *worlds, int n);

/* All Fortune rows into `out` (at most `cap`). Strings point into *result_out, which the caller
 * releases with db_release(). Returns the row count or -1. */
int db_fetch_fortunes(Fortune *out, int cap, void **result_out);
void db_release(void *result);

#endif
