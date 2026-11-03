# RFC 0003: Terminal rendering backend for CLI applications (`valdi_tui`)

- Status: Draft
- Author(s): @beaucollins
- Discussion: [This pull request](https://github.com/Snapchat/Valdi/pull/202).
- Implementation: [#203](https://github.com/Snapchat/Valdi/pull/203) carries the backend described here, as the landing-plan commits in order; the JS-painter prototype it started as is in that pull request's history. The standalone view manager fix is a separate pull request.

## Summary

Add a terminal rendering backend to the Valdi runtime: a `TerminalViewManager` that implements `IViewManager` with a new `RenderingBackendTypeTerminal`, registers the existing `<view>`, `<label>` and `<scroll>` classes under both platform names, measures text natively in cells, and paints a cell grid to a terminal sink with minimal ANSI updates. On top of it, `valdi_tui` is a module in the spirit of [Ink](https://github.com/vadimdemedes/ink): `Box`, `Text`, `Static`, `render()`, input and focus helpers, built from the existing primitives rather than new view classes. The terminal host (raw mode, stdin, resize, signal handling) is enabled only by `valdi_cli_application(terminal = True)`. A latent defect in the standalone view manager that shadowed four layout attributes is fixed in a separate change.

## Motivation

A Valdi CLI application can print lines today, and nothing else. Developer tools, build dashboards, interactive setup flows and test reporters want the declarative component model, state handling and flexbox layout that Valdi gives screens on device. Building those with raw escape sequences, or in a second framework such as Ink, means a second component model and no code sharing with Valdi modules.

Valdi already has every piece except the backend: Yoga runs in the standalone runtime, view managers are chosen per `ViewManagerContext`, and SnapDrawing shows how a non-platform backend plugs in. A terminal view manager is the missing last mile. Because it renders the existing primitives, an existing Valdi component built from `<view>` and `<label>` has a chance of rendering in a terminal unchanged, which Ink cannot offer.

## Detailed design

### Where the backend sits

```
 JS                        C++ runtime                               terminal
 ───────────────────────   ─────────────────────────────────────     ───────────
 valdi_tui (Box, Text,     TerminalModuleFactoriesProvider            TtySink
 Static, render(),   ───►  └ TerminalViewManager (IViewManager)  ───► MemorySink
 useInput, …)                 ├ ViewClass / LabelClass / ScrollClass / StaticClass
                              ├ TerminalViewTransaction → TerminalView tree
                              └ TerminalRoot: layout specs, Painter, FrameWriter
 TerminalBackendBridge ◄──  TerminalBackendModule (native module)
 (native module typings)   TerminalHost: raw mode, stdin, SIGWINCH, signal restore
```

The backend is a new library, `//valdi:valdi_terminal` (`valdi/src/valdi/terminal/`), depending on `valdi_runtime` only: no SnapDrawing, no Skia, no `backward`. It follows the SnapDrawing model:

- **Backend type.** `RenderingBackendType` gains `RenderingBackendTypeTerminal`; `getBackendString` returns `"terminal"`. `getBackendRenderingTypeForContextId` returns a new value `4` for contexts on the terminal view manager, `BackendRenderingType.TERMINAL` in `valdi_core`, and `Asset.ts` resolves images and video to `AssetOutputType.DUMMY` for it.
- **View classes** mirror `ILayerClass`: each carries its iOS and Android class names, a parent class, `canBeMeasured`, `bindAttributes(binder)`, and is a `DefaultMeasureDelegate`. `TerminalViewManager` registers each class under both names, so JSX resolution needs no change on any platform the CLI reports.

  | Class | Names | Role |
  |---|---|---|
  | `ViewClass` | `SCValdiView`, `com.snap.valdi.views.ValdiView` | Box: background, border |
  | `LabelClass` | `SCValdiLabel`, `com.snap.valdi.views.ValdiTextView` | Measured, wrapped, styled text |
  | `ScrollClass` | `SCValdiScrollView`, `com.snap.valdi.views.ValdiScrollView` | Clipped viewport, translated by the scroll offset |
  | `StaticClass` | `ValdiTerminalStatic` (both platforms) | Children written once above the live frame |

- **View manager context.** A `TerminalModuleFactoriesProvider` lazily creates one `TerminalViewManager` and one `ViewManagerContext` on it, as `SnapDrawingModuleFactoriesProvider` does. The default view manager of the standalone runtime is untouched.
- **Root creation** copies the SnapDrawing managed-context path (`createValdiContextWithSnapDrawing`): the native `TerminalBackendModule` creates a `Context` on the terminal `ViewManagerContext`, creates its `ViewNodeTree`, calls `setRootViewWithDefaultViewClass()` and `setRetainsLayoutSpecsOnInvalidateLayout(true)`, wraps it in a `TerminalRoot`, and returns `{ contextId, native }`. `render()` in `valdi_tui` then renders with `jsx.makeRenderer(contextId)`. Views inflate because a root view exists.
- **Transaction.** `TerminalViewTransaction` implements `IViewTransaction` over a tree of `TerminalView`s (class, parent, children, integer cell frame, style, cached label lines, scroll offset). Every mutation marks the root dirty; `didUpdateRootView` schedules one coalesced paint on the main queue.
- **Frame cycle.** `TerminalRoot::paintNow()` walks the view tree into a `CellGrid` (grapheme, cell width 1 or 2, style), the `FrameWriter` diffs it against the previous grid and emits the minimal escape sequences (erase live region, new `Static` lines, redraw), and the bytes go to an `ITerminalSink`: `TtySink` (a file descriptor, color level from `COLORTERM`, `TERM`, `NO_COLOR`, `FORCE_COLOR`) or `MemorySink` (tests). Frame observers registered from JS receive the plain lines and the ANSI text asynchronously; this is what `nextFrame()` resolves on. No JS code runs per frame, and frames keep painting while the JS thread is busy.
- **Layout specs** are owned by `TerminalRoot`, as the platform layout pass owns them on every other backend: `Size(columns, NaN)` for content-height output (the existing NaN → Unspecified path in `doCalculateLayoutOnNode`), `Size(columns, rows)` for `fitTerminal`. They are sent at creation and on resize. Because the tree retains its specs on invalidation, a re-render re-lays out at content height with no host round trip.
- **Rounding** happens once, in Yoga. `RuntimeManager::createViewManagerContext` gains an optional Yoga config parameter; the terminal context uses point scale 1, so frames land on whole cells and flex siblings stay contiguous. Each `ViewNode` takes its Yoga config from its `ViewManagerContext`'s `AttributesManager`, and Yoga's pixel-grid rounding reads the node's own config, so the override is sufficient. The one runtime-level mutation of the shared config (`YGExperimentalFeatureFixFlexBasisFitContent`, applied from tweaks) is applied to the terminal config as well. `setViewFrame` casts and debug-asserts integrality.

### Attribute vocabulary

Attributes are per view manager, so the terminal binds existing names where the meaning carries and adds terminal-only names where it does not.

`<view>` (`ViewClass`): `backgroundColor`; `borderColor`; `border` (`[width, color]`, the existing shorthand); `borderWidth` is paint-only (any value above 0 draws a one-cell border inside the frame), and `Box` reserves the cell through Yoga's per-side `borderTopWidth` and friends, so a bordered box's children never overlap the frame. Terminal-only: `borderStyle` (`single`, `double`, `round`, `bold`, `singleDouble`, `doubleSingle`, `classic`, `arrow`), `borderTop` / `borderBottom` / `borderLeft` / `borderRight` (default true), per-side colors, `borderDim`.

`<label>` (`LabelClass`): `value` as a string or attributed text whose spans carry `color`, `font`, `textDecoration` and `background.color`; `color`; `font` in the existing `"<family>-<weight>[-italic] <size>"` convention, where bold, semibold, heavy and black map to bold, light and thin map to dim (both can appear, as in `system-bold-light 1`), italic maps to italic, and size is ignored; Yoga padding on the label insets the text; `textDecoration` (`underline`, `strikethrough`, `none`); `textAlign`; `numberOfLines` (Valdi semantics: absent means 1; `Text` passes 0 for unlimited); `textOverflow` (`ellipsis`, `clip`). Terminal-only: `inverse`, and `textWrap` (`wrap`, `truncate`, `truncate-start`, `truncate-middle`, `truncate-end`) for Ink's modes that `textOverflow` cannot express. Colors are Valdi ARGB values; the backend downgrades them to the detected color level (truecolor, 256, 16) natively. `onMeasure` wraps the runs at the integer width Yoga offers with the same code the painter uses, so measurement and painting cannot disagree. Control characters and invalid UTF-8 in label text are dropped before layout, so untrusted text cannot inject escape sequences.

`<scroll>` (`ScrollClass`): `bindScrollAttributes()`; `setViewScrollSpecs` stores the offset; the painter clips children to the frame and translates by the offset. Key-driven scrolling is not in this proposal.

`Static` (`StaticClass`): laid out at terminal width but positioned absolutely so it adds no live height; `TerminalRoot` tracks how many children each `Static` view has emitted and hands new children's lines to the `FrameWriter` as permanent lines above the live frame. `Static.tsx` emits `<custom-view iosClass='ValdiTerminalStatic' androidClass='ValdiTerminalStatic'>`; no new JSX element is added to `valdi_core`.

### Module and public API

`valdi_tui` (`//src/valdi_modules/src/valdi/valdi_tui`) keeps the API of the prototype and loses its painter:

| Export | Role |
|---|---|
| `Box` | Flexbox container over the `SCValdiView` class: flex, padding, margin, gap, size and position props; `backgroundColor`; `borderStyle` with per-side toggles and colors. `Box` and `Text` emit `<custom-view>` with the platform class names rather than `<view>` and `<label>`, because the typings of those elements do not accept the terminal-only attributes; the native classes are the same. |
| `Text` | Text over the `SCValdiLabel` class: `value` as a string or an array of strings and styled spans; `color`, `backgroundColor`, `bold`, `dimColor`, `italic`, `underline`, `strikethrough`, `inverse`; `wrap`. Ink and CSS color names, `#rrggbb`, `rgb()` and `ansi256(n)` are converted to Valdi colors in JS. |
| `Newline`, `Spacer` | Vertical whitespace and a `flexGrow` gap. |
| `Static` | Items written once above the live frame and never erased. |
| `Transform` | Transforms the text value passed to its children's labels. Ink's `Transform` runs per output line; this one runs per value, because wrapping is native. |
| `render(component, viewModel, options)` | Creates a terminal root through the bridge, wraps the component in a column view so a root `Text` or `Static` works, and mounts it; returns `rerender`, `unmount`, `waitUntilExit`, `clear`, `lastFrame`, `nextFrame`. Options: `sink` (`tty` or `memory`), `columns`, `rows`, `exitOnCtrlC`, `patchConsole`, `fitTerminal`, `debug`, `errorBoundary`. Construction and `unmount()` are exception-safe: a failing root render ends the keep-alive, restores `console` and destroys the root before rethrowing. |
| `useInput(component, handler)`, `useResize`, `useFocus` | Register handlers for the lifetime of a component; disposed through `registerDisposable`. `Key` has Ink's flags. |
| `getTerminalApp()` | Exit, terminal size, resize events and the focus manager. |

Ink's hooks map onto class components as helper functions because Valdi has no hook model. Key parsing stays in JS: escape sequences arrive as bytes from the host and are application-facing. Tests use `render(App, vm, { sink: 'memory', columns, rows })` and drive it with the bridge's `feedInput` and `setSize`; `MemoryTerminal` from the prototype moves below the view manager as `MemorySink`.

The bridge, `valdi_tui/src/TerminalBackendBridge`, is a native module registered by the provider: `createRoot(options)`, `finishRoot`, `destroyRoot`, `observeFrame`, `getLastFrame`, `setSize` and `feedInput` (memory sink), `writeAbove` and `clearRoot` (patched console output and `clear()`), `getOutput`, `getSize`, `isTTY`, `setRawMode`, and per-root `observeInput` / `observeResize`. Frames carry a sequence number so a frame painted synchronously on the JS thread (for example by `finishRoot`) cannot be overtaken by an older one still queued from the main thread.

### Host and CLI scoping

The terminal host is a CLI concern. `valdi_cli_application` gains `terminal = False`; when set, the single CLI main template registers the `TerminalModuleFactoriesProvider` with a `TerminalHost` through `CLIRunOptions.moduleFactoriesProviders` and links `//valdi:valdi_terminal`. `CLIRunner` takes that options struct (request manager, providers, platform type, a `beforeRun` hook that installs the terminal's signal handlers after the crash handler) alongside its current signature. Linking the library is not enough; without `terminal = True`, `render()` fails with a clear error. The standalone runtime itself, the module test runner and any embedding host under [RFC 0002](0002-linux-headless-sdk.md) register no terminal module.

The CLI reports its real platform: `PlatformTypeMacOS` on macOS, `PlatformTypeLinux` on Linux. JSX already resolves `iosClass` names on both, with the `linuxClass` → `macosClass` → `iosClass` fallthrough that [#200](https://github.com/Snapchat/Valdi/pull/200) added for [RFC 0001](0001-linux-snapdrawing-host.md). `DeviceModule::getSystemType()` keeps reporting `ios` for now: `DeviceSystemType` is a public enum and consumers switch over it exhaustively, so adding `macos` and `linux` is a breaking change that belongs with [RFC 0002](0002-linux-headless-sdk.md)'s Linux service identity work.

`TerminalHost` is a process-wide singleton that owns raw mode (Node's `setRawMode` subset: no echo, no canonical mode, no signal keys, output post-processing on; entered only when both stdin and stdout are terminals), a stdin reader thread with a wake pipe, and resize via a `SIGWINCH` handler that writes to that pipe. It installs handlers after the existing crash handler: `SIGINT`, `SIGTERM`, `SIGHUP`, `SIGQUIT` and `SIGPIPE` restore termios and the cursor with async-signal-safe calls, reset the disposition and re-raise; the crash signals `backward` handles (`SIGSEGV`, `SIGABRT`, `SIGBUS`, `SIGFPE`, `SIGILL`, `SIGTRAP`, `SIGSYS`, `SIGXCPU`, `SIGXFSZ`) restore and chain into `backward`'s saved handler with other signals masked, so the stack trace still prints whole. With signal keys off, Ctrl+C arrives as data: when `exitOnCtrlC` is set (the default) the reader thread restores the terminal and requests exit on the main queue, and a watchdog calls `_exit(130)` if the queue has not drained within a second, so a hung JS thread cannot trap the terminal; a second Ctrl+C exits at once. Otherwise the byte is forwarded to JS input handlers. The exit status is 130 and no final frame is painted. The `atexit` restore remains. Input delivered from the reader thread never performs the last release of a root handle; releases hop to the JS thread.

### Platforms

The backend targets the CLI runtime on macOS and Linux. The module compiles everywhere, but only a `terminal = True` CLI binary registers the provider; `render()` without it fails with a clear "terminal backend not registered" error. Windows is out of scope.

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

`apps/tui_example` is a fuller demo built with `valdi_cli_application(terminal = True)`.

### Standalone view manager fix (separate change)

With `registerCustomAttributes` on (the default in `createValdiStandaloneRuntime`, which backs CLI applications and the module test runner), `StandaloneViewManager` binds untyped `padding`, `justifyContent`, `alignItems` and `top` so C++ tests can assert them on `StandaloneView`. `AttributesManager` lets a view-manager binding replace the default Yoga handler, so those four attributes silently stopped affecting layout in CLI apps and in every module test suite. The fix drops the four dummies and gives the C++ integration fixture a test-only hook to register them, so the tests that assert them keep passing and every other host gets real layout. It lands on its own so it can roll back independently.

## Automated test strategy

- **C++ backend tests** (`//valdi:test_terminal`, part of `//valdi:test`, run by `tools/ci/run_tests.sh` on macOS and Linux; 150 and more cases): cell grid (wide characters, clipping, opaque and translucent background fills), text layout (word wrap, hard breaks, truncation modes, CJK, emoji, combining marks, ZWJ, flags, control-character and invalid-UTF-8 sanitizing), ANSI (color downgrade matching `ansi-styles`, minimal SGR runs, erase sequences), painter (borders, nested frames, `overflow: hidden`, static blocks, label padding), frame writer (redraw in place, static lines once, a full-screen frame keeps its first row, static lines survive a tall frame, non-TTY single frame) checked against a small VT emulator, label measurement across measure modes, root lifecycle (coalesced paints, observers called without locks, destroy finishes output, dropped frame requests, view manager outlives its runtime), the sinks, and end-to-end renders of hand-built trees through a real `ViewNodeTree` into `MemorySink`. The expected strings are the prototype's spec expectations, ported, so the rework is checked against the behavior reviewers already saw.
- **Module tests** (`//src/valdi_modules/src/valdi/valdi_tui:test`, jasmine under the standalone runtime, run by `tools/ci/test_modules.sh`): `render()` against `MemorySink` with `nextFrame()`: layout and wrapping at a width, re-render, `justifyContent` distribution, keyboard input through `feedInput`, `Static` emission, Ctrl+C exit, resize re-layout, non-TTY output, a root `Text`, the first frame already laid out, two renders with separate input, exception-safe mount and unmount; plus key parsing (including Shift+Tab), focus and color-name specs. The module test runner links the backend so these specs can run; the TTY host is never engaged by a spec.
- **Runtime regressions:** `//valdi:test_integration` and `//valdi:test_runtime` keep asserting dummy attributes on `StandaloneView` through the fixture hook; a new module spec asserts that `justifyContent`, `alignItems`, `padding` and `top` affect layout under the standalone runtime, which fails if the shadowing returns.
- **End-to-end:** `//apps/tui_example:tui_example_smoke_test` (`sh_test`) runs the binary with `--once` and stdout piped and checks the single printed frame. `//apps/tui_example:tui_example_pty_test` (`py_test`) drives the binary through a pseudo-terminal with `termios` snapshots of the slave: raw mode while running, arrow-key and typed input change the frame, `SIGWINCH` re-lays out, `Esc` exits cleanly, `SIGTERM` and `SIGHUP` restore every termios field and end the output with the cursor-show sequence, Ctrl+C exits 130 with the terminal restored (also with a deliberately hung JS thread, and on a second press), `SIGSEGV` and `SIGABRT` restore the terminal and still print a stack trace, and a run with stdout piped never enters raw mode.
- **What a regression looks like:** a wrapping or width-table change shows up as a byte difference in a gtest golden; a lost frame or layout-specs regression makes a `nextFrame()` spec time out; a view-manager class registration mistake fails the `justifyContent` module spec; a tty-restore or signal regression fails the pty test; a CLI binary that no longer registers the provider fails the smoke test.
- **Not automatable:** rendering in real terminal emulators (fonts, wide-glyph handling, deferred wrap). Verified by driving the example through a pty, replaying through a VT emulator and reviewing the recording checked in with the doc. The recording in the doc is still the prototype's and will be re-captured.

## Compatibility

Additive: a new library, a new `RenderingBackendType` value and JS backend id, a new native module path, a new `valdi_cli_application` attribute, an optional parameter on `RuntimeManager::createViewManagerContext`, a `platformType` on `StandaloneArguments`, an options overload of `valdiCLIRun`, and `getCatchAllUncaughtErrorHandler()` in `valdi_core`. The two CLI main templates become one with substitutions; the generated `main.cpp` for existing applications is equivalent. No existing public API changes. `valdi_tui` and the terminal bridge start as experimental under [VERSIONING.md](../VERSIONING.md).

Behavior change from the separate standalone fix: `padding`, `justifyContent`, `alignItems` and `top` now affect layout under the standalone runtime, as they always did on device. Module test suites outside this repository that implicitly relied on the attributes being inert would see different frames; the change lands on its own so it can be bisected and rolled back.

## Landing plan

Each change builds and tests green on `main`:

1. Standalone view manager fix (independent, its own pull request).
2. Plumbing: backend enum and ids, `Asset.ts`, Yoga config parameter.
3. `//valdi:valdi_terminal` with gtests (`StaticClass` and the MIT notices for the ported cli-boxes border sets, ansi-styles color math and ansi-regex pattern included); nothing links it yet.
4. Bridge module, `valdi_tui` rewrite, module specs, docs; the test runner links the backend.
5. TTY host, CLI template and options, signals, the example and its pty test.

Steps 2 to 5 are the commits of #203, in that order, each building and testing green on its own. Follow-ups: ICU-backed `East_Asian_Width` and grapheme segmentation, key-driven scrolling, `<textfield>`, desktop device identity (RFC 0002), a fresh demo recording.

## Alternatives considered

- **JS painter over `getRenderedElement` (the #203 prototype).** Rejected: it sits beside the rendering model, re-sends layout specs and copies the whole tree every frame, rounds in two places that must agree, resolves its classes only because the CLI reports iOS, and cannot be reached by existing components.
- **JS renderer delegate, as `valdi_ssr` does.** No layout engine on that path; a terminal needs flexbox.
- **Ink itself under Node, with Valdi components transpiled.** Different component model and runtime; no reuse of Valdi modules, bridges or the compiler.
- **TS painter behind the view manager.** Keeps the prototype's TypeScript, but native measurement and JS wrapping would have to agree byte for byte, and the per-frame snapshot copy to JS stays. Measurement must be native, so the painter is too.
- **Switching the shared Yoga config to point scale 1.** Changes layout results for every standalone test; the per-context config keeps the change local to the terminal.
- **A new JSX element for `Static`.** `<custom-view>` with the registered class works on every platform the CLI reports; a core element adds backend vocabulary for no gain.

## Drawbacks and risks

- More C++ than the prototype: text layout, width tables, SGR emission and frame diffing move native. They are small and golden-tested, but Unicode width coverage is a long tail; the vendored ICU is the path to full `East_Asian_Width` and grapheme segmentation.
- A stdin reader thread and signal handlers in the CLI host. Install order with `backward` matters; the pty test covers restore on `SIGTERM`, `SIGHUP`, Ctrl+C, `SIGSEGV` and `SIGABRT`. `SIGWINCH` replaces any earlier handler rather than chaining, and the terminating-signal handlers replace `backward`'s `SIGQUIT` trace.
- Linking the backend into the shared module test runner grows that binary and makes the terminal available to every module spec, with the memory sink only. The terminal context disables viewport culling for its JS `Runtime`, which the test runner shares with every other context in a run.
- Nested `<Text>` does not flow inline; spans do. `Transform` is value-level, not line-level. `inverse` applies to a whole `Text`, not per span. Named ANSI colors are converted to RGB in JS and downgraded natively, so a 16-color terminal gets the nearest palette entry rather than its own.
- `<textfield>`, scrolling by key, mouse and the alternate screen are not proposed.

## Unresolved questions

- Should the module test runner link the backend, or should `valdi_test` grow a per-module runner override?
- When `DeviceSystemType` gains `macos` / `linux` (RFC 0002), should every standalone host report them or only the terminal CLI path? Consumers with exhaustive switches over the enum need a migration note either way.
