#!/usr/bin/env bash

set -e
set -x

echo "=================================================="
echo "Testing GitHub Actions workflow locally"
echo "=================================================="

# Get to the open_source directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/../.."

echo ""
echo "Working directory: $(pwd)"
echo "Platform: $(uname)"
echo ""

# Check prerequisites
echo "Checking prerequisites..."

if ! command -v java &> /dev/null; then
    echo "ERROR: Java is not installed"
    exit 1
fi

echo "Java version:"
java -version

if ! command -v bazel &> /dev/null && ! command -v bzl &> /dev/null; then
    echo "ERROR: Bazel is not installed"
    echo "Install with: brew install bazelisk"
    exit 1
fi

# Create bzl alias if needed
if ! command -v bzl &> /dev/null; then
    echo "Creating bzl alias..."
    bzl() { bazel "$@"; }
    export -f bzl
fi

echo "Bazel version:"
bazel --version || bzl --version

# Check Android SDK (optional for macOS)
if [[ $(uname) == Linux ]]; then
    if [ -z "$ANDROID_HOME" ]; then
        echo "WARNING: ANDROID_HOME is not set"
        echo "Android builds will not work"
    else
        echo "ANDROID_HOME: $ANDROID_HOME"
    fi
else
    echo "Skipping Android check on macOS"
fi

echo ""
echo "=================================================="
echo "Step 1: Running bazel_build.sh"
echo "=================================================="
./tools/ci/bazel_build.sh

echo ""
echo "=================================================="
echo "Step 2: Building core targets"
echo "=================================================="
./tools/ci/build_core_targets.sh

echo ""
echo "=================================================="
echo "Step 3: Running tests (macOS only)"
echo "=================================================="
if [[ $(uname) != Linux ]]; then
    ./tools/ci/run_tests.sh
else
    echo "Skipping tests on Linux (tests require macOS)"
fi

echo ""
echo "=================================================="
echo "✅ All workflow steps completed successfully!"
echo "=================================================="

