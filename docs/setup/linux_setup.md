# Linux Setup Reference Guide

> **Get the CLI from npm:** `npm install -g @snap/valdi`. Then run `valdi dev_setup` for automated setup. This guide is a reference for manual installation or troubleshooting only.

## About

This guide documents the dependencies Valdi needs on Linux and how to install them manually. For the quickest setup, use [`valdi dev_setup`](../INSTALL.md) which automates all of these steps.

This guide assumes you're using the default shell (bash). Setup is possible for other shells, but you'll need to adapt the configuration file paths.

## Setup git-lfs deb

```
curl -s https://packagecloud.io/install/repositories/github/git-lfs/script.deb.sh | sudo bash
```

## apt-get install dependencies

On Debian/Ubuntu, install the same dependencies that `valdi dev_setup` would use:

```
apt-get install npm openjdk-17-jdk git-lfs watchman adb libfontconfig1-dev zlib1g-dev
```

(On other distros, use the equivalent packages: e.g. RHEL/Fedora use `java-17-openjdk-devel`, `android-tools`, `fontconfig-devel`, `zlib-devel`. The CLI detects your distro and installs the right packages.)

## Install bazel

> [!NOTE]
> **`valdi dev_setup` installs Bazelisk automatically.** It downloads the bazelisk binary to `~/.valdi/bin/` and adds it to your PATH.

For manual installation, follow the [Bazelisk installation guide](https://github.com/bazelbuild/bazelisk/blob/master/README.md) or install via npm:

```bash
npm install -g @bazel/bazelisk
```

## Install git-lfs

Git Large File Storage (LFS) manages the binaries that we need for Valdi.

```
git lfs install
```

# Install Android SDK

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

Add the following to your `.bashrc`

```
echo "export ANDROID_HOME=$HOME/Android/Sdk" >> ~/.bashrc
echo "export ANDROID_NDK_HOME=\$ANDROID_HOME/ndk-bundle" >> ~/.bashrc
echo "export PATH=\$ANDROID_HOME/platform-tools:\$PATH" >> ~/.bashrc
source ~/.bashrc
```

# Next steps

[Installation guide](../INSTALL.md#installation)

## Troubleshooting

### Warning: swap space

Bazel eats a lot of memory, if you see Java memory errors, you don't have enough swap space.

8GB should be enough for an Android build of the hello world app.
