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

# Android SDK and NDK

> **You do not need to install the Android SDK, build tools, or NDK manually.** Bazel downloads the correct versions hermetically during the build.

If you want to use Android Studio or `adb` outside of Bazel, you can optionally install the SDK via `valdi dev_setup` or [Android Studio](https://developer.android.com/studio).

# Next steps

[Installation guide](../INSTALL.md#installation)

## Troubleshooting

### Warning: swap space

Bazel eats a lot of memory, if you see Java memory errors, you don't have enough swap space.

8GB should be enough for an Android build of the hello world app.
