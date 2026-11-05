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

    # Usage: run_smoke_tests <app dir> [node test filter flags]
    run_smoke_tests() {
        local app_dir=$1
        shift
        OPEN_SOURCE_DIR=$OPEN_SOURCE_DIR PROJECT_ROOT=$app_dir PROJECT_NAME=valdi_app node -r ts-node/register --test --test-force-exit "$@" "test/**/*test.ts"
    }

    if [[ -n "${SMOKE_TEST_PARALLEL:-}" && -z "${SMOKE_TEST_NAME_PATTERN:-}" ]]; then
        # Opt-in for CI hosts with enough cores and memory; the public runners (7 GB of RAM) leave it unset.
        # Every test bootstraps its own app and Bazel workspace (about 30 s each), so tests that share
        # /tmp/valdi_app queue behind each other. Run three groups at once, each in its own app dir and
        # with its own Bazel server. Group 3 runs every test that groups 1 and 2 do not list, so a new
        # or renamed test is never skipped.
        GROUP1='build android|valdi doctor|finds project references'
        GROUP2='build ios|build macos|finds code completions'
        pids=()
        start_group() {
            local group=$1
            shift
            (
                set -o pipefail
                run_smoke_tests "${APP_DIR}_$group" "$@" 2>&1 | sed -u "s/^/[smoke $group] /"
            ) &
            pids+=($!)
        }
        start_group 1 --test-name-pattern="$GROUP1"
        start_group 2 --test-name-pattern="$GROUP2"
        start_group 3 --test-skip-pattern="$GROUP1|$GROUP2"
        failed=0
        for pid in "${pids[@]}"; do
            wait "$pid" || failed=1
        done
        if [[ $failed != 0 ]]; then
            exit 1
        fi
    else
        run_smoke_tests "$APP_DIR" "${NAME_FILTER[@]}"
    fi
    popd
fi
