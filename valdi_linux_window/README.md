# Experimental SDL3 Linux window host

CPU raster bring-up for a single SnapDrawing `LayerRoot`. The host owns the SDL
window, main-thread event loop, bounded temporary RGBA buffer, and presentation
through `SDL_GetWindowSurface` / `SDL_UpdateWindowSurface`. SnapDrawing renders
on the CPU; SDL owns presentation and may internally use an accelerated renderer
and graphics context for that surface. The host creates no SnapDrawing GPU
context and manages no swapchain. Disabling `SDL_FRAMEBUFFER_ACCELERATION` is
not portable: the tested Wayland backend required SDL's default presentation
path. SDL3 is linked only by the optional graphical targets, but the prototype
registers system discovery in the root Bazel module; dependency-resolution
isolation is still a draft gate.

The caller supplies the root, optional Valdi `TaskQueue`, and `WindowClient`.
Those objects outlive the host. Destroy the host before destroying the root or
runtime. It exclusively occupies the root listener slot, chains an existing
queue listener and restores it during shutdown. Do not replace either listener
while open. It currently consumes the process-wide SDL event loop: multiple
independent SDL windows are outside this prototype's contract.

When `pkg-config` or its `sdl3` package is absent, the development repository
writes an incompatible `sdl3` stub and prints installation guidance. Wildcard
queries, fetches and unrelated targets can still resolve the repository; SDL
targets are incompatible until the development package is installed. An installed
SDL3 version below 3.2 or invalid package metadata remains a hard error.

## Prototype status and RFC review gates

