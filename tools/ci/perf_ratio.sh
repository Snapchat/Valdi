#!/usr/bin/env bash
#
# Non-blocking NIGHTLY perf ratio for the Valdi hot-path microbenchmark SUITE.
#
# Runs once a day against master tip (not per-PR): perf is a trend signal, not a
# correctness gate, and the baseline rebuild is expensive — nightly keeps that
# cost bounded and off every contributor's critical path.
#
# TWO metrics per benchmark, with very different noise profiles:
#   * allocs/op + bytes/op  — DETERMINISTIC (same input -> same count, run to run).
#     This is the PRIMARY signal: a strict threshold (any increase flags) is
#     high-signal and does not flap, even on shared/variable CI hardware.
#   * wall-time             — noisy on shared runners, so SECONDARY: a loose
#     threshold, and the workflow de-bounces it (file only on 2 consecutive
#     nightly crossings). We still compare tip vs master-as-of-~24h-ago built on
#     the SAME runner in the SAME job to cancel most cross-runner variance.
#
# NON-BLOCKING on regressions: it annotates the job summary and emits a machine-
# readable verdict; the workflow files a public issue for the day's commit range.
# Degrades gracefully: a partial build/run/lookup failure reports what it has and
# exits 0. Exits 1 only when no benchmark produced metrics at HEAD.
set -uo pipefail

# The suite: "bazel_target|benchmark_name". Add a line per benchmark as the suite grows.
BENCHMARKS="${PERF_BENCHMARKS:-$(cat <<'EOF'
//valdi:heapdump_benchmark|BuildHeapDump
//valdi:marshalling_benchmark|MarshalRoundTripQuickJS
//valdi:marshalling_benchmark|MarshalNativeToJsQuickJS
//valdi:layout_benchmark|LayoutDeepTree
//valdi:render_benchmark|RenderInitialTree
//valdi:render_benchmark|RenderStateUpdate
//valdi:runtime_init_benchmark|RuntimeInitQuickJS
EOF
)}"

BASE_BRANCH="${PERF_BASE_BRANCH:-main}"
BASELINE_AGO="${PERF_BASELINE_AGO:-24 hours ago}"
REPS="${PERF_REPETITIONS:-5}"
MIN_TIME="${PERF_MIN_TIME:-0.5s}"
# allocs are exact -> strict (any increase flags). time is noisy -> loose + workflow de-bounce.
ALLOC_THRESHOLD="${PERF_ALLOC_THRESHOLD:-1.0}"
TIME_THRESHOLD="${PERF_TIME_THRESHOLD:-1.25}"
# Empty on the public repo (Copybara defaults use_local_compiler=true); set
# --//bzl/valdi:use_local_compiler=true for internal/local runs.
BUILD_FLAGS="${PERF_BUILD_FLAGS:-}"
# `bazel` on the public nightly runner; the internal nightly points it at bzl.
BAZEL="${PERF_BAZEL:-bazel}"

SUMMARY="${GITHUB_STEP_SUMMARY:-/dev/stdout}"
log() { echo "[perf] $*"; }

emit() { # regressed improved head_sha base_sha
  [ -n "${GITHUB_OUTPUT:-}" ] || return 0
  { echo "regressed=$1"; echo "improved=$2"; echo "head_sha=$3"; echo "base_sha=$4"; } >> "$GITHUB_OUTPUT"
}

# Build+run one target, append its JSON to $OUT_DIR keyed by target (built once,
# reused for every benchmark_name in that target).
declare -A RAN_TARGETS
run_target() { # $1 target  $2 out_json ; echo path on success
  local target="$1" out="$2"
  if [ -n "${RAN_TARGETS[$target]:-}" ]; then
    cp "${RAN_TARGETS[$target]}" "$out"; return 0
  fi
  # shellcheck disable=SC2086
  "$BAZEL" build $BUILD_FLAGS "$target" >&2 || return 1
  local bin
  # shellcheck disable=SC2086
  bin="$("$BAZEL" cquery --output=files $BUILD_FLAGS "$target" 2>/dev/null | head -n1)"
  [ -n "$bin" ] && [ -x "$bin" ] || return 1
  "$bin" --benchmark_format=json --benchmark_repetitions="$REPS" \
    --benchmark_min_time="$MIN_TIME" > "$out" 2>/dev/null || return 1
  RAN_TARGETS[$target]="$out"
}

# median real_time + allocs/op + bytes/op for a benchmark_name (prints "time allocs bytes"
# or empty). Prefers the "<name>_median" aggregate; falls back to mean of iterations.
metrics() { # $1 json  $2 benchmark_name
  python3 - "$1" "$2" <<'PY'
import json, sys
d = json.load(open(sys.argv[1])); name = sys.argv[2]
bs = d.get("benchmarks", [])
def pick(field):
    m = next((b.get(field) for b in bs if b.get("name") == f"{name}_median"), None)
    if m is not None: return m
    xs = [b.get(field) for b in bs if b.get("name") == name and b.get("run_type") == "iteration" and b.get(field) is not None]
    return sum(xs)/len(xs) if xs else None
t, a, by = pick("real_time"), pick("allocs/op"), pick("bytes/op")
print("" if t is None else f"{t} {a if a is not None else 'NA'} {by if by is not None else 'NA'}")
PY
}

# Collect metrics for every benchmark into an assoc file: "name|time|allocs|bytes"
collect() { # $1 = out_tsv
  : > "$1"
  local line target bench json m
  while IFS='|' read -r target bench; do
    [ -n "$target" ] || continue
    json="$(mktemp)"
    if run_target "$target" "$json"; then
      m="$(metrics "$json" "$bench")"
      [ -n "$m" ] && echo "${bench}|${m// /|}" >> "$1"
    else
      log "build/run failed for $target (skipping $bench)"
    fi
  done <<< "$BENCHMARKS"
}

