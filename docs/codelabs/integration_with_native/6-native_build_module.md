# Build a new Valdi module
If we were working with pure TypeScript, the hotreloader would handle our new module but we want to work with native code so we need to compile our code to generate the native classes and createa a **valdimodule**.

First, specify what native class names you want to use in the `@ExportModel` annotation for your component.

```typescript
/**
 * @Component
 * @ExportModel({
 *  ios: 'SCCHelloWorldView',
 *  android: 'com.snap.playground.HelloWorldView'
 * })
 */
export class HelloWorldComponent extends StatefulComponent<ViewModel, State, Context> { 
```

Do the same for the **Context**, **ViewModel**, and **Friend** objects.

```typescript
/**
 * @ViewModel
 * @ExportModel({
 *  ios: 'SCCHelloWorldViewModel',
 *  android: 'com.snap.playground.HelloWorldViewModel'
 * })
 *
 * Represents the input parameters for your Component
 */
interface ViewModel {
  // ... define interface
}

/**
 * @Context
 * @ExportModel({
 *  ios: 'SCCHelloWorldContext',
 *  android: 'com.snap.playground.HelloWorldContext'
 * })
 *
 * Represents the shared Context for your Component and all its child Components.
 * Typically used to provide any bridging methods that native code will implement.
 */
interface Context {
  // define interface
}

/**
 * @ExportModel({
 *  ios: 'SCCHelloWorldFriend',
 *  android: 'com.snap.playground.HelloWorldFriend'
 * })
 */
export interface Friend {
  // define interface
}
```

Resynchronize your project.

```
valdi projectsync
```

## Configuring a module
In this codelab, we're working with the `tsconfig.json` and `BUILD.bazel` defaults, but you may need custom configuration in your own projects.

### tsconfig.json
`tsconfig.json` specifies the TypeScript compiler options. This is a standard config file, you can read more about it in the [official TypeScript documentation](https://www.typescriptlang.org/docs/handbook/tsconfig-json.html).

### BUILD.bazel
`BUILD.bazel` configures the compiled module output via a single `valdi_module()` call. We'll cover a few common attributes here but the full set is documented in [Core Module](../../docs/core-module.md#buildbazel).

Common attributes:
- **`ios_output_target`** / **`android_output_target`**: one per platform
    - **`debug`**: for local testing and development, won't become part of release builds
    - **`release`**: ready for production
- **`deps`**: the other Valdi modules this module depends on (Bazel labels)
- **`strings_dir`**: the location of your strings files
- **`ios_module_name`**: used for specifying iOS build targets and naming directories
- **`ios_class_prefix`**: prefix for generated iOS native code (usually the same as `ios_module_name`)
- **`ios_language`**: `objc`, `swift`, or `["objc", "swift"]`
- **`android_class_path`**: Kotlin package path for generated Android code
- **`downloadable_assets`**: when `True`, assets are hosted on remote asset storage rather than bundled with the app, which reduces binary size. This requires configuring a remote asset storage backend and is not recommended for open source projects without additional setup.

> **Note:** Older Valdi projects may have a `module.yaml` file alongside `BUILD.bazel`. `module.yaml` is deprecated; all module configuration belongs in the `valdi_module()` rule. See [glossary](../../docs/glossary.md#moduleyaml).

## `valdimodule` troubleshooting
Any time you update the annotations or the definitions of the **Context**, **ViewModel**, or **Friend** objects, you will need to run the `valdi projectsync` script to regenerate the native code.

## Choose your own adventure
This is the last step that applies to both Android and iOS. From here, pick one platform and follow the path to the end before coming back to do the other one (if you want).

### [iOS (Objective-C)>](../integration_with_native/ios/1-ios_setup_for_development.md)
### [iOS (Swift)>](../integration_with_native/ios_swift/1-ios_setup_for_development.md)
### [Android >](../integration_with_native/android/1-android_setup_for_development.md)

