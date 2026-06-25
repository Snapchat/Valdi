#!/usr/bin/env bash
set -e

# Run Valdi API surface compatibility check
# Intended to be run from open_source/

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

cd "$ROOT_DIR"

echo "=== Valdi API Surface Check ==="
echo "Comparing current API against baseline..."
echo

python3 tools/api_surface/check_api_surface.py .
