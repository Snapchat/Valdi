# @ExportModel vs @ExportProxy: Marshalling Guide

This document explains the difference between `@ExportModel` and `@ExportProxy` in the Valdi compiler, with emphasis on **marshalling** (passing values across the TypeScript–native boundary). Using the wrong marshalling API for a type can cause subtle runtime bugs.

## Summary

| Aspect | @ExportModel | @ExportProxy |
|--------|--------------|--------------|
| **TypeScript** | Class or interface (bag of properties) | Interface only (callable contract) |
| **Native semantics** | Generated **concrete class** with fields | Generated **protocol/interface**; native **implements** it |
| **Marshalling** | Serialize **field values** so the other side can reconstruct an equivalent object | Register a **proxy** so the other side can **call methods** on this instance |
| **Wrong marshalling** | N/A | Passing a proxy to model marshalling serializes the wrong thing and breaks behavior |

---

## @ExportModel: Data Objects

- **Purpose**: Represent a **data shape**—a container of properties (and optional callbacks) that can be copied across the boundary.
- **Generated native code**: A **concrete class** (e.g. `SCMyContext`) that extends `SCValdiMarshallableObject` (iOS) or equivalent on Android. The class has storage for each property and a descriptor that describes its fields.
- **Marshalling**: The object is marshalled by **serializing its fields** (and function references) into the marshaller. On the other side, an equivalent object can be **reconstructed** from that data.

**When to use**: View models, context objects, configs, DTOs—anything that is conceptually “a bag of data” that can be copied.

---

## @ExportProxy: Callable Interfaces

- **Purpose**: Represent a **contract** that native code implements—an interface with methods (and optionally properties) that TypeScript can call. The “value” passed across the boundary is not copied; it is a **handle** to the implementation so the other side can invoke methods.
- **Generated native code**:
  - **Protocol** (iOS) / **interface** (Android) that native must implement.
  - A **proxy class** (e.g. `SCMyService`) used by the runtime to represent the TypeScript side of the proxy.
  - A **free function** (ObjC only) to marshal an implementation instance, e.g. `NSInteger SCMyServiceMarshall(SCValdiMarshallerRef marshaller, id<SCMyService> instance)`.
- **Marshalling**: The implementation instance is **not** serialized field-by-field. Instead, marshalling **registers a proxy** in the marshaller so that when the other side receives it, they get a handle that forwards method calls back to this instance.

**When to use**: Services, fetchers, delegates, launchers—anything that is “something you call” rather than “something you copy.”

---

## Marshalling APIs by Platform

### Swift and Kotlin

Both **models** and **proxies** conform to the same marshalling protocol (`ValdiMarshallable` / `push(to:)` / `pushToMarshaller`). The generated code internally does the right thing:

- **Model**: `push` serializes the object’s fields.
- **Proxy**: `push` registers the instance as a proxy (e.g. via `marshaller.pushProxy(object: self)` or equivalent).

So in Swift/Kotlin you do **not** need to choose a different API; calling `push(to: marshaller)` (or the Kotlin equivalent) is correct for both.

### Objective-C

Objective-C does **not** have protocol extensions that can provide a single `pushToValdiMarshaller:` implementation for both cases. So:

- **@ExportModel**: The generated **class** extends `SCValdiMarshallableObject` and implements `pushToValdiMarshaller:`. You marshal by calling:
  ```objc
  SCValdiMarshallableObjectMarshall(marshaller, modelInstance);
  ```
  This is correct for **data objects only**.

- **@ExportProxy**: There is **no** single method on the protocol that marshals “as a proxy.” The compiler emits a **free function** per proxy type, e.g.:
  ```objc
  NSInteger SCMyServiceMarshall(SCValdiMarshallerRef marshaller, id<SCMyService> instance);
  ```
  You **must** use this generated function to marshal a proxy. Under the hood it calls `SCValdiMarshallableObjectMarshallInterface(marshaller, instance, [SCMyService class])`, which registers the instance as a proxy by **interface**, not by copying fields.

---

## The Bug: Using Model Marshalling for a Proxy

**Wrong (and dangerous):**

```objc
// productFetcher is an id<SCCPlusIapProductFetcher> (an @ExportProxy type)
SCValdiMarshallableObjectMarshall(marshaller, productFetcher);
```

- `SCValdiMarshallableObjectMarshall` expects an `SCValdiMarshallableObject *`. It marshals the object by **field layout** (using the object’s class descriptor). So it only makes sense for **@ExportModel** types.
- If the native implementation class (e.g. `ProductFetcher`) is made to **subclass** `SCValdiMarshallableObject` so that the call compiles, then:
  - The compiler accepts the code.
  - At runtime, the object is marshalled as if it were a **model** (field-by-field). For a proxy, the “fields” are not the intended representation; the other side expects a **proxy handle**, not a reconstructed data object.
  - Result: incorrect behavior, missing calls, or subtle bugs on the TypeScript or other side.

**Correct:**

```objc
SCCPlusIapProductFetcherMarshall(marshaller, productFetcher);
```

So for **any** `@ExportProxy` type, always use the **generated** `SC<Name>Marshall(marshaller, instance)` function, and **never** use `SCValdiMarshallableObjectMarshall` for proxy instances.

---

## Rule of Thumb

- **Data object (model)** → use `SCValdiMarshallableObjectMarshall(marshaller, object)` (or the object’s `pushToValdiMarshaller:` in ObjC, or `push(to:)` in Swift).
- **Proxy (interface implementation)** → use the **generated** `SC<ProxyName>Marshall(marshaller, instance)` in Objective-C; in Swift/Kotlin, use the normal `push(to:)` / `pushToMarshaller` and the generated code will choose the right path.

---

## Native Implementation Guidelines for @ExportProxy (iOS)

1. **Do not** make your implementation class extend `SCValdiMarshallableObject` (or `SCValdiProxyMarshallableObject`) just to satisfy a call to `SCValdiMarshallableObjectMarshall`. That is a sign you are using the wrong API.
2. **Do** implement only the generated protocol (e.g. `<SCCPlusIapProductFetcher>`).
3. **Do** marshal your implementation using the generated free function, e.g. `SCCPlusIapProductFetcherMarshall(marshaller, self)` (or the appropriate type name for your proxy).

---


## Related

- [Annotations (native-annotations.md)](native-annotations.md) – Full list of `@ExportModel` and `@ExportProxy` and other annotations.
- [Native bindings (native-bindings.md)](native-bindings.md) – How generated types are used across the boundary.
