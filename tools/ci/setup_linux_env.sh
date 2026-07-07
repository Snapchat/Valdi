#!/usr/bin/env bash
#
# setup_linux_env.sh — Linux CI environment setup for Valdi open-source builds.
#
# This script is the single source of truth for Linux CI environment preparation.
# It is called by:
#   - GitHub Actions (.github/workflows/bzl-changes.yml)
#   - Internal SnapCI (client_validate_valdi_open_source.sh)
#
# The script is idempotent and safe to run multiple times.

set -euo pipefail

if [[ "$(uname)" != "Linux" ]]; then
    echo "setup_linux_env.sh: Not on Linux, skipping."
    return 0 2>/dev/null || exit 0
fi

echo "=== Valdi Linux CI Environment Setup ==="

# ---------------------------------------------------------------------------
# 1. Free disk space
#
# GitHub Actions runners ship with many pre-installed SDKs we don't need.
# Remove them to avoid running out of disk during Bazel builds.
#
# IMPORTANT: Never remove /usr/local/lib/android (ANDROID_HOME) — Bazel's
# rules_android resolves the SDK from there. Removing it breaks Android builds.
# ---------------------------------------------------------------------------
echo "--- Freeing disk space ---"

CLEANUP_PATHS=(
    /usr/share/dotnet
    /opt/ghc
    /opt/hostedtoolcache/CodeQL
    /usr/local/share/boost
    /opt/pip
    /usr/share/swift
    /usr/share/miniconda
    /opt/az
)

for path in "${CLEANUP_PATHS[@]}"; do
    if [ -d "$path" ]; then
        echo "  Removing $path"
        sudo rm -rf "$path"
    fi
done

sudo apt-get clean || true
df -h

# ---------------------------------------------------------------------------
# 2. Install system dependencies
# ---------------------------------------------------------------------------
echo "--- Installing system dependencies ---"

sudo apt-get update -y
sudo apt-get install -y \
    libboost-all-dev \
    libfontconfig1-dev \
    zlib1g-dev

# libtinfo5 is not in the default repos on Ubuntu 24.04+ (used by newer GHA runners).
# Fall back to the 22.04 archive package if apt can't find it. Discover the deb
# filename dynamically so a security-update revision bump (Ubuntu drops the old
# file when it publishes the new one) doesn't 404 our pinned URL.
ARCHIVE_URL="http://archive.ubuntu.com/ubuntu/pool/universe/n/ncurses"
if ! sudo apt-get install -y libtinfo5 2>/dev/null; then
    echo "libtinfo5 not available in apt, downloading from Ubuntu archive..."
    DEB=$(curl -fsSL "$ARCHIVE_URL/" | grep -oE 'libtinfo5_[^"]*_amd64\.deb' | sort -Vu | tail -n1)
    if [ -z "$DEB" ]; then
        echo "Could not locate a libtinfo5 deb under $ARCHIVE_URL/" >&2
        exit 1
    fi
    echo "Fetching $DEB"
    curl -fsSLO "$ARCHIVE_URL/$DEB"
    sudo apt install -y "./$DEB"
    rm "$DEB"
fi

# ---------------------------------------------------------------------------
# 3. Install Bazel / Bazelisk
#
# If `bzl` (or `bazel`) is already on PATH (e.g. installed by a prior step or
# the host image), skip this.
# ---------------------------------------------------------------------------
echo "--- Setting up Bazel ---"

if ! command -v bzl &>/dev/null && ! command -v bazel &>/dev/null; then
    echo "  Installing Bazelisk..."
    wget -q https://github.com/bazelbuild/bazelisk/releases/latest/download/bazelisk-linux-amd64
    chmod +x bazelisk-linux-amd64
    sudo mv bazelisk-linux-amd64 /usr/local/bin/bazel
    sudo ln -sf /usr/local/bin/bazel /usr/local/bin/bzl
elif command -v bazel &>/dev/null && ! command -v bzl &>/dev/null; then
    # bazel exists but bzl symlink is missing — create it
    echo "  Creating bzl symlink..."
    sudo ln -sf "$(command -v bazel)" /usr/local/bin/bzl
fi

bazel --version 2>/dev/null || echo "  Bazel version check skipped (custom wrapper)"

# ---------------------------------------------------------------------------
# 4. Java setup
#
# GitHub Actions uses actions/setup-java to configure JAVA_HOME before this
# script runs. Internal CI may or may not have Java pre-installed.
# If JAVA_HOME is already set and valid, skip. Otherwise install OpenJDK 17.
# ---------------------------------------------------------------------------
echo "--- Setting up Java ---"

if [ -n "${JAVA_HOME:-}" ] && [ -d "$JAVA_HOME" ]; then
    echo "  JAVA_HOME already set: $JAVA_HOME"
else
    echo "  Installing OpenJDK 17..."
    sudo apt-get install -y openjdk-17-jdk
    # Find the installed JDK path
    JAVA_HOME=$(dirname "$(dirname "$(readlink -f "$(command -v java)")")")
    export JAVA_HOME
    echo "  JAVA_HOME set to: $JAVA_HOME"
fi

java -version

echo "=== Linux CI environment setup complete ==="
