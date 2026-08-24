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

# A cold Valdi build wants roughly 25GB of output base (about half of it downloaded deps:
# NDK, skia, v8, boost), and the runner has ~26GB free after the first eight entries here.
# That is too tight: both "Linux: Build & Export" and the Android instrumentation job have
# died mid-compile with "No space left on device", on main as well as on PRs. The rest of
# the list is toolchains no Valdi job uses.
#
# Do not add these, they are load-bearing:
#   /usr/local/lib/node_modules  - holds the runner's global npm, so removing it breaks
#                                  install_cli.sh and bootstrap_app.sh ("Setup environment
#                                  and install Valdi CLI" failed this way).
#   /opt/hostedtoolcache/Java*   - where actions/setup-java puts the JDK.
#   /opt/hostedtoolcache/Python  - actions/setup-python populates it in other jobs.
#   anything under ANDROID_HOME   - rules_android resolves the SDK from there, and
#                                  rules_android_ndk resolves the NDK from there.
CLEANUP_PATHS=(
    /usr/share/dotnet
    /opt/ghc
    /opt/hostedtoolcache/CodeQL
    /usr/local/share/boost
    /opt/pip
    /usr/share/swift
    /usr/share/miniconda
    /opt/az
    /opt/hostedtoolcache/PyPy
    /opt/hostedtoolcache/Ruby
    /opt/hostedtoolcache/go
    /usr/local/.ghcup
    /usr/local/lib/heroku
    /usr/local/share/chromium
    /usr/lib/google-cloud-sdk
    /opt/microsoft
)

for path in "${CLEANUP_PATHS[@]}"; do
    if [ -d "$path" ]; then
        echo "  Removing $path"
        sudo rm -rf "$path"
    fi
done

# Prebuilt images we never run, worth a couple of GB. GitHub-hosted runners only: this
# script also runs on internal CI, where pruning images other steps rely on would just
# force slow re-pulls.
if [ "${GITHUB_ACTIONS:-}" = "true" ] && command -v docker &> /dev/null; then
    docker system prune -af || true
fi

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

# Chrome/Puppeteer headless-shell needs a set of GTK/X/ALSA shared libraries. The GitHub
# ubuntu runner image ships them by default, but the internal SnapCI VM (Ubuntu 22.04) does
# not, so web_integration_test.sh's headless Chrome fails to launch with e.g.
# "libatk-1.0.so.0: cannot open shared object file". Install them here (this script is shared
# by both CIs; harmless where already present). Ubuntu 24.04+ (newer GHA runners) renamed some
# of these with a t64 suffix, so fall back to the t64 name when the classic one is unavailable.
echo "--- Installing Chrome/Puppeteer runtime libraries ---"
for lib in \
    libnss3 libnspr4 libatk1.0-0 libatk-bridge2.0-0 libatspi2.0-0 libcups2 \
    libdrm2 libgbm1 libxkbcommon0 libxcomposite1 libxdamage1 libxfixes3 \
    libxrandr2 libgtk-3-0 libpango-1.0-0 libcairo2 libasound2; do
    sudo apt-get install -y "$lib" 2>/dev/null || sudo apt-get install -y "${lib}t64"
done

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

# watchman: the hot-reload smoke's file watcher. Not in Ubuntu apt, so pull the
# prebuilt linux release. Installed here (vs only the external hotreload job) so
# internal cool Linux — which sources this script, then runs run_tests.sh, whose
# internal-only branch calls hotreload_smoke.sh — has it too. Bump WATCHMAN_VERSION
# to a tag that publishes a *-linux.zip (https://github.com/facebook/watchman/releases).
if ! command -v watchman >/dev/null 2>&1; then
    echo "--- Installing watchman ---"
    WATCHMAN_VERSION=v2026.04.20.00
    WATCHMAN_ZIP="watchman-${WATCHMAN_VERSION}-linux.zip"
    curl -fsSLO "https://github.com/facebook/watchman/releases/download/${WATCHMAN_VERSION}/${WATCHMAN_ZIP}"
    unzip -q "$WATCHMAN_ZIP"
    WATCHMAN_DIR="watchman-${WATCHMAN_VERSION}-linux"
    sudo mkdir -p /usr/local/bin /usr/local/lib /usr/local/var/run/watchman
    sudo cp "$WATCHMAN_DIR/bin/"* /usr/local/bin/
    sudo cp -r "$WATCHMAN_DIR/lib/"* /usr/local/lib/
    sudo chmod 755 /usr/local/bin/watchman
    sudo chmod 2777 /usr/local/var/run/watchman
    sudo ldconfig
    rm -rf "$WATCHMAN_ZIP" "$WATCHMAN_DIR"
    # watchman is optional here (only the hot-reload smoke uses it, and that's not in
    # the //valdi:test gate). Some runners are too old for the pinned release's prebuilt
    # binary (e.g. it needs a newer glibc than Ubuntu 22.04 ships), where `watchman
    # --version` exits non-zero and, under `set -e`, would abort the whole setup. Don't
    # let a broken optional tool fail environment setup.
    watchman --version || echo "warning: watchman unavailable on this runner; hot-reload smoke will be skipped"
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
