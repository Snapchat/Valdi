#!/usr/bin/env bash
#
# Post-publish test: after `npm publish`, verify the package on the npm registry actually works.
# Waits for @snap/valdi@<version> to appear on the registry, installs it globally, bootstraps an app
# with the INSTALLED CLI (not one built from source, unlike release_test.sh), then builds and tests it.
#
# Usage: run from repo root (open_source). Requires Node, Bazel, and (on macOS) Xcode.
#
#   ./tools/ci/post_publish_test.sh <version>
#
# Optional env:
#   APP_DIR       - directory for the bootstrapped app (default: /tmp/valdi_post_publish_test)
#   WAIT_TIMEOUT  - seconds to wait for the version to appear on the registry (default: 600)
#   WAIT_INTERVAL - seconds between registry polls (default: 15)
#   SKIP_BUILD    - if non-empty, skip the iOS build (only bootstrap + test)
#
set -e
set -x

if [[ $# -ne 1 || -z "$1" ]]; then
    echo "usage: $0 <version>" >&2
    exit 2
fi
VERSION="$1"
PACKAGE="@snap/valdi"

# This script calls `bazel` directly. Some environments expose only the `bzl` wrapper and no bare
# `bazel`; fall back to it.
if ! command -v bazel > /dev/null 2>&1 && command -v bzl > /dev/null 2>&1; then
    bazel() { bzl "$@"; }
    export -f bazel
fi

APP_DIR="${APP_DIR:-/tmp/valdi_post_publish_test}"
PROJECT_NAME="post_publish_test"
WAIT_TIMEOUT="${WAIT_TIMEOUT:-600}"
WAIT_INTERVAL="${WAIT_INTERVAL:-15}"

echo "=============================================="
echo "Valdi post-publish test (npm registry)"
echo "=============================================="
echo "PACKAGE=$PACKAGE@$VERSION"
echo "APP_DIR=$APP_DIR"
echo ""

# Wait for the registry to serve the version we just published (propagation can lag `npm publish`).
echo "Waiting for $PACKAGE@$VERSION on the npm registry (timeout ${WAIT_TIMEOUT}s)..."
deadline=$((SECONDS + WAIT_TIMEOUT))
until [[ "$(npm view "$PACKAGE@$VERSION" version 2>/dev/null || true)" == "$VERSION" ]]; do
    if (( SECONDS >= deadline )); then
        echo "ERROR: $PACKAGE@$VERSION did not appear on the npm registry within ${WAIT_TIMEOUT}s"
        exit 1
    fi
    sleep "$WAIT_INTERVAL"
done
echo "$PACKAGE@$VERSION is available on the registry ✓"

# Install the published CLI globally. Fall back to a user-owned prefix when the default isn't writable.
npm_prefix="$(npm config get prefix)"
if [[ ! -w "$npm_prefix/lib" && ! -w "$npm_prefix" ]]; then
    mkdir -p ~/.npm-global/lib
    npm config set prefix ~/.npm-global
fi
export PATH="$(npm config get prefix)/bin:$PATH"
npm i -g "$PACKAGE@$VERSION"

installed="$(valdi --version | tail -n 1)"
echo "Installed valdi --version: $installed"
if [[ "$installed" != *"$VERSION"* ]]; then
    echo "ERROR: installed CLI reports '$installed', expected $VERSION"
    exit 1
fi

# Optional: fake ios_webkit_debug_proxy so valdi doctor doesn't complain (same as release_test.sh)
mkdir -p ~/bin
export PATH="$HOME/bin:$PATH"
touch ~/bin/ios_webkit_debug_proxy 2>/dev/null || true
chmod +x ~/bin/ios_webkit_debug_proxy 2>/dev/null || true

# Bootstrap app with the registry CLI's default pinned versions (no version overrides)
echo "Bootstrapping app (registry CLI, default pinned versions)..."
mkdir -p "$APP_DIR"
rm -rf "${APP_DIR:?}"/* "${APP_DIR:?}"/.[!.]* 2>/dev/null || true
cd "$APP_DIR"
valdi bootstrap \
  -y \
  "-n=$PROJECT_NAME" \
  -t=ui_application \
  --with-cleanup

# Build and test. The iOS build only exists on macOS; Linux runs bootstrap + test.
if [[ -z "${SKIP_BUILD:-}" && "$(uname)" == "Darwin" ]]; then
  echo "Building iOS app..."
  bazel build //:${PROJECT_NAME}_app_ios
fi

echo "Running module test..."
bazel test //modules/${PROJECT_NAME}:test

echo ""
echo "=============================================="
echo "Post-publish test passed ✓ ($PACKAGE@$VERSION)"
echo "=============================================="
