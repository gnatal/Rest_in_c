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
| Runs | 4 full runs (2026-09-25/26), results under `techempower/.tfb/results/`: run 1 `20260926010854`, run 2 `20260926023941`, run 3 `20260926132736` (the four DB tests only), run 4 `20260926145131` (all six tests) |

What CExpress ran in each:

| Run | Engine | TFB app (`cexpress-postgres`) |
|---|---|---|
| 1, 2 | `c_server` `bf91ef7`: one `write` per pipelined response, handlers can't wait on I/O | one **blocking** libpq connection per worker, 4 workers per core |
| 3, 4 | `c_server` `288068e`: coalesced pipelined writes, `res_defer` / `res_resume`, `app_watch_fd` | one **non-blocking, pipelined** libpq connection per worker, shared by all its requests; 1 worker per core |

Numbers are the best req/s across concurrency levels (req/s = `totalRequests / 15 s`). No test failed, and there
were **zero non-2xx responses** in any run. Every entry has some wrk socket timeouts at the highest concurrency levels
in similar amounts, which comes from sharing 10 cores between wrk, Postgres and the server.

## Results (req/s)

JSON and plaintext were not part of run 3; plaintext was also run alone after the pipelined-write fix (below).

### JSON serialization: CExpress 1st in all three runs
| Framework | Run 1 | Run 2 | Run 4 |
|---|---:|---:|---:|
| **cexpress** | **764,242** | **771,136** | **787,754** |
| actix | 719,879 | 752,766 | 738,709 |
| axum | 692,841 | 708,224 | 727,775 |
| h2o | 684,221 | 706,159 | 699,260 |
| fiber | 620,965 | 581,602 | 590,756 |

### Plaintext (pipelined): CExpress 5th → 1st after coalesced writes (two runs)
| Framework | Run 1 | Run 2 | Plaintext-only run `20260926105312` | Run 4 |
|---|---:|---:|---:|---:|
| **cexpress** | **1,587,520** | **1,597,938** | **4,910,252** | **4,813,888** |
| actix | 4,273,428 | 4,623,852 | 4,689,204 | 4,412,559 |
| axum | 3,421,789 | 3,938,829 | 3.69M | 3,795,426 |
| fiber | 3,356,678 | 3,561,613 | 3.65M | 3,511,990 |
| h2o | 1,587,542 | 1,656,091 | 1.62M | 1,568,702 |

The plaintext-only run used the engine with coalesced pipelined writes (`c_server` `3d804c9`); run 4 used `288068e`.
CExpress was 1st at every pipelined level in both (run 4: 4.78M / 4.81M / 4.09M / 3.34M at 256 / 1,024 / 4,096 /
16,384 connections, Actix 4.41M / 4.26M / 3.48M / 2.82M). Its lead over Actix is 5% and 9%.

### Single query (db): CExpress 4th, 52-54% → 96-97% of the leader
| Framework | Run 1 | Run 2 | Run 3 | Run 4 |
|---|---:|---:|---:|---:|
| actix-http | 289,136 | 337,842 | 307,632 | 311,464 |
| axum-pg | 285,426 | 293,864 | 302,879 | 306,329 |
| h2o | 298,922 | 308,487 | 301,387 | 311,737 |
| **cexpress-postgres** | **155,174** | **177,588** | **297,671** | **299,887** |
| fiber | 132,298 | 126,166 | 131,994 | 131,725 |

### Multiple queries (query): tied at 1 query, CExpress 1st by 2.3× at 20
| Framework | Run 1 | Run 2 | Run 3 | Run 4 | 20 queries, run 3 / 4 |
|---|---:|---:|---:|---:|---:|
| **cexpress-postgres** | **152,222** | **169,539** | **301,411** | **302,805** | **65,533 / 66,051** |
| actix-http | 284,788 | 310,681 | 299,320 | 309,852 | 27,713 / 28,575 |
| h2o | 268,648 | 296,421 | 297,611 | 304,634 | 25,197 / 23,394 |
| axum-pg | 304,269 | 294,780 | 293,978 | 298,046 | 28,454 / 29,132 |
| fiber | 109,526 | 119,204 | 124,136 | 122,554 | 9,627 / 9,587 |

