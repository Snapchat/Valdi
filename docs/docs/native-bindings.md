# Native Bindings

## Communicating between native and JS

Valdi is designed with a clear performance motto: *"What is the highest level API we can expose that's easy and safe to use while allowing us to achieve a high level of performance?"*

To achieve this, Valdi uses a unified data container called `Valdi::Value` in C++ to represent data as it crosses the bridge between TypeScript, Objective-C, and Java. This minimizes the overhead of data conversion and enables features like interned strings and styles.

There are two main ways you can communicate between native and JS.

Choose which one to use based on your use case.

### Using the Context (most common, recommended)

For interaction between your view's TypeScript and native code, the Context object will probably be the best solution for you.

Using the [annotations](native-annotations.md) API, the Valdi compiler can generate strongly typed native interfaces/classes which do the conversions in and out between TypeScript and native code.

This is an example of an interaction between both platforms with them:

You can call native logic from TypeScript valdi code:

```ts
/**
 * @Context
 * @ExportModel({
 *   ios: 'SCYourComponentContext',
 *   android: 'com.snap.myfeature.YourComponentContext'
 * })
 */
interface YourComponentContext {
  callMeFromTS?();
}
/**
 * @Component
 * [...]
 */
class YourComponent extends Component<YourComponentViewModel, YourComponentContext> {
  onMyButtonWasTapped() {
    // Calls callMeFromTS: on the SCYourComponentContext (if it has been configured)
    this.context.callMeFromTS?.();
  }
}
```

This is what the generated Obj-C counterpart looks like:

```objectivec
@interface SCYourComponentContext: SCValdiMarshallableObject

@property (copy, nonatomic) SCYourComponentContextOnDoneBlock _Nullable callMeFromTS;

- (instancetype _Nonnull)init;

// ...

@end

//////////

// So you can instantiate SCYourComponentContext and configure it with the callMeFromTS block:
SCYourComponentContext *componentContext = [[SCYourComponentContext alloc] init];
componentContext.callMeFromTS = ^{
  // Will be called when this.context.callMeFromTS() is called in TS.
  NSLog(@"Hello from Objective-C");
}
```

and Kotlin:
```kotlin
package com.snap.myfeature.YourComponentContext

class SCYourComponentContextImpl {
    val onDone: (() -> Unit)?
    // ...
}

//////////

// So you can instantiate YourComponentContext and configure it with the callMeFromTS block:
val componentContext = YourComponentContext()
componentContext.callMeFromTS = {
  // Will be called when this.context.callMeFromTS() is called in TS.
  print("Hello from Kotlin")
}
```


You can also pass callbacks from JS to native:

TypeScript:
```ts
interface YourComponentContext {
  callMeFromTS?(completion: (arg: string) => void);
}
class YourComponent extends Component<any, YourComponentContext> {
  onMyButtonWasTapped() {
    this.context.callMeFromTS((arg) => {
      console.log('the native code called the completion function with arg:', arg);
    });
  }
}
```

Objective-C:
```objectivec
componentContext.callMeFromTSWithCompletion = ^(YourComponentContextCallMeFromTSCompletionBlock completion) {
  // This will call the TS callback and provide the given value.
  completion(@"I got you loud and clear");
}
```

Kotlin:
```kotlin
componentContext.callMeFromTSWithCompletion = { completion ->
  // This will call the TS callback and provide the given value.
  completion(@"I got you loud and clear");
}
```

This will print 'the native code called the completion function with arg: I got you loud and clear'.

### ExportFunction

If you need to use TypeScript code outside of a Valdi component, you can use the `@ExportFunction` annotation:
```ts
// @ExportFunction({ios: 'SCMultiplier', android: 'com.valdi.example.Multiplier'})
export function multiply(left: number, right: number): number {
  return left * right;
}
```
This will generate Objective-C/Swift/Kotlin files which can call this function, and will handle parameter serialization/deserialization automatically:
```objectivec
#import <ModuleName/SCMultiplier.h>

- (void)viewDidLoad
{
  /// inject the id<SCValdiRuntimeProtocol> dependency
  [valdiRuntime getJSRuntimeWithBlock:^(id<SCValdiJSRuntime> runtime) {
    double result = SCMultiplierMultiply(runtime, 2, 4);
    NSLog(@"Result is: %fs", result);
  }];
}
```

