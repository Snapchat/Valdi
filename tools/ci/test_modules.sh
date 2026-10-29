#!/usr/bin/env bash
#
# Runs every Valdi module test (valdi_test targets anywhere in the repo, plus //modules/...),
# minus tools/ci/test_skiplist.txt; see select_tests.sh. They share one host-built copy of the
# runtime and compiler, so they run in this job rather than with the C++ tests.
#
# One CI unit = one script (see AGENTS.md).

set -euxo pipefail

# Intended to be run from open_source/
cd "$(dirname "$0")/../.."

source tools/ci/select_tests.sh
select_tests modules
bzl test "${TEST_TARGETS[@]}" --keep_going --skip_incompatible_explicit_targets --test_output=errors
