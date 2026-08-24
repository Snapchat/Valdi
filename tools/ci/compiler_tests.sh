#!/usr/bin/env bash
#
# Runs the Valdi compiler's own Swift unit suite (compiler/compiler/Compiler/Tests/CompilerTests)
# through Bazel/rules_swift as //compiler/compiler:CompilerTests.
#
# This used to shell out to SPM `swift test`, which needed a system `swift` on PATH — present on
# macOS via Xcode but absent on the Linux CI runner, so the suite self-skipped there. Running it as
# a swift_test target uses the same hermetic Swift toolchain the rest of the build already uses, so
# it runs on both Linux and macOS. Factored into a script so the external compiler-tests.yml
# workflow and the internal cool entry script call the same thing.
set -euo pipefail

cd "$(dirname "$0")/../.."

bzl test //compiler/compiler:CompilerTests --test_output=errors
