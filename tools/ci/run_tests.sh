#!/usr/bin/env bash

set -eux

(

# Intended to be run from open_source/
cd "$(dirname "$0")/../.."

# Every test target in the repo except valdi_test modules (test_modules.sh runs those), minus
# `manual` tests and tools/ci/test_skiplist.txt; see select_tests.sh. Running a query instead of a
# hand-written list means a new test target is gated by both CIs (this script runs on public GitHub
# Actions and in internal CI) without anyone wiring it in.
source tools/ci/select_tests.sh
select_tests cpp
# --keep_going reports every failing target in one run. --skip_incompatible_explicit_targets skips
# targets that can't build on this platform (e.g. Linux-only or iOS-platform tests on macOS), the
# way `bzl test //...` would, instead of failing because they were named explicitly. Raise the
# console stdout/stderr cap: the //valdi:test binary's output (~1.1MB) exceeds bazel's 1MB default,
# and bazel then hides it. --valdi_js_engine=auto overrides the workspace's Hermes default so tests
# pick the engine the way release builds do (JavaScriptCore on iOS, QuickJS on Android). Hermes is
# only compiled into debug builds, so tests that cover it select it explicitly.
TEST_ARGS=(--keep_going --skip_incompatible_explicit_targets --test_output=errors
    --experimental_ui_max_stdouterr_bytes=33554432 --valdi_js_engine=auto)
if [[ "${GITHUB_ACTIONS:-}" == "true" ]] ; then
    # Quarantines the worker tests that leave a worker running past host teardown (see
    # orphanedWorkerTestsQuarantined in Runtime_tests.cpp). Remove when VALDI_ENABLE_TEARDOWN_STOP_ORPHANED_WORKERS
    # defaults on.
    TEST_ARGS+=(--test_env=VALDI_QUARANTINE_ORPHANED_WORKER_TESTS=1)
fi
# iOS test bundles all launch into one shared simulator. Started together, they hang in their first
# test until the 300s timeout, so run them one at a time after everything else.
IOS_TEST_TARGETS=()
while IFS= read -r t ; do
    [[ -n "$t" ]] && IOS_TEST_TARGETS+=("$t")
done <<< "$(bzl query "kind('ios_(unit|ui)_test', set(${TEST_TARGETS[*]}))" --output=label)"
OTHER_TEST_TARGETS=()
for t in "${TEST_TARGETS[@]}" ; do
    [[ " ${IOS_TEST_TARGETS[*]-} " == *" $t "* ]] || OTHER_TEST_TARGETS+=("$t")
done
status=0
bzl test "${OTHER_TEST_TARGETS[@]}" "${TEST_ARGS[@]}" || status=$?
if (( ${#IOS_TEST_TARGETS[@]} )) ; then
    bzl test "${IOS_TEST_TARGETS[@]}" "${TEST_ARGS[@]}" --local_test_jobs=1 || status=$?
fi
(( status == 0 )) || exit "$status"

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
    # The compiler's Swift unit suite runs as its own parallel job on external
    # GitHub Actions (compiler-tests.yml). Gate it here too — but ONLY internally
    # (skip on GitHub Actions to avoid duplicating that job) — so internal cool,
    # which runs this aggregate rather than the workflow, catches compiler
    # regressions before they mirror to the public repo.
    if [[ "${GITHUB_ACTIONS:-}" != "true" ]] ; then
        ./tools/ci/compiler_tests.sh
    fi
fi

)
