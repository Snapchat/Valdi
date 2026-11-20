#!/usr/bin/env bash

# Runs Valdi's C++ integration, runtime and snap_drawing tests under AddressSanitizer to catch
# teardown / threading / renderer memory-safety bugs (use-after-free, heap corruption) that a
# normal optimized build hides.
#
# Slow by design: ASan is a full instrumented build. It runs nightly, on main, on manual dispatch,
# and on PRs that touch the code these targets build from (see the trigger paths in
# valdi-sanitizer-tests.yml); unrelated PRs skip it. Hermes is excluded -- its parser trips a
# benign stack-use-after-scope under ASan; the QuickJS engines exercise the ThreadedDispatchQueue
# teardown paths that matter here.
#
# Usage: sanitizer_tests.sh [asan|tsan]   (default: asan)
# `tsan` runs only //valdi:test_integration under ThreadSanitizer, with the same Hermes exclusion as ASan.
# It is nightly-only and non-blocking in CI while its first findings are triaged.

set -eux

# Intended to be run from open_source/
cd "$(dirname "$0")/../.."

MODE="${1:-asan}"
case "$MODE" in
    asan|tsan) ;;
    *) echo "usage: $0 [asan|tsan]" >&2; exit 2 ;;
esac

EXTRA=(--test_output=errors)
if [ "$MODE" = tsan ]; then
    # report_thread_leaks=0: ResourceManager's own dispatch queue threads are never joined at
    # teardown, so every test reports a "leak" (311 in the first run). It is runtime-owned and not
    # fixable from a test. halt_on_error=0 reports every race in one run instead of stopping at the
    # first. Known reports are listed with reasons in tsan_suppressions.txt.
    EXTRA=(
        --test_output=errors
        --experimental_ui_max_stdouterr_bytes=-1
        "--test_env=TSAN_OPTIONS=halt_on_error=0:report_thread_leaks=0:print_suppressions=1:suppressions=$PWD/tools/ci/tsan_suppressions.txt"
        --strategy=TestRunner=local
    )
fi
bzl test //valdi:test_integration \
    --config="$MODE" \
    "${EXTRA[@]}" \
    --test_arg=--gtest_filter='-*/Hermes'

if [ "$MODE" = tsan ]; then
    exit 0
fi

# snap_drawing renderer and its Valdi glue (a ranked hot system; bitmap, path and layer lifetime
# bugs are exactly what ASan catches). Both are engine-free.
bzl test //snap_drawing:test //valdi:test_snap_drawing \
    --config=asan \
    --test_output=errors

# Replays the module-archive fuzzer's corpus. It links the whole runtime, which this job already
# builds under ASan, so it's cheap here and expensive anywhere else.
bzl test //valdi/fuzz:ValdiModuleArchive_fuzz_test \
    --config=asan \
    --test_output=errors

# C++ runtime unit tests (Value, ValueUtils, JavaScriptTypes, ...). On Linux, `bzl test` trips a
# pre-existing static-destructor segfault in gtest's XML writer for this target (all tests pass;
# see run_tests.sh), so build it and run the binary directly, as internal CI does.
#
# Running directly skips the asan config's --test_env, so set the same ASAN_OPTIONS here (leak
# checking off, like every other target in this script). Excluded until it's fixed:
# ViewNode.providesLayoutDirtyFlagWhenLayoutAttributeChange passes normally but fails under ASan.
bzl build //valdi:test_runtime --config=asan
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 "$(bzl info --config=asan bazel-bin)/valdi/test_runtime" \
    --gtest_filter='-ViewNode.providesLayoutDirtyFlagWhenLayoutAttributeChange'
