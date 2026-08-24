#!/usr/bin/env bash
#
# Builds the Valdi compiler from source with the local-compiler toggle, so a change to the
# compiler is exercised as a build gate.
#
# One CI unit = one script (see AGENTS.md).

set -euxo pipefail

# Intended to be run from open_source/
cd "$(dirname "$0")/../.."

bzl build //compiler/compiler:local_valdi_compiler --//bzl/valdi:use_local_compiler=true
