# macOS Setup Reference Guide

> **Get the CLI from npm:** `npm install -g @snap/valdi`. Then run `valdi dev_setup` for automated setup. This guide is a reference for manual installation or troubleshooting only.

## About

This guide documents the dependencies Valdi needs on macOS and how to install them manually. For the quickest setup, use [`valdi dev_setup`](../INSTALL.md) which automates most of these steps.

This guide assumes you're using the default shell (zsh). Setup is possible for other shells, but you'll need to adapt the configuration file paths.

## Setting up XCode

Make sure you have the latest version of XCode installed in addition to iPhone Simulator packages for iOS development. Latest versions of [XCode](https://apps.apple.com/us/app/xcode/id497799835) can be found on Apple's App Store.

Make sure XCode tools are in your path:

```
sudo xcode-select -s /Applications/Xcode.app
```

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
brew install npm bazelisk openjdk@17 temurin git-lfs watchman ios-webkit-debug-proxy
```

## Setup JDK path

```
sudo ln -sfn /opt/homebrew/opt/openjdk@17/libexec/openjdk.jdk /Library/Java/JavaVirtualMachines/openjdk-17.jdk
echo 'export PATH="/opt/homebrew/opt/openjdk@17/bin:$PATH"' >> ~/.zshrc
echo 'export JAVA_HOME=`/usr/libexec/java_home -v 17`' >> ~/.zshrc
```

## Install git-lfs

Git Large File Storage (LFS) manages the binaries that we need for Valdi.

```
git lfs install
```

## Install Android SDK (only required for Android development)

> [!NOTE]
> **`valdi dev_setup` installs Android SDK command-line tools automatically.** You only need Android Studio if you prefer using its GUI or need Android emulator management.

### Option 1: Automated (Recommended)
Run `valdi dev_setup` - it will download and install Android SDK command-line tools, including:
- Platform tools (API level 35)
- Build tools (version 34.0.0)
- NDK (version 25.2.9519653)

### Option 2: Manual via Android Studio
If you prefer using Android Studio's GUI:

1. Download and install Android Studio from [developer.android.com/studio](https://developer.android.com/studio)
2. Open any project, navigate to `Tools` -> `SDK Manager`
3. Under **SDK Platforms**, install **API level 35**
4. Under **SDK Tools**, uncheck `Hide obsolete packages`, check `Show Package Details`
5. Install build tools **version 34.0.0**
6. Install NDK version **25.2.9519653**

Update `.zshrc` with the following:

```
echo "export ANDROID_HOME=$HOME/Library/Android/sdk" >> ~/.zshrc
echo "export ANDROID_NDK_HOME=\$ANDROID_HOME/ndk-bundle" >> ~/.zshrc
echo "export PATH=\$ANDROID_HOME/platform-tools:\$PATH" >> ~/.zshrc
source ~/.zshrc
```

# Next steps

[Installation guide](../INSTALL.md#installation)
