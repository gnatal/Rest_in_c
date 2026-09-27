# TechEmpower entry for CExpress

## Build and layout
- `../run.sh` rebuilds `.tfb/frameworks/C/cexpress/` on every call from this folder (`src/`, `Makefile`,
  `*.dockerfile`, `benchmark_config.json`, `config.toml`, `README.md`) plus the engine copied from
  `../../vendor/cexpress` (its `Makefile` and `lib/`), not from `c_server/`. An engine change reaches the TFB images
  only once the vendor tree is updated.
- The engine is built with `NO_URING=1` (epoll): the io_uring backend is only a readiness poller and measured slower.
- Two entries: `cexpress` (json, plaintext; no database code runs) and `cexpress-postgres` (db, query, update,
  fortune), selected at runtime by `CEXPRESS_DB=1`. Both use one worker process per core (`CEXPRESS_WORKERS`
  overrides it). With one non-blocking connection per worker, more workers only add Postgres backends and context
  switches (MEASURED, run 5: 1 per core beats 2 per core by 5-6% on db/query).
- `cexpress-postgres.dockerfile` installs libpq from the PostgreSQL apt repository (18.x); Ubuntu 24.04's own libpq
  is 16. `db.c` picks the buffered sync with `#ifdef LIBPQ_HAS_SEND_PIPELINE_SYNC` (defined by libpq-fe.h from 17),
  so the same source builds against 16, where each request flushes on its own.

## Database path (`src/db.c`, types in `src/tfb_types.h`)
- **One connection per worker**, opened in the `app_on_worker_start` hook (after fork, before the loop), blocking
  only for connect and for preparing `world` and `fortune`. Then non-blocking, pipeline mode, and its socket is
  registered with `app_watch_fd` (applied when the loop opens). Every request on the worker shares it; there is no
  cross-worker state.
- **Jobs.** A handler takes a `DbJob`, `res_defer`s its response, queues its queries followed by a sync, and returns.
  Jobs sit in one FIFO in send order. The pipeline answers in that order and each job ends with a sync, so every
  `PGRES_PIPELINE_SYNC` completes the head job. `finish` calls `res_resume`: NULL means the client left, and the
  result is dropped; otherwise `done` builds the response (or 500 if a query failed or rows are missing).
- **`/updates` has two phases in one job.** Phase 1: the n SELECTs and a sync. When that sync arrives, new random
  numbers are drawn and the job is re-queued at the tail with phase 2: one `UPDATE ... FROM (VALUES ...)` and a sync.
  The UPDATE is prepared lazily per row count (`upd<n>`); `update_prepared[n]` is set as soon as the PREPARE is
  queued, so later jobs of the same size use it behind it in the pipeline, and it is reset if the PREPARE fails.
  Ids are sorted before the UPDATE so concurrent batches lock rows in the same order (no deadlocks); handlers pass
  distinct ids.
- **One implicit transaction per request** (each sync ends one), so a failed query fails only its own request.

## Flush policy
- Every sync is `PQsendPipelineSync` (buffered, no send) and sets `g_unsent`. The `app_on_turn_end` hook sends
  everything the turn queued (handlers and result callbacks alike) with one `PQflush`. Write readiness is watched
  only while `PQflush` returns 1; the readiness callback flushes only on write readiness (the rest of a partial
  send). `flush_output` clears `g_unsent`, so a write-readiness flush also makes the turn-end flush a no-op.
- An `/updates` phase 2 queued while reading results therefore goes out at the end of the same turn.
- MEASURED:
  - Run 5 (one run): one flush per turn beats one flush per request by 10-13% on db and single-query.
  - Run 6 (one run): flushing a result callback's follow-up queries right after the read, instead of at turn end,
    changed updates by +0.5-2.8%, inside noise, so it was not kept. Run 5's update loss at 1-5 queries did not
    reproduce in run 6.
  - Numbers and result folders: `../../benchmark_techempower.md`.

## Failure handling
- A send or `PQconsumeInput`/`PQflush` failure marks the connection broken (`g_broken`): the socket is unwatched,
  every queued job is answered 500, and every later request gets 500 at once. There is no reconnect.
- Over the engine's memory budget `res_defer` builds a 503 itself; the job goes back to the free list.

## Memory lifecycle
- `DbJob`: `malloc`'d on first need, recycled through a free list (`job_release`), never freed; lives as long as
  the worker process. Never allocated in the request arena, because the client may leave while its queries are in
  the pipeline.
- Fortunes: the job holds the `PGresult` (`pg_result`) that its `Fortune.message` pointers point into, until
  `job_release` `PQclear`s it, after `done` ran. `done_fortunes` copies the array, adds the extra fortune (a string
  literal), sorts it and writes the HTML into the response arena.
- Every other `PGresult` is cleared in `on_result` right after its values are copied out.
- `send_update` uses static buffers for the SQL and parameter text: each worker is single-threaded and libpq copies
  the parameters when the query is queued.
- The connection is closed by process exit.

## Verification
- `./run.sh` (no arguments) runs TFB verify on both entries; it must be all PASS. Benchmarks:
  `./run.sh --mode benchmark --test ... --type ...`; results in `../.tfb/results/<timestamp>/results.json`
  (req/s = `totalRequests / 15`, best across levels).
- Single-run noise is up to ~15% per entry (MEASURED: an A/A pair in run 6 was 12% apart at 16 connections), so
  compare variants inside one run. wrk socket timeouts appear for every framework in this setup and are not a
  signal on their own.
