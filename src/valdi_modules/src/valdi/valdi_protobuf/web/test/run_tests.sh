#!/bin/bash
# Run Jest tests for valdi_protobuf web
set -e

# Find the runfiles directory
if [[ -n "$RUNFILES_DIR" ]]; then
    RUNFILES="$RUNFILES_DIR"
elif [[ -d "${BASH_SOURCE[0]}.runfiles" ]]; then
    RUNFILES="${BASH_SOURCE[0]}.runfiles"
else
    echo "Cannot find runfiles directory"
    exit 1
fi

# Find node binary - try common locations
NODE_BIN=""
for path in /opt/homebrew/bin/node /usr/local/bin/node /usr/bin/node; do
    if [[ -x "$path" ]]; then
        NODE_BIN="$path"
        break
    fi
done
if [[ -z "$NODE_BIN" ]]; then
    echo "Could not find node binary in standard locations"
    exit 1
fi

# The valdi external repo name
VALDI_REPO="_main~local_repos~valdi"

# Path to node_modules in runfiles
NODE_MODULES_DIR="$RUNFILES/$VALDI_REPO/bzl/valdi/npm/node_modules"

# Test directory
TEST_DIR="$RUNFILES/$VALDI_REPO/src/valdi_modules/src/valdi/valdi_protobuf"

# Jest binary
JEST_BIN="$NODE_MODULES_DIR/jest/bin/jest.js"

if [[ ! -f "$JEST_BIN" ]]; then
    echo "Could not find jest binary at: $JEST_BIN"
    echo "Looking in runfiles: $RUNFILES"
    ls -la "$RUNFILES" || true
    exit 1
fi

# Set NODE_PATH to allow jest to find its dependencies
export NODE_PATH="$NODE_MODULES_DIR"

# Create a minimal jest config to avoid filesystem scanning
JEST_CONFIG=$(cat <<EOF
{
  "rootDir": "$TEST_DIR",
  "testMatch": ["**/web/test/*.spec.js"],
  "testEnvironment": "node",
  "moduleDirectories": ["node_modules", "$NODE_MODULES_DIR"],
  "watchman": false,
  "haste": {
    "enableSymlinks": true
  }
}
EOF
)

# Run jest
cd "$TEST_DIR"
"$NODE_BIN" "$JEST_BIN" \
    --config="$JEST_CONFIG" \
    --verbose
