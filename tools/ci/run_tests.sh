#!/usr/bin/env bash

set -eux

(

# Intended to be run from open_source/
cd "$(dirname "$0")/../.."

bzl test //valdi:test_snap_drawing //valdi:test_hermes --test_output=errors
bzl test //valdi:test_layout --test_output=all --test_arg=--gtest_print_time=1

# test_svg passes but was not gated. Safe on Linux: it only deps :valdi_svg and
# //snap_drawing:test_utils, so it avoids the runtime link that keeps test_runtime macOS-only
# (see the block below). //valdi:test and //valdi:test_integration are deliberately absent: they
# abort on a pre-existing assertion in JavaScriptRuntime.cpp ("The main thread must never dispatch
# synchronously into a worker runtime") that reproduces on clean master.
bzl test //valdi:test_svg --test_output=errors

if [[ $(uname) != Linux ]] ; then
    bzl test //valdi:valdi_ios_objc_test --test_output=errors
    bzl test //valdi:valdi_ios_swift_test --test_output=errors
    bzl test //valdi:valdi_macos_objc_test --test_output=errors

    # C++ runtime unit tests (Value, ValueUtils, JavaScriptTypes, etc.), engine-agnostic.
    # External counterpart of the internal ValdiBazelTestStep addition (#115098).
    # macOS-only: on Linux `bzl test` trips a pre-existing static-destructor segfault in
    # gtest's XML writer when linked against valdi_standalone_runtime (all tests pass; only
    # teardown crashes under bzl test). Internal CI works around it by running the binary directly.
    # //valdi:test_integration is intentionally NOT wired — separate pre-existing framework
    # failures (RuntimeFixture async-dispatch assert, remote-component mock fixtures).
    bzl test //valdi:test_runtime --test_output=errors
fi

)