`SDLWindowHost`, `WindowClient`, `ValdiWindowApplication` and their option/metric
types are experimental APIs. They can change as the
[RFC review](https://github.com/Snapchat/Valdi/pull/196#issuecomment-6370179837)
is resolved. The manual targets are a raster bring-up prototype, not a supported
Linux application target or a Linux support-tier change. There is no window-host
mode in `valdi_application` or the CLI yet.

The following review prerequisites remain unimplemented here:

- **Main queue and shutdown:** the host replaces the single queue-listener slot
  and forwards it, rather than using a supported subscription. The standalone
  idle-exit coordinator is still active, and the pump does not close on a disposed
  main queue. Teardown destroys SDL resources before releasing the component and
  has no explicit context-destruction completion barrier before runtime teardown.
  Repeated prototype cycles do not prove that this ordering is safe.
- **Scheduling and lifecycle:** delayed work uses a 16 ms poll. Queue deadlines
  and frame-invalidation wakeups, host-owned main-thread initialization, explicit
  exit-code/close semantics and minimize/pause integration need coordinated core
  changes and regression tests.
- **Drawing and Device data:** the adapter constructs a second drawing runtime
  without the standalone runtime's `FontResolverWithRuntimeManager`. Reusing the
  existing drawing runtime remains required. Device identity/window dimensions
  and effective-scale updates are missing; only appearance is forwarded.
- **Fonts and scale:** a supplied font file in one weight is prototype coverage.
  There is no system discovery, CJK/emoji fallback qualification or vendored text
  baseline. SDL display scale is reported but not applied to layout/input, so
  X11 HiDPI remains incorrect. Native fractional scaling is unqualified.
- **Dependency isolation:** root-level `pkg-config` discovery is non-hermetic
  even though missing installations now yield incompatible targets. The RFC
  proposes a separately rooted host Bazel module with pinned BCR SDL3; that migration and
  its license/runtime-dependency audit are not implemented. Wayland presentation
  may need EGL/Mesa despite CPU rasterization; the llvmpipe/unwind issue remains
  open.
- **Acceptance testing:** graphical targets are manual and have no agreed Linux
  CI job/budget, vendored text font, Linux text snapshots/tolerance or failure
  artifact upload. The portable JSX regression runs in the existing module-test
  step; that does not qualify the graphical host. A full current-base engine
  build and lifecycle/sanitizer qualification are still required.

Real editing/selection/IBus/Fcitx acceptance needs a separate editing RFC and
cross-platform tests. Public keyboard/hover APIs, cursors/multiple pointers,
custom-native-view registration, accessibility, multiple windows, arm64 and GPU
rendering are outside this prototype's qualification. Debugger and hot reload
are off in the adapter; any future enabled debugger needs a loopback-only gate.
RFC acceptance itself assigns no maintainer, CI budget or support-tier change.

## Build

Install SDL3 >= 3.2 headers/libraries and pkg-config. The initial Bazel binding
uses the system SDL3 installation when this optional target is requested.
It is a local development binding, not a hermetic dependency or SDK packaging
policy. A supported upstream dependency/version/distribution policy is still
required before shipping this as a supported platform.

```sh
bazel build //apps/linux_window_demo:linux_window_demo
bazel test //valdi_linux_window:framebuffer_test
xvfb-run -a -s "-screen 0 1280x900x24 -noreset" bazel test \
  //valdi_linux_window:sdl_window_host_test --test_env=DISPLAY \
  --test_env=SDL_VIDEODRIVER=x11 --test_env=SDL_FRAMEBUFFER_ACCELERATION=0 --test_output=errors
bazel-bin/apps/linux_window_demo/linux_window_demo /path/to/font.ttf
```

The demo registers an explicitly supplied font, renders a native SnapDrawing
layer tree, counts taps and displays text commit/preedit events. It does not
yet instantiate a compiled Valdi component or a complete text editor.

`ValdiWindowApplication` adds standalone runtime ownership, a SnapDrawing
view-manager context, a compiled component mounted through `ValdiLayer`, font
registration and prototype shutdown. The [calculator](../apps/calculator/README.md)
uses this adapter without application-specific services:

```sh
bazel build //apps/calculator:calculator_linux \
  --//bzl/valdi:js_engine=quickjs \
  --//bzl/valdi:js_bytecode_format=quickjs
bazel-bin/apps/calculator/calculator_linux /path/to/modules /path/to/font.ttf
```

Keep the runtime engine, generated module bytecode and compiler/toolbox cohort
matched. The host inherits the runtime's configured engine; it does not require
Hermes. The explicit QuickJS flags above match the qualified clean SDK consumer
cohort. See the calculator notes before consuming a prebuilt SDK.

Clean SDK-consumer validation uses public runtime source
`53b70efda8c520037ecba12a7264b777d9e3f4fd` and SDK export recipe
`b37c808620d64a67a44f43823a824ea02a007f98`. The calculator and host suites
passed three complete lifecycles on each of X11, Wayland 1× and Wayland 2×.
The initial calculator run found that JSX bootstrap rejected Linux platform ID 5.
This change accepts Linux and uses the iOS class names already registered by
SnapDrawing, with Linux custom-class precedence `linuxClass`, then `macosClass`, then
`iosClass`. The precedence correction has portable source-level coverage; the
historical native runs below predate it.

The corrected core module was source-generated in Bazel with the matched retained
compiler/toolbox and a source-restored companion; its native stub and four
transitive modules remained unchanged. A consumer-side core-resource archive
replaced only the generated resource object. The sealed SDK engine/manifest was
unchanged. Application/test compilation and the module-generation audit found no
engine compilation. Final runtime checks ran with the module producer source and
Bazel cache inaccessible.

This qualifies an explicit SDK/source cohort. A full Bazel build of these targets
against the newer upstream engine remains unverified. Copied SDK compiler tools
also remain unqualified for arbitrary standalone TypeScript compilation; use the
matched source build for module generation.

For already-built test binaries, `bash valdi_linux_window/test/run-displays.sh
/path/to/host-test /path/to/results` starts private Xvfb and headless Weston
displays, checks 1× and 2× Wayland outputs, and cleans up those display processes.
It requires Weston with headless, Pixman and fake-seat support, Xvfb, Mesa
software rendering and `timeout`. It creates no VM, container or deployment.

The headless Wayland harness defaults to Mesa `softpipe` for its controlled
software presentation check; set `GALLIUM_DRIVER` explicitly to investigate
other drivers. A retained LLVM/libunwind consumer with Mesa llvmpipe emitted
bad-FDE diagnostics on the validation VM even though native host assertions
passed. Softpipe completed those assertions without these diagnostics; this does
not resolve or qualify LLVM/JIT unwind interoperability. See the
[Mesa driver-selection documentation](https://docs.mesa3d.org/envvars.html#gallium-driver).

The bootstrap platform-mapping regression test exercises the real TypeScript
module with native/debugging services stubbed, including Linux initialization,
built-in class names, desktop custom-view fallbacks, and existing platforms:

```sh
bazel test //src/valdi_modules/src/valdi/valdi_core:jsx_platform_test --test_output=errors
# Or run directly with the repository-pinned TypeScript:
node src/valdi_modules/src/valdi/valdi_core/test/JSXPlatformTest.cjs /path/to/typescript/lib/typescript.js
```

Use TypeScript 5.3.3. `tools/ci/test_modules.sh` includes the Bazel target in
the existing `test-modules` job. This source-level test complements the native
calculator integration; it does not establish native custom-view support.

## Input, scale, scheduling and lifetime

- Mouse button/motion/wheel events enter `LayerRoot::dispatchTouchEvent`. Layout,
  pointer positions and IME rectangles share SDL window coordinates. Framebuffer
  size comes from the reacquired SDL surface. Pixel density and the user's
  display/content scale are reported separately; content-scale policy belongs
  in the application adapter. Touch, hover, key binding and scroll-step policy
  need follow-up work.
- Key events are commands, never synthesized characters. Committed UTF-8 text
  and composing UTF-8 text are separate callbacks. Composition offsets retain
  SDL's character units, including `-1`; a future editor adapter must convert
  those to its own indexing model. `setTextInput` controls activation and caret
  rectangle; focus loss stops input and clears composition. Clipboard APIs
  copy data with SDL's allocator correctly paired with `SDL_free`.
- The optional task queue wakes SDL without calling into the queue under its
  lock. Each pump processes at most 256 events and 32 tasks (with a 2 ms budget
  checked between tasks; an individual callback is not preemptible). Rendering
  is capped near 60 Hz and runs when the tree or window is dirty. A 16 ms timed
  queue check covers delayed work because `TaskQueue` exposes no next deadline.
  This is not a compositor presentation-clock implementation.
- Framebuffer allocation is bounded and checked before multiplication. SDL
  owns its window surface; it is reacquired after resizing. A local bitmap
  drawable cannot outlive or retain the temporary pixels across reallocations.
  The root listener detaches and the original queue listener is restored before
  window/video cleanup. The caller then destroys the root and runtime in its
  own ownership order.

## Tests and remaining gates

The display test checks exact raster output, resize, event routing, separate
preedit/commit handling, clipboard, tap delivery, worker/delayed queue work,
bounded queue batches, idle rendering and three open/close cycles. These tests
also verify forwarding/restoration of a pre-existing queue listener. The
calculator checks its real Valdi layout, tap handlers, keyboard bridge and
runtime reopen. The host tests inject SDL text events. They do **not** establish real IBus/Fcitx composition,
candidate placement, surrounding text or full editor behavior.

Before this is a complete Valdi Linux application host: validate a clean current
upstream source build, establish a supported dependency binding, add editor
focus/selection/index conversion, validate real desktop IMEs and clipboard ownership changes, implement accessibility, and test physical
GPU/multi-monitor/fractional-scale configurations. GPU presentation, external
native views and performance claims require separate work and measurements.

Primary API references: [SDL window surfaces](https://wiki.libsdl.org/SDL3/SDL_GetWindowSurface),
[text composition](https://wiki.libsdl.org/SDL3/SDL_TextEditingEvent),
[text input area](https://wiki.libsdl.org/SDL3/SDL_SetTextInputArea),
[window display scale](https://wiki.libsdl.org/SDL3/SDL_GetWindowDisplayScale).
See also [SDL framebuffer acceleration](https://wiki.libsdl.org/SDL3/SDL_HINT_FRAMEBUFFER_ACCELERATION).

## System appearance

After SDL video initialization, the host reads
[`SDL_GetSystemTheme`](https://wiki.libsdl.org/SDL3/SDL_GetSystemTheme) on the main
thread and reports its initial `SystemTheme` before `open()` returns. It queries
again on `SDL_EVENT_SYSTEM_THEME_CHANGED`, suppressing unchanged values. The
host preserves `Unknown`, `Light` and `Dark`; it does not infer appearance from
wall-clock time, window background colors or a theme-name environment variable.

`ValdiWindowApplication` supplies the effective value through the standalone
runtime's `setDarkMode()` before loading the application, then forwards updates.
The existing `Device.isDarkMode()` / `Device.observeDarkMode()` APIs use light
for unknown/no preference. Applications own their palettes and cancel their
subscriptions when destroyed; the calculator demonstrates both. A desktop or
SDL backend that does not publish appearance will remain on the light fallback.
This requires the standalone Device observer fix submitted separately from the
optional SDL host; no SDL dependency is added to headless runtime targets.

The host regression test wraps only `SDL_GetSystemTheme` at link time. The
calculator's optional `calculator_linux_theme_test` binary uses the same deterministic
source to assert initial dark/light/unknown and live transitions through the real
Device API, Valdi component and raster pixels, without changing user settings:

```sh
bazel build //apps/calculator:calculator_linux_theme_test
mkdir -p /tmp/valdi-calculator-modules
SDL_VIDEODRIVER=dummy SDL_FRAMEBUFFER_ACCELERATION=0 \
  bazel-bin/apps/calculator/calculator_linux_theme_test \
  /tmp/valdi-calculator-modules /path/to/font.ttf --self-test
```

Synthetic changes validate our event/bridge/rendering path, not a desktop portal
implementation. Real desktop preference publication and notifications require
separate integration testing on each supported desktop/backend.
