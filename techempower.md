# TechEmpower plan for CExpress

Goal: run the TechEmpower Framework Benchmarks (TFB) with CExpress next to Fiber, Axum and others, and if
the results hold up, get CExpress into the official TechEmpower runs.

Status (2026-09-25): CExpress implements all six test types (json, plaintext, db, query, update, fortune)
and **passes TFB's own verification** (`./techempower/run.sh`).

Status (2026-09-26): steps 1 and 2 done. Results in `benchmark_techempower.md`: CExpress is 1st on JSON, last on
plaintext (one write per pipelined response), and 4th of 5 on every DB test at 50-70% of Actix/Axum/h2o (blocking
libpq; a worker-count sweep didn't close the gap). Decision pending: **improve first** is recommended.

Where things are:
- `techempower/cexpress/`: the TFB framework folder (source, Dockerfiles, `benchmark_config.json`, `config.toml`, README).
- `techempower/run.sh`: copies that folder and `vendor/cexpress/lib` into a TFB checkout (`techempower/.tfb`, gitignored) and runs `./tfb` with your arguments.

---

## 1. Run the benchmarks

### 1.1 Give Docker enough machine
Right now the Docker VM has **2 CPUs and 2 GB RAM**, and the load generator (wrk), Postgres and the server
all share it. Numbers from that setup aren't useful.

- [ ] Docker Desktop → Settings → Resources: as many CPUs as you can spare (8+), 8 GB+ memory. Apply & restart.
- [ ] Check: `docker info --format '{{.NCPU}} cpus {{.MemTotal}}'`
- [ ] Close heavy apps, plug the laptop in, and don't use it during the run. The earlier benchmarks showed
      results changing with machine load.

### 1.2 Re-verify (a few minutes)
```sh
./techempower/run.sh
```
All six lines in the "Verification Summary" should say PASS.

### 1.3 Quick run: json + plaintext only
```sh
./techempower/run.sh --mode benchmark --test cexpress fiber axum --type json plaintext
```
The first run builds the Fiber and Axum images, and the Rust build is slow.

### 1.4 Full run: every test type
```sh
./techempower/run.sh --mode benchmark --test cexpress cexpress-postgres fiber axum
```
- Expect an hour or more. Other entries can be added with `COMPARE="Go/fiber Rust/axum Rust/actix C/h2o"`
  (TFB folder names) and then naming their tests after `--test`.
- Results: `techempower/.tfb/results/<timestamp>/results.json`, plus per-test logs in the same folder.
- [ ] Run the full set **at least twice** and keep both result folders. Treat a difference under ~5% as noise.

### 1.5 Read the results
- [ ] For each test type, write down CExpress's best req/s and where it ranks against Fiber and Axum.
- [ ] Check the logs for errors: non-2xx responses or socket errors in any run make its number unreliable.
- [ ] Save a short summary (a table per test type) next to `benchmark_pg.md`.

---

## 2. Decide if the results are good

TFB accepts any framework, fast or slow. This step is your own decision about whether to submit now or improve first.

- [ ] **Plaintext / JSON**: these measure the engine. If CExpress is within ~10% of Fiber/Axum or ahead of
      them, the engine is competitive.
- [ ] **db / query / fortune / update**: likely CExpress's weak spot. Handlers wait on Postgres and
      block the worker's event loop, while Fiber and Axum do other work during the wait. The Postgres variant
      runs 4 workers per core to make up for it (`CMD` in `cexpress-postgres.dockerfile`).
  - [ ] If the DB numbers are low, first try a different worker count (2x, 8x, 16x cores) and rerun.
  - [ ] If that isn't enough, the real fix is in the engine: let libpq's socket join CExpress's
        event loop, so a worker keeps serving other requests during a query. This is the larger project.
- [ ] Keep in mind: a laptop with everything on one machine is not TechEmpower's hardware (separate
      server, database and load-generator machines, many cores, 10 GbE). The ranking against Fiber/Axum
      on the same run matters more than the raw req/s.

Result of this step: **submit now**, or **improve first** (back to step 1 after the change).

---

## 3. Send it to TechEmpower

TechEmpower adds frameworks through pull requests to
[TechEmpower/FrameworkBenchmarks](https://github.com/TechEmpower/FrameworkBenchmarks). Contribution guide:
<https://github.com/TechEmpower/FrameworkBenchmarks/wiki/Development-Contributing-Guide>.

### 3.1 Make the framework folder self-contained (required)
Right now the Dockerfiles `COPY engine`, which `run.sh` fills from this repo. In the PR, the Dockerfile
has to fetch CExpress itself.
- [ ] Make CExpress's source publicly available on GitHub (this repo, `gnatal/Rest_in_c`, or a separate
      `cexpress` repo, which reads better for a framework) with a license (there is one in `vendor/cexpress/LICENSE`).
- [ ] Tag a release, e.g. `v0.1.0`.
- [ ] Change both Dockerfiles to download that exact tag (or commit) instead of `COPY engine`, e.g.
      `RUN git clone --depth 1 --branch v0.1.0 https://github.com/<you>/cexpress engine`
      (and add `git ca-certificates` to the build stage's `apt-get install`).
- [ ] Pin the tag. Don't clone a moving branch: the results have to be reproducible.

### 3.2 Polish the metadata
- [ ] Add `"maintainers": ["gnatal"]` (your GitHub handle) to `benchmark_config.json`, as other entries do.
- [ ] README: point the link at the public CExpress repo.
- [ ] Double-check `classification: "Micro"` (a small framework with routing and middleware, not a bare
      platform) and the display names `cexpress` / `cexpress [pg]`.

### 3.3 Open the PR
- [ ] Fork `TechEmpower/FrameworkBenchmarks` on GitHub and clone your fork.
- [ ] Copy `techempower/cexpress/` (with the Dockerfile changes, **without** `engine/`) to `frameworks/C/cexpress/`.
- [ ] In the fork: `./tfb --mode verify --test cexpress cexpress-postgres`. Everything must PASS.
- [ ] Commit on a branch, push, and open a PR titled like "Add CExpress (C)". In the description, briefly
      describe the framework and the two variants.
- [ ] Their GitHub Actions CI runs the verification on the PR. Fix anything it reports. Maintainers may ask
      for changes: answer in the PR.

### 3.4 After it's merged
- [ ] CExpress joins TechEmpower's continuous runs on their own hardware. Results show up on
      <https://tfb-status.techempower.com>, and in the next official round on <https://www.techempower.com/benchmarks/>.
- [ ] Those numbers are the ones to quote publicly. Link to the run instead of local results.
- [ ] Updating CExpress later: tag a new release and open a small PR that bumps the tag in the Dockerfiles.
