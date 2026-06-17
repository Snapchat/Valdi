#!/usr/bin/env bash

# Builds the Valdi compiler with Bazel
# (//compiler/compiler:local_valdi_compiler swift_binary) instead of
# invoking the Swift toolchain directly. Output lands at
# <bin_output_path>/{linux,macos}/valdi_compiler (-o interface).
#
# Linux: single build; the target statically links the Swift runtime with
# hermetic deps (see compiler/compiler/BUILD.bazel), so the host needs no
# Swift install and the binary runs on hosts without one.
#
# macOS: Bazel produces one arch per invocation, so this builds arm64 and
# x86_64 separately, combines them with lipo into a universal binary, and
# codesigns it with the entitlements. Note: this does not run tests or
# produce a dSYM for Sentry.

set -e
set -x

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
ENTITLEMENTS_PATH="$SCRIPT_DIR/entitlements.plist"

# Workspace root and target label depend on which repo we're in:
# - Mobile repo: SCRIPT_DIR contains "open_source", workspace is client/,
#   and the compiler package is addressed through the @valdi repo.
# - Public/mirrored repo (post-Copybara): workspace is the repo root.
if [[ "$SCRIPT_DIR" == *"open_source"* ]]; then
    WORKSPACE_ROOT="$(cd "$SCRIPT_DIR/../../../../../" && pwd)"
    TARGET="@valdi//compiler/compiler:local_valdi_compiler"
    PLATFORMS_PKG="@valdi//bzl/platforms/os"
else
    WORKSPACE_ROOT="$(cd "$SCRIPT_DIR/../../../" && pwd)"
    TARGET="//compiler/compiler:local_valdi_compiler"
    PLATFORMS_PKG="//bzl/platforms/os"
fi

bin_output_path=""

usage() {
  echo "Usage: $0 [-o bin_output_path] [-s]"
  exit 1
}

# -s (skip analytics) is accepted for CLI compatibility but is a no-op:
# the Bazel build never uploads analytics.
while getopts ":o:s" opt; do
  case "$opt" in
    s)
      ;;
    o)
      bin_output_path=$OPTARG
      ;;
    \? )
      echo "Invalid option: $OPTARG" 1>&2
      usage
      ;;
    : )
      echo "Invalid option: $OPTARG requires an argument" 1>&2
      usage
      ;;
  esac
done

shift $((OPTIND -1))

if [ -z "$bin_output_path" ] && [ $# -ge 1 ]; then
  bin_output_path=$1
  shift
fi

if [ -z "$bin_output_path" ]; then
  usage
fi

# Resolve bin_output_path relative to WORKSPACE_ROOT (not the caller's CWD).
if [[ "$bin_output_path" != /* ]]; then
  bin_output_path="$WORKSPACE_ROOT/$bin_output_path"
fi

# Prefer the bzl wrapper (mobile repo convention); fall back to bazel for
# the public repo.
BAZEL_BIN="$(command -v bzl || command -v bazel)"

cd "$WORKSPACE_ROOT"

# Builds the target with the given extra flags and echoes the path of the
# produced binary (relative to the workspace, through the bazel-out
# convenience symlink). The grep guards against wrapper banners on stdout.
build_compiler() {
    "$BAZEL_BIN" build -c opt "$@" "$TARGET" 1>&2
    "$BAZEL_BIN" cquery -c opt "$@" "$TARGET" --output=files 2>/dev/null \
        | grep -E 'local_valdi_compiler$' | tail -1
}

if [[ "$(uname)" == "Linux" ]]; then
    OUT_DIR="$bin_output_path/linux"
    OUTPUT_FILE_PATH="$(build_compiler)"

    if [[ -z "$OUTPUT_FILE_PATH" || ! -f "$OUTPUT_FILE_PATH" ]]; then
        echo "Could not locate built compiler binary for $TARGET" 1>&2
        exit 1
    fi

    mkdir -p "$OUT_DIR"
    rm -f "$OUT_DIR/valdi_compiler"
    cp "$OUTPUT_FILE_PATH" "$OUT_DIR/valdi_compiler"
    chmod +w "$OUT_DIR/valdi_compiler"
else
    OUT_DIR="$bin_output_path/macos"

    TMP_DIR="$(mktemp -d)"
    trap 'rm -rf "$TMP_DIR"' EXIT

    # Builds the target for one arch and stashes the thin binary in
    # TMP_DIR. The copy must happen before the next arch builds: both
    # configurations write to the same bazel-out path (--platforms
    # retargets the toolchain but does not change the output directory
    # name), so the second build overwrites the first.
    #
    # --platforms, not legacy --cpu: --cpu renames the output directory
    # without retargeting the toolchain, producing two host-arch binaries.
    # Deployment target matches Package.swift's .macOS(.v11): without it the
    # build inherits the SDK's own version as the minimum, producing a binary
    # that refuses to run on older hosts and hard-links Swift runtime dylibs
    # (e.g. libswift_DarwinFoundation1.dylib) that only ship with that OS.
    MACOS_MIN_OS="11.0"

    build_arch() {
        local arch="$1"
        local built
        built="$(build_compiler --platforms="$PLATFORMS_PKG:macos_$arch" --macos_minimum_os="$MACOS_MIN_OS")"
        if [[ -z "$built" || ! -f "$built" ]]; then
            echo "Could not locate built $arch compiler binary for $TARGET" 1>&2
            return 1
        fi
        cp "$built" "$TMP_DIR/valdi_compiler.$arch"
        local archs
        archs="$(lipo -archs "$TMP_DIR/valdi_compiler.$arch")"
        if [[ "$archs" != "$arch" ]]; then
            echo "Expected the $arch build to produce a $arch binary, got: $archs" 1>&2
            return 1
        fi
    }

    build_arch arm64
    build_arch x86_64

    mkdir -p "$OUT_DIR"
    rm -f "$OUT_DIR/valdi_compiler"
    lipo -create "$TMP_DIR/valdi_compiler.arm64" "$TMP_DIR/valdi_compiler.x86_64" \
        -output "$OUT_DIR/valdi_compiler"
    chmod +wx "$OUT_DIR/valdi_compiler"
    codesign --force --sign - --entitlements "$ENTITLEMENTS_PATH" "$OUT_DIR/valdi_compiler"
fi

echo "Valdi compiler built and copied to $OUT_DIR/valdi_compiler"