HEAD_SHA="$(git rev-parse HEAD 2>/dev/null || true)"
HEAD_TSV="$(mktemp)"
log "Running suite at HEAD..."
collect "$HEAD_TSV"
if [ ! -s "$HEAD_TSV" ]; then
  log "No benchmark produced metrics at HEAD — the suite is broken, failing."
  emit false false "$HEAD_SHA" ""; exit 1
fi

BASE_SHA="${PERF_BASE_SHA:-}"
[ -z "$BASE_SHA" ] && BASE_SHA="$(git rev-list -1 --before="$BASELINE_AGO" "origin/${BASE_BRANCH}" 2>/dev/null || true)"

BASE_TSV=""
if [ -n "$BASE_SHA" ] && [ "$BASE_SHA" != "$HEAD_SHA" ]; then
  ORIG_REF="$(git symbolic-ref -q --short HEAD 2>/dev/null || git rev-parse HEAD)"
  if [ -n "$(git status --porcelain --untracked-files=no 2>/dev/null)" ]; then
    log "Working tree not clean — skipping baseline (non-blocking)."
  elif git checkout -q --detach "$BASE_SHA" 2>/dev/null; then
    # shellcheck disable=SC2064
    trap "git checkout -q '$ORIG_REF' >/dev/null 2>&1 || true" EXIT
    unset RAN_TARGETS; declare -A RAN_TARGETS
    log "Building baseline suite at ${BASE_SHA} (same workspace)..."
    BASE_TSV="$(mktemp)"; collect "$BASE_TSV"
    git checkout -q "$ORIG_REF" >/dev/null 2>&1 || true
    trap - EXIT
  else
    log "Could not check out baseline ${BASE_SHA} — skipping (non-blocking)."
  fi
fi

# Compare per benchmark; allocs strict, time loose; write the summary + verdict.
RES="$(mktemp)"; IMP="$(mktemp)"
python3 - "$HEAD_TSV" "${BASE_TSV:-}" "$ALLOC_THRESHOLD" "$TIME_THRESHOLD" "$HEAD_SHA" "${BASE_SHA:-}" "$SUMMARY" "$RES" "$IMP" <<'PY'
import sys
head_tsv, base_tsv, alloc_thr, time_thr, head_sha, base_sha, summary, res, imp = \
    sys.argv[1], sys.argv[2], float(sys.argv[3]), float(sys.argv[4]), sys.argv[5], sys.argv[6], sys.argv[7], sys.argv[8], sys.argv[9]
def load(p):
    out = {}
    if not p: return out
    for ln in open(p):
        parts = ln.rstrip("\n").split("|")
        if len(parts) == 4:
            n, t, a, b = parts
            out[n] = (float(t), (None if a=="NA" else float(a)), (None if b=="NA" else float(b)))
    return out
H, B = load(head_tsv), load(base_tsv)
regressed = False
improved = False
rows = []
for name in sorted(H):
    ht, ha, hb = H[name]
    if name in B and B[name][0]:
        bt, ba, bb = B[name]
        tr = ht/bt if bt else float("inf")
        # A 0-alloc baseline still needs a verdict: 0->N is an infinite regression
        # (must flag — several hot paths are 0-alloc), 0->0 is flat. A falsy 0.0
        # must not collapse to None and silently hide the regression.
        if ha is not None and ba is not None:
            ar = (ha / ba) if ba else (float("inf") if ha > 0 else 1.0)
        else:
            ar = None
        alloc_reg = ar is not None and ar > alloc_thr
        alloc_imp = ar is not None and ar < 1.0
        time_reg = tr > time_thr
        # Deterministic allocation deltas drive the file-an-issue verdicts: an increase
        # regresses, any decrease improves (allocs are exact, so a drop is a real win worth
        # locking in). Wall-time is advisory-only (noisy on shared runners) — table, never files.
        if alloc_reg: regressed = True
        if alloc_imp: improved = True
        flag = ("🟠 alloc regression" if alloc_reg
                else "🟢 alloc improvement" if alloc_imp
                else "🟡 time drift (advisory)" if time_reg
                else "✅")
        rows.append((name, ha, ba, ar, ht, bt, tr, flag))
    else:
        rows.append((name, ha, None, None, ht, None, None, "· no baseline"))
with open(summary, "a") as f:
    f.write("## Perf: Valdi microbenchmark suite\n\n")
    f.write(f"tip `{head_sha[:12]}` vs baseline `{(base_sha or '—')[:12]}` · allocs strict (>{alloc_thr}), time loose (>{time_thr}, de-bounced by workflow)\n\n")
    f.write("| benchmark | allocs/op tip | base | ratio | time tip (ns) | base | ratio | |\n|---|---|---|---|---|---|---|---|\n")
    for n, ha, ba, ar, ht, bt, tr, flag in rows:
        f.write("| {} | {} | {} | {} | {:.0f} | {} | {} | {} |\n".format(
            n,
            "—" if ha is None else f"{ha:.0f}",
            "—" if ba is None else f"{ba:.0f}",
            "—" if ar is None else f"{ar:.3f}",
            ht,
            "—" if bt is None else f"{bt:.0f}",
            "—" if tr is None else f"{tr:.3f}",
            flag))
    f.write("\nNon-blocking. 🟠 = deterministic allocation increase (review); 🟢 = allocation decrease (lock it in); 🟡 = time drift (advisory).\n")
open(res, "w").write("true" if regressed else "false")
open(imp, "w").write("true" if improved else "false")
PY
emit "$(cat "$RES")" "$(cat "$IMP")" "$HEAD_SHA" "${BASE_SHA:-}"
log "Done (non-blocking)."
exit 0
