# Debugging Performance

## Tracing

To help you debug performance issues, Valdi provides a cross-platform tracing API. You can insert your own traces in your TypeScript code to understand how long your call is taking. The framework also traces some key events by default, which are covered in the section below `Framework events`.

### Recording traces

> [!NOTE]
> Please make sure to add `benchmarking` under `dependencies` and `allowed_debug_dependencies` in the module's `module.yaml` file.
> Also add `//src/valdi_modules/src/valdi/benchmarking` to the module's `BUILD.bazel` file.

#### `recordTracesWithTimeout()`

This method works on iOS, Android and Desktop, and requires the hot reloader to be connected. You will need to insert a call to `recordTracesWithTimeout()` from `TraceRecorder.ts` at the appropriate time, which depends on what you are trying to debug. This call will make the runtime starts collecting traces, and store the result as a Chrome Trace file on your local machine that you can open using Perfetto UI.

For instance, if you wanted to understand what is happening during the creation of your root component, you could insert a `recordTracesWithTimeout()` as follow:
```tsx
export class MyRootComponent extends Component {
  onCreate() {
    recordTracesWithTimeout(10000, '~/mytrace');
  }
}
```

After 10 seconds, you should see a message in the console like `Saved traces to ~/mytrace.trace`. You can then open the file using [Perfetto UI](https://ui.perfetto.dev/).

#### Using Android Systrace

This method works on Android only, and does not require the hot reloader to be connected. Valdi traces on Android are also emitted as Android systraces, which means you can use Android's scripts to start recording traces. You can find this script On Snapchat Android at `snapchat/scripts/trace/start_trace.sh`. The script will store the result as a html file on your local machine, which you can open using Perfetto.

### Adding your own traces

All the following functions are from `Trace.ts` in the `valdi_core` module.

#### trace()

This function allows to trace a specific section of your code and pass an arbitrary string to identify it.
Example:
```ts
function fn() {
  trace('BuildSection', () => {
    // Do some expensive work there
  });
}
```

#### `@Trace` annotation

This annotation can be put on a TypeScript class, and will make all its methods automatically traced. Traced methods using the annotation have lower overhead than the `trace()` function, and they can be thus put on hot code paths.
Example:
```ts
@Trace
class MyClass {
 // This method will be traced automatically as MyClass.aMethod when called
 aMethod() {
   // Do some work here
 }
}
```

#### `@TraceMethod` annotation

This annotation can be put on a single method in a TypeScript class. It is similar to `@Trace`, but is meant to trace a single function instead of all the functions of a class.
Example:
```ts
class MyClass {
 // This method will be traced as MyClass.aMethod when called
 @TraceMethod
 aMethod() { }
}
```

#### `installTraceProxy()`

The function takes a TypeScript class as its parameter, and will make all its methods automatically traced. It is used by the `@Trace` annotation under the hood. You can use this to inject traces in classes that already exist or are defined in files on which you don't have control on. If you wanted to trace the Renderer calls, as to see what is happening during rendering, you could do this:
```ts
// Will make all the methods of Renderer to be be shown in tracing
installTraceProxy(Renderer);
```

### Framework events

The Valdi runtime traces some default important events that happens during the lifecycle of your component. This section explains some of them:

- `Valdi.processRenderRequest`: TypeScript rendering has completed, and the framework is now processing the request to update the nodes. Every time a render happens at the TypeScript level, if the render has made any changes, a render request will be emitted that will end up being processed by the runtime. This is where the Valdi C++ runtime syncs its internal state with the state of the nodes in TypeScript.

> [!Note]
> The Valdi runtime uses **Source Maps** to symbolicate stack traces in the JavaScript engine. This ensures that performance traces and error reports provide meaningful information about your TypeScript code, even when running minified or compiled bytecode.

- `Valdi.updateViewTree`: The framework is going through the nodes and updating the view hierarchy to reflect the changes. This is where the backing views of the nodes are created/destroyed/recycled. This pass might happen after processing a render request, or when scrolling.
- `Valdi.setUserDefinedViewport`: The framework is reacting to a scroll change.
- `Valdi.updateVisibility`: The framework is resolving the viewports for the nodes. It is finding out which nodes are visible and which nodes are not visible on the screen.
- `Valdi.calculateLayout`: The framework is calculating the frames (rectangles) for the nodes. This can happen because nodes have been inserted/removed, because a layout attribute has changed (like `padding`), or because the available space for the root component has changed (for instance if the window has been resized). This can be an expensive operation. Because of this, you should avoid triggering changes in the elements that will cause a layout pass to happen when scrolling. If you need to move elements or show/hide them when scrolling, prefer using the `translationX`/`translationY` attributes or `opacity` which are not layout attributes and don't trigger layout passes when they change.

