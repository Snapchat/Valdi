#!/usr/bin/env bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"

RC_FILE="$HOME/.bashrc"
if [ -n "$($SHELL -c 'echo $ZSH_VERSION')" ]; then
    RC_FILE="$HOME/.zshrc"
fi

pushd "$SCRIPT_DIR"

echo
echo "********************************************************************************"
echo "Updating apt-get..."
sudo apt-get --quiet update

echo
echo "********************************************************************************"
echo "Installing Swift dependencies..."
sudo apt-get --quiet --assume-yes install \
    curl \
    clang \
    libicu-dev \
    binutils \
    git \
    gnupg2 \
    libc6-dev \
    libcurl4-openssl-dev \
    libedit2 \
    libgcc-9-dev \
    libpython3.8 \
    libsqlite3-0 \
    libstdc++-9-dev \
    libxml2-dev \
    libz3-dev \
    pkg-config \
    tzdata \
    unzip \
    zlib1g-dev

SWIFT_VERSION=swift-5.9.2-RELEASE
echo
echo "********************************************************************************"
echo "Installing Swift: $SWIFT_VERSION"
if [ -z "$SWIFT_TOOLCHAIN_ROOT" ]
then
    SWIFT_TOOLCHAIN_ROOT="$SCRIPT_DIR/valdi-swift-toolchain"
fi

mkdir -p "$SWIFT_TOOLCHAIN_ROOT"
pushd $SWIFT_TOOLCHAIN_ROOT

SWIFT_PLATFORM=ubuntu18.04
SWIFT_TOOLCHAIN_NAME="$SWIFT_VERSION-$SWIFT_PLATFORM"
SWIFT_TOOLCHAIN_DIR="$SWIFT_TOOLCHAIN_ROOT/$SWIFT_TOOLCHAIN_NAME"
SWIFT_BIN_DIR="$SWIFT_TOOLCHAIN_DIR/usr/bin"
SWIFT_LIBS_DIR="$SWIFT_TOOLCHAIN_DIR/usr/lib/swift/linux"

if [[ -d "$SWIFT_TOOLCHAIN_DIR" ]]; then
    echo "$SWIFT_TOOLCHAIN_DIR already exists!"
else
    SWIFT_WEBROOT=https://download.swift.org
    SWIFT_BRANCH=swift-5.9.2-release
    SWIFT_WEBDIR="$SWIFT_WEBROOT/$SWIFT_BRANCH/$(echo $SWIFT_PLATFORM | tr -d .)"
    SWIFT_BIN_URL="$SWIFT_WEBDIR/$SWIFT_VERSION/$SWIFT_VERSION-$SWIFT_PLATFORM.tar.gz"
    SWIFT_SIG_URL="$SWIFT_BIN_URL.sig"
    SWIFT_SIGNING_KEY=A62AE125BBBFBB96A6E042EC925CC1CCED3D1561

    # Download the toolchain and signature
    curl --progress-bar -fSL "$SWIFT_BIN_URL" -o swift.tar.gz "$SWIFT_SIG_URL" -o swift.tar.gz.sig

    # Verify the signature
    gpg --batch --quiet --keyserver keyserver.ubuntu.com --recv-keys "$SWIFT_SIGNING_KEY" && gpg --batch --verify swift.tar.gz.sig swift.tar.gz

    # Unpack the toolchain, set libs permissions, and clean up.
    mkdir -p "$SWIFT_TOOLCHAIN_DIR" && tar -xzf swift.tar.gz --directory "$SWIFT_TOOLCHAIN_DIR" --strip-components=1 && chmod -R o+r $SWIFT_TOOLCHAIN_DIR/usr/lib/swift && rm -rf swift.tar.gz.sig swift.tar.gz
fi

echo
echo "********************************************************************************"
echo "Updating PATH to contain the Swift toolchain binaries..."
if [[ $PATH == *"$SWIFT_BIN_DIR"* ]]; then
    echo "PATH already contains $SWIFT_BIN_DIR!"
else
    echo "" >>"$RC_FILE"
    echo "export PATH=\"$SWIFT_BIN_DIR:\${PATH}\"" >>"$RC_FILE"
fi

echo
echo "********************************************************************************"
echo "Updating LD_LIBRARY_PATH to contain the Swift toolchain libraries..."
if [[ $LD_LIBRARY_PATH == *"$SWIFT_LIBS_DIR"* ]]; then
    echo "LD_LIBRARY_PATH already contains $SWIFT_LIBS_DIR!"
else
    echo "" >>"$RC_FILE"
    echo "export LD_LIBRARY_PATH=\"$SWIFT_LIBS_DIR:\${LD_LIBRARY_PATH}\"" >>"$RC_FILE"
fi
popd

echo
echo "********************************************************************************"
echo "Updating LD_LIBRARY_PATH to contain the path to the JavaScriptCore dynamic library..."
JSCORE_LIB_DIR="$SCRIPT_DIR/../../third-party/jscore/libs/linux/x86_64"
if [[ $LD_LIBRARY_PATH == *"$JSCORE_LIB_DIR"* ]]; then
    echo "LD_LIBRARY_PATH already contains $JSCORE_LIB_DIR!"
else
    echo "" >>"$RC_FILE"
    echo "export LD_LIBRARY_PATH=\"$JSCORE_LIB_DIR:\${LD_LIBRARY_PATH}\"" >>"$RC_FILE"
fi
popd

source "$RC_FILE"

echo
echo "********************************************************************************"
echo "Installing usbmuxd, adb..."
sudo apt-get --assume-yes --quiet install usbmuxd adb
