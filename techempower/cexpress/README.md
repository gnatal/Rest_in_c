# CExpress Benchmarking Test

[CExpress](https://github.com/gnatal/Rest_in_c) is an Express.js-style HTTP/1.1 framework written in C11:
one single-threaded event loop (epoll) per worker process, a Patricia-tree router, a per-request arena
allocator, [yyjson](https://github.com/ibireme/yyjson) for JSON and picohttpparser for request parsing.

### Test Type Implementation Source Code

* [JSON](src/main.c): `handler_json`, serialized with yyjson on every request
* [PLAINTEXT](src/main.c): `handler_plaintext`
* [DB](src/main.c): `handler_db`
* [QUERY](src/main.c): `handler_queries`
* [UPDATE](src/main.c): `handler_updates`
* [FORTUNES](src/main.c): `handler_fortunes`

## Important Libraries

* libpq, one connection per worker process, in pipeline mode: a request's queries are sent as
  separate statements in one batch and read back after one round trip ([src/db.c](src/db.c)).
* Updates are written with one `UPDATE ... FROM (VALUES ...)` statement per request.

## Variants

* `cexpress`: json and plaintext, one worker per core.
* `cexpress-postgres`: db, query, fortunes and updates. Handlers block on Postgres, so the container
  runs 4 workers per core.

## Test URLs

* JSON: http://localhost:8080/json
* PLAINTEXT: http://localhost:8080/plaintext
* DB: http://localhost:8080/db
* QUERY: http://localhost:8080/queries?queries=
* UPDATE: http://localhost:8080/updates?queries=
* FORTUNES: http://localhost:8080/fortunes
