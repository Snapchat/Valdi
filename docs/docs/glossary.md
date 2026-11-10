# Glossary

A comprehensive guide to Valdi terminology and concepts.

## A

### Annotation
TypeScript decorator used by the Valdi compiler to generate native code. Examples: `@ExportModel`, `@Context`, `@ViewModel`. See [Native Annotations](./native-annotations.md).

### Asset
A resource file (image, font, video, etc.) bundled with a Valdi module. Can be referenced using the `Asset` type. See [Images](./core-images.md).

## B

### Bridge / Bridging
The mechanism for passing data and invoking methods between TypeScript/JavaScript and native (iOS/Android) code. A "bridged" architecture uses native implementations for data/services. See [Native Bindings](./native-bindings.md) and [Full Stack Valdi](./advanced-full-stack.md).

### BUILD.bazel
The Bazel build configuration file for a Valdi module. Replaces the deprecated `module.yaml`. See [Valdi Module](./core-module.md).

## C

### Component
A reusable UI building block implemented in TypeScript. Components have lifecycle methods and can contain other components or elements. See [The Mighty Component](./core-component.md).

### Component Context
Data and services passed from native code to a Valdi component at instantiation time. Used to provide platform-specific functionality. See [Component Context](./native-context.md).

### ComponentDisposable
A function or RxJS Subscription that can be registered with `registerDisposable()` to be automatically cleaned up when a component is destroyed.

### Custom View
A native iOS or Android view wrapped for use in Valdi templates using the `<custom-view>` element. See [Custom Views](./native-customviews.md).

## D

### Declarative Rendering
A programming paradigm where you describe *what* the UI should look like based on state, rather than *how* to update it. Valdi uses TSX to declaratively define UI.

### Diff Algorithm
The process Valdi uses to compare the previous and current render output to determine what changed and needs to be updated in the native UI.

## E

### Element
A native UI primitive provided by Valdi, such as `<view>`, `<label>`, `<image>`. Elements are lowercase and backed by native views. See [API Reference](../api/api-reference-elements.md).

### Element Reference
A way to get direct access to a native element instance from TypeScript. See [Element References](./advanced-element-references.md).

### ExportModel
An annotation that marks a TypeScript interface or class to be exported to native code. See [Native Annotations](./native-annotations.md).

### ExportProxy
An annotation that marks a TypeScript interface to generate a native protocol/interface that can be implemented in native code. See [Native Annotations](./native-annotations.md).

## F

### FlexBox
A layout system borrowed from CSS that allows efficient arrangement of elements. Valdi uses Facebook's Yoga engine for flexbox. See [FlexBox Layout](./core-flexbox.md).

### Full-Stack Valdi
An architecture where the majority of a feature (including data/services) is implemented in Valdi/TypeScript rather than native code. See [Full Stack Valdi](./advanced-full-stack.md).

## H

### Hot Reload
The ability to update running code without restarting the application. Valdi supports hot reloading of TypeScript code. See [Getting Started](./start-install.md).

### Host (Worker Service)
The TypeScript main thread that creates and communicates with worker services.

## I

### IStyle
The TypeScript interface that defines style properties for elements. See [Style Attributes](../api/api-style-attributes.md).

## J

### JSX / TSX
TypeScript extension syntax for writing declarative UI. Valdi uses TSX in `onRender()` methods. Example: `<view><label value="Hello" /></view>`

## L

### Layout Node
A `<layout>` element that performs layout calculations but doesn't create a native view. More efficient than `<view>` when you don't need view properties. See [Optimization](./performance-optimization.md).

