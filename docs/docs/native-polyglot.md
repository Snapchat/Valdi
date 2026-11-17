# Polyglot Modules

## Introduction

Polyglot modules are a special kind of Valdi modules that expose a TypeScript API backed by a foreign language implementation, like Objective-C, Swift, C++, Rust, Java or Kotlin. When a regular Valdi module depends on a polyglot module, it will also depend on its implementation in the separate language. Some example of usecases for polyglot modules include:

- Exposing a third party library written in another language like C into TypeScript.
- Providing a more optimized implementation of a function or module in another language.
- Implementing some functionality that requires access to dependencies that are not currently visible by TypeScript.

## Implementing a polyglot module

A polyglot module has two key components:

- It has some part of the TypeScript type definition implemented in a separate language.
- Its `valdi_module()` Bazel target in the `BUILD.bazel` depends on user defined Android, iOS or native dependencies.

A regular Valdi module can be turned into a polyglot module at any time. We will look at an example where we implement a `join(components: string[], delimiter: string)` function in Objective-C, Kotlin and then C++, that gets exposed to TypeScript. The rest of this document expects that we have a Valdi module where we can add files and modify its `BUILD.bazel` file.

### TypeScript definition

We start by declaring the TypeScript API of the module for which we want an implementation in a foreign language:

```ts
/* @ExportModule */

export const DEFAULT_DELIMITER: string;

export function join(components: string[], delimiter: string): string;
```

The `@ExportModule` annotation tells the Valdi compiler to generate Objective-C, Swift, and Kotlin bindings that will help with implementing the module. Note that the file needs to be a TypeScript definition (`.d.ts` file), at `my_module/src/Joiner.d.ts` for instance.

### Bazel file

The `valdi_module` Bazel rule that is used for defining Valdi modules exports three properties that can be used to help writing a polyglot modules:

- `android_deps`: Defines a list of `android_library` dependencies, implemented in a JVM language, that will be built and included on an Android build. This can be used to add a JVM based implementation when targeting Android.
- `ios_deps`: Defines a list of `apple_library` or `cc_library` dependencies, implemented in a native language like Objective-C, Swift, and will be built and included on an iOS build. This can be used to add a native implementation when targeting iOS.
- `native_deps`: Defines a list of `cc_library` dependencies, implemented in a native cross-platform language like C++, and will be built and included on all platforms like iOS, Android or Desktop. This can be used to add a native and cross-platform implementation.

In a typical implementation, either `native_deps` is used, or `android_deps` and `ios_deps` are used. If the implementation is native cross-platform, like when writing bindings for a native library like `zstd`, `native_deps` would be used. If the implementation is platform dependent, like when writing bindings for a Camera API, `android_deps` and `ios_deps` would be used.

### iOS implementation

We start by adding a `objc_library` target in the `BUILD.bazel` file of the module, which will contain the iOS specific implementation of the polyglot module:

```python
objc_library(
    name = "my_module_ios_impl",
    srcs = glob([
        "ios/**/*.m",
    ]),
    hdrs = [],
    copts = ["-I."],
    deps = [
        # Depend on the generated Objective-C API of the module
        ":my_module_api_objc",
        # Depend on the Valdi runtime
        "//valdi",
    ],
)

```

The implementation target will need to be referenced as `ios_deps` on the `valdi_module` target of the `BUILD.bazel` representing the module:

```python
valdi_module(
    name = "my_module",
    ios_deps = [":my_module_ios_impl"],
    ios_module_name = "SCCMyModule",
    ios_output_target = "release",
    # Etc...
)
```

We then write an Objective-C implementation file:

```objc
#import "valdi_core/SCValdiModuleFactoryRegistry.h"
#import <SCCMyModuleTypes/SCCMyModuleTypes.h>
#import <Foundation/Foundation.h>

@interface SCCMyModuleJoinerModuleImpl: NSObject<SCCMyModuleJoinerModule>

@end

@implementation SCCMyModuleJoinerModuleImpl

- (NSString *)DEFAULT_DELIMITER
{
    return @" ";
}

- (void)setDEFAULT_DELIMITER:(NSString *)defaultDelimiter
{

}

- (NSString *)joinWithComponents:(NSArray<NSString *> *)components delimiter:(NSString *)delimiter
{
  return [components componentsJoinedByString:delimiter];
}

@end

@interface SCCMyModuleJoinerModuleFactoryImpl : SCCMyModuleJoinerModuleFactory

@end

@implementation SCCMyModuleJoinerModuleFactoryImpl

// Registers the module into the Valdi runtime
VALDI_REGISTER_MODULE()

- (id<SCCMyModuleJoinerModule>)onLoadModule
{
    // Return the module implementation. Will be called lazily when the module
    // is imported for the first time.
    return [SCCMyModuleJoinerModuleImpl new];
}

@end
```

That's it! When TypeScript imports the `.d.ts` file, on iOS the `onLoadModule` of `SCCMyModuleJoinerModuleFactoryImpl` will be called, which will return the module instance that backs the implementation of the module.

