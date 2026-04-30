# Valdi

Valdi is a language, framework, and view abstraction layer that lets you declaratively build complex UIs.

## Build for macOS

Open `client/src/open_source/compiler/compiler/Compiler/Package.swift` in Xcode and allow it to resolve the dependencies, and then it should build successfully.

## Build for Ubuntu 18.04

Run client/src/dev_setup.sh. This should download and install Swift, clone and build watchman from source and install all the necessary apt-get dependencies.

## Building the compiler locally

To build and use the compiler from source (instead of the prebuilt binary):

```sh
compiler/compiler/scripts/update_compiler.sh -s -o compiler/compiler/out
```

- `-s` skips analytics upload (recommended for local builds)
- `-o compiler/compiler/out` writes the binary to `out/macos/valdi_compiler` (or `out/linux/valdi_compiler` on Linux)

The `out/` directory is gitignored.

To use your local build with Bazel, pass `--//bzl/valdi:use_local_compiler=true`:

```sh
bazel build //src/valdi_modules/src/valdi/jasmine --//bzl/valdi:use_local_compiler=true
```

Or via the CLI hotreload command:

```sh
valdi hotreload --local
```

See [docs/docs/workflow-bazel.md](../../docs/docs/workflow-bazel.md) for more details.

## Updating the prebuilt compiler binary in the repo

To update the prebuilt binary that gets downloaded by other developers (SnapCI job):

```sh
./scripts/update_compiler.sh
```

## Debugging C++ JNI crashes

- Copy paste the native crash from adb logcat into a txt file. It should have the **\* \*** etc... at the beginning
- Run this:

```
$ANDROID_NDK/ndk-stack -sym "{PATH_TO_CLIENT_REPO}/src/buck-out/gen/client-jni#android-armv7,shared/"
```

It will show you the files and lines of the stacktrace.

## Troubleshooting

My breakpoints are not activating in Xcode.

1. Close Xcode
2. Delete Xcode cache

   - `rm -rf ~/Library/Developer/Xcode/DerivedData/*`
   - `rm -rf ~/Library/Caches/com.apple.dt.Xcode`

3. Delete ~/.lldbinit-Xcode

   - `rm -rf ~/.lldbinit-Xcode`
   - Optionally `rm -rf ~/.lldbinit-*`

4. Kill lldb-rpc-server process (may not be active if Xcode was previously closed

   - `pkill -9 "lldb-rpc-server"`

5. Open Xcode, build and run the project again.
