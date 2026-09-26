# TechEmpower (TFB) Benchmark: CExpress vs Actix, Axum, h2o, Fiber

All six TFB test types, run by TechEmpower's own harness (`./techempower/run.sh`): their Postgres image,
their wrk load generator, their request mix and concurrency levels. Every entry passes TFB verification.

## Setup

| | |
|---|---|
| Host | Apple Silicon Mac, 12 cores, 18 GB |
| Docker | Colima VM, **10 CPUs, 12 GB**. Server, Postgres and wrk share it (TechEmpower uses 3 separate machines) |
| Entries | `cexpress` / `cexpress-postgres`, `actix` / `actix-http`, `axum` / `axum-pg`, `h2o`, `fiber` |
| Load | TFB defaults: 15 s per level; concurrency 16-512 (json, db, fortune), 512 (query, update, 1-20 queries), 256-16384 with pipelining 16 (plaintext) |
| Runs | 3 full runs (2026-09-25/26), results under `techempower/.tfb/results/`: run 1 `20260926010854`, run 2 `20260926023941` (all six tests), run 3 `20260926132736` (the four DB tests only) |

What CExpress ran in each:

| Run | Engine | TFB app (`cexpress-postgres`) |
|---|---|---|
| 1, 2 | `c_server` `bf91ef7`: one `write` per pipelined response, handlers can't wait on I/O | one **blocking** libpq connection per worker, 4 workers per core |
| 3 | `c_server` `288068e`: coalesced pipelined writes, `res_defer` / `res_resume`, `app_watch_fd` | one **non-blocking, pipelined** libpq connection per worker, shared by all its requests; 1 worker per core |

Numbers are the best req/s across concurrency levels (req/s = `totalRequests / 15 s`). No test failed, and there
were **zero non-2xx responses** in any run. Every entry has some wrk socket timeouts at the highest concurrency levels
in similar amounts, which comes from sharing 10 cores between wrk, Postgres and the server.

## Results (req/s)

JSON and plaintext were not part of run 3; plaintext was re-run alone after the pipelined-write fix (below).

### JSON serialization: CExpress 1st
| Framework | Run 1 | Run 2 |
|---|---:|---:|
| **cexpress** | **764,242** | **771,136** |
| actix | 719,879 | 752,766 |
| axum | 692,841 | 708,224 |
| h2o | 684,221 | 706,159 |
| fiber | 620,965 | 581,602 |

### Plaintext (pipelined): CExpress 5th → 1st after coalesced writes (one run)
| Framework | Run 1 | Run 2 | Plaintext-only run `20260926105312` |
|---|---:|---:|---:|
| **cexpress** | **1,587,520** | **1,597,938** | **4,910,252** |
| actix | 4,273,428 | 4,623,852 | 4,689,204 |
| axum | 3,421,789 | 3,938,829 | 3.69M |
| fiber | 3,356,678 | 3,561,613 | 3.65M |
| h2o | 1,587,542 | 1,656,091 | 1.62M |

The plaintext-only run used the engine with coalesced pipelined writes (`c_server` `3d804c9`). CExpress was 1st at every pipelined level there;
its 5% lead over Actix is inside Actix's own run-to-run spread (4.27M / 4.62M / 4.69M).

### Single query (db): CExpress 4th, 52-54% → 97% of the leader
| Framework | Run 1 | Run 2 | Run 3 |
|---|---:|---:|---:|
| actix-http | 289,136 | 337,842 | 307,632 |
| axum-pg | 285,426 | 293,864 | 302,879 |
| h2o | 298,922 | 308,487 | 301,387 |
| **cexpress-postgres** | **155,174** | **177,588** | **297,671** |
| fiber | 132,298 | 126,166 | 131,994 |

### Multiple queries (query): CExpress 4th → 1st
| Framework | Run 1 | Run 2 | Run 3 | Run 3, 20 queries |
|---|---:|---:|---:|---:|
| **cexpress-postgres** | **152,222** | **169,539** | **301,411** | **65,533** |
| actix-http | 284,788 | 310,681 | 299,320 | 27,713 |
| h2o | 268,648 | 296,421 | 297,611 | 25,197 |
| axum-pg | 304,269 | 294,780 | 293,978 | 28,454 |
| fiber | 109,526 | 119,204 | 124,136 | 9,627 |

### Fortunes: CExpress 4th → 3rd, 59-60% → 96% of the leader
| Framework | Run 1 | Run 2 | Run 3 |
|---|---:|---:|---:|
| h2o | 275,478 | 301,100 | 295,283 |
| actix-http | 294,812 | 295,137 | 288,853 |
| **cexpress-postgres** | **170,498** | **180,969** | **282,844** |
| axum-pg | 294,532 | 279,486 | 282,753 |
| fiber | 113,815 | 117,632 | 119,492 |