### Android implementation

We start by adding a `valdi_android_library` target in the `BUILD.bazel` file of the module, which will contain the Android specific implementation of the polyglot module:

```python
load("//bzl/valdi:valdi_android_library.bzl", "valdi_android_library")

valdi_android_library(
    name = "my_module_android_impl",
    srcs = glob([
        "android/**/*.kt",
    ]),
    deps = [
        # Depend on the generated Kotlin API of the module
        ":my_module_api_kt",
        # Depend on the Valdi runtime
        "//valdi:valdi_java",
    ],
)

```

The implementation target will need to be referenced as `android_deps` on the `valdi_module` target of the `BUILD.bazel` representing the module:

```python
valdi_module(
    name = "my_module",
    android_class_path = "com.snap.valdi.modules.my_module",
    android_deps = [":my_module_android_impl"],
    android_output_target = "release",
    # Etc...
)
```

We then write a Kotlin implementation file:

```kotlin
package com.snap.valdi.modules.my_module

import com.snapchat.client.valdi_core.ModuleFactory
import com.snap.valdi.modules.RegisterValdiModule
import com.snap.valdi.modules.my_module.MyJoinerModule
import com.snap.valdi.modules.my_module.MyJoinerModuleFactory
import com.snap.valdi.modules.hello_world.NativeModuleModule

// Registers the module into the Valdi runtime
@RegisterValdiModule
class MyJoinerModuleFactoryImpl: MyJoinerModuleFactory() {

    override fun onLoadModule(): NativeModuleModule {
          // Return the module implementation. Will be called lazily when the module
          // is imported for the first time. We use an anonymous class here, but a
          // proper subclass can also be used of course.
          return object: MyJoinerModule {
            override val DEFAULT_DELIMITER = " "
            override fun join(components: List<String>, delimiter: String) -> String {
              return components.joinToString(delimiter)
            }
        }
    }
}
```

That's it! When TypeScript imports the `.d.ts` file, on Android the `onLoadModule` of `MyJoinerModuleFactoryImpl` will be called, which will return the module instance that backs the implementation of the module. Please note that for the `@RegisterValdiModule` annotation to work, the Kotlin file needs to be compiled through a `valdi_android_library` rule and that rule processes the annotations at build time to add some required information at runtime that is used to register the module.

### C++ implementation

We start by adding a `cc_library` target in the `BUILD.bazel` file of the module, which will contain the native implementation of the polyglot module:

```python
cc_library(
    name = "my_module_native_impl",
    srcs = glob([
        "native/**/*.cpp",
    ]),
    # Required for automatic registration of the module into the Valdi runtime
    alwayslink = 1,
    deps = [
        # Depend on the generated C++ API of the module
        ":my_module_cpp",
    ],
)

```

The implementation target will need to be referenced as `native_deps` on the `valdi_module` target of the `BUILD.bazel` representing the module:

```python
valdi_module(
    name = "my_module",
    native_deps = [":my_module_native_impl"],
    # Etc...
)
```

We then write a C++ implementation file. The Valdi compiler does not yet support C++ codegen, so unfortunately the bindings currently have to be hand written in C++:

```c++
#include "valdi_modules/my_module/my_module.hpp"

using namespace Valdi;

namespace snap::valdi_modules::my_module {

class JoinerModuleFactoryImpl: public JoinerModuleFactory {
public:
    JoinerModuleFactoryImpl() = default;
    ~JoinerModuleFactoryImpl() override = default;

    Ref<JoinerModule> onLoadModule() final {
        class JoinerModuleImpl: public JoinerModule {
        public:
            JoinerModuleImpl() = default;
            ~JoinerModuleImpl() override = default;

            StringBox DEFAULT_DELIMITER() final {
                return StringBox::fromCString(" ");
            }

            StringBox join(const std::vector<StringBox> &components, const StringBox &delimiter) final {
                return StringBox::join(components, delimiter);
            }
        };

        return makeShared<JoinerModuleImpl>();
    }
};

// Register our module to the Valdi runtime
auto kRegisterModule = RegisterModuleFactory::registerTyped<MyJoinerModule>();

}
```

That's it! When TypeScript imports the `.d.ts` file, on all platforms the `loadModule()` of `MyJoinerModule` will be called, which will return the module instance that backs the implementation of the module.

### Rust implementation

Set `native_language = "rust"` to generate a safe Rust API instead of the high-level C++ API. The default is `"cpp"`, so existing modules are unchanged.

```python
load("@rules_rust//rust:defs.bzl", "rust_library")
load("//bzl/valdi:valdi_module.bzl", "valdi_module")

valdi_module(
    name = "my_module",
    native_deps = [":my_module_rust_impl"],
    native_language = "rust",
    # Etc...
)

rust_library(
    name = "my_module_rust_impl",
    srcs = ["native/Joiner.rs"],
    crate_name = "my_module_rust_impl",
    edition = "2024",
    deps = [":my_module_rust"],
)
```