```java (works better than kotlin syntax highlighting)
@Inject lateinit var runtime: IValdiRuntime

fun onCreate() {
  runtime.createScopedJSRuntime {
    val result = Multiplier.multiply(it, 2, 4)
    println("Result is: ${result}")
  }
}
```

This API supports the whole range of the Valdi [annotations](native-annotations.md). As such it can be used to pass in/return complex objects:
```ts
// @ExportModel({ios: 'SCValdiUser', android: 'com.valdi.example.User'})
interface User {
  name: string;
}

// @ExportModel({ios: 'SCValdiSearchEngine', android: 'com.valdi.example.SearchEngine'})
interface SearchEngine {
  search(term: string, completion: (results: User[]) => void);
}

// @ExportFunction({ios: 'SCValdiSearchEngineFactory', android: 'com.valdi.example.SearchEngineFactory'})
export function makeSearchEngine(users: User[]): SearchEngine {
  // Note: in a near future, you will be able to make the class itself
  // implements the interface and return it.
  const engine = new ConcreteSearchEngine(users);

  return {
    search(term: string, completion: (results: User[]) => void) {
      const results = engine.performSearch(term);
      completion(results);
    }
  };
}

```

iOS:
```objectivec
- (void)viewDidLoad
{
  [UIView.valdiRuntime getJSRuntimeWithBlock:^(id<SCValdiJSRuntime> runtime) {
    NSArray<SCValdiUser *> *allUsers = fetchAllUsers();
    SCValdiSearchEngine *searchEngine = SCValdiSearchEngineFactoryMakeSearchEngine(runtime, allUsers);
    self.searchEngine = searchEngine;
  }];
}

// Later on...
- (void)updateUsers
{
  [self.searchEngine searchWithTerm:@"Simon" withCompletion:^(NSArray<SCValdiUser *> *results) {
    NSLog(@"Found those users: %@", results);
  }];
}
```

Android:
```java (works better than kotlin syntax highlighting)
@Inject lateinit var runtime: IValdiRuntime
var searchEngine: SearchEngine? = null

fun onCreate() {
  runtime.createScopedJSRuntime {
    val allUsers = fetchAllUsers()
    val searchEngine = SearchEngineFactory.makeSearchEngine(it, allUsers)
    this.searchEngine = searchEngine
  }
}

// Later on...
fun updateUsers() {
  searchEngine?.search("Simon") {
    println("Found those users: ${it}")
  }
}
```

Note that calling a JS function will automatically asynchronously dispatch to the JS thread if needed. If your function need to return a value synchronously to platform code, make sure you call this function within a createScopedJSRuntime scope. If you use asynchronous completion functions this will never be a problem.

### Using a Polyglot Module

If you want to be able to write a reuseable module using a separate language like Kotlin, Java, Objective-C, C++ or Swift, with an exposed TS API, you can make a polyglot module. See documentation about [polyglot modules](./native-polyglot.md) for more details.

---

## Type System Reference

This section documents all TypeScript types that can be marshalled between TypeScript and native code.

### Supported Primitive Types

| TypeScript Type | iOS (Obj-C) | iOS (Swift) | Android (Kotlin) | C++ | Notes |
|----------------|-------------|-------------|------------------|-----|-------|
| `string` | `NSString *` | `String` | `String` | `std::string` | UTF-8 encoded |
| `number` | `double` | `Double` | `Double` | `double` | 64-bit floating point |
| `boolean` | `BOOL` | `Bool` | `Boolean` | `bool` | |
| `void` | `void` | `Void` | `Unit` | `void` | For return types |
| `any` | `id` | `Any` | `Any` | `Value` | Dynamic/untyped value |
| `object` | `id` | `Any` | `Any` | `Value` | Untyped object |

### Supported Complex Types

