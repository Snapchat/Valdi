# Valdi Inspector

Valdi Inspector is a desktop application, written in Valdi itself, which can be used to help debugging a Valdi VirtualNode tree.

> [!Note]
> Valdi provides a suite of development tools to assist engineers, including a live TypeScript debugger integrated into VSCode, the Valdi Inspector for UI debugging, and a logs viewer for application logs. These tools work in tandem with the hot reloader to provide a responsive development experience.

![Screenshot of Valdi Inspector](./assets/advanced-inspector/Inspector.png)

## Features
* Inspect the node tree remotely
  * Can highlight components, elements, and views independently
* Display accessibilityIds
  * A toggle lets you display accessibilityId values right next to the views that have it set on them. Should make adding karma tests much easier
* Inspect element attributes
  * Selecting an element will display the currently-applied attribute values
* Preview Valdi Components
  * The app can display a **preview of any Component, without requiring you to build iOS/android**

## How do I use this?
<!-- TODO: do these ./scripts work as-is for Open Source? -->
* Check out fresh client/master
* Run the hot reloader: 
    ```
    valdi hotreload
    ```
* To just launch the inspector: 
    ```
    ./scripts/start_inspector.sh
    ```
    You should now be able to inspect other instances of Valdi UI that are connected to the hot reloader
* To preview a Component: 
    ```
    ./scripts/preview_component.sh --root_component ValdiCatalog@valdi_catalog/src/ValdiCatalog
    ```
    You should now be able see and interact with the component from the provided component path in a window on your desktop

## Brief implementation details

[the implementation]: #todo-implementation-link

* Standalone View tree implementation using Skia.
* Inspector app is a small AppKit+Metal shell exposing the Skia implementation
* Inspector UI itself is written in Valdi!
    * Take a peek at [the implementation][]
    * Feel free to play around with the code if you want to improve the tool. You can hot reload the inspector the same way you can hot reload your components on iOS and Android.

### Hot Reloader Communication

The hot reloader establishes a TCP connection between the device/simulator and the developer's machine running the `valdi hotreload` daemon. This bidirectional communication enables:
- **Live code reloading**: The runtime detects module changes and triggers re-renders via `RootComponentsManager`
- **Inspector debugging**: Remote tree inspection and attribute viewing through `DaemonClientManager`
- **File transfer**: Reading/writing files on the host machine using `DaemonClientFileManager` (useful for debug data export/import)
- **Custom messages**: Development tools can send custom requests between device and host

> **Note**: The connection uses a native TCP implementation with a custom packet protocol (not the `TCPSocket` TypeScript module). A low-level `TCPSocket` module exists for specialized internal tooling and is only available in dev/gold builds.

## Limitations
* Currently only supported on macOS
    * Implementing the Linux shell around Skia will take some work (sorry Robert)
* Inaccurate attribute inspection from CSS documents on .vue components
* Of course, since the Component preview runs outside of iOS/android, any custom native view will not actually render anything




