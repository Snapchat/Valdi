#!/usr/bin/env bash
#
# Runs the Valdi module test suite (//modules/...).
#
# One CI unit = one script (see AGENTS.md).

set -euxo pipefail

# Intended to be run from open_source/
cd "$(dirname "$0")/../.."

bzl test //modules/... --test_output=errors
