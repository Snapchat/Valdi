#!/usr/bin/env bash

set -eux

(

# Intended to be run from open_source/
cd "$(dirname "$0")/../.."

bzl test //valdi:test_snap_drawing //valdi:test_hermes --test_output=errors
bzl test //valdi:test_layout --test_output=all --test_arg=--gtest_print_time=1

# test_svg passes but was not gated. Safe on Linux: it only deps :valdi_svg and
# //snap_drawing:test_utils, so it avoids the runtime link that keeps test_runtime macOS-only
# (see the block below). The full //valdi:test (incl. test_integration) runs on both platforms
# at the bottom of this script; the old "main thread dispatch" abort is gone (its trigger,
# canLockAllJSContexts, is DISABLED, so the suite runs green).
bzl test //valdi:test_svg --test_output=errors

# The hot-reload smoke runs as its own parallel job on external GitHub Actions
# (the hotreload-smoke matrix leg). Gate it here too — but ONLY internally (skip
# on GitHub Actions to avoid duplicating that job) and only on Linux (it needs
# watchman, installed by setup_linux_env.sh) — so internal cool Linux catches
# reloader / compiler-monitor breakage before it mirrors to the public repo.
#
# Force the from-source reloader (use_local_compiler=true). Copybara flips this
# on for the public mirror, so external CI already builds it from source; the
# internal monorepo defaults it off and would otherwise run the GCS-prebuilt
# reloader binary. That prebuilt (pinned, older) binary hangs on startup in
# --usb mode here — it prints "Started listening for ADB devices" and never
# reaches the initial compile — so the smoke times out waiting for the reloader.
# Building from source matches the external path that passes.
if [[ "${GITHUB_ACTIONS:-}" != "true" && "$(uname)" == "Linux" ]] ; then
    HOTRELOAD_BUILD_FLAGS=--//bzl/valdi:use_local_compiler=true ./tools/ci/hotreload_smoke.sh
fi

if [[ $(uname) != Linux ]] ; then
    bzl test //valdi:valdi_ios_objc_test --test_output=errors
    bzl test //valdi:valdi_ios_swift_test --test_output=errors
    bzl test //valdi:valdi_macos_objc_test --test_output=errors

    # C++ runtime unit tests (Value, ValueUtils, JavaScriptTypes, etc.), engine-agnostic.
    # External counterpart of the internal ValdiBazelTestStep addition (#115098).
    # macOS-only: on Linux `bzl test` trips a pre-existing static-destructor segfault in
    # gtest's XML writer when linked against valdi_standalone_runtime (all tests pass; only
    # teardown crashes under bzl test). Internal CI works around it by running the binary directly.
    bzl test //valdi:test_runtime --test_output=errors

    # The compiler's Swift unit suite runs as its own parallel job on external
    # GitHub Actions (compiler-tests.yml). Gate it here too — but ONLY internally
    # (skip on GitHub Actions to avoid duplicating that job) — so internal cool,
    # which runs this aggregate rather than the workflow, catches compiler
    # regressions before they mirror to the public repo.
    if [[ "${GITHUB_ACTIONS:-}" != "true" ]] ; then
        ./tools/ci/compiler_tests.sh
    fi
fi

# Full Valdi C++ suite: test_integration + runtime + snap_drawing + svg + hermes libs, run as one
# binary across all engines (QuickJS/QuickJSWithTSN/JSCore/Hermes) — ~1866 tests. This is the whole
# //valdi:test target the internal suite runs, now gated externally too. The old "main thread must
# never dispatch synchronously into a worker runtime" abort no longer fires (its only trigger,
# canLockAllJSContexts, is DISABLED), so the suite is green on macOS.
#
# Raise the console stdout/stderr cap: //valdi:test is a single binary and its whole-suite output
# (~1.1MB) exceeds bazel's 1MB default (--experimental_ui_max_stdouterr_bytes), so on failure bazel
# SKIPS the output entirely and the failing test is invisible in CI logs. Bump to 32MB so failures
# actually surface.
bzl test //valdi:test --test_output=errors --experimental_ui_max_stdouterr_bytes=33554432

)
