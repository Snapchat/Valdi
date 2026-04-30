#!/usr/bin/env bash
# Auto-snapshot-and-diff loop: watches for hot-reload completions and
# automatically captures a snapshot + generates a pixel diff against a mock.
#
# This script monitors /tmp/hotreload.log for "Recompilation pass finished"
# lines, waits for the app to re-render, then runs snapshot + diff. It writes
# the latest result to /tmp/watch-diff-latest.txt so an AI agent can poll it.
#
# The script does NOT kill any running processes -- it only observes and snapshots.
#
# Usage:
#   # Watch and auto-diff after each hot-reload
#   bash watch-and-diff.sh /tmp/mock.png --key CommunitiesFeed --port 13592
#
#   # With context for in-app Playground
#   bash watch-and-diff.sh /tmp/mock.png --key CommunitiesFeed --port 13592 --context "0 8"
#
# Arguments:
#   $1         - Mock image path (required)
#   --key      - Component key for snapshot (default: "PreviewRoot")
#   --port     - App port (default: 13591)
#   --context  - Context args for in-app snapshots, e.g. "0 8"
#   --output   - Diff output path (default: /tmp/mock-vs-render-diff.png)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# --- Argument parsing ---
MOCK_PATH="${1:?Usage: watch-and-diff.sh <mock_path> [--key KEY] [--port PORT] [--context \"IDX IDX\"] [--output PATH]}"
shift

KEY="PreviewRoot"
PORT="13591"
CONTEXT_ARGS=""
OUTPUT="/tmp/mock-vs-render-diff.png"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --key) KEY="$2"; shift 2 ;;
    --port) PORT="$2"; shift 2 ;;
    --context) CONTEXT_ARGS="$2"; shift 2 ;;
    --output) OUTPUT="$2"; shift 2 ;;
    *) echo "Unknown arg: $1" >&2; exit 1 ;;
  esac
done

# Validate mock image exists
if [[ ! -f "$MOCK_PATH" ]]; then
  echo "ERROR: Mock image not found: $MOCK_PATH" >&2
  exit 1
fi

HOTRELOAD_LOG="/tmp/hotreload.log"
LATEST_FILE="/tmp/watch-diff-latest.txt"

# We use a FIFO so tail runs as a separate process we can kill on cleanup,
# while the read loop runs in the main shell (preserving function access).
FIFO="/tmp/watch-diff-fifo.$$"
mkfifo "$FIFO"

TAIL_PID=""

# --- Graceful shutdown ---
cleanup() {
  echo ""
  echo "[$(date '+%H:%M:%S')] Stopping watch-and-diff..."
  if [[ -n "$TAIL_PID" ]] && kill -0 "$TAIL_PID" 2>/dev/null; then
    kill "$TAIL_PID" 2>/dev/null || true
    wait "$TAIL_PID" 2>/dev/null || true
  fi
  rm -f "$FIFO"
  echo "[$(date '+%H:%M:%S')] Stopped."
  exit 0
}
trap cleanup SIGINT SIGTERM EXIT

# --- Resolve snapshot command ---
SNAPSHOT_CMD=""
if command -v valdi-sc &>/dev/null; then
  SNAPSHOT_CMD="valdi-sc inspect snapshot"
elif command -v valdi &>/dev/null; then
  SNAPSHOT_CMD="valdi inspect snapshot"
else
  echo "ERROR: Neither valdi-sc nor valdi found in PATH" >&2
  exit 1
fi

# --- Snapshot + diff function ---
do_snapshot_and_diff() {
  local ts
  ts="$(date '+%Y-%m-%d %H:%M:%S')"
  echo ""
  echo "[$ts] Hot-reload detected. Waiting 4s for re-render..."
  sleep 4

  echo "[$ts] Capturing snapshot (key=$KEY, port=$PORT)..."

  local snapshot_output snapshot_path
  # shellcheck disable=SC2086
  snapshot_output=$($SNAPSHOT_CMD $CONTEXT_ARGS --key "$KEY" --port "$PORT" 2>&1) || true
  snapshot_path=$(echo "$snapshot_output" | grep -o '"path":"[^"]*"' | head -1 | sed 's/"path":"//;s/"//')

  if [[ -z "$snapshot_path" || ! -f "$snapshot_path" ]]; then
    echo "[$ts] ERROR: Could not capture snapshot. Output:" >&2
    echo "$snapshot_output" >&2
    {
      echo "timestamp=$ts"
      echo "status=error"
      echo "error=snapshot_failed"
    } > "$LATEST_FILE"
    return 1
  fi

  echo "[$ts] Snapshot: $snapshot_path"
  echo "[$ts] Generating diff..."

  local diff_output match_pct
  diff_output=$(python3 "$SCRIPT_DIR/diff.py" "$MOCK_PATH" "$snapshot_path" --output "$OUTPUT" 2>&1) || true
  echo "$diff_output"

  # Extract match percentage from diff.py output
  match_pct=$(echo "$diff_output" | grep -o 'Match: [0-9.]*%' | head -1 | grep -o '[0-9.]*')
  if [[ -z "$match_pct" ]]; then
    match_pct="unknown"
  fi

  # Write result to latest file for AI agent polling
  {
    echo "timestamp=$ts"
    echo "status=ok"
    echo "match_pct=$match_pct"
    echo "snapshot=$snapshot_path"
    echo "diff=$OUTPUT"
    echo "mock=$MOCK_PATH"
  } > "$LATEST_FILE"

  echo ""
  echo "[$ts] Result: ${match_pct}% match"
  echo "[$ts] Diff image: $OUTPUT"
  echo "[$ts] Latest result written to: $LATEST_FILE"
  echo "---"
}

# --- Main loop ---
echo "============================================"
echo "  watch-and-diff"
echo "============================================"
echo "Mock:    $MOCK_PATH"
echo "Key:     $KEY"
echo "Port:    $PORT"
echo "Context: ${CONTEXT_ARGS:-<none>}"
echo "Output:  $OUTPUT"
echo "Log:     $HOTRELOAD_LOG"
echo "Latest:  $LATEST_FILE"
echo "============================================"
echo ""

# Ensure the hotreload log exists
if [[ ! -f "$HOTRELOAD_LOG" ]]; then
  echo "Waiting for $HOTRELOAD_LOG to appear..."
  while [[ ! -f "$HOTRELOAD_LOG" ]]; do
    sleep 1
  done
  echo "Log file appeared."
fi

echo "[$(date '+%H:%M:%S')] Monitoring for hot-reload completions (Ctrl+C to stop)..."
echo ""

# Start tail writing to the FIFO in the background.
# -n 0 means start from the current end of the file (ignore old lines).
tail -n 0 -f "$HOTRELOAD_LOG" > "$FIFO" &
TAIL_PID=$!

# Read from the FIFO in the main shell so functions and variables are accessible.
while IFS= read -r line; do
  if [[ "$line" == *"Recompilation pass finished"* ]]; then
    do_snapshot_and_diff || true
  fi
done < "$FIFO"