| TypeScript Type | Native Equivalent | Notes |
|----------------|------------------|-------|
| `T[]` | Array/NSArray/List | Arrays of any supported type |
| `Promise<T>` | Async operation | Mapped to platform async patterns |
| `CancelablePromise<T>` | Cancelable async | Extended Promise with cancellation |
| `Map<string, any>` | Dictionary/Map | Currently only supports string keys and any values |
| `(arg: T) => R` | Block/Closure/Lambda | Function/callback types |
| `T \| null \| undefined` | Optional/nullable | Nullable types |
| `T?` | Optional/nullable | TypeScript optional syntax |

### Special Types

| TypeScript Type | Purpose | Native Equivalent |
|----------------|---------|------------------|
| `Uint8Array` | Binary data | `Data` (iOS) / `ByteArray` (Android) |
| `Long` | 64-bit integers | `int64_t` / `Long` |
| `Number` | Explicit double | Same as `number` but explicit |

### Generated Types

Types defined with annotations can be passed between TypeScript and native:

- **`@ExportModel`** interfaces/classes → Generated native classes
- **`@ExportProxy`** interfaces → Native must implement
- **`@ExportEnum`** enums → Generated native enums

**Example:**
```typescript
// @ExportModel({ios: 'SCUser', android: 'com.example.User'})
interface User {
  name: string;
  age: number;
  friends?: User[];  // Optional array of User
}
```

### Generic Types

Generics are supported on generated types:

```typescript
// @ExportModel({ios: 'SCContainer', android: 'com.example.Container'})
interface Container<T> {
  value: T;
}

// Usage
interface MyContext {
  userContainer: Container<User>;
  stringContainer: Container<string>;
}
```

> **Note:** Generics only work on types annotated with `@ExportModel`. Generic parameters must be resolved to concrete types at the interface boundary.

---

## Type Limitations and Constraints

### Union Types

**✅ SUPPORTED:** Union with null/undefined
```typescript
property: string | null;
property: string | undefined;
property?: string;  // Equivalent to: string | undefined
```

**❌ NOT SUPPORTED:** Other union types
```typescript
property: string | number;  // ERROR: Only null/undefined unions supported
property: 'red' | 'blue' | 'green';  // ERROR: Use enum instead
```

**Solution:** Use enums for multiple value choices:
```typescript
// @ExportEnum({ios: 'Color', android: 'com.example.Color'})
enum Color {
  Red = 'red',
  Blue = 'blue',
  Green = 'green'
}

interface Config {
  color: Color;  // ✅ Works
}
```

### Map Type Limitations

**⚠️ PARTIAL SUPPORT:**
```typescript
// ✅ SUPPORTED: Map with string keys and any values
metadata: Map<string, any>;

// ❌ NOT FULLY SUPPORTED: Typed maps
typedMap: Map<string, User>;  // Limited - treated as Map<string, any>

// ❌ NOT SUPPORTED: Non-string keys
numberMap: Map<number, string>;  // Not supported
```

**Workaround:** Use objects with `@ExportModel` for typed dictionaries:
```typescript
// @ExportModel
interface UserMap {
  [key: string]: User;  // Use index signature
}
```

### Array Nesting

Arrays can be nested to any depth:
```typescript
matrix: number[][];  // ✅ 2D array
cube: number[][][];  // ✅ 3D array
```

---

## Type Conversion Behavior

### Number Precision

TypeScript `number` always maps to native `double` (64-bit floating point):

```typescript
interface Data {
  count: number;  // Native: double (not int!)
}
```

**For 64-bit integers:**
```typescript
interface Data {
  timestamp: Long;  // Use Long type for precise 64-bit integers
}
```

> **Warning:** JavaScript numbers can only precisely represent integers up to 2^53 - 1. For larger integers, use `Long` type or string representation.

### Array Conversion

Arrays are **copied** when crossing the native/TypeScript boundary:

```typescript
interface DataProcessor {
  // Array is copied from native to TS
  processData(items: string[]): void;
  
  // Array is copied from TS to native
  getData(): string[];
}
```

**Implications:**
- Modifications in one environment don't affect the other
- Large arrays have serialization overhead
- Consider using `Uint8Array` for binary data to reduce overhead

### Object Marshalling

Only objects with annotations can be marshalled:

```typescript
// ✅ Can be passed - has @ExportModel
// @ExportModel
interface User {
  name: string;
}

// ❌ Cannot be passed - plain object
interface Config {
  data: { key: string, value: number };  // ERROR
}
```

