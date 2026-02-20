#!/usr/bin/env bash

set -e
set -x

echo "Updating Valdi compiler..."

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
CLIENT_ROOT_DIR="$SCRIPT_DIR/../../../../../"
BASE_PATH="${SCRIPT_DIR}/../Compiler"
OUTPUT_FILENAME="Compiler"
ENTITLEMENTS_PATH="$SCRIPT_DIR/entitlements.plist"

# previous version 5.9 for XCode 15
# current version 6.0 for XCode 16
SWIFT_CURRENT_VERSION="6.0"
SWIFT_ALLOWED_VERSIONS="$SWIFT_CURRENT_VERSION\|5.7\|5.8\|5.9\|6.1\|6.2"

skip_analytics=false
bin_output_path=""

usage() {
  echo "Usage: $0 [-o bin_output_path] [-s] [skip_analytics]"
  exit 1
}

# Parse simple command line options
while getopts ":o:s" opt; do
  case "$opt" in
    s)
      skip_analytics=true
      ;;
    o)
      bin_output_path=$OPTARG
      ;;
    \? )
      echo "Invalid option: $OPTARG" 1>&2
      usage
      ;;
    : )
      echo "Invalid option: $OPTARG requires an argument" 1>&2
      usage
      ;;
  esac
done

shift $((OPTIND -1))

if [[ ! -z $ENV_SKIP_ANALYTICS ]] && [ "$ENV_SKIP_ANALYTICS" = true ]; then
    skip_analytics=true
fi

# Assign positional arguments if options were not provided
if [ -z "$bin_output_path" ] && [ $# -ge 1 ]; then
  bin_output_path=$1
  shift
fi

if [ -z "$bin_output_path" ]; then
  usage
fi

# Main
cd "$BASE_PATH"
VARIANT="release"

if [[ $1 ]]; then
    VARIANT="$1"
fi

SWIFT_BIN="/usr/bin/swift"

# For debugging purposes
echo $($SWIFT_BIN --version)

# Check if the current swift version is the expected version
SWIFT_VERSION_OUTPUT=$($SWIFT_BIN --version | grep -e "$SWIFT_ALLOWED_VERSIONS") || true

CURRENT_SYSTEM=$(uname)
IS_LINUX=false
if [[ "$CURRENT_SYSTEM" == "Linux" ]]; then
    IS_LINUX=true
fi

if [[ "$IS_LINUX" = true ]]; then
    if [[ -z $SWIFT_VERSION_OUTPUT ]]; then
        echo "Need to build using Swift $SWIFT_CURRENT_VERSION. Please run the linux_dev_setup.sh script."
        exit 1
    fi

    # Run tests
    # $SWIFT_BIN test
    # Can't run tests on Linux because of https://github.com/apple/swift-corelibs-xctest/issues/438

    # Passing -Xswiftc -g ensures we output debug information even for release builds
    $SWIFT_BIN build $SWIFT_BUILD_ADDITIONAL_ARGS -c "$VARIANT" -Xswiftc -g --static-swift-stdlib
    OUTPUT_FILE_PATH=$($SWIFT_BIN build $SWIFT_BUILD_ADDITIONAL_ARGS -c "$VARIANT" -Xswiftc -g --show-bin-path)/$OUTPUT_FILENAME
else
    # If PATH `swift` is not the expected version, we try to use the latest known expected version toolchain
    if [[ -z $SWIFT_VERSION_OUTPUT ]]; then
        SWIFT_BIN="/Library/Developer/Toolchains/swift-$SWIFT_CURRENT_VERSION-RELEASE.xctoolchain/usr/bin/swift"
        SWIFT_VERSION_OUTPUT=$($SWIFT_BIN --version | grep -e "$SWIFT_CURRENT_VERSION") || true

        echo "Swift $SWIFT_CURRENT_VERSION not found, falling back to $SWIFT_PREVIOUS_VERSION"

        if [[ -z $SWIFT_VERSION_OUTPUT ]]; then
            SWIFT_BIN="/Library/Developer/Toolchains/swift-$SWIFT_PREVIOUS_VERSION-RELEASE.xctoolchain/usr/bin/swift"
            SWIFT_VERSION_OUTPUT=$($SWIFT_BIN --version | grep -e "$SWIFT_PREVIOUS_VERSION") || true
        fi
    fi

    if [[ -z $SWIFT_VERSION_OUTPUT ]]; then
        echo "Need to build using Swift $SWIFT_CURRENT_VERSION. Please download and install the toolchain from https://swift.org/builds/swift-$SWIFT_CURRENT_VERSION-release/xcode/swift-$SWIFT_CURRENT_VERSION-RELEASE/swift-$SWIFT_CURRENT_VERSION-RELEASE-osx.pkg"
        exit 1
    fi

    # Run tests
    $SWIFT_BIN test

    OUTPUT_PATH=$($SWIFT_BIN build $SWIFT_BUILD_ADDITIONAL_ARGS -c "$VARIANT" --arch arm64 --arch x86_64 -Xswiftc -g --show-bin-path)
    OUTPUT_FILE_PATH=$OUTPUT_PATH/$OUTPUT_FILENAME
    $SWIFT_BIN build $SWIFT_BUILD_ADDITIONAL_ARGS -c "$VARIANT" --arch arm64 --arch x86_64 -Xswiftc -g
    codesign --force --sign - --entitlements "$ENTITLEMENTS_PATH" "$OUTPUT_FILE_PATH"

    if $skip_analytics; then
        echo "Analytic uploading for this run will be skipped..."
    fi
fi

if [[ "$IS_LINUX" = true ]]; then
    OUT_DIR="$bin_output_path/linux"
    SHA256_VAR_NAME="VALDI_COMPILER_LINUX_SHA256"
else
    OUT_DIR="$bin_output_path/macos"
    SHA256_VAR_NAME="VALDI_COMPILER_MACOS_SHA256"
fi

(
cd $CLIENT_ROOT_DIR
source src/composer/jenkins/jenkins_helpers.sh

mkdir -p "$OUT_DIR"
rm -f "$OUT_DIR/valdi_compiler"
cp "$OUTPUT_FILE_PATH" "$OUT_DIR/valdi_compiler"
)
