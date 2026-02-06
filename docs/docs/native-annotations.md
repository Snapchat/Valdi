# Annotations

## Concept

The Valdi compiler integrates tightly with the TypeScript compiler. It supports a few annotations, that can be used to associate TypeScript types with some Valdi concepts.

The annotations are declared in comments, because TypeScript does not support compile time annotations.

> [!Warning]
> When composing multiple classes with `@Export*`, e.g. having an `@ExportModel` `Component` which exposes a `@ExportModel` on a `Context`, ensure that they are all in the same file. This restriction does not apply for pure TS/JS usages, only for when where generated native code need to depend on other generated native code.


## Example

### TypeScript annotated code

```ts
interface MyComponentViewModel {}
/**
 * @Context
 * @ExportModel({
 *   ios: 'SCMyComponentContext',
 *   android: 'com.snap.myfeature.MyComponentContext'
 * })
 */
interface MyComponentContext {
    // IMPORTANT NOTE: using optional fields will allow mutating fields
    // without breaking the consuming native android and iOS code:
    // - optional fields will not be initialized in the constructor
    // - non-optional fields will require to be initialized in the constructor
    str?: string;
    callback?: () => void;
}
/**
 * @Component
 * @ExportModel({
 *   ios: 'SCMyComponentView',
 *   android: 'com.snap.myfeature.MyComponentView'
 * })
 */
class MyComponent extends Component<MyComponentViewModel, MyComponentContext> {
    onRender() {
        <view onTap={this.onTap}>
            <label str={"context.str:" + this.context.str ?? ""}/>;
        </view>
    }
    onTap = () => {
        if (this.context.callback) {
            this.context.callback()
        }
    }
}
```

### Usage in Obj-c

```objectivec
// Init context
SCMyComponentContext *context = [[SCMyComponentContext alloc] init];
context.str = @"Hello!"; // NOTE: since the field is declared optional in typescript, it can be initialized independently of the constructor
context.callback = ˆ{
    NSLog("the context's callback was called from typescript!");
}
// Init view
UIView *view = [[SCMyComponentView alloc]
    initWithViewModel:nil
     componentContext:context];
```

> [!Note]
> The TypeScript component associated to the root view will be destroyed once the Objective-C root view instance is deallocated. Please make sure that there are no retain cycles between the dependencies passed to the root view through the view model and component context otherwise the root view might leak and the TypeScript component won't be destroyed.

### Usage in Kotlin

```java (works better than kotlin syntax highlighting)
@Inject lateinit var runtime: IValdiRuntime
// Init context
val context = MyComponentContext()
context.str = "Hello!"; // NOTE: since the field is declared optional in typescript, it can be initialized independently of the constructor
context.callback = {
    println("the context's callback was called from typescript!");
}
// Init view
val view: View = MyComponentView.create(
    runtime = runtime,
    viewModel = null,
    componentContext = context
)

// Destroy the view once we're done with it, which will call onDestroy()
// and release the associated resources
view.destroy()
```

> [!Note]
> Unlike regular Android views, Valdi views need to be explicitly destroyed by calling `destroy()`. Failure to call destroy will result in leaks and will prevent the TypeScript component's destructure!

## Exhaustive list

This is the list of available annotations:

### Export Annotations

These annotations control what gets exported to native code.

```ts
/**
 * Parameters that can be passed to generate native annotations
 */
interface NativeClass {
    ios?: string;
    android?: string;
}

/**
 * Can be set on a Component class or any TypeScript interface
 * Asks the compiler to emit a native class representing the Component/Type.
 */
@ExportModel(class: NativeClass);

/**
 * Can be set on a TypeScript interface.
 * Asks the compiler to emit an interface representing the Component/Type.
 * Native code will have to implement the interface.
 */
@ExportProxy(class: NativeClass);

/**
 * Can be set on an exported TypeScript function.
 * Asks the compiler to emit Objective-C/Swift/Kotlin functions which can call
 * this function.
 */
@ExportFunction(class: NativeClass);

/**
 * Can be set on a TypeScript enum.
 * Asks the compiler to emit Objective-C/Swift/Kotlin enums.
 * Only string and int enums are supported.
 */
@ExportEnum(class: NativeClass);

/**
 * Can be set on a TypeScript definition file (.d.ts).
 * Tells the compiler to generate Objective-C/Swift/Kotlin modules
 * that must implement the API for the file itself.
 * See documentation about polyglot modules for more details.
 */
@ExportModule(class: NativeClass);
```

### Component Annotations

These annotations mark component-related interfaces and classes.

```ts
/**
 * Notifies that the class is the exported component class for the TSX file.
 * must be used: alongside @ExportModel
 * must be used: on a class extending Component<>
 */
@Component();

/**
 * Can be set on a TypeScript interface
 * Notifies that the interface is the view model for the matching @Component.
 * This is used whenever generating a native view class, so that
 * the viewModel parameter will be typed with this interface.
 * must be used: alongside @ExportModel
 * must be used: on an interface
 * must be used: in the same file as the matching @Component
 */
@ViewModel();

/**
 * Can be set on a TypeScript interface
 * Notifies that the interface is the context for the matching @Component.
 * This is used whenever generating a native view class, so that
 * the context parameter will be typed with this interface.
 * must be used: alongside @ExportModel
 * must be used: on an interface
 * must be used: in the same file as the matching @Component
 */
@Context();
```