The generated crate contains a trait for every `@ExportModule` and a deterministic registration macro. A minimal implementation looks like this:

```rust
use my_module::{JoinerModule, register_joiner};
use valdi_core_rust::{ArrayRef, StringRef};

#[derive(Default)]
struct Joiner;

impl JoinerModule for Joiner {
    fn default_delimiter(&self) -> Result<String, String> {
        Ok(" ".to_owned())
    }

    fn join(
        &self,
        components: ArrayRef<'_, StringRef<'_>>,
        delimiter: StringRef<'_>,
    ) -> Result<String, String> {
        let components = components.to_vec()?;
        let delimiter = delimiter.as_str()?;
        Ok(components.join(delimiter.as_ref()))
    }
}

register_joiner!(Joiner::default);
```

Rust mode creates these Bazel targets:

- `<name>_cpp_ffi` is a declaration-only C header target for native consumers of the stable C ABI. It contains no generated C++ implementation.
- `<name>_rust` is the generated Rust API crate used by the implementation.
- `<name>_cpp` is the stable native-facing target. It exposes the declaration-only C ABI header and links the Rust implementation from `native_deps` plus Valdi's shared Rust runtime facade.
- `<name>_native_android`, `<name>_native_ios`, and `<name>_native_desktop` continue to link through `<name>_cpp`.

For a TypeScript module whose tests call a Rust-backed module, put that module in `deps`. The test executable links native factories from those dependencies, including transitive ones. `test_deps` supplies JavaScript test helpers such as Jasmine; it does not link native factories.

Valdi-module dependencies continue to propagate through their `<dep>_cpp` targets. Rust crates are not automatically added as dependencies of another module's generated Rust crate. An exported Rust signature may therefore use models, enums, and proxies declared in the same Valdi module, but not native declarations imported from another Valdi module. The compiler reports the foreign declaration when this restriction is violated. Ordinary TypeScript dependencies are unaffected.

Rust bindings cover exported and generic models, numeric and string enums, proxies, modules, exported TypeScript functions, primitives, strings, `Long`, `bigint`, bytes, arrays, maps, dynamic values, nullability, callbacks, and promises. Generic models remain ordinary Rust generic types and are not specialized into C ABI declarations. Maps follow the existing native contract and are exposed as `Map<String, Value>`. Generated identifiers escape Rust keywords and report collisions after sanitization.

Generated Rust code owns schema and module registration, deserializes directly from borrowed `Valdi::Value` references, and serializes Rust results directly back to `Valdi::Value`; it never constructs the corresponding C++ model. A single handwritten runtime facade exposes the fixed C ABI needed to access Valdi's C++ `Value`, schema registry, module registry, callbacks, proxies, and futures.

Each synchronous call uses one stack-owned Valdi marshaller across the C ABI: arguments occupy the initial slots and Rust appends the result to the same stack. The common case fits four values inline. Null, undefined, primitives, and borrowed strings, bytes, models, arrays, and maps do not allocate bridge handles merely to cross the call, and existing callbacks, proxies, and futures are forwarded without another handle. The marshaller allocates spill storage only when more than four values are live. Returned collections still allocate their actual payload; creating or retaining a callback, proxy, future, or dynamic object beyond the call acquires the ownership state required by that runtime object.

Incoming strings, bytes, arrays, models, callbacks, proxies, and futures use borrowed wrappers such as `StringRef<'a>`, `BytesRef<'a>`, `ArrayRef<'a, T>`, and `FooRef<'a>`. They are valid only for the call lifetime; call `to_owned()` before retaining one. Owned TypeScript arrays use `Array<T>`, while `Uint8Array` uses `BytesView`, so both remain unambiguous even inside generic models. A `BytesView` retained from Valdi retains the original typed-array wrapper through Valdi's intrusive reference counting; retaining, cloning, and returning it do not copy the byte buffer or allocate a Rust bridge handle. For JS-owned input, do not use a retained view after its JavaScript context has been torn down. A Rust-created `BytesView` owns a `Vec<u8>` and transfers that allocation to Valdi when returned. Proxy references and other owned runtime objects use RAII wrappers: `Drop` releases each owned value exactly once, and wrappers that implement `Clone` retain it before sharing ownership. Runtime-bound wrappers are not `Send` or `Sync` unless the underlying Valdi facility supports it; generated future completers are `Send` so background work can resolve or reject a future. Rust panics are translated to Valdi errors and never cross the C ABI; the C++ facade reports failures through status values rather than exceptions.

Rust mode is supported on Android, iOS, macOS, and Linux/desktop. Web output is unchanged. It generates no per-module C++ source, high-level C++ models, proxies, or factory interfaces. The C-compatible header exposed by `<name>_cpp` contains the generic `Valdi::Value` module entry points and stable method identifiers; all per-type conversion remains in Rust. The toolchain contract is Rust 1.87 with edition 2024.
