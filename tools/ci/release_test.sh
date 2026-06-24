#!/usr/bin/env bash
#
# Release test: bootstrap an app using the CLI's default pinned versions,
# build it, and run tests. Validates the exact flow a user would experience
# when running `valdi bootstrap` after installing the published CLI.
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

# Bootstrap app using the CLI's default pinned versions (no version overrides)
echo "Bootstrapping app (default pinned versions)..."
mkdir -p "$APP_DIR"
rm -rf "${APP_DIR:?}"/* "${APP_DIR:?}"/.[!.]* 2>/dev/null || true
cd "$APP_DIR"
node "$CLI_DIR/dist/index.js" bootstrap \
  -y \
  "-n=$PROJECT_NAME" \
  -t=ui_application \
  --with-cleanup

# Verify the project references public GitHub (not local).
# Bootstrapped projects may use MODULE.bazel (bzlmod) or WORKSPACE.
if [[ -f MODULE.bazel ]]; then
  CHECK_FILE="MODULE.bazel"
elif [[ -f WORKSPACE ]]; then
  CHECK_FILE="WORKSPACE"
else
  echo "ERROR: Neither MODULE.bazel nor WORKSPACE found in bootstrapped app"
  exit 1
fi
if grep -q 'local_path_override.*module_name.*=.*"valdi"' "$CHECK_FILE"; then
  echo "ERROR: $CHECK_FILE uses local_path_override for valdi (expected remote)"
  exit 1
fi
if ! grep -q 'github.com/Snapchat/Valdi' "$CHECK_FILE"; then
  echo "ERROR: $CHECK_FILE does not reference public GitHub Valdi"
  exit 1
fi
echo "$CHECK_FILE references public GitHub (remote, not local) ✓"

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