### Property and Function Modifiers

These annotations modify the behavior of properties and functions in exported types.

```ts
/**
 * Marks a property in a Context or ViewModel to be injected from native
 * dependency injection (Dagger for Android).
 * 
 * Options:
 *   iosOnly: true - Only inject on iOS platform
 *   androidOnly: true - Only inject on Android platform
 * 
 * must be used: on non-optional properties only
 * must be used: in a Context or ViewModel interface
 * 
 * Example:
 *   // @Injectable
 *   configurationProvider: ConfigurationProvider;
 * 
 *   // @Injectable({androidOnly: true})
 *   androidService: SomeService;
 */
@Injectable(options?: {iosOnly?: boolean, androidOnly?: boolean});

/**
 * Marks a function/callback that should only be callable once.
 * After the first call, subsequent calls will be no-ops.
 * 
 * Use cases:
 *   - One-time initialization callbacks
 *   - Preventing multiple form submissions
 *   - Event handlers that should only fire once
 * 
 * must be used: on function properties
 * 
 * Example:
 *   // @SingleCall
 *   callback: () => void;
 */
@SingleCall();

/**
 * Indicates that a function should be executed on a worker/background thread
 * instead of the main thread.
 * 
 * must be used: on functions returning Promise<T> or void only
 * must be used: on function properties or methods
 * 
 * Example:
 *   // @WorkerThread
 *   heavyComputation: (data: string) => Promise<Result>;
 * 
 *   // @WorkerThread
 *   backgroundTask: () => void;
 */
@WorkerThread();

/**
 * (LEGACY) Marks optional properties that should NOT be included in the
 * generated native constructor. These properties can be set after construction.
 * 
 * must be used: on optional properties only (marked with ?)
 * cannot be used: with @ExportProxy
 * deprecated: when not using legacy constructors
 * 
 * Example:
 *   requiredParam: string;
 *   
 *   // @ConstructorOmitted
 *   optionalParam?: string; // Can be set after construction
 */
@ConstructorOmitted();
```

### Type Conversion Annotations

These annotations handle type conversion and marshalling between TypeScript and native code.

```ts
/**
 * Marks an unrecognized TypeScript type to be marshalled as an untyped/any
 * value in native code.
 * 
 * Use cases:
 *   - Working with third-party types not registered with Valdi
 *   - Dynamic data structures
 *   - Gradual migration of code
 * 
 * must be used: on property types
 * 
 * Example:
 *   // @Untyped
 *   dynamicData: SomeUnrecognizedType;
 */
@Untyped();

/**
 * Marks an unrecognized TypeScript type to be marshalled as a string-keyed
 * map with untyped values (Map<string, any> in native code).
 * 
 * must be used: on property types
 * 
 * Example:
 *   // @UntypedMap
 *   metadata: SomeMapLikeType;
 */
@UntypedMap();

/**
 * Defines a custom type converter function for transforming TypeScript types
 * to/from native types.
 * 
 * must be used: on exported functions
 * must return: a generic type with two type parameters
 * 
 * Example:
 *   // @NativeTypeConverter
 *   export function convertMyType<From, To>(value: From): To {
 *     // Conversion logic
 *     return converted as To;
 *   }
 */
@NativeTypeConverter();
```

### Advanced/Internal Annotations

These annotations are typically used internally by the framework or for advanced use cases.

```ts
/**
 * Marks a method in a Component class as an "action" that can be tracked.
 * 
 * must be used: on class member functions
 * must be used: within a @Component class
 * must be used: in .vue, .ts, or .tsx files
 */
@Action();

/**
 * (INTERNAL) Registers a native type that is implemented natively but used
 * from TypeScript. User code should generally use @ExportModel or @ExportProxy.
 */
@NativeClass(class: NativeClass);

/**
 * (INTERNAL) Similar to @NativeClass but for interface types.
 */
@NativeInterface(class: NativeClass);

/**
 * (INTERNAL) Marks an interface as a native template element (UI component)
 * definition. Used internally by Valdi for defining native UI elements.
 */
@NativeTemplateElement();
```

### Deprecated Annotations

These annotations have been superseded by newer alternatives but may still be supported for backwards compatibility.

```ts
/**
 * @deprecated Use @ExportModel instead
 */
@GenerateNativeClass(class: NativeClass);

/**
 * @deprecated Use @ExportProxy instead
 */
@GenerateNativeInterface(class: NativeClass);

/**
 * @deprecated Use @ExportEnum instead
 */
@GenerateNativeEnum(class: NativeClass);

/**
 * @deprecated Use @ExportFunction instead
 */
@GenerateNativeFunction(class: NativeClass);
```
