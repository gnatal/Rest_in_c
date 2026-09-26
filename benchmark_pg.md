# Postgres API Benchmark: C (CExpress) vs Go (Fiber) vs Rust (Axum)

`GET /api/articles` (RealWorld Conduit: latest 20 articles with author, tags, favorite state, total count),
served by three implementations against **the same Postgres database with byte-identical SQL**.
All three return the same 10,627-byte response (same MD5).

## Setup

| | |
|---|---|
| Host | Apple Silicon Mac, 12 cores (wrk, all three servers, and Docker run on this one machine) |
| Postgres | 15.19 (`postgres:15-alpine` in Docker), **2 CPUs**, default config (`shared_buffers=128MB`) |
| Data | 500,000 articles · 100,000 users · 500 tags · 1,498,289 article_tags rows. `favorites`, `follows`, `comments` are empty. |
| DB connections | 4 per server (C: 4 cluster workers x 1 libpq connection; Go: `SetMaxOpenConns(4)`; Rust: `max_connections(4)`) |
| Driver | C: libpq `PQexecParams` · Go: `sqlx` + `lib/pq` · Rust: `sqlx` 0.9 |
| Query | One statement; Postgres builds the JSON (`json_build_object` / `json_agg`), the server returns the text unchanged |
| Load | `wrk -t4 -c100`, 5 s warmup + 30 s measured, 2 rounds with the server order rotated each round |

Reproduce: `psql -d realworld -f counts_pg.sql` once, start the three servers, then `./bench_pg.sh`.

## Results

### `/api/articles` (DB-backed)

| Server | Round 1 req/s | Round 2 req/s | Avg req/s | p50 | p99 | DB commits per request |
|---|---|---|---|---|---|---|
| **C / CExpress** | 1,410 | 1,425 | **1,417** | 60–65 ms | 151–162 ms | 1.0 |
| Go / Fiber | 1,098 | 1,161 | 1,130 | 79–84 ms | 407–433 ms | 2.0 |
| Rust / Axum | 988 | 994 | 991 | 81–83 ms | 164–181 ms | 1.0 |

### `/api/ping` (static baseline, no DB)

| Server | Avg req/s | p50 |
|---|---|---|
| C / CExpress | 187,800 | ~0.40 ms |
| Go / Fiber | 181,200 | ~0.33 ms |
| Rust / Axum | 184,900 | ~0.39 ms |

The three baselines are within 4% of each other. At ~185k req/s wrk shares the same CPUs as the server,
so this is roughly the ceiling of the machine and the load generator, not a ranking of the frameworks.

## What limits throughput

1. **Postgres CPU.** One query takes ~3.2 ms of execution (`EXPLAIN ANALYZE`), and the container has
   2 CPUs. During the C run the container used 163% of its 200% CPU, while all four C workers together
   used 5.8% of one core, so C is close to the database's ceiling. Each server reaches 1,000–1,400
   req/s, while `/api/ping` shows each can handle ~180k, so the web framework accounts for well under 1% of the cost.
2. **Round trips per request, which account for most of the gap between servers.**
   - Go's `lib/pq` handles a parameterized query without `binary_parameters` as two round trips:
     prepare, then execute. That shows up as 2.0 commits per request and the worst p99 (~420 ms),
     because each request holds one of the 4 pooled connections for two trips.
   - C and Rust both use one round trip. Rust is still ~30% behind C. The likely cause is sqlx's
     per-acquire pool overhead (connection health check / pool bookkeeping), which adds latency while the
     connection is held. This was not verified; profiling would confirm it.
3. **The pool size is 4 connections everywhere.** It's set to match the 4 C workers, and each C worker blocks
   on its libpq call. Go and Rust could use larger pools. With a 2-CPU Postgres, a larger pool would mostly
   add queueing inside Postgres rather than throughput.

## What changed from the first run (84–436 req/s)

| Problem in the first run | Fix |
|---|---|
| The "C on Postgres" number (436 req/s) came from a stale SQLite build still bound to port 8080; the Postgres build served zero requests (`realworld` DB committed 0 transactions during the run) | Stale process stopped; every run now records DB commits per request |
| `articlesCount` was `SELECT COUNT(*) FROM articles` on every request: ~24 ms of the ~26 ms query | Exact count kept in `article_stats` by statement-level triggers (`counts_pg.sql`); query drops to ~3 ms |
| C used `dbname=realworld`, Go/Rust used `dbname=postgres` | All three use `realworld` |
| Go/Rust skipped `to_char` and `COALESCE`, so the responses differed from C's | Same SQL text in all three; responses are byte-identical |
| The explanation "Go/Rust use an ORM / N+1 queries" was wrong: all three always ran the same single query | Removed |
| 10 s runs, no warmup, no percentiles, fixed order | 5 s warmup, 30 s runs, `--latency`, 2 rotated rounds |

With the real Postgres path and the old `COUNT(*)`, all three servers measured ~84 req/s: the count scan saturated Postgres.

## Takeaways

- Having Postgres build the JSON is what makes this endpoint fast in every language. The cheap count was a
  17x throughput win (84 to 1,417 req/s for C). The choice of language had no comparable effect.
- With identical SQL, CExpress leads by **25% over Go and 43% over Rust**. The gap comes from the DB client
  path (one round trip through libpq, with no pool layer in front of it), not from HTTP handling.
- The p99 latency difference is large: C at ~155 ms versus Go at ~420 ms under the same load.
- To measure the HTTP layers themselves, run wrk on a separate machine. On this machine the static
  baselines are indistinguishable.
