# RFC: Terminal renderer for CLI applications (`valdi_tui`)

- Status: Draft
- Author(s): @beaucollins
- Discussion: This pull request.
- Implementation: IMPLEMENTATION_PR_PLACEHOLDER

## Summary

Add `valdi_tui`, a module that renders Valdi components to a terminal, in the spirit of [Ink](https://github.com/vadimdemedes/ink) for React. Components are laid out by the same flexbox engine Valdi uses on device, measured in terminal cells, painted as ANSI text and redrawn in place as state changes. It runs under the existing CLI runtime (`valdi_cli_application`), which gains a small native terminal bridge for raw keyboard input, window size and resize notifications. Two latent defects in the standalone runtime that block any host driving layout from JS are fixed along the way.

## Motivation

A Valdi CLI application can print lines today, and nothing else. Developer tools, build dashboards, interactive setup flows and test reporters want the same declarative component model, state handling and flexbox layout that Valdi gives screens on device. Building those with raw escape sequences, or in a second framework such as Ink, means a second component model and no code sharing with Valdi modules.

Valdi already has every piece except the last mile: the standalone runtime runs Yoga, text can measure itself through the `onMeasure` callback, and `getRenderedElement` exposes frames and attributes of the laid-out tree. A terminal painter on top of that is small, and it keeps the CLI runtime honest about layout, which surfaced the runtime fixes below.

## Detailed design

### Module and public API

`valdi_tui` is a regular Valdi module (`//src/valdi_modules/src/valdi/valdi_tui`) that depends on `valdi_core` and `valdi_standalone`. Applications import from `valdi_tui/src/...` and build with `valdi_cli_application()`.

| Export | Role |
|---|---|
| `Box` | Flexbox container, the terminal `<view>`. Flex, padding, margin, gap, size and position props; `backgroundColor`; `borderStyle` (`single`, `double`, `round`, `bold`, `singleDouble`, `doubleSingle`, `classic`, `arrow` or a custom character set) with per-side toggles and colors. A border adds one cell of padding on each enabled side. |
| `Text` | Measured, wrapped text. `value` is a string or an array of strings and styled spans; style props are `color`, `backgroundColor`, `bold`, `dimColor`, `italic`, `underline`, `strikethrough`, `inverse`; `wrap` is `wrap`, `truncate`, `truncate-start` or `truncate-middle`. |
| `Newline`, `Spacer` | Vertical whitespace and a `flexGrow` gap. |
| `Static` | Items written once above the live frame and never erased (logs, finished work). |
| `Transform` | Post-processes the rendered lines of its children. |
| `render(component, viewModel, options)` | Mounts the root and returns an instance with `rerender`, `unmount`, `waitUntilExit`, `clear`, `lastFrame` and `nextFrame`. Options: `terminal`, `exitOnCtrlC`, `patchConsole`, `debug`, `fitTerminal`, `columns`, `errorBoundary`. |
| `useInput(component, handler)`, `useResize`, `useFocus` | Register handlers for the lifetime of a component (call from `onCreate()`); disposed with the component through `registerDisposable`. `Key` has Ink's flags (`upArrow`, `return`, `escape`, `ctrl`, `meta`, `shift`, `tab`, `backspace`, `delete`, `pageUp`, `pageDown`, `home`, `end`). |
| `getTerminalApp()` | Exit, terminal size, resize events and the focus manager (Tab / Shift+Tab cycle focus). |
| `MemoryTerminal` | In-memory terminal for tests. |

Ink's hooks map onto class components as helper functions because Valdi has no hook model; this keeps the lifetime semantics (disposal with the component) without a new primitive.

Colors accept Ink names, CSS names, `#rrggbb`, `rgb()` and `ansi256(n)`, and are downgraded to the detected level (`COLORTERM`, `TERM`, `NO_COLOR`, `FORCE_COLOR`). When stdout is not a terminal, static output is written immediately and the live frame once at unmount, like Ink in CI.

### Rendering pipeline

Components render through the normal native pipeline of the standalone runtime rather than a JS-side `IRendererDelegate` (the approach `valdi_ssr` takes). The SSR route has no layout engine; the terminal needs real flexbox.

1. `Text` emits a `custom-view` node with an `onMeasure` callback that wraps its runs at the width Yoga offers and returns the size in cells. `Box` emits a `custom-view` carrying its border and background attributes. The painter identifies both by view class (`ValdiTUIBox`, `ValdiTUIText`, `ValdiTUIStatic`).
2. Every render pass ends with `IRendererEventListener.onRenderEnd`; the renderer coalesces that into one frame per task.
3. Per frame: `setLayoutSpecs(columns, previousHeight)`, then `renderer.onLayoutComplete` to wait for the layout pass, then `measureContext` for the content height (unless `fitTerminal`), `setLayoutSpecs` again if the height changed, then `getRenderedElement`.
4. A painter walks the returned tree, rounds frames to cells, paints backgrounds, borders and wrapped text into a cell grid, and serializes each row with minimal SGR changes.
5. A frame writer erases the previous frame line by line and writes the new one (log-update semantics), emitting new `Static` lines above it once. Frames taller than the viewport clear the screen and repaint.

Yoga's config in Valdi uses point scale 0, so flex distribution yields fractional frames. `Text` measures against the rounded width and the painter wraps at the same rounded width, so measured and painted line counts agree.

### Native terminal bridge

The standalone runtime gains `TerminalModule`, registered like `DeviceModule`, exposed to JS as `valdi_standalone/src/TerminalBridge`: `isTTY(fd)`, `getSize()`, `setRawMode(enabled)`, `write(fd, text)`, `observeInput(cb)` and `observeResize(cb)`. A reader thread started by the first observer polls stdin and the window size (100 ms), delivers whole UTF-8 sequences to JS asynchronously, and is joined in `ValdiStandaloneRuntime`'s destructor before the JS runtime is torn down. Raw mode mirrors Node's `setRawMode` (no echo, no canonical mode, no signal keys; output post-processing stays on) and is restored at shutdown and through `atexit`. `valdi_tui` loads the bridge lazily and degrades to environment-derived size and `process.stdout.write` when it is absent (for example when the module is loaded by a non-CLI host).

### Runtime fixes

Both fixes are in the standalone runtime and apply to every host that drives layout from JS, not only `valdi_tui`.

- **Dummy attributes shadowed layout attributes.** With `registerCustomAttributes` on (the default in `createValdiStandaloneRuntime`, which backs `valdi_cli_application` and the module test runner), `StandaloneViewManager` binds untyped `padding`, `justifyContent`, `alignItems` and `top` so C++ tests can assert them on `StandaloneView`. `AttributesManager` lets a view-manager binding replace the default Yoga handler, so those four attributes silently stopped affecting layout in CLI apps and in `valdi_test` suites. A new `setRegisterLayoutAttributeOverrides(false)` is applied in `createValdiStandaloneRuntime`; the C++ runtime tests keep the dummies.
- **Layout specs contract.** A render that invalidates layout marks the tree's layout specs dirty, and `performUpdates` skips layout (and layout-complete callbacks) until the host calls `setLayoutSpecs` again. On device the platform layout pass does that. The terminal renderer re-sends specs every frame; this is documented so other hosts do the same.

### Platforms

`valdi_tui` targets the CLI runtime on macOS and Linux (the `valdi_cli_application` platforms). It is not meant to run on iOS, Android, macOS app hosts or web; the module compiles everywhere but the bridge is absent, so `render()` has no terminal to write to. Windows is out of scope.

### Example

```tsx
class Counter extends StatefulComponent<{}, { readonly count: number }> {
  state = { count: 0 };

  onCreate(): void {
    useInput(this, (input, key) => {
      if (key.upArrow) this.setState({ count: this.state.count + 1 });
      if (key.escape) getTerminalApp().exit();
    });
  }

  onRender(): void {
    <Box borderStyle='round' paddingX={1}>
      <Text value={['count: ', { text: String(this.state.count), color: 'green', bold: true }]} />
    </Box>;
  }
}

const app = render(Counter);
await app.waitUntilExit();
```

`apps/tui_example` is a fuller demo (header with spinner, task list driven by the arrow keys, typed note prompt, `Static` log, `Transform`), with a recording in the workflow doc.

## Automated test strategy

- **Module tests** (`//src/valdi_modules/src/valdi/valdi_tui:test`, jasmine under the standalone runtime, run by `tools/ci/test_modules.sh` in the Valdi CI workflow on macOS and Linux): unit specs for color parsing and SGR downgrade, cell widths (CJK, emoji, combining marks, ZWJ, flags), wrapping and truncation, the cell grid (wide-character overwrites, backgrounds, minimal SGR), the painter (borders, clipping, static blocks, transforms), key parsing (arrows, modifiers, ctrl/meta, paste) and the frame writer (erase sequences, static lines, non-TTY deferral). Integration specs render real components through the runtime into a `MemoryTerminal` and assert frames: layout and wrapping at a given width, re-render, `justifyContent` distribution, keyboard input, `Static` emission, Ctrl+C exit, resize re-layout and non-TTY output.
- **End-to-end smoke test** (`//apps/tui_example:tui_example_smoke_test`, an `sh_test` picked up by `tools/ci/run_tests.sh`): runs the example binary with stdout piped and checks the single frame it prints. This covers the native `cli_runner` link, the bridge's non-TTY path and the real `getRenderedElement` round trip.
- **Runtime regressions:** `//valdi:test_integration` and `//valdi:test_runtime` keep asserting the dummy attributes on `StandaloneView` (the C++ path unchanged by `setRegisterLayoutAttributeOverrides`). The `justifyContent` integration spec above is the regression test for the shadowing fix; it fails if the CLI runtime stops applying layout attributes.
- **What a regression looks like:** a wrapped line miscounted against its measured height shows up as a shifted or clipped frame in the `MemoryTerminal` specs; a lost frame after a re-render makes the `nextFrame()` specs time out; a bridge lifecycle bug (thread not joined, tty not restored) fails the smoke test or the Ctrl+C spec.
- **Not automatable:** actual terminal emulator rendering (fonts, wide-glyph handling, deferred wrap). Verified by driving the example through a pty, replaying the stream through a VT emulator and reviewing the recorded GIF; the recording is checked in with the doc.

## Compatibility

Additive: a new module, a new native module path and a new Bazel test. No existing public API changes. `valdi_tui` and `TerminalBridge` start as experimental under [VERSIONING.md](../VERSIONING.md) until the API settles.

Behavior change for CLI applications and `valdi_test` suites: `padding`, `justifyContent`, `alignItems` and `top` now affect layout under the standalone runtime, as they always did on device. Tests that implicitly relied on the attributes being inert would see different frames; none in this repository did. The C++ standalone tests are unchanged.

## Alternatives considered

- **JS renderer delegate, as `valdi_ssr` does.** No layout engine on that path; a terminal needs flexbox. Porting Yoga to JS or shipping a wasm build adds a second layout engine to keep in sync with the native one.
- **Ink itself under Node, with Valdi components transpiled.** Different component model and runtime; no reuse of Valdi modules, bridges or the compiler.
- **A curses-style C++ backend (a `StandaloneViewManager` that paints cells natively).** More code in C++ for a surface that changes often, and it would not be testable with the module test runner. The JS painter keeps the whole TUI layer in TypeScript with a native surface of six functions.
- **Switching the CLI runtime's Yoga config to point scale 1** so frames are integral, like Ink. Changes layout results for every standalone test; rounding in the TUI layer is local and sufficient.

## Drawbacks and risks

- One more module to maintain, with terminal-specific behavior (escape parsing, color detection, wide characters) that has long tails. The width tables cover the common ranges, not full Unicode `East_Asian_Width`.
- Nested `<Text>` does not flow inline; spans do. Ink users will notice.
- A stdin reader thread in the standalone runtime, started lazily. It is joined before runtime teardown; a leak here would hang CLI exit, which the smoke test and the Ctrl+C spec cover.
- The per-frame `getRenderedElement` dump copies the full tree across the main thread; fine for terminal-sized trees, not a path to reuse for large screens.
- Fractional Yoga frames are rounded in two places (measure and paint); they must stay in sync.

## Unresolved questions

- Should nested `<Text>` flow inline as in Ink? It needs either a compiler-visible way for a component to read its children as data, or a text-run collection pass; spans cover the common case today.
- Mouse support and the alternate screen are not proposed; worth deciding whether `fitTerminal` applications want them.
- Whether `createValdiStandaloneRuntime` should also opt the CLI runtime into Yoga rounding (point scale 1) once the standalone tests are audited for fractional expectations.
- RFC number is assigned at acceptance.
