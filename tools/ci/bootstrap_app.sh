#!/usr/bin/env bash

set -e
set -x

# Should be run after cli is installed

# We're not installing the ios_webkit_debug_proxy and we're not runnig any simulators at this point
# However we want the valdi doctor command to believe there is an ios_webkit_debug_proxy
mkdir -p ~/bin
export PATH=$HOME/bin:$PATH
touch ~/bin/ios_webkit_debug_proxy
chmod +x ~/bin/ios_webkit_debug_proxy
which ios_webkit_debug_proxy


OPEN_SOURCE_DIR="$(pwd)"
APP_DIR="/tmp/valdi_app"

# Auto-repin Rust crate lockfiles so the digest matches regardless of whether
# @valdi is the root module (public repo) or a dependency (internal monorepo).
export CARGO_BAZEL_REPIN=true

# Make sure the targets build
if [[ $(uname) != Linux ]] ; then
    pushd "$OPEN_SOURCE_DIR/npm_modules/cli"
    # --test-force-exit: the smoke tests spawn detached `valdi`/bazel children; if one
    # is left alive (e.g. a build that failed on an idle timeout), node --test otherwise
    # never exits and the whole job idles to its wall-clock cap. Force exit once the
    # suite finishes so a leaked handle can't turn a red into a multi-hour timeout.
    # SMOKE_TEST_NAME_PATTERN narrows the run (e.g. 'build ios' on runners too small for all of it).
    NAME_FILTER=()
    if [[ -n "${SMOKE_TEST_NAME_PATTERN:-}" ]]; then
        NAME_FILTER=(--test-name-pattern="$SMOKE_TEST_NAME_PATTERN")
    fi
    OPEN_SOURCE_DIR=$OPEN_SOURCE_DIR PROJECT_ROOT=$APP_DIR node -r ts-node/register --test --test-force-exit "${NAME_FILTER[@]}" "test/**/*test.ts"
    popd
fi
