#!/usr/bin/env bash
#
# validate_swift_toolchain.sh — Verify the hermetic Swift toolchain is properly
# set up for the external (public) repo.
#
# The open_source tree has its own Bazel workspace. When Copybara mirrors it to
# github.com/Snapchat/Valdi, the workspace root becomes the repo root. The
# hermetic Swift toolchain must live inside this tree so it reaches the public
# repo and Linux CI can build the Valdi compiler from source.
#
# Called by: client_validate_valdi_open_source.sh (SnapCI)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OPEN_SOURCE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

FAILURES=0
fail() {
    echo "FAIL: $1"
    FAILURES=$((FAILURES + 1))
}
pass() { echo "PASS: $1"; }

echo "=== Validating Swift Toolchain Setup ==="

pushd "$OPEN_SOURCE_DIR" > /dev/null

# ---------------------------------------------------------------------------
# 1. Toolchain resolves for the compiler target
#
# The compiler (swift_binary) needs a Swift toolchain and is
# target_compatible_with @platforms//os:linux, so it is only meaningfully
# analyzable on Linux (where our hermetic toolchain repo must be registered).
#
# `build --nobuild` runs the same loading/analysis and Swift toolchain
# resolution as the real build below, without executing actions. A bare
# `cquery ... 2>/dev/null` was unsound: on Linux it false-negatived — reporting
# broken resolution while the actual build of the same target succeeded — and
# on macOS it never exercised the compiler. Skip on non-Linux, where the target
# is incompatible (matching the build check below).
# ---------------------------------------------------------------------------
echo ""
echo "--- Compiler target resolves with use_local_compiler=true ---"

if [[ "$(uname)" == "Linux" ]]; then
    if bzl build --nobuild //compiler/compiler:local_valdi_compiler \
        --//bzl/valdi:use_local_compiler=true; then
        pass "local_valdi_compiler analyzable"
    else
        fail "local_valdi_compiler CANNOT be analyzed with use_local_compiler=true — Swift toolchain resolution is broken"
    fi
else
    pass "Skipping compiler analysis (compiler is Linux-only; running on $(uname))"
fi

# ---------------------------------------------------------------------------
# 2. C++ targets analyzable under use_local_compiler=true
#
# Even pure C++ targets go through the valdi_toolchain, which references the
# compiler as an attribute. If the Swift toolchain can't resolve, the entire
# toolchain fails to load and ALL targets break — not just Swift ones.
# ---------------------------------------------------------------------------
echo ""
echo "--- C++ test targets analyzable with use_local_compiler=true ---"

CPP_TARGETS="//valdi:test_snap_drawing //valdi:test_hermes"
if bzl cquery "set($CPP_TARGETS)" \
    --//bzl/valdi:use_local_compiler=true \
    --output=label 2>/dev/null; then
    pass "C++ test targets analyzable with use_local_compiler=true"
else
    fail "C++ test targets CANNOT be analyzed — valdi_toolchain registration is broken under use_local_compiler=true"
fi

# ---------------------------------------------------------------------------
# 3. Full compiler chain: valdi_module targets
#
# Targets built with valdi_module() invoke the compiler at build time. This
# validates the complete chain: toolchain resolution → compiler analysis →
# module compilation (at analysis time, Bazel verifies the action graph).
# ---------------------------------------------------------------------------
echo ""
echo "--- valdi_module targets resolve (full compiler chain) ---"

if bzl cquery //valdi:test_layout \
    --//bzl/valdi:use_local_compiler=true \
    --output=label 2>/dev/null; then
    pass "test_layout (uses valdi_module → compiler) analyzable"
else
    fail "test_layout CANNOT be analyzed — compiler toolchain chain is broken"
fi

# ---------------------------------------------------------------------------
# 4. Swift toolchain repo is fetchable
#
# Verify the module extension creates the expected external repo. This catches
# issues like a missing bzlmod extension file or broken MODULE.bazel wiring
# before we hit a build timeout.
# ---------------------------------------------------------------------------
echo ""
echo "--- Swift toolchain repo exists ---"

if bzl cquery @swift_toolchain_linux_x86_64//:rules_swift_toolchain \
    --output=label 2>/dev/null; then
    pass "swift_toolchain_linux_x86_64 repo resolved and toolchain target exists"
else
    fail "swift_toolchain_linux_x86_64 repo could not be resolved — MODULE.bazel extension wiring is broken"
fi

# ---------------------------------------------------------------------------
# 5. Actually build the compiler on Linux
#
# On Linux, the hermetic Swift toolchain downloads a pinned Swift archive
# and builds the compiler from source. This is the definitive test: if the
# compiler binary builds, the toolchain works. Skipped on macOS where
# rules_swift's xcode-based autoconfig always works.
# ---------------------------------------------------------------------------
echo ""
echo "--- Build compiler on Linux ---"

if [[ "$(uname)" == "Linux" ]]; then
    if bzl build //compiler/compiler:local_valdi_compiler \
        --//bzl/valdi:use_local_compiler=true 2>&1; then
        pass "local_valdi_compiler built successfully on Linux"
    else
        fail "local_valdi_compiler FAILED to build on Linux — hermetic Swift toolchain is broken"
    fi
else
    pass "Skipping Linux compiler build (running on $(uname))"
fi

# ---------------------------------------------------------------------------
# 6. Internal/external parity (monorepo only)
#
# The internal repo has its own copy at client/bzl/swift_toolchains/. If
# someone updates one without the other, Linux CI breaks on the public repo.
# ---------------------------------------------------------------------------
echo ""
echo "--- Internal/external parity ---"

INTERNAL_TOOLCHAINS="$OPEN_SOURCE_DIR/../../bzl/swift_toolchains.bzl"
EXTERNAL_TOOLCHAINS="$OPEN_SOURCE_DIR/bzl/swift_toolchains.bzl"

if [[ -f "$INTERNAL_TOOLCHAINS" ]]; then
    if diff -q "$INTERNAL_TOOLCHAINS" "$EXTERNAL_TOOLCHAINS" > /dev/null 2>&1; then
        pass "swift_toolchains.bzl in sync with internal copy"
    else
        fail "swift_toolchains.bzl differs from internal copy — toolchain version/URL drift will break public CI"
    fi
else
    pass "No internal copy found (running outside monorepo, skipping parity check)"
fi

popd > /dev/null

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
echo ""
echo "=== Swift toolchain validation complete ==="
if [[ "$FAILURES" -gt 0 ]]; then
    echo "RESULT: $FAILURES check(s) failed"
    exit 1
else
    echo "RESULT: All checks passed"
fi