### Lifecycle Methods
Methods on a Component that are called at specific times: `onCreate()`, `onViewModelUpdate()`, `onRender()`, `onDestroy()`. See [Component Lifecycle](./core-component.md#lifecycle).

## M

### Marshalling
The process of converting data between TypeScript and native representations when crossing the bridge. Has a performance cost for complex objects.

### Module
A logical unit of Valdi code that can include TypeScript sources, resources, native code, and configuration. Compiled into a `.valdimodule` file. See [Valdi Module](./core-module.md).

### module.yaml
**Deprecated.** Old configuration format for Valdi modules, replaced by `BUILD.bazel`.

## N

### Native Annotations
TypeScript decorators that instruct the Valdi compiler to generate native code. See [Native Annotations](./native-annotations.md).

### Native Reference
A way to pass references to native objects (that aren't data) to Valdi. See [Native References](./advanced-native-references.md).

### Native Template Element
An element like `<view>` or `<label>` that is backed by native code. Different from components which are TypeScript.

## O

### Observable
An RxJS primitive for handling asynchronous data streams. Valdi has special support for bridging Observables to native code. See [RxJS](./client-libraries-rxjs.md).

## P

### Polyglot Module
A Valdi module that includes native code (iOS/Android/C++) in addition to TypeScript. See [Polyglot Modules](./native-polyglot.md).

### Promise
An asynchronous operation that can be passed across the native bridge. Valdi automatically handles Promise conversion. See [Native Types](./native-types.md).

### Props
React terminology for component parameters. In Valdi, these are called "ViewModel".

### Provider
A pattern for dependency injection in Valdi, allowing services to be accessed by components. See [Provider](./advanced-provider.md).

## R

### Reactive Programming
A programming paradigm based on data streams and change propagation. Valdi's UI automatically updates when state changes.

### Render Tree
The hierarchy of components and elements that make up the UI at any given time.

### Runtime
The Valdi engine that executes TypeScript code and manages the native UI. Each application needs a `ValdiRuntime` instance.

## S

### Slot
A pattern for passing child elements to a component. Similar to React's `children` prop. See [Slots](./core-slots.md).

### State
Internal data within a component that triggers re-renders when updated via `setState()`. See [Component States](./core-states.md).

### `Style<T>`
An object containing style properties for an element. The type parameter specifies which element it's for. See [Styling](./core-styling.md).

### StatefulComponent
A component class that has internal state managed via `this.state` and `setState()`. See [Component States](./core-states.md).

## T

### Type Converter
A mechanism for converting custom types between TypeScript and native code. See [Native Types](./native-types.md).

### Type Marshalling
See **Marshalling**.

## U

### Unsubscribable
An object with an `unsubscribe()` method, typically an RxJS Subscription. Can be registered with `registerDisposable()`.

## V

### Valdi Module
See **Module**.

### valdimodule File
The compiled output of a Valdi module, containing bytecode, resources, and metadata. Used at runtime.

### View
In Valdi context, usually refers to the `<view>` element. In native context, refers to iOS `UIView` or Android `View`.

### ViewModel
The parameters/props passed to a component from its parent. Changing the ViewModel triggers a re-render. See [View Model](./native-view-model.md).

### View Recycling
An optimization technique where native views are reused rather than destroyed and recreated. See [View Recycling](./performance-view-recycling.md).

## W

### Worker Service
A service that runs in a background TypeScript thread to avoid blocking UI operations. See [Worker Service](./advanced-worker-service.md).

### Worker (Worker Service)
The background thread that executes worker service code.

### WORKSPACE
The root Bazel configuration file that defines external dependencies and workspace settings.

## Y

### Yoga
Facebook's cross-platform flexbox layout engine used by Valdi for layout calculations. See [Style Attributes - Yoga](../api/api-style-attributes.md#yoga-layout-engine).

---

## Related Documentation

- [Native Annotations](./native-annotations.md) - Full list of available annotations
- [Native Bindings](./native-bindings.md) - Complete type system reference
- [API Reference](../api/api-reference-elements.md) - All native elements
- [Component Lifecycle](./core-component.md#lifecycle) - Lifecycle methods detail

## Contributing to the Glossary

If you encounter a term that should be added to this glossary, please consider contributing. See the main README for contribution guidelines.