### Updates: CExpress 4th → 1st
| Framework | Run 1 | Run 2 | Run 3 | Run 3, 20 updates |
|---|---:|---:|---:|---:|
| **cexpress-postgres** | **75,005** | **77,888** | **158,507** | **34,266** |
| actix-http | 142,456 | 168,677 | 151,049 | 18,930 |
| axum-pg | 135,391 | 142,293 | 143,699 | 18,616 |
| h2o | 118,694 | 139,556 | 126,403 | 15,267 |
| fiber | 63,222 | 68,221 | 68,507 | 7,353 |

## Run 3: before and after non-blocking libpq

CExpress best req/s, runs 1 / 2 (blocking libpq) against run 3 (non-blocking, pipelined), with its place and its
share of that run's leader:

| Test | Runs 1 / 2 | Place | vs leader | Run 3 | Place | vs leader |
|---|---:|---|---:|---:|---|---:|
| db | 155,174 / 177,588 | 4th | 52% / 54% | 297,671 | 4th | 97% |
| query (best level, 1 query) | 152,222 / 169,539 | 4th | 54% / 55% | 301,411 | **1st** | 101% of 2nd |
| fortune | 170,498 / 180,969 | 4th | 59% / 60% | 282,844 | 3rd | 96% |
| update (best level, 1 query) | 75,005 / 77,888 | 4th | 49% / 46% | 158,507 | **1st** | 105% of 2nd |

**db and fortune are a four-way tie.** The top four are within 3.2% (db) and 4.2% (fortune) of each other, well inside
the ~15% run-to-run spread seen for single entries (e.g. actix-http db 289k / 338k / 308k). The places there are not
meaningful from one run. query at one query is the same (top four within 2.5%); update at one query leads Actix by 5%.
The clear wins are at 5-20 queries, below.

**Per query count** (query and update, 512 connections, run 3):

| Queries per request | 1 | 5 | 10 | 15 | 20 |
|---|---:|---:|---:|---:|---:|
| query: cexpress / best other | 301k / 299k | 166k / 102k | 109k / 56k | 82k / 38k | 66k / 28k |
| update: cexpress / best other | 159k / 151k | 84k / 61k | 57k / 35k | 42k / 25k | 34k / 19k |

From 5 queries on, CExpress does 1.4-2.3× the best other entry. Each request's queries go out in one pipeline with
one sync, so they cost one Postgres round trip, and other requests' queries share the same connection.

**Per concurrency** (db, run 3):

| Connections | 16 | 32 | 64 | 128 | 256 | 512 |
|---|---:|---:|---:|---:|---:|---:|
| cexpress-postgres | 79k | 160k | 208k | 247k | 278k | 298k |
| actix-http | 146k | 166k | 195k | 240k | 279k | 308k |
| axum-pg | 71k | 168k | 194k | 243k | 278k | 303k |
| h2o | 63k | 175k | 191k | 253k | 281k | 301k |

At 16 connections Actix is the outlier: about 2× everyone else (fortune too: 169k vs cexpress 78k, axum 78k, h2o 64k).
From 32 connections on the top four track each other; CExpress is highest on db only at 64 connections (208k vs 195k).

wrk socket timeouts (sum over all levels, run 3) are in the same range for every entry: db 748-1,540, fortune
981-1,467, query 2,497-4,517, update 2,969-4,063. CExpress is at the high end on query (3,959) and update (4,063),
where it also completes the most requests.

An earlier head-to-head with only `actix-http` (`20260926125325`, same app code) gave the same picture: db 295k vs 307k,
fortune 280k vs 299k, 65k vs 29k at 20 queries, 34k vs 19k at 20 updates.

## Worker-count sweep (blocking libpq, before run 3; one run each)

| Workers per core | db | query | fortune | update |
|---|---:|---:|---:|---:|
| 2x | **216,946** | **206,326** | 196,882 | 65,227 |
| 4x (runs 1/2, avg) | 166,381 | 160,881 | 175,734 | 76,447 |
| 8x | 162,088 | 140,014 | 172,581 | **86,795** |
| 16x | 180,131 | 143,256 | **197,681** | 83,613 |

This sweep is from the blocking-libpq app, where workers stood in for concurrency; no setting got past ~70% of the
leaders. The non-blocking app runs one worker per core and has not been swept yet.

## What the numbers say

- **The engine is competitive on the request path.** CExpress wins JSON in both runs that included it.
- **Pipelined plaintext is fixed.** Responses to a pipelined batch are now gathered and written once, as Actix, Axum
  and Fiber do (they were written one syscall each). Plaintext went from 1.60M to 4.91M, 1st in one run.
- **Database tests are at parity, and ahead with many queries per request.** With libpq's socket in the event loop, a
  worker serves other requests while its queries are in flight, and many requests share one pipelined connection. db
  and fortune are in a tie with Actix, Axum and h2o; query and update lead at every query count, by 1.4-2.3× from 5
  queries on.
- **Open questions:** whether run 3's ranking holds in a second full run (it is one run), the worker count for the
  non-blocking app, and the 16-connection gap to Actix.
- Everything ran on one laptop. Rankings within a run matter more than the absolute numbers.
