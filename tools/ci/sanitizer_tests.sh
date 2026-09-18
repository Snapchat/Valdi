#!/usr/bin/env bash

# Runs Valdi's C++ integration tests under AddressSanitizer to catch teardown / threading
# memory-safety bugs (use-after-free, heap corruption) that a normal optimized build hides.
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
