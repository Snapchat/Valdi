#!/usr/bin/env bash
#
# Release test: bootstrap an app from the bleeding edge (main) of the public
# GitHub Valdi/Valdi_Widgets, build it, and run tests. Use this before cutting
# a release to verify that if we cut a release now from main, things won't fail.
#
# Usage: run from repo root (open_source). Requires Node, Bazel, and (on macOS) Xcode.
#
#   ./tools/ci/release_test.sh
#
# Optional env:
#   APP_DIR  - directory for the bootstrapped app (default: /tmp/valdi_release_test)
#   SKIP_BUILD - if non-empty, skip the iOS build (only bootstrap + test)
#
set -e
set -x

OPEN_SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
APP_DIR="${APP_DIR:-/tmp/valdi_release_test}"
PROJECT_NAME="release_test"
CLI_DIR="${OPEN_SOURCE_DIR}/npm_modules/cli"

# Optional: fake ios_webkit_debug_proxy so valdi doctor doesn't complain (same as bootstrap_app.sh)
mkdir -p ~/bin
export PATH="$HOME/bin:$PATH"
touch ~/bin/ios_webkit_debug_proxy 2>/dev/null || true
chmod +x ~/bin/ios_webkit_debug_proxy 2>/dev/null || true

echo "=============================================="
echo "Valdi release test (public GitHub)"
echo "=============================================="
echo "OPEN_SOURCE_DIR=$OPEN_SOURCE_DIR"
echo "APP_DIR=$APP_DIR"
echo ""

# Build CLI from source
echo "Building Valdi CLI..."
cd "$CLI_DIR"
npm ci
npm run build
cd "$OPEN_SOURCE_DIR"

# Bootstrap app using bleeding edge (main) from public GitHub (no -l, so no local path)
echo "Bootstrapping app (bleeding edge / main from public GitHub)..."
mkdir -p "$APP_DIR"
rm -rf "${APP_DIR:?}"/* "${APP_DIR:?}"/.[!.]* 2>/dev/null || true
cd "$APP_DIR"
node "$CLI_DIR/dist/index.js" bootstrap \
  -y \
  "-n=$PROJECT_NAME" \
  -t=ui_application \
  --valdiVersion=main \
  --valdiWidgetsVersion=main \
  --with-cleanup

# Verify WORKSPACE points at public GitHub (not local)
if ! grep -q 'github.com/Snapchat/Valdi' WORKSPACE; then
  echo "ERROR: WORKSPACE does not reference public GitHub Valdi"
  exit 1
fi
echo "WORKSPACE references public GitHub ✓"

# Build and test
if [[ -z "${SKIP_BUILD:-}" ]]; then
  echo "Building iOS app..."
  bazel build //:release_test_app_ios
fi

echo "Running module test..."
bazel test //modules/release_test:test

echo ""
echo "=============================================="
echo "Release test passed ✓"
echo "=============================================="
