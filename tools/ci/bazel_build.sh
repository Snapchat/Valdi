#!/usr/bin/env bash

set -e
set -x

# Create bzl alias for bazel if it doesn't exist
if ! command -v bzl &> /dev/null; then
    alias bzl='bazel'
    # For scripts that run in subshells, create a function
    bzl() { bazel "$@"; }
    export -f bzl
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
OPEN_SOURCE_DIR="$(cd "$SCRIPT_DIR/../../"; pwd)"

# Determine workspace root by checking for internal-only directories
# Internal repo has Jenkins/ directory at the root level
POTENTIAL_ROOT="$(cd "$SCRIPT_DIR/../../../../"; pwd)"
if [ -d "$POTENTIAL_ROOT/Jenkins" ]; then
    # Internal repo: workspace root is 4 levels up (mobile/)
    ROOT_DIR="$POTENTIAL_ROOT"
else
    # Mirrored repo: open_source dir is the workspace root
    ROOT_DIR="$OPEN_SOURCE_DIR"
fi

pushd "$ROOT_DIR"

# Flags
export CLIENT_FLAVOR=client_development

if [[ $(uname) == Linux ]] ; then
    # Use Java from environment if already set up (e.g., GitHub Actions)
    if [ -z "$JAVA_HOME" ]; then
        sudo apt-get update -y
        sudo apt-get install -y openjdk-8-jdk libboost-all-dev
        sudo update-java-alternatives --set java-1.8.0-openjdk-amd64
        export JAVA_HOME=/usr/lib/jvm/java-1.8.0-openjdk-amd64
    fi
fi

# Android SDK and NDK are downloaded hermetically by Bazel — no local install needed.

popd

pushd "$OPEN_SOURCE_DIR"

# Everything we want to make sure builds in CI

./tools/ci/build_core_targets.sh

# Test suite
./tools/ci/run_tests.sh

# Reroute global because we can't sudo anything
mkdir -p ~/.npm-global/lib
npm config set prefix '~/.npm-global'
npm install -g npm@8
PATH=~/.npm-global/bin:$PATH

# Optional: Setup git credentials for internal CI (not mirrored to external repos)
if [ -f ./scripts/mirroring/git_init.sh ]; then
    ./scripts/mirroring/git_init.sh
fi

./tools/ci/install_cli.sh

./tools/ci/bootstrap_app.sh

popd
