#!/usr/bin/env bash
#
# Deliver the perf-ratio verdict. Shared by the external nightly workflow
# (.github/workflows/perf-nightly.yml) and the internal SnapCI nightly wrapper,
# so the notification logic lives in a script both sides run, not in YAML.
#
# Verdict comes from perf_ratio.sh; the caller passes it via env:
#   PERF_REGRESSED / PERF_IMPROVED / PERF_BUILDFAIL = "true"/"false"
#   PERF_HEAD_SHA / PERF_BASE_SHA                   = commit SHAs
#   PERF_RUN_URL                                    = link to the run/log with the table
#
# External (GitHub Actions, `gh` + GH_TOKEN present): files/comments a rolling
#   `perf-regression` and/or `perf-improvement` issue. Non-blocking (exits 0).
# Internal (not GitHub Actions): there is no issue tracker to file into, so it
#   prints the verdict and exits non-zero on any deterministic change (or a
#   benchmark that stopped building) to trip the SnapCI job, whose schedule's
#   Slack notify then fires. The per-benchmark table is in the job log.
set -uo pipefail

regressed="${PERF_REGRESSED:-false}"
improved="${PERF_IMPROVED:-false}"
buildfail="${PERF_BUILDFAIL:-false}"
head_sha="${PERF_HEAD_SHA:-}"
base_sha="${PERF_BASE_SHA:-}"
run_url="${PERF_RUN_URL:-}"

compare_url=""
if [ -n "$base_sha" ] && [ -n "$head_sha" ] && [ -n "${GITHUB_REPOSITORY:-}" ]; then
  compare_url="https://github.com/${GITHUB_REPOSITORY}/compare/${base_sha}...${head_sha}"
fi

# label title lead footer
file_issue() {
  local label="$1" title="$2" lead="$3" footer="$4" existing body
  body="$(printf '%s\nPer-benchmark table (allocations + time) in the run summary: %s\n\n| | commit |\n|---|---|\n| tip | `%s` |\n| baseline (~24h earlier) | `%s` |\n\n%s\n\n_%s_\n' \
    "$lead" "$run_url" "$head_sha" "$base_sha" "$compare_url" "$footer")"
  # `// empty` so an empty result list yields no output, not the string "null"
  # (jq prints null as "null", which would make the -n check below pass).
  existing="$(gh issue list --state open --label "$label" --json number --jq '.[0].number // empty' 2>/dev/null || true)"
  if [ -n "$existing" ]; then
    gh issue comment "$existing" --body "$body" >/dev/null && echo "[perf-notify] commented on #$existing ($label)"
  else
    gh issue create --title "$title" --body "$body" --label "$label" >/dev/null && echo "[perf-notify] opened $label issue"
  fi
}

if [ "${GITHUB_ACTIONS:-}" = "true" ] && command -v gh >/dev/null 2>&1; then
  if [ "$regressed" = "true" ]; then
    file_issue "perf-regression" "Perf regression: Valdi microbenchmark suite" \
      "The nightly Valdi microbenchmark suite flagged a **deterministic allocation regression** in a frequently-used code path." \
      "Filed automatically (non-blocking). Allocation counts are deterministic, so this is not runner noise; wall-time drift is advisory-only and not filed."
  fi
  if [ "$improved" = "true" ]; then
    file_issue "perf-improvement" "Perf improvement: Valdi microbenchmark suite" \
      "The nightly Valdi microbenchmark suite recorded a **deterministic allocation improvement** in a frequently-used code path." \
      "Filed automatically (non-blocking). Consider refreshing the documented allocs/op figures for the affected benchmark."
  fi
  exit 0
fi

# Internal / non-GitHub: trip the job so the SnapCI Slack notify fires.
if [ "$regressed" = "true" ] || [ "$improved" = "true" ] || [ "$buildfail" = "true" ]; then
  echo "[perf-notify] deterministic perf change or benchmark build failure" \
       "(regressed=$regressed improved=$improved buildfail=$buildfail) -> alerting Slack"
  exit 1
fi
echo "[perf-notify] no deterministic allocation change; all benchmarks built."
exit 0
