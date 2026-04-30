#!/bin/bash

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
cd $SCRIPT_DIR/..

# Run dummy tests
bzl test  --bzl_arch=native //libs/dummy:test --@valdi//bzl/runtime_flags:enable_asserts_override --@valdi//bzl/runtime_flags:enable_tracing  --@valdi//bzl/runtime_flags:enable_logging

# Build with no asserts,tracing and logging
bzl run  --bzl_arch=native //libs/dummy:dummy

# Build with everything enabled
bzl run --bzl_arch=native //libs/dummy:dummy --@valdi//bzl/runtime_flags:enable_asserts_override --@valdi//bzl/runtime_flags:enable_tracing  --@valdi//bzl/runtime_flags:enable_logging -c fastbuild
