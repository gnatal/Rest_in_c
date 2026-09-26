#!/usr/bin/env bash
# Runs the TechEmpower Framework Benchmarks harness against CExpress (and whichever other
# frameworks you name), using TFB's own toolset, database and load generator.
#
#   ./run.sh                                   # verify both cexpress variants
#   ./run.sh --mode verify --test cexpress fiber axum
#   ./run.sh --mode benchmark --test cexpress cexpress-postgres fiber axum
#   ./run.sh --mode benchmark --test cexpress fiber --type json plaintext
#
# Arguments go straight to TFB's ./tfb. Other frameworks are fetched on demand: set
# COMPARE="Go/fiber Rust/axum Rust/actix C/h2o" (directories under TFB's frameworks/).
# Results land in .tfb/results/<timestamp>/results.json.
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
ENGINE=$HERE/../vendor/cexpress
TFB=${TFB_DIR:-$HERE/.tfb}
COMPARE=${COMPARE:-"Go/fiber Rust/axum Rust/actix C/h2o"}

if [ ! -d "$TFB/.git" ]; then
    git clone --depth 1 --filter=blob:none --sparse \
        https://github.com/TechEmpower/FrameworkBenchmarks.git "$TFB"
fi
# shellcheck disable=SC2086
git -C "$TFB" sparse-checkout set toolset $(printf 'frameworks/%s ' $COMPARE)

DEST=$TFB/frameworks/C/cexpress
rm -rf "$DEST"
mkdir -p "$DEST/engine"
cp -R "$HERE/cexpress/src" "$HERE/cexpress/Makefile" "$HERE"/cexpress/*.dockerfile \
    "$HERE/cexpress/benchmark_config.json" "$HERE/cexpress/config.toml" "$HERE/cexpress/README.md" "$DEST/"
cp "$ENGINE/Makefile" "$DEST/engine/"
cp -R "$ENGINE/lib" "$DEST/engine/lib"

cd "$TFB"
if [ $# -eq 0 ]; then
    set -- --mode verify --test cexpress cexpress-postgres
fi
exec ./tfb "$@"
