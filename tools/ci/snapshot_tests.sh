#!/usr/bin/env bash
set -euo pipefail

# This script calls `bazel` directly. Some environments expose only the `bzl` wrapper and no bare
# `bazel`; fall back to it (mirror of the shim in bazel_build.sh, which goes the other direction).
if ! command -v bazel > /dev/null 2>&1 && command -v bzl > /dev/null 2>&1; then
    bazel() { bzl "$@"; }
    export -f bazel
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TESTDATA_DIR="$REPO_ROOT/apps/snapshot_tests/testdata"
OUTPUT_DIR="${1:-/tmp/valdi-snapshots}"
OPEN_DIFFS="${2:-}"

BASELINE_STAGING="$OUTPUT_DIR/baseline"
ACTUAL_RENDERS="$OUTPUT_DIR/actual"
DIFF_OUTPUT="$OUTPUT_DIR/diffs"
mkdir -p "$BASELINE_STAGING" "$ACTUAL_RENDERS" "$DIFF_OUTPUT"

CLI_TARGET="//apps/snapshot_tests:snapshot_tests_cli"

echo "=== Valdi Snapshot Tests ==="
echo "Testdata: $TESTDATA_DIR"
echo "Output:   $OUTPUT_DIR"
echo ""

# Symlink {Name}_{hash}.png baselines into staging dir as {Name}.png
BASELINE_COUNT=0
for f in "$TESTDATA_DIR"/*.png; do
  [ -f "$f" ] || continue
  BASENAME=$(basename "$f" .png)
  # Strip the trailing _<hash> to get the test name
  NAME="${BASENAME%_*}"
  ln -sf "$f" "$BASELINE_STAGING/${NAME}.png"
  BASELINE_COUNT=$((BASELINE_COUNT + 1))
done

if [ "$BASELINE_COUNT" -eq 0 ]; then
  echo "No baseline images in testdata/ — rendering HEAD only."
  echo ""

  cd "$REPO_ROOT"
  bazel build "$CLI_TARGET"
  CLI_BIN="$REPO_ROOT/bazel-bin/apps/snapshot_tests/snapshot_tests_cli"
  "$CLI_BIN" render --output-dir "$ACTUAL_RENDERS"

  echo ""
  echo "Initial render complete. Run rebase to populate testdata/."
  exit 0
fi

echo "Found $BASELINE_COUNT baseline image(s)"
echo ""

# 1. Build + render at HEAD
echo "--- Step 1: Render at HEAD ---"
cd "$REPO_ROOT"
bazel build "$CLI_TARGET"
CLI_BIN="$REPO_ROOT/bazel-bin/apps/snapshot_tests/snapshot_tests_cli"
"$CLI_BIN" render --output-dir "$ACTUAL_RENDERS"
echo ""

# 2. Compare against checked-in baselines
echo "--- Step 2: Compare ---"
COMPARE_EXIT=0
"$CLI_BIN" compare --baseline-dir "$BASELINE_STAGING" --actual-dir "$ACTUAL_RENDERS" --output-dir "$DIFF_OUTPUT" --tolerance 8 || COMPARE_EXIT=$?

# Auto-open diff images on macOS if requested
if [ "$OPEN_DIFFS" = "--open" ] && [ -d "$DIFF_OUTPUT" ]; then
  for f in "$DIFF_OUTPUT"/*_diff.png; do
    [ -f "$f" ] && open "$f"
  done
fi

exit $COMPARE_EXIT
