# Valdi Module

A Valdi Module is a logical building block in a Valdi-based application. A typical Valdi module consists of TypeScript code and optional resources and localization data. The Valdi Compiler takes a module and produces a `.valdimodule` artifact used by the final application.

## Creating a new module

Valdi provides a convenient script to create a new module that sets up the basic structure and creates a few important files.

```sh
$ valdi new_module --help
valdi new_module [module-name]


******************************************
Valdi Module Creation Guide
******************************************

Requirements for Valdi module names:
- May contain: A-Z, a-z, 0-9, '-', '_', '.'
- Must start with a letter.

Recommended Directory Structure:
my_application/          # Root directory of your application
├── WORKSPACE            # Bazel Workspace
├── BUILD.bazel          # Bazel build
└── modules/
    ├── module_a/
    │   ├── BUILD.bazel
    │   ├── android/     # Native Android sources
    │   ├── ios/         # Native iOS sources
    │   ├── cpp/         # Native C++ sources
    │   └── src/         # Valdi sources
    │       └── ModuleAComponent.tsx
    ├── module_b/
        ├── BUILD.bazel
    │   ├── res/         # Image and font resources
    │   ├── strings/     # Localizable strings
        └── src/
            └── ModuleBComponent.tsx
```

## Project Structure

A Valdi project is defined by:
- **`WORKSPACE`**: The Bazel workspace file that defines external dependencies. Created automatically by `valdi bootstrap`.
- **`.bazelrc`**: Bazel configuration file with build settings.
- **User Config** (`~/.valdi/config.yaml`): Per-user CLI settings (created by `valdi bootstrap`).
- **Modules**: A list of directories, each containing a `BUILD.bazel` file defining a Valdi module.

Run `valdi bootstrap` to initialize a new project with the correct structure.

### Module Archives
The compiler outputs a `.valdimodule` file for each module, which is a bundled archive of compiled TypeScript and metadata. Modules can reference each other as dependencies using standard Bazel target syntax (e.g., `//src/valdi/other_module`).

## Module directory layout

### `res`

The folder with resources (eg: images).

### `src`

The folder with TypeScript sources.

### `strings`

The folder with localized strings.

## `BUILD.bazel`

The main configuration file of a Valdi module.

> **Note:** Older Valdi projects may have `module.yaml` files. These are deprecated in favor of `BUILD.bazel` configuration. All module configuration should now be done in the `valdi_module()` rule in `BUILD.bazel`.

```python
load("@valdi//bzl/valdi:valdi_module.bzl", "valdi_module")

valdi_module(
    name = "hello_world",
    srcs = glob([
        "src/**/*.ts",
        "src/**/*.tsx",
    ]),
    android_class_path = "com.snap.valdi.modules.hello_world",
    android_deps = ["//src/android:native_module_android"],
    ios_deps = ["//src/ios:native_module_ios"],
    ios_module_name = "SCCHelloWorld",
    res = glob([
        "res/**/*.jpeg",
        "res/**/*.jpg",
        "res/**/*.png",
        "res/**/*.svg",
        "res/**/*.webp",
    ]),
    visibility = ["//visibility:public"],
    deps = [
        "@valdi//src/valdi_modules/src/valdi/valdi_core",
        "@valdi//src/valdi_modules/src/valdi/valdi_tsx",
        "@valdi_widgets//valdi_modules/widgets",
        "//src/valdi/some_other_valdi_module",
    ]
)
```

#### `name`

The name of the Valdi module.

#### `srcs`

glob of the sources to include in the build.

#### `ios_deps`

bazel path of iOS sources this module depends on.

#### `ios_module_name`

Class prefix to use for the generated Objective-C classes.

#### `ios_output_target` (Default: `debug`)

One of: `debug` or `release`.
Change the output_target to `release` if you'd like your module to be available in non-debug builds. Defaults to `debug` to avoid accidentally leaking in-development features.

#### `android_class_path`

Package prefix to use for generated Kotlin classes.

#### `android_output_target` (Default: `debug`)

One of: `debug` or `release`.
Change the output_target to `release` if you'd like your module to be available in non-debug builds. Defaults to `debug` to avoid accidentally leaking in-development features.

#### `res`

glob of the resources to include in the build.

#### `visibility`

See [Bazel Visibility](https://bazel.build/concepts/visibility).

#### `deps` (Default: `[]`)

A list of Valdi modules this module depends on.

#### `strings_dir` (Default: `None`)

The directory containing the strings files. See [Localization](./advanced-localization.md) for details on string file format.

#### `android_deps` (Default: `[]`)

List of native Android dependencies (Bazel targets) that this module requires.

#### `native_deps` (Default: `[]`)

List of native code dependencies (C++) that this module requires.

#### `web_deps` (Default: `[]`)

List of web-specific dependencies that this module requires.

#### `compilation_mode` (Default: `js_bytecode`)

The JavaScript compilation mode for the module. Options:
- `js_bytecode` - Compile to bytecode (default, best performance)
- `js` - Keep as JavaScript (easier debugging)
- `native` - Native compilation

#### `downloadable_assets` (Default: `True`)

Whether module resources can be downloaded remotely at runtime (`True`) or must be bundled with the app (`False`).

#### `inline_assets` (Default: `False`)

Whether to inline assets directly into the module instead of as separate resource files.

#### `disable_annotation_processing` (Default: `False`)

Disable processing of TypeScript annotations (e.g., `@ExportModel`). Use when module doesn't need native code generation.

#### `disable_dependency_verification` (Default: `False`)

Skip verification of module dependencies. Generally not recommended.

#### `disable_code_coverage` (Default: `False`)

Disable code coverage instrumentation for this module.

#### `disable_hotreload` (Default: `False`)

Disable hot reload support for this module. Use for performance-critical modules.

#### `single_file_codegen` (Default: `True`)

Use single-file code generation mode. When `True`, generates one file per module instead of multiple files.

#### `ios_language` (Default: `objc`)

Language for generated iOS code. Options:
- `objc` - Objective-C
- `swift` - Swift  
- `both` - Both Objective-C and Swift

#### `sql_db_names` (Default: `None`)

List of SQL database names if this module uses SQL databases.

#### `sql_srcs` (Default: `[]`)

SQL source files to include in the module.

## Module Loading & Execution

### `JsEvaluator`
The Valdi C++ runtime uses a `JsEvaluator` to load and execute modules. When a module is requested, the evaluator:
1. Locates the file within the `.valdimodule` archive.
2. Evaluates the code within the JavaScript engine (QuickJS or JavaScriptCore).
3. Returns a module function that encapsulates the module's scope.

### Lazy Loading
To optimize application startup time, Valdi uses **Lazy Module Loading**. When you `import` or `require()` a module, Valdi returns an ES6 Proxy instead of the actual exports. The module's code is only evaluated the first time you access a property on that proxy.

This ensures that a large Valdi project doesn't slow down the "cold start" of your application, as only the code path required for the initial screen is executed.

For more details on the runtime implementation, see [Runtime Internals](./internals-runtime.md).

## `ids_yaml`

File with accessibility ids. Refer to the [testing](./workflow-testing.md) documentation on how it can be used to implement UI tests.

See also: ([iOS](../codelabs/integration_with_native/ios/5-ios_testing.md), [Android](../codelabs/integration_with_native/android/5-android_testing.md)) on how it can be used to implement UI tests.

## `protodecl_srcs`

If protobuf is used, refer to [Protobuf Integration](./advanced-protobuf.md#protobuf-config-file).
