#!/usr/bin/env bash
set -e

# Fail when a native element attribute declared in TypeScript (NativeTemplateElements.d.ts)
# has no iOS or no Android implementation. Static source scan: no Bazel build needed.
# Intended to be run from open_source/

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

cd "$ROOT_DIR"

echo "=== Native implementation check: unit tests ==="
python3 -m unittest discover -s tools/ci/native_impl_check -p 'test_*.py'
echo

echo "=== Native implementation check ==="
python3 tools/ci/native_impl_check/check_native_impl.py .
