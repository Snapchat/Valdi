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
* To launch the browser-based debugger:
    ```
    valdi debugger
    ```
    Open the printed `VALDI_DEBUGGER_URL` to inspect running Valdi targets. The
    command auto-selects a free local port when `8765` is already in use, and
    `--json` prints startup details for automation.
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

### Automating a live target

Debug `valdi_application` targets register the debugger input contract automatically. The browser debugger uses
this contract for its interactive preview. The fastest scriptable path is the CLI, which prints exactly one JSON
result on standard output:

```sh
valdi inspect input capabilities --port 13591
valdi inspect input query --port 13591 --selector '#composer'
valdi inspect input text --port 13591 --accessibility-id composer --text 'Hello from automation'
valdi inspect input key --port 13591 --accessibility-id composer --key Enter
```

As with `valdi inspect tree` and `snapshot`, omit the context when only one is active, or pass it as the last
positional argument. The `capabilities` action is context-free and only needs a connected client. Use `--client`
when more than one target is connected. Port `13591` is the standalone macOS app port; the CLI's default `13592`
targets in-app mobile clients.

The same contract is also exposed by the browser debugger for tools already using its HTTP API. Start
`valdi debugger --json`, then use the returned loopback URL:

```sh
curl -X POST "$VALDI_DEBUGGER_URL/api/input?port=13591&clientId=CLIENT_ID&contextId=CONTEXT_ID" \
  -H 'content-type: application/json' \
  -d '{"type":"tap","accessibilityId":"send-button"}'
```

The response's `input.contractVersion` is `1`. Call `{"type":"capabilities"}` to discover the operations and
selector forms supported by the connected target. Contract version 1 provides:

* `query` — returns typed element descriptors. With no selector, it returns all rendered elements in the
  context. Descriptors include the element and parent IDs, tag, local and absolute frame, accessibility
  metadata, enabled/focused state, and supported actions.
* `tap` — invokes the rendered element's nearest `onTap` callback.
* `focus` — sets the `focused` interactive attribute on a `textfield` or `textview`.
* `text` — sets the input value and selection, then invokes `onChange`.
* `key` — supports `Enter`/`Return`, `Escape`, grapheme-safe `Backspace`/`Delete`, and one printable grapheme.
  Return inserts a newline in editable `textview` elements unless `ignoreNewlines` is set; return callbacks and
  focus-closing behavior remain independent.
* `scroll` — changes the nearest scroll container's content offset by `deltaX` and `deltaY`.

An action can identify its element with a numeric `elementId`, an `accessibilityId`, or one of these stable
selector forms:

```json
{ "selector": "#composer" }
{ "selector": "[accessibilityId=\"composer\"]" }
{ "selector": { "accessibilityId": "composer", "tag": "textfield" } }
```

Prefer unique `accessibilityId` values. Ambiguous selectors fail and return the matching element descriptors
instead of choosing an arbitrary element. Numeric element IDs are scoped to one renderer context and may change
after a render or hot reload.

Debugger input intentionally follows Valdi's rendered callbacks and interactive attributes rather than
synthesizing operating-system events. This makes the same contract work across platforms, including
SnapDrawing-backed elements. Use platform UI automation when validating behavior that specifically depends on
the operating system's event dispatch.

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