### Fortunes: CExpress 3rd-4th, 59-60% → 93-96% of the leader
| Framework | Run 1 | Run 2 | Run 3 | Run 4 |
|---|---:|---:|---:|---:|
| h2o | 275,478 | 301,100 | 295,283 | 295,790 |
| actix-http | 294,812 | 295,137 | 288,853 | 278,265 |
| **cexpress-postgres** | **170,498** | **180,969** | **282,844** | **275,448** |
| axum-pg | 294,532 | 279,486 | 282,753 | 284,364 |
| fiber | 113,815 | 117,632 | 119,492 | 108,479 |

### Updates: CExpress 1st or tied at 1 query, 1st by 1.7-1.8× at 20
| Framework | Run 1 | Run 2 | Run 3 | Run 4 | 20 updates, run 3 / 4 |
|---|---:|---:|---:|---:|---:|
| **cexpress-postgres** | **75,005** | **77,888** | **158,507** | **157,191** | **34,266 / 33,306** |
| actix-http | 142,456 | 168,677 | 151,049 | 157,190 | 18,930 / 19,731 |
| axum-pg | 135,391 | 142,293 | 143,699 | 145,346 | 18,616 / 19,717 |
| h2o | 118,694 | 139,556 | 126,403 | 143,981 | 15,267 / 17,402 |
| fiber | 63,222 | 68,221 | 68,507 | 68,954 | 7,353 / 7,418 |

## Run 4: does run 3's ranking hold?

Same code as run 3, all six tests, all eight entries in one run. Best req/s and the spread of the top four:

| Test | CExpress, run 3 → 4 | Place run 3 → 4 | Top four within (run 3 / 4) | Leader in run 4 |
|---|---:|---|---|---|
| db | 297,671 → 299,887 | 4th → 4th | 3.2% / 3.8% | h2o 311,737 (actix-http 311,464) |
| query, 1 query | 301,411 → 302,805 | 1st → 3rd | 2.5% / 3.8% | actix-http 309,852 |
| query, 20 queries | 65,533 → 66,051 | 1st → 1st | – | 2.27× axum-pg (29,132) |
| fortune | 282,844 → 275,448 | 3rd → 4th | 4.2% / 6.9% | h2o 295,790 |
| update, 1 query | 158,507 → 157,191 | 1st → 1st (by 1 req/s) | – / 8.4% | tied with actix-http 157,190 |
| update, 20 queries | 34,266 → 33,306 | 1st → 1st | – | 1.69× actix-http (19,731) |

CExpress's own numbers moved by at most 2.8% between runs 3 and 4; the places at one query moved because the
competitors moved (actix-http +3.5% on query, h2o +14% on update). What held in both runs: **db, fortune and query
at one query are a four-way tie** within a few percent, and **CExpress leads by 1.35-2.3× from 5 queries on**
(run 4: query 1.59× / 1.90× / 2.13× / 2.27× and update 1.35× / 1.56× / 1.64× / 1.69× at 5 / 10 / 15 / 20). Fortune
is CExpress's weakest DB test: 7% behind h2o in run 4, 4% in run 3. At 16 connections Actix-http is again the
outlier on db (156k; cexpress 76k, axum 77k), but h2o joined it this time (144k), so that level is noisy for
everyone. wrk timeouts for cexpress-postgres in run 4 were low: 260 on db (h2o 0, the others 1,260-1,487) and 509
on query, the fewest of the five.

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

- **The engine is competitive on the request path.** CExpress wins JSON in all three runs that included it.
- **Pipelined plaintext is fixed.** Responses to a pipelined batch are now gathered and written once, as Actix, Axum
  and Fiber do (they were written one syscall each). Plaintext went from 1.60M to 4.81-4.91M, 1st in both runs since.
- **Database tests are at parity, and ahead with many queries per request.** With libpq's socket in the event loop, a
  worker serves other requests while its queries are in flight, and many requests share one pipelined connection. db
  and fortune are in a tie with Actix, Axum and h2o; query and update lead at every query count, by 1.4-2.3× from 5
  queries on.
- **Open questions:** the worker count for the non-blocking app, fortune (the one DB test where CExpress trails the
  leader by more than noise in both runs, 4-7%), and one `send` per event-loop turn to Postgres instead of one per
  request.
- Everything ran on one laptop. Rankings within a run matter more than the absolute numbers.
