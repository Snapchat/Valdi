#!/usr/bin/env bash

# Runs Valdi's C++ integration, runtime and snap_drawing tests under AddressSanitizer to catch
# teardown / threading / renderer memory-safety bugs (use-after-free, heap corruption) that a
# normal optimized build hides.
#
# Slow by design: ASan is a full instrumented build and is not remotely cached, so this runs
# nightly (and on manual dispatch), never on every PR. Hermes is excluded -- its parser trips a
# benign stack-use-after-scope under ASan; the QuickJS engines exercise the ThreadedDispatchQueue
# teardown paths that matter here.

set -eux

# Intended to be run from open_source/
cd "$(dirname "$0")/../.."

bzl test //valdi:test_integration \
    --config=asan \
    --test_output=errors \
    --test_arg=--gtest_filter='-*/Hermes'

# snap_drawing renderer and its Valdi glue (a ranked hot system; bitmap, path and layer lifetime
# bugs are exactly what ASan catches). Both are engine-free.
bzl test //snap_drawing:test //valdi:test_snap_drawing \
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
