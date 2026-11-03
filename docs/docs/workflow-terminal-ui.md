---
title: Terminal user interfaces
---

The `valdi_tui` module renders Valdi components to a terminal, in the spirit of
[Ink](https://github.com/vadimdemedes/ink) for React. Components are laid out with the
same flexbox engine that Valdi uses on device, measured in terminal cells, and painted
as ANSI text. Each state change redraws the previous frame in place.

A terminal application is a [CLI application](./workflow-cli-application.md): it is
built with `valdi_cli_application()` and runs under the standalone runtime. The runtime
has to link the native terminal backend (`//valdi:valdi_terminal`), which provides the
view classes `valdi_tui` renders into.

## Building blocks

| valdi_tui | Ink | Role |
|---|---|---|
| `<Box>` | `<Box>` | Flexbox container (a `<view>`) with optional border and background |
| `<Text>` | `<Text>` | Styled, wrapped text (a `<label>`) measured in cells |
| `<Newline>` | `<Newline>` | Vertical spacing |
| `<Spacer>` | `<Spacer>` | Flexible gap between siblings |
| `<Static>` | `<Static>` | Permanent output written above the live frame (a `ValdiTerminalStatic` view) |
| `<Transform>` | `<Transform>` | Post-processes the text of its value |
| `render()` | `render()` | Mounts the root component and returns an instance |
| `useInput()` | `useInput` | Keyboard input for the lifetime of a component |
| `useFocus()` | `useFocus` | Tab-cycled focus between components |
| `getTerminalApp()` | `useApp`, `useStdout` | Exit, terminal size, resize events |

### Boxes and text

`<Box>` accepts the usual Valdi flexbox attributes (`flexDirection`, `justifyContent`,
`alignItems`, `padding*`, `margin*`, `gap`, `width`, `height`, …) plus `borderStyle`
(`single`, `double`, `round`, `bold`, `singleDouble`, `doubleSingle`, `classic`, `arrow`),
`borderColor`, `borderDimColor`, per-side `borderTop`/`borderLeft`/… toggles and
`backgroundColor`. A border occupies one cell on each enabled side; it is part of the
box's layout, so it needs no extra padding. Like Ink, `flexDirection` defaults to `row`.

The root component has to render a `<Box>`: the root of a context is a plain view, so a
`<Text>` at the root has nowhere to put its text and renders nothing.

`<Text>` takes its content through `value`. A plain string wraps inside the available
width; an array of strings and spans styles parts of one paragraph:

```tsx
import { Component } from 'valdi_core/src/Component';
import { Box } from 'valdi_tui/src/Box';
import { Text } from 'valdi_tui/src/Text';

class Status extends Component<{ readonly ok: boolean }> {
  onRender(): void {
    <Box borderStyle='round' paddingX={1} flexDirection='column'>
      <Text bold value='Deploy' />
      <Text value={['Status: ', { text: this.viewModel.ok ? 'ok' : 'failed', color: this.viewModel.ok ? 'green' : 'red' }]} />
      <Text dimColor wrap='truncate' value='This line is cut with an ellipsis when the terminal is narrow.' />
    </Box>;
  }
}
```

Text style props are `color`, `backgroundColor`, `bold`, `italic`, `underline`,
`strikethrough`, `inverse` and `dimColor`. Colors accept Ink names (`green`,
`redBright`, `gray`), CSS names, `#rrggbb`, `rgb()` and `ansi256(n)`; they are resolved to
RGB values and downgraded to what the terminal supports. `wrap` is one of `wrap`,
`truncate`, `truncate-start` or `truncate-middle`.

`<Text>` is a `<label>`, so bold, dim and italic reach the backend through the label's
`font` (`system-bold 1`, `system-light 1`, `system-italic 1`), underline and strikethrough
through `textDecoration`, and styled parts through attributed text. `inverse` applies to the
whole text, not to a single span.

Because text is a measured leaf node, nested `<Text>` components do not flow into one
paragraph. Use spans for inline styling, and a `<Box>` with `flexDirection='row'` for
side-by-side text.

### Rendering and exiting

```tsx
import { getStandaloneRuntime } from 'valdi_standalone/src/ValdiStandalone';
import { render } from 'valdi_tui/src/TerminalRenderer';

const instance = render(App, { title: 'Hello' });
void instance.waitUntilExit().then(() => getStandaloneRuntime().exit(0));
```

`render()` returns an instance with `rerender(viewModel)`, `unmount()`,
`waitUntilExit()`, `clear()` and, for tests, `lastFrame()`, `nextFrame()`, `feedInput()`,
`setSize()` and `output()`. The renderer keeps the runtime alive until `unmount()` is
called or a component calls `getTerminalApp().exit()`. Ctrl+C unmounts as well unless
`exitOnCtrlC: false` is passed.

By default the root is as tall as its content, like Ink. Pass `fitTerminal: true` to size
it to the terminal instead, for full-screen layouts that use `flexGrow` to fill the
viewport.

While the application runs, `console.log` and friends are redirected above the live
frame so they do not corrupt it (`patchConsole: false` disables this).

### Input and focus

Keyboard handlers follow the class component model. Register them in `onCreate()`; the
subscription is disposed with the component:

```tsx
import { StatefulComponent } from 'valdi_core/src/Component';
import { getTerminalApp, useFocus, useInput } from 'valdi_tui/src/App';

class Menu extends StatefulComponent<{}, { readonly index: number }> {
  state = { index: 0 };
  private focus = useFocus(this, { autoFocus: true });

  onCreate(): void {
    useInput(this, (input, key) => {
      if (key.downArrow) {
        this.setState({ index: this.state.index + 1 });
      } else if (input === 'q') {
        getTerminalApp().exit();
      }
    });
  }

  onRender(): void {
    <Text inverse={this.focus.isFocused} value={`Item ${this.state.index}`} />;
  }
}
```

The `key` object has the same flags as Ink's (`upArrow`, `return`, `escape`, `ctrl`,
`meta`, `shift`, `tab`, `backspace`, `delete`, `pageUp`, `pageDown`, `home`, `end`).
Tab and Shift+Tab move focus between components registered with `useFocus()`;
`getTerminalApp().focus` exposes `focusNext()`, `focusPrevious()`, `focus(id)` and
`disableFocus()`.