**Solution:** Use `any` with `@Untyped` annotation:
```typescript
interface Config {
  // @Untyped
  data: any;  // Dynamic object, no type safety
}
```

Or define a proper interface:
```typescript
// @ExportModel
interface KeyValue {
  key: string;
  value: number;
}

interface Config {
  data: KeyValue;  // ✅ Works
}
```

### Callback Lifecycle

Callbacks are reference-counted:

```typescript
interface Context {
  callback: () => void;
}
```

**Memory Management:**
- Native holds a strong reference to the callback
- Callback is released when native object is destroyed
- Use `@SingleCall` to automatically release after first invocation

```typescript
interface Context {
  // @SingleCall
  onComplete: () => void;  // Automatically released after first call
}
```

---

## Performance Considerations

### Thread Safety

All TypeScript function calls are dispatched to the JavaScript thread:

```typescript
interface DataFetcher {
  // Automatically dispatches to JS thread
  fetchData(): Promise<Data>;
}
```

**For synchronous calls:**
```objectivec
// iOS: Use scoped runtime
[valdiRuntime getJSRuntimeWithBlock:^(id<SCValdiJSRuntime> runtime) {
  // Synchronous calls work here
  double result = MyFunction(runtime, arg);
}];
```

```kotlin
// Android: Use scoped runtime
runtime.createScopedJSRuntime { jsRuntime ->
  // Synchronous calls work here
  val result = MyFunction.call(jsRuntime, arg)
}
```

**For heavy operations:**
```typescript
interface Processor {
  // @WorkerThread
  processLargeDataset(data: Uint8Array): Promise<Result>;
}
```

### Data Serialization Overhead

Each parameter is serialized/deserialized when crossing the boundary:

```typescript
// ❌ BAD: Many small calls
for (let i = 0; i < 1000; i++) {
  context.updateProgress(i);  // 1000 boundary crossings!
}

// ✅ GOOD: Batch operations
context.updateProgressBatch(progressArray);  // 1 boundary crossing
```

### Callback Overhead

Minimize chatty APIs:

```typescript
// ❌ BAD: Callback for each item
interface ItemProcessor {
  processItems(items: Item[], onEachItem: (item: Item) => void): void;
}

// ✅ GOOD: Single completion callback
interface ItemProcessor {
  processItems(items: Item[]): Promise<Item[]>;
}
```

---

## Best Practices

### Do's ✅

1. **Keep interfaces simple** - Use primitive types when possible
2. **Use `@ExportModel` for complex types** - Don't rely on `any`
3. **Document null behavior** - Be explicit about optional properties
4. **Use `@SingleCall` for one-time callbacks** - Prevents memory leaks
5. **Batch operations** - Reduce boundary crossings
6. **Use `Promise` for async operations** - More ergonomic than callbacks
7. **Use `Uint8Array` for binary data** - More efficient than arrays

### Don'ts ❌

1. **Don't use union types** (except with null/undefined)
2. **Don't pass large objects frequently** - High serialization cost
3. **Don't assume synchronous execution** - Use scoped runtime if needed
4. **Don't hold strong references to callbacks indefinitely**
5. **Don't pass plain JavaScript objects** - Use `@ExportModel` instead
6. **Don't use `Map` with non-string keys**

### Common Patterns

**Async Operations:**
```typescript
interface DataFetcher {
  // ✅ PREFERRED: Promise-based
  fetchData(): Promise<Data>;
}
```

**Bidirectional Communication:**
```typescript
interface Chat {
  sendMessage(text: string): void;
  
  // Native → TS callbacks
  onMessageReceived?: (text: string) => void;
  onError?: (error: string) => void;
}
```

**Resource Management:**
```typescript
interface FileReader {
  open(path: string): void;
  read(): Uint8Array;
  close(): void;  // Important: Always provide cleanup
}
```

---

## See Also

- [Native Annotations](./native-annotations.md) - Complete annotation reference
- [Native Context](./native-context.md) - Context patterns and dependency injection
- [Polyglot Modules](./native-polyglot.md) - Writing native modules
- [Integration Codelabs](../codelabs/integration_with_native/1-introduction.md) - Step-by-step integration guides
