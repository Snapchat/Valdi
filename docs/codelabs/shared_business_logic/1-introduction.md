# What is this codelab?
Valdi is not just a UI framework, it also includes a javascript runtime. In this codelab you will learn how to implement business logic in typescript and use it from Android and iOS.

Before diving in, you should be comfortable building your platform of choice and have a basic understanding of Valdi. If you're not there yet, check out the [**Getting Started Codelab**](../getting_started/1-introduction.md).

This code lab starts by building common typescript logic and then splits off in to **Android** and **iOS** specific paths. We recommend going all the way through one platform first and then coming back for the other.

# Pre-work
Let's make sure you're setup to develop.

In the [**Getting Started Codelab**](../getting_started/1-introduction.md) you were only modifying TypeScript which meant that the hotreloader could do all of the work of updating the UI. 

In this codelab, we're going to be modifying native interfaces which means that we need a module linked build.

For more details on native bindings and annotations, see the [Native Annotations documentation](../../docs/native-annotations.md) and [Native Bindings documentation](../../docs/native-bindings.md).

### [Next >](./2-setup_module.md)