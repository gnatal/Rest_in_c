# Pending: re-run the TechEmpower benchmark after the CExpress improvements

**Status:** waiting on engine work in `../c_server`. Recorded 2026-09-26.

## Why

The first TechEmpower (TFB) run (`benchmark_techempower.md`) put CExpress 1st on JSON but last on plaintext and
4th of 5 on every database test, at 50-60% of Actix, Axum and h2o. The causes are in the engine, and the fixes are
listed in `../c_server/tech_empower_improv.md`:

- **T1**: pipelined responses are written one syscall each (plaintext).
- **T2/T3/T4**: handlers can't wait on the database without blocking the worker (db, query, fortune, update).
- **T5**: retune the worker count once T2/T4 land.

Until those are done, the current numbers are the result. They shouldn't be used to submit to TechEmpower (step 3
of `techempower.md`). **After the improvements, run the benchmark again** to see whether the gaps closed, and
decide on submission from the new numbers.

## When

- After **T1**: a quick run of json + plaintext is enough to check it.
- After **T2 + T3 + T4**: the full run, then the T5 worker sweep.

## How

1. **Refresh the vendored engine.** `vendor/cexpress` is a plain copy of `../c_server`, and `techempower/run.sh`
   builds from `vendor/cexpress/lib`. Without this step the re-run measures the old engine.
   ```sh
   rsync -a --delete --exclude .git --exclude 'build*' --exclude todos.db ../c_server/ vendor/cexpress/
   ```
2. **Update the TFB app for T4** (`techempower/cexpress/src/db.c`: non-blocking libpq on the new engine API), and set
   the Postgres variant's worker count (`CMD` in `techempower/cexpress/cexpress-postgres.dockerfile`) as T5 says.
3. **Check the machine.** Docker here is Colima; it should still have 10 CPUs / 12 GB:
   `docker info --format '{{.NCPU}} cpus {{.MemTotal}}'`. Plug in the laptop and leave it idle during the run.
4. **Verify** (all six tests must PASS):
   ```sh
   ./techempower/run.sh
   ```
5. **Benchmark with the same competitors, in the same run**, twice (about 1.5 h each):
   ```sh
   ./techempower/run.sh --mode benchmark \
     --test cexpress cexpress-postgres fiber h2o axum axum-pg actix actix-http \
     --type json plaintext db query update fortune
   ```
   For the quick post-T1 check: `--test cexpress fiber h2o axum actix --type json plaintext`.
6. **Compare** against the first run: results are in `techempower/.tfb/results/<timestamp>/results.json`. The
   baseline runs are `20260926010854` and `20260926023941`. Check that there are still zero non-2xx responses.
7. **Record it:** add the new tables to `benchmark_techempower.md` (keep the old ones as the "before"), mark the
   fixed items in `../c_server/tech_empower_improv.md`, and update the status in `techempower.md`.

## Targets

| Test | Baseline (best of 2 runs) | Target after the fixes |
|---|---:|---|
| JSON | 771k, 1st | stay 1st (no regression from T1) |
| Plaintext | 1.60M, 5th | ~3-4M, alongside Fiber/Axum |
| db / query / fortune | 170-181k, 4th | within 10-20% of the leaders (~300k here) |
| update | 78k, 4th | within 10-20% of the leaders (~140-170k here) |

If the targets are met, continue with step 3 of `techempower.md` (submit to TechEmpower). If not, go back to
`tech_empower_improv.md`.
