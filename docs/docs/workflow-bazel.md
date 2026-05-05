# Valdi rules for Bazel

Each [valdi module](../../valdi_modules/src/valdi) contains a `BUILD.bazel` file which has a single call to `valdi_module` macros. The macro setups a few targets and aliases to build the module from source using the Valdi Toolchain.

The list of targets created by the `valdi_module` macro includes:

* `module_name:module_name` target which builds just the `.valdimodule` file. It is the default target and can be referenced as `module_name` for example `//src/valdi_modules/src/valdi/jasmine`.
* `module_name:module_name_kt` target which builds the `android_library` and packages the Valdi generated Kotlin code, if any. It depends on the Valdi module target above and adds the `.valdimodule` and other resources to the `data` attribute.
* `module_name:module_name_objc` target which builds the `objc_library` and packages the Valdi generated Objective-C code, if any. It depends on the Valdi module target above and adds the `.valdimodule` and other resources to the `data` attribute.
* `module_name:module_name_objc_api` target which builds the `objc_library` and wraps the generated Objective-C API code, if any. It also depends on the `module_name_objc` target above.
* `module_name:{ModuleName}` an alias pointing at `module_name:module_name_objc` to keep the compatibility with iOS
* `module_name:{ModuleName}Types` an alias pointing at `module_name:module_name_objc_api` to keep the compatibility with iOS

## Building

To build a single module using the default, macOS toolchain:

```sh
bazel build //src/valdi_modules/src/valdi/jasmine
```

### Android

To build the Android target:

```sh
bazel build //src/valdi_modules/src/valdi/jasmine --platforms=@snap_platforms//os:android_arm64
```

### iOS

To build the iOS target:
```sh
bazel build //src/valdi_modules/src/valdi/jasmine --platforms=//bzl/platforms:ios_x64
```

## Maintaining `BUILD.bazel` files

To ease the burden of writing and maintainting `BUILD.bazel` Valdi provides a script to automatically generate/update/format `BUILD.bazel` files based on `module.yaml` content. In addition to that, the CI infrastructure includes a pre-cool hook to validate `BUILD.bazel` files.

To add a new dependency:

1. Update `module.yaml`
2. Run the script:

```sh
./scripts/regenerate_valdi_modules_build_bazel_files.sh
```

## Testing

You can run your tests using `bazel test` directly. See documentation about it here: [Testing](./workflow-testing.md).

## Linting

Once implemented, users will be able to run linting for a single module using Bazel.

## Rebuilding the compiler and companion

By default, Bazel uses the **prebuilt compiler** and builds the **companion from source** (the companion is the JS/TS app in `compiler/companion`). To use your own builds so they are not overwritten by prebuilts:

### Using a locally built compiler


1. Build the Valdi compiler and place the binary where the toolchain can find it:
   ```sh
   compiler/compiler/scripts/update_compiler.sh -s -o compiler/compiler/out
   ```
   This produces `compiler/compiler/out/macos/valdi_compiler` (or `out/linux/valdi_compiler` on Linux). The `-s` flag skips analytics upload. The script works in both the mirrored public repo and the mobile monorepo.

2. Build Valdi modules with the local compiler:
   ```sh
   bazel build //src/valdi_modules/src/valdi/jasmine --//bzl/valdi:use_local_compiler=true
   ```

The `out/` directory is gitignored so your local binary is never committed.

> [!TIP]
> To confirm the local binary is being picked up, check the Bazel worker log after the first build:
> ```sh
> cat "$(ls ~/.cache/bazel/*/bazel-workers/worker-*-ValdiCompile.log 2>/dev/null | head -1)"
> ```
> The first line should read `[local-compiler] valdi compiler (local build) starting` (or whatever marker you added). The worker log path is also printed by Bazel when `--worker_verbose` is passed.

### Using a locally built companion

The toolchain already uses the **source-built companion** by default (`use_prebuilt_companion` is false), so the companion is built from `compiler/companion` (e.g. webpack output). To avoid Bazel overwriting your npm-built companion output:

1. Build the companion in dev mode (writes to `dist_dev/` instead of `dist/`):
   ```sh
   cd compiler/companion && npm run build-dev
   ```

2. Build or run Valdi with the dev-mode companion:
   ```sh
   bazel build //src/valdi_modules/src/valdi/jasmine --//compiler/companion:dev_mode=true
   ```

Bazel will use `dist_dev/bundle.js` and will not overwrite your `dist/` folder.

## Known issues and limitations

* Updating proto config doesn't trigger a valdi module rebuild
