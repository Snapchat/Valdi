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
In this codelab, we're working with the `tsconfig.json` and `module.yaml` defaults, but you may need custom configuration in your own projects.

### tsconfig.json
`tsconfig.json` specifies the TypeScript compiler options. This is a standard config file, you can read more about it in the [official TypeScript documentation](https://www.typescriptlang.org/docs/handbook/tsconfig-json.html).

### module.yaml
`module.yaml` is specific to Valdi. This config file controls the compiled module output. We'll cover a few common options here but the full set of options is available in the [Core Module documentation](../../docs/core-module.md#moduleyaml).

Common options:
- **`output_target`**: this can be specified globally or on a per platform basis
    - **`debug`**: for local testing and development, won't become part of release builds
    - **`release`**: ready for production
- **`dependencies`**: the other modules that this module depends on
- **`strings_dir`**: The location of your strings files
- **`ios`**: this separates iOS specific configuration
    - **`module_name`**: used for specifying build targets and naming directories
    - **`class_prefix`**: used as a prefix for generated native code (usually the same as **`module_name`**)
    - **`output_target`**: the same as discussed above but specific to iOS
- **`android`**: Android specific configuration
    - **`class_path`**: a Kotlin class path for the generated native code
    - **`output_target`**: as discussed above
- **`downloadable_assets`**: When enabled, assets are hosted on remote asset storage rather than bundled with the app, which reduces binary size. Note: this feature requires configuring a remote asset storage backend and is not recommended for open source projects without additional setup.
- **`disable_precompilation`**: This only impacts Android and refers to compiling javascript into bytecode that the Android JS engine quickly run. Disabling precompilation will reduce binary size but lead to slower cold starts because the JS engine needs to parse the raw JS file.

## `valdimodule` troubleshooting
Any time you update the annotations or the definitions of the **Context**, **ViewModel**, or **Friend** objects, you will need to run the `valdi projectsync` script to regenerate the native code.

## Choose your own adventure
This is the last step that applies to both Android and iOS. From here, pick one platform and follow the path to the end before coming back to do the other one (if you want).

### [iOS (Objective-C)>](../integration_with_native/ios/1-ios_setup_for_development.md)
### [iOS (Swift)>](../integration_with_native/ios_swift/1-ios_setup_for_development.md)
### [Android >](../integration_with_native/android/1-android_setup_for_devleopment.md)

