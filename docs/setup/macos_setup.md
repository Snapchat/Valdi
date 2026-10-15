# macOS Setup Reference Guide

> **Get the CLI from npm:** `npm install -g @snap/valdi`. Then run `valdi dev_setup` for automated setup. This guide is a reference for manual installation or troubleshooting only.

## About

This guide documents the dependencies Valdi needs on macOS and how to install them manually. For the quickest setup, use [`valdi dev_setup`](../INSTALL.md) which automates most of these steps.

This guide assumes you're using the default shell (zsh). Setup is possible for other shells, but you'll need to adapt the configuration file paths.

## Setting up Xcode

Install **Xcode 26.5 or 27** plus the iOS Simulator runtime (see [Prerequisites](../INSTALL.md#prerequisites)). Older Xcode versions are available from [Apple Developer downloads](https://developer.apple.com/download/all/).

Select it so the command-line tools use it (adjust the path to where Xcode is installed):

```
sudo xcode-select -s /Applications/Xcode-26.5.app
```

If you have more than one Xcode installed, Bazel may still pick a different one; see [Troubleshooting](../TROUBLESHOOTING.md#bazel-uses-the-wrong-xcode-version).

## Homebrew

Most of Valdi's required dependencies can be installed via Homebrew.

Follow these instructions to install: https://brew.sh/

Add Homebrew to your path:

```
echo >> ~/.zprofile
echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> /Users/$USER/.zprofile
eval "$(/opt/homebrew/bin/brew shellenv)"
```

## Autoload compinit

Add the following to the top of your `.zshrc` to setup for autocomplete.

```
autoload -U compinit && compinit
autoload -U bashcompinit && bashcompinit
```

Make sure to load your changes via `source ~/.zshrc`.

## Brew install dependencies

```
brew install npm bazelisk openjdk@17 temurin watchman ios-webkit-debug-proxy android-platform-tools
```

## Setup JDK path

```
sudo ln -sfn /opt/homebrew/opt/openjdk@17/libexec/openjdk.jdk /Library/Java/JavaVirtualMachines/openjdk-17.jdk
echo 'export PATH="/opt/homebrew/opt/openjdk@17/bin:$PATH"' >> ~/.zshrc
echo 'export JAVA_HOME=`/usr/libexec/java_home -v 17`' >> ~/.zshrc
```

## Android SDK and NDK

> **You do not need to install the Android SDK, build tools, or NDK manually.** Bazel downloads the correct versions hermetically during the build.

If you want to use Android Studio or `adb` outside of Bazel, you can optionally install the SDK via `valdi dev_setup` or [Android Studio](https://developer.android.com/studio).

# Next steps

[Installation guide](../INSTALL.md#installation)
