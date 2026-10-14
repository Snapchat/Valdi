#!/usr/bin/env bash
#
# Build every Valdi microbenchmark binary so that an API/source change which
# breaks a benchmark fails the PR, instead of the benchmark silently dropping
# out of the nightly suite (perf_ratio.sh skips any target it can't build).
# Build-only: it does not run them (that's the nightly's job).
#
# Single source of truth for the target list = the benchmarks registered in
# perf_ratio.sh. Parsing that here means a benchmark added to the nightly is
# automatically covered by this PR gate, so the two lists can't drift.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/../.."   # -> open_source workspace root (tools/ci/ -> ../..)

# `bzl` is present in every context that runs this (external test-cpp task and
# internal SnapCI, same as build_core_targets.sh); override with BENCH_BAZEL.
BAZEL="${BENCH_BAZEL:-bzl}"
# use_local_compiler=true is what the rest of external CI builds with (see the
# EXTERNAL_FLAG in test_external_build_graph.sh) and what internal needs too.
BUILD_FLAGS="${BENCH_BUILD_FLAGS:---//bzl/valdi:use_local_compiler=true}"

# Unique benchmark targets from perf_ratio.sh's BENCHMARKS list ("//valdi:x|Bench").
# First column (before "|") of every "//target|Bench" line, any package — not
# just //valdi — so a benchmark added from e.g. //snap_drawing is still gated.
mapfile -t TARGETS < <(awk -F'|' '/^\/\// {print $1}' "$SCRIPT_DIR/perf_ratio.sh" | sort -u)
if [ "${#TARGETS[@]}" -eq 0 ]; then
  echo "[build_benchmarks] no benchmark targets found in perf_ratio.sh" >&2
  exit 1
fi

echo "[build_benchmarks] building: ${TARGETS[*]}"
# shellcheck disable=SC2086
"$BAZEL" build $BUILD_FLAGS "${TARGETS[@]}"
echo "[build_benchmarks] all benchmark binaries built."
