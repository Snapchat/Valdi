#!/usr/bin/env bash
# Capture a Valdi snapshot and generate a pixel diff against a mock.
#
# Usage:
#   snapshot-and-diff.sh <mock_path> [--key <element_key>] [--port <port>] [--context <device_idx> <context_idx>]
#
# Outputs:
#   - Snapshot PNG (path printed to stdout)
#   - Diff PNG at /tmp/mock-vs-render-diff.png
#   - Match percentage printed to stdout

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MOCK_PATH="${1:?Usage: snapshot-and-diff.sh <mock_path> [--key KEY] [--port PORT]}"
shift

KEY="ProfileStoriesPage"
PORT="13591"
CONTEXT_ARGS=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --key) KEY="$2"; shift 2 ;;
    --port) PORT="$2"; shift 2 ;;
    --context) CONTEXT_ARGS="$2 $3"; shift 3 ;;
    *) echo "Unknown arg: $1" >&2; exit 1 ;;
  esac
done

echo "Capturing snapshot (key=$KEY, port=$PORT, context=$CONTEXT_ARGS)..."
# Parse the JSON output to get the actual snapshot path
SNAPSHOT_CMD="valdi"
command -v valdi-sc &>/dev/null && SNAPSHOT_CMD="valdi-sc"
# shellcheck disable=SC2086
SNAPSHOT_OUTPUT=$($SNAPSHOT_CMD inspect snapshot $CONTEXT_ARGS --key "$KEY" --port "$PORT" 2>&1) || true
SNAPSHOT_PATH=$(echo "$SNAPSHOT_OUTPUT" | grep -o '"path":"[^"]*"' | head -1 | sed 's/"path":"//;s/"//')

if [[ -z "$SNAPSHOT_PATH" || ! -f "$SNAPSHOT_PATH" ]]; then
  echo "ERROR: Could not capture snapshot. Output was:" >&2
  echo "$SNAPSHOT_OUTPUT" >&2
  exit 1
fi

echo "Snapshot: $SNAPSHOT_PATH"
echo "Generating diff..."
python3 "$SCRIPT_DIR/diff.py" "$MOCK_PATH" "$SNAPSHOT_PATH"

echo ""
echo "Diff: /tmp/mock-vs-render-diff.png"

# Open the diff image in a window for visual inspection
open /tmp/mock-vs-render-diff.png