### Static output

`<Static>` renders a list of items once, above the live frame, and never erases them.
Use it for logs and completed work. It is positioned absolutely, so it takes no rows from
the live frame:

```tsx
<Static items={this.state.log} render={(entry: LogEntry) => <Text dimColor value={entry.text} />} />
```

Only lines that have not been written yet are emitted when the list grows.

## Testing

Passing `sink: 'memory'` renders into an in-memory terminal that records everything it is
asked to write, instead of the process terminal. Combined with `nextFrame()` and
`feedInput()`, components can be tested inside a regular Valdi module test:

```tsx
import { render } from 'valdi_tui/src/TerminalRenderer';

const instance = render(Counter, {}, { sink: 'memory', columns: 40, rows: 10 });
expect(await instance.nextFrame()).toEqual(['count: 0']);
instance.feedInput('\x1b[A');
expect(await instance.nextFrame()).toEqual(['count: 1']);
instance.unmount();
```

A frame can be painted before a change has finished laying out, so tests that care about a
particular frame should keep calling `nextFrame()` until the expected one arrives. Other
options for the memory sink: `isTTY: false` writes the output once at `unmount()` like a
redirected stdout, `colorLevel` selects how many colors the recorded output uses, `output()`
returns everything written, and `setSize(columns, rows)` resizes the terminal.

The module test runner (`valdi_standalone`) registers the terminal backend, so
`bazel test` on a module that uses `valdi_tui` works without extra setup.

## How it works

`render()` asks the native terminal backend for a root with `createRoot()`. The root owns a
Valdi context whose view manager is the terminal view manager, and `render()` mounts the
component tree into that context with the normal renderer, so components go through the same
pipeline as on device:

- `<Box>` is a `<view>` and `<Text>` is a `<label>`. Their attributes are bound by the
  terminal view classes (`SCValdiView`, `SCValdiLabel`, `ValdiTerminalStatic`). The border and
  text-wrap attributes are not part of the `<view>` and `<label>` typings, so the components
  emit them through `<custom-view>` with the same classes.
- Yoga computes the layout in cells. Labels measure themselves natively, wrapping and
  truncating text at the available width with East Asian width and emoji clusters taken into
  account, so no measure callback runs in JavaScript.
- The backend paints the laid-out tree into a grid of cells, serializes it to ANSI at the
  sink's color level, and writes it through a frame writer that redraws the previous frame in
  place. Frames are coalesced to one per turn of the main thread.
- `observeFrame()` delivers each painted frame to JavaScript; `lastFrame()` and `nextFrame()`
  are built on it. `writeAbove()` queues lines that are written above the live frame, which is
  how `console.log` is redirected, and `finishRoot()` paints the last frame and leaves the
  output consistent when the application unmounts.
- Input is carried by the same bridge. `feedInput()` delivers text to the observers registered
  with `observeInput()`, which parse it into key presses and dispatch them to `useInput()`
  handlers.

The bridge module is `valdi_tui/src/TerminalBackendBridge`; its declarations live in
`TerminalBackendBridge.d.ts` and `TerminalBackend.ts` wraps it.

Limitations: no mouse support, no alternate screen, nested `<Text>` does not flow inline,
named colors are fixed RGB values (a 16-color terminal gets the nearest match rather than
its own palette entry), and Windows is not supported. Keyboard input, raw mode and resize
signals from a real terminal are not wired up yet, so with the `tty` sink an application
renders output only until a host reader feeds the input bridge.
