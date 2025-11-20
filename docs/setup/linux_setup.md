# Getting ready to build things with Valdi on Linux

## About

Valdi needs a handful of dependencies in order to build. You can find information about installation in this doc.

This doc assumes you're using the default shell, bash. Setup should be possible for other setups but you're on your own for updating config files.

## Setup git-lfs deb

```
curl -s https://packagecloud.io/install/repositories/github/git-lfs/script.deb.sh | sudo bash
```

## apt-get install dpependencies

```
apt-get install npm openjdk-11-jdk git-lfs libfontconfig1-dev
```

## Install watchman

Watchman [install instructions](https://facebook.github.io/watchman/docs/install#linux).

## Install bazel

The recommended way to install Bazel is via Bazelisk. Follow instructions for your platform [here](https://github.com/bazelbuild/bazelisk/blob/master/README.md).

Or via npm:

`npm install -g @bazel/bazelisk`

## Install git-lfs

Git Large File Storage (LFS) manages the binaries that we need for Valdi.

```
git lfs install
```

# Install Android SDK (required for all platforms for now)

Download and install Android Studio by following [Google's directions](https://developer.android.com/studio).

Open the Android SDK Manager. You can find it on the main screen of Android Studio under `More Actions` -> `SDK Manager`. When a project is open, you can also find it under `Tools` -> `SDK Manager`.

Under **SDK Platforms**, install **API level 35**.

Under **SDK Tools**, uncheck `Hide obsolete packages` check `Show Package Details`

Install build tools **version 34.0.0**.

Install ndk version **25.2.9519653**

Install the latest version of **Android SDK Command-line Tools**

Add the following to your `.bashrc`

```
echo "export ANDROID_HOME=$HOME/Android/Sdk" >> ~/.bashrc
echo "export ANDROID_NDK_HOME=$HOME/Android/Sdk/ndk/25.2.9519653" >> ~/.bashrc
echo "export PATH=\$PATH:$HOME/Android/Sdk/tools" >> ~/.bashrc
source ~/.bashrc
```

# Next steps

[Valdi setup](https://github.com/Snapchat/Valdi/blob/main/docs/INSTALL.md#valdi-setup)

## Troubleshooting

### Warning: swap space

Bazel eats a lot of memory, if you see Java memory errors, you don't have enough swap space.

8GB should be enough for an Android build of the hello world app.
