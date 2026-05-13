#!/usr/bin/env bash
# Build the Valdi compiler, package it as macos.tar.gz, and point the repo at it
# so Bazel uses your local compiler instead of the prebuilt one.
#
# Run from the client repo root (the directory containing src/open_source).
# After running:
#   - Clear Bazel cache if you had a previous compiler: bzl clean --expunge (or rm -rf ~/.cache/bazel/.../external/valdi_compiler_macos)
#   - Run tests (see below)
#
# Usage: ./scripts/use_local_compiler.sh
#   or:  CLIENT_ROOT=/path/to/client ./scripts/use_local_compiler.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPILER_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
# Client root: directory that contains src/open_source (compiler/compiler -> open_source -> src -> client)
CLIENT_ROOT="${CLIENT_ROOT:-$(cd "$COMPILER_DIR/../../../.." && pwd)}"
OPEN_SOURCE="$CLIENT_ROOT/src/open_source"
BIN_DIR="$COMPILER_DIR/bin"
ARCHIVE="$BIN_DIR/macos.tar.gz"
ARCHIVES_BZL="$OPEN_SOURCE/bzl/open_source_archives.bzl"

echo "Compiler dir: $COMPILER_DIR"
echo "Client root:  $CLIENT_ROOT"
echo "Archives:     $ARCHIVES_BZL"

# 1) Build compiler (output goes to $CLIENT_ROOT/bin/macos/macos/valdi_compiler)
echo "Building compiler..."
"$SCRIPT_DIR/update_compiler.sh" -o bin/macos

# 2) Package: copy binary into compiler bin and create tar.gz
echo "Packaging..."
mkdir -p "$BIN_DIR"
cp "$CLIENT_ROOT/bin/macos/macos/valdi_compiler" "$BIN_DIR/valdi_compiler"
rm -f "$ARCHIVE"
tar -czf "$ARCHIVE" -C "$BIN_DIR" valdi_compiler

# 3) Hash
HASH=$(shasum -a 256 "$ARCHIVE" | awk '{print $1}')
echo "New hash: $HASH"

# 4) Point archives at local file (absolute file:// URL)
LOCAL_URL="file://$ARCHIVE"

# 5) Update open_source_archives.bzl for valdi_compiler_macos (hash + url)
UPDATED=0
if [[ -f "$ARCHIVES_BZL" ]]; then
  awk -v hash="$HASH" -v url="$LOCAL_URL" '
    /"src\/open_source\/bin\/compiler\/macos":/ { in_block=1 }
    in_block && /"hash":/ { sub(/"hash": "[^"]+"/, "\"hash\": \"" hash "\"") }
    in_block && /"url":/  { gsub(/"url": "[^"]+"/, "\"url\": \"" url "\"") }
    in_block && /},/      { in_block=0 }
    { print }
  ' "$ARCHIVES_BZL" > "$ARCHIVES_BZL.tmp" && mv "$ARCHIVES_BZL.tmp" "$ARCHIVES_BZL" && UPDATED=1
fi

if [[ "$UPDATED" -eq 0 ]]; then
  echo "Update $ARCHIVES_BZL manually: set hash to $HASH and url to $LOCAL_URL for valdi_compiler_macos."
fi

echo ""
echo "Done. Next steps:"
echo "  Clear Bazel cache (so it re-fetches the compiler):"
echo "     rm -rf ~/.cache/bazel/*/external/valdi_compiler_macos"
echo "     # or from repo root: bzl clean --expunge"
