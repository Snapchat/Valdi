# Experimental SDL3 Linux window host

CPU raster bring-up for a single SnapDrawing `LayerRoot`. The host owns the SDL
window, main-thread event loop, bounded temporary RGBA buffer, and presentation
through `SDL_GetWindowSurface` / `SDL_UpdateWindowSurface`. SnapDrawing renders
on the CPU; SDL owns presentation and may internally use an accelerated renderer
and graphics context for that surface. The host creates no SnapDrawing GPU
context and manages no swapchain. Disabling `SDL_FRAMEBUFFER_ACCELERATION` is
not portable: the tested Wayland backend required SDL's default presentation
path. SDL3 is an optional dependency of this target;
the existing Linux runtime, Android, and Apple targets do not depend on it.

The caller supplies the root, optional Valdi `TaskQueue`, and `WindowClient`.
Those objects outlive the host. Destroy the host before destroying the root or
runtime. It exclusively occupies the root listener slot, chains an existing
queue listener and restores it during shutdown. Do not replace either listener
while open. It currently consumes the process-wide SDL event loop: multiple
independent SDL windows are outside this prototype's contract.

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
registration and ordered shutdown. The [calculator](../apps/linux_calculator/README.md)
uses this adapter without application-specific services:

```sh
bazel build //apps/linux_calculator:calculator \
  --//bzl/valdi:js_engine=quickjs \
  --//bzl/valdi:js_bytecode_format=quickjs
bazel-bin/apps/linux_calculator/calculator /path/to/modules /path/to/font.ttf
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
SnapDrawing, including the existing desktop custom-view fallback policy.

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
node valdi_linux_window/test/JSXPlatformTest.cjs /path/to/typescript/lib/typescript.js
```

Use TypeScript 5.3.3. This source-level test complements the native calculator
integration; it does not establish native custom-view support.

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
calculator's optional `calculator_theme_test` binary uses the same deterministic
source to assert initial dark/light/unknown and live transitions through the real
Device API, Valdi component and raster pixels, without changing user settings:

```sh
bazel build //apps/linux_calculator:calculator_theme_test
mkdir -p /tmp/valdi-calculator-modules
SDL_VIDEODRIVER=dummy SDL_FRAMEBUFFER_ACCELERATION=0 \
  bazel-bin/apps/linux_calculator/calculator_theme_test \
  /tmp/valdi-calculator-modules /path/to/font.ttf --self-test
```

Synthetic changes validate our event/bridge/rendering path, not a desktop portal
implementation. Real desktop preference publication and notifications require
separate integration testing on each supported desktop/backend.
