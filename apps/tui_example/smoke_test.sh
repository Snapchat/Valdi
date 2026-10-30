#!/usr/bin/env bash
# Renders the example once with stdout piped (non-TTY mode) and checks the frame.
set -euo pipefail

binary="$1"
output="$("$binary" --once </dev/null)"
echo "$output"

check() {
  if ! grep -qF -- "$1" <<<"$output"; then
    echo "FAIL: expected output to contain: $1" >&2
    exit 1
  fi
}

check "Valdi TUI"
check "┌"
check "Tasks"
check "Build runtime"
check "ready."
