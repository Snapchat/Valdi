# Setup your Module
The time has come to create your own unit of Valdi. Features are separated into modules.

## What is a module
In a project created with `valdi bootstrap` (see [INSTALL.md](../../INSTALL.md)), modules live in `modules/<module_name>` relative to the project root.

A module is a set of typescript classes, components, and functions. Modules may, but do not have to, contain image assets, localized strings, and TypeScript tests.

Modules can depend on other modules.

All of these things get compiled down into **valdimodule** files that get read by the **Valdi Runtime** and render natively on each platform.

## Create a new module
The Valdi CLI can create a new module for you that sets up the basic structure and creates a few important files. Run it from the root of your project:

```
valdi new_module hello_world --template ui_component
```

This creates a new module called `hello_world` in `modules/hello_world`. Then regenerate the editor configuration so your editor picks up the new module:

```
valdi projectsync
```

Open the project in your editor and navigate to `modules/hello_world`. You should see:

- `BUILD.bazel` - `valdi_module()` declaration with the module's dependencies and build configuration
- `src/` - TypeScript code
    - `NewModuleComponent.tsx` - Starter UI component

Modules can also contain `res/` (image assets) and localized strings; see [Valdi Module](../../docs/core-module.md).

The defaults in `BUILD.bazel` will work fine for our purposes but feel free to check them out.

`NewModuleComponent.tsx` is the default starting point for UI Components but we won't need it. Go ahead and delete it.

Instead, create a new TypeScript file `Calculator.ts`. This should be a TypeScript file not a TSX file. Learn more about TSX in the [**Getting Started codelab**](../getting_started/4-declarative_rendering.md#tsxjsx).

### [Next >](./3-business_logic.md)
