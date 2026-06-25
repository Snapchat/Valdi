#!/bin/bash

set -e

# Avoid causing Homebrew to run 'brew cleanup' automatically
export HOMEBREW_NO_INSTALL_CLEANUP=1

install_or_upgrade() {
    echo "Checking $1"
    if brew ls --versions "$1" >/dev/null; then
        if (brew outdated | grep "$1" >/dev/null); then
            echo "Upgrading $1"
            brew upgrade "$1"
        fi
    else
        echo "Installing $1"
        HOMEBREW_NO_AUTO_UPDATE=1 brew install "$1"
    fi
}

echo
echo "********************************************************************************"
echo "Updating Homebrew..."
brew update

echo
echo "********************************************************************************"
echo "Installing/upgrading Homebrew dependencies..."
install_or_upgrade "watchman"
install_or_upgrade "jq"
install_or_upgrade "libimobiledevice"
install_or_upgrade "libusbmuxd"
install_or_upgrade "libplist"
