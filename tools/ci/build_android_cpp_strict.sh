#!/usr/bin/env bash
#
# Compile Valdi's Android C++ with --config=android_strict_cxx (see .bazelrc): NDK API 21 and
# clang's default-on warnings under -Werror, the flags Valdi's C++ sees when a larger Android app
# vendors it. The regular Android build (build_core_targets.sh) uses the NDK toolchain's defaults,
# which target API 31 and silence C++20 deprecations and several default-on warnings, so code can
# pass it and still fail to compile downstream. Build-only, arm64-only.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/../.."   # -> open_source workspace root

if [ -n "${BAZEL:-}" ]; then
  :
elif command -v bzl >/dev/null 2>&1; then
  BAZEL=bzl
else
  BAZEL=bazel
fi

# The native libraries an Android app links: the Valdi runtime (which pulls in valdi_core,
# snap_drawing, yoga, djinni support and the JS engines for Android) plus valdi_core directly.
TARGETS=(
  //valdi:valdi
  //valdi_core:valdi_core
)

"$BAZEL" build --config=android_strict_cxx "$@" "${TARGETS[@]}"
echo "[build_android_cpp_strict] Android C++ compiled cleanly with the strict flag set."
