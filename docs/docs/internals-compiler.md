# Valdi Compiler Internals

The Valdi Compiler is a sophisticated toolchain that transforms TypeScript source code, resources, and configuration into optimized `.valdimodule` artifacts and native bindings.

## Compiler Architecture

The compiler uses a **Pipeline-based Architecture** where inputs are transformed through a series of specialized processors.

### Compilation Items
Every input to the compiler is represented as a `CompilationItem`. Items are immutable objects that can represent:
- Source files (TypeScript, JavaScript, TSX)
- Resources (Images, SVG, Fonts)
- Metadata (OWNERS files, `strings.yaml`)
- Intermediate states (Exported types, processed assets)
- Final artifacts (Minified JS, bytecode, native headers)

### Compilation Processors
A `CompilationProcessor` handles specific item types and emits zero or more new items. 
- **Example**: The Image Processor takes an SVG `CompilationItem` and emits multiple PNG and WebP items for different display densities.
- **Example**: The Save Processor takes "Final File" items and writes them to the output directory.

## TypeScript XML (TSX) Compilation

Valdi uses a custom TSX transformer built on top of the TypeScript compiler. Unlike React, which transforms TSX into `createElement` calls, Valdi transforms them into a high-performance stack-based API.

### Transformation Example

**Input Code:**
```tsx
function render() {
  <view>
    <layout />
  </view>
}
```

**Generated Output:**
```ts
const __Renderer = require('valdi_core/jsx/JSX').jsx;
const __nodeView1 = __Renderer.makeNodePrototype('view');
const __nodeLayout1 = __Renderer.makeNodePrototype('layout');

function render() {
  __Renderer.beginRender(__nodeView1);
  __Renderer.beginRender(__nodeLayout1);
  __Renderer.endRender();
  __Renderer.endRender();
}
```

### Key Optimizations
- **Prototypes**: `makeNodePrototype` is called once at the module level. This blueprint is reused across all render passes.
- **Specialized Setters**: The compiler detects attribute types and uses specialized functions (e.g., `setAttributeNumber`, `setAttributeString`) to avoid type introspection at runtime.
- **Static Properties**: Attributes that never change are passed directly into the prototype, removing them from the per-render execution path.

## Transformation Nuances

### Elements vs. Components
- **Elements**: Defined by lowercase tags (e.g., `<view>`). Transformed into `beginRender` / `endRender` calls using a `NodePrototype`.
- **Components**: Defined by PascalCase tags (e.g., `<Header>`). Transformed into `beginComponent` / `endComponent` calls using a `ComponentPrototype`.

### Slots
Children passed to a component are automatically wrapped in a render function and passed via `setSlot`. The component then uses `renderSlot` to evaluate that function at the desired location in its tree.

## Compiler Pipeline Responsibilities

The compiler performs several critical tasks beyond simple code transformation:

1. **Asset Generation**: Automatically generates missing image variants (densities) and compresses them.
2. **Asset Catalog**: Generates a type-safe TypeScript accessor for all assets. It also pre-calculates image dimensions so the runtime can perform layout without loading the image data.
3. **Localization**: Transforms `strings.yaml` into TypeScript accessors and native platform string files (`Localizable.strings` for iOS, `strings.xml` for Android).
4. **TSX to TS Transformation**: Our custom transformer converts TSX expressions into high-performance stack-based API calls.
5. **Minification**: Uses Terser to reduce JavaScript code size.
6. **Bytecode Compilation**: On Android, the compiler produces QuickJS-compatible bytecode for even faster execution.
7. **Native Bindings**: Generates Objective-C, Swift, and Kotlin models and view classes from TypeScript code annotations (`@ExportModel`, `@Component`).
8. **Build System Integration**: Generates Bazel `BUILD.bazel` files so each module can be referenced in the main build.
9. **Source Maps**: Generates source maps to enable symbolic stack traces in the runtime.

## Compiler Companion

The compiler utilizes a "Companion" binary implemented in TypeScript. The main compiler (written in Swift) makes the following requests to the companion via a JSON protocol:
- **Minify JS**: Invokes Terser.
- **Compile TS/TSX**: Invokes the custom TypeScript transformer.
- **Extract Symbols**: Finds TypeScript symbols with comments for code annotation processing.
- **Dump AST**: Provides AST information (interfaces, enums, function signatures) for native binding generation.
- **Debug Proxy**: Sets up a proxy socket for VSCode's JS debugger.
