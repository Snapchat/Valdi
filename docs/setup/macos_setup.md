# Getting ready to build things with Valdi on MacOS

## About

Valdi needs a handful of dependencies in order to build. You can find information about installation in this doc.

This doc assumes you're using the default shell, zsh. Setup should be possible for other setups but you're on your own for updating config files.

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
brew install npm bazelisk openjdk@11 temurin git-lfs watchman ios-webkit-debug-proxy
```

## Setup JDK path

```
sudo ln -sfn /opt/homebrew/opt/openjdk@11/libexec/openjdk.jdk /Library/Java/JavaVirtualMachines/openjdk-11.jdk
echo 'export PATH="/opt/homebrew/opt/openjdk@ll/bin:$PATH"' >> ~/.zshrc
echo 'export JAVA_HOME=`/usr/libexec/java_home -v 11`' >> ~/.zshrc
```

## Install git-lfs

Git Large File Storage (LFS) manages the binaries that we need for Valdi.

```
git lfs install
```

## Install Android SDK (only required for Android development)

Download and install Android Studio by following [Google's directions](https://developer.android.com/studio).

Open the Android SDK Manager. You can find it on the main screen of Android Studio under `More Actions` -> `SDK Manager`. When a project is open, you can also find it under `Tools` -> `SDK Manager`.

Under **SDK Platforms**, install **API level 35**.

Under **SDK Tools**, uncheck `Hide obsolete packages` check `Show Package Details`

Install build tools **version 34.0.0**.

Install ndk version **25.2.9519653**

Install the latest version of **Android SDK Command-line Tools**

Update `.zshrc` with the following:

```
echo "export ANDROID_HOME=$HOME/Library/Android/sdk" >> ~/.zshrc
echo "export ANDROID_NDK_HOME=$HOME/Library/Android/Sdk/ndk/25.2.9519653" >> ~/.zshrc
echo "export PATH=\$PATH:$HOME/Library/Android/sdk/platform-tools" >> ~/.zshrc
source ~/.zshrc
```

# Next steps

[Valdi setup](https://github.com/Snapchat/Valdi/blob/main/docs/INSTALL.md#valdi-setup)
