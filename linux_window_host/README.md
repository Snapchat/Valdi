# Experimental SDL3 Linux window host

This separately rooted Bazel module hosts one CPU-raster SnapDrawing `LayerRoot`
in an SDL3 window. It is an experimental stage-1 prototype for
[RFC 0001](../rfcs/0001-linux-snapdrawing-host.md), not a supported Linux application
target or a support-tier change. `SDLWindowHost`, `WindowClient`,
`ValdiWindowApplication` and their options remain experimental APIs.

SnapDrawing rasterizes on the CPU. SDL owns the window surface and presentation;
it may use an accelerated presentation path internally. This host does not create
a SnapDrawing GPU context or own a swapchain. Debugger and hot reload are off.

## Module and dependency isolation

Run graphical builds from this directory. Its `MODULE.bazel` pins the BCR
[`sdl3` 3.4.18 module](https://github.com/bazelbuild/bazel-central-registry/tree/main/modules/sdl3/3.4.18)
and imports the parent checkout as `@valdi`. SDL is built from the registry's
checksum-verified release archive; there is no system SDL or `pkg-config` discovery.
Valdi's root `MODULE.bazel` has no SDL registration, and `.bazelignore` excludes
this module from root wildcard queries/builds. The shared calculator model, TSX
component and web launcher remain in `@valdi//apps/calculator`.

Bazel does not inherit root-only overrides from dependencies. This module repeats
Valdi's source submodule overrides and version pins; `check_config.py` runs before
the host CI build and fails if those settings or `.bazelversion` drift from the
parent checkout. Update them together when changing the root configuration.

Both modules import compiler/toolchain flags from `bzl/valdi/common.bazelrc`, so
those flags have one source. The host's `.bazelrc` selects the checkout's own
registry (`file://%workspace%/../registry`) before BCR, keeping patched build rules
in step with the source tree. It does not enable automatic Cargo repinning.
If a crate dependency/lockfile intentionally changes, repin explicitly using
`--repo_env=CARGO_BAZEL_REPIN=true` for that invocation.

The SDL Linux build uses registry-provided ALSA, X11, Wayland and xkbcommon headers.
SDL loads the corresponding desktop/audio libraries by soname at runtime; they
are not required for the offscreen software-surface test. Interactive X11/Wayland
runs require those backend libraries, and Wayland surface presentation may require
EGL/Mesa. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for SDL's preserved
zlib notice and the backend dependency/license inventory. This does not qualify
a distributable SDK or complete a deployment-specific runtime audit.

## Build and run

Use Bazel 8.2.1 on Linux x86_64. No SDL development package is needed. From the
repository root, run the same headless test as CI:

```sh
./tools/ci/linux_window_host_tests.sh
```

For individual targets, change into the module first:

```sh
cd linux_window_host
bazel test //valdi_linux_window:framebuffer_test //valdi_linux_window:sdl_window_host_test
bazel build //apps/linux_window_demo:linux_window_demo
bazel-bin/apps/linux_window_demo/linux_window_demo /path/to/font.ttf

bazel build //apps/calculator:calculator_linux
mkdir -p /tmp/valdi-calculator-modules
bazel-bin/apps/calculator/calculator_linux /tmp/valdi-calculator-modules /path/to/font.ttf
```

The module selects QuickJS for both runtime and generated module bytecode. Keep
runtime, bytecode and compiler/toolbox profiles matched when changing engines.
The native demo uses a SnapDrawing layer tree; the calculator mounts the shared
Valdi TSX component through `ValdiWindowApplication`. Both need an explicitly
supplied font file; font discovery and fallback are not yet implemented.

## Headless CI and coverage

`tools/ci/linux_window_host_tests.sh` checks configuration consistency, exercises
the drift checker, and runs the framebuffer and SDL host tests from this module.
The `linux-window-host` matrix job in `Valdi CI` is non-blocking on
`ubuntu-24.04`, with a 45-minute test/build step and a 60-second per-test timeout.
The script itself fails on build/test errors. Failed test logs are uploaded.
PR and main-push filters include this module, shared sources and CI/build settings.

The SDL test uses the actual `offscreen` video driver and sets
`SDL_FRAMEBUFFER_ACCELERATION=0` to use software window surfaces without a display
server or graphics driver. It covers exact raster pixels, resize, synthetic text
preedit/commit, clipboard, taps, worker/delayed queue work, bounded queue batches,
idle rendering, theme changes and three open/close cycles. It also checks that an
existing queue listener is forwarded and restored. It does not establish safe
full-runtime teardown, native IME behavior, physical input or desktop integration.

For already-built binaries, `valdi_linux_window/test/run-displays.sh` starts
private Xvfb and headless Weston displays, including Wayland 1×/2×. Those backends
remain manual follow-ups to this first offscreen job. The harness requires Weston
with headless/Pixman/fake-seat support, Xvfb and Mesa; it defaults to `softpipe`
because historical llvmpipe runs exposed LLVM/unwind diagnostics.

The calculator model and upstream JSX tests still run in the root module through
`tools/ci/run_tests.sh` automatic discovery. They do not depend on this SDL module.

## Stage-1 gaps shared with RFC 0002

These items must land with the experimental facade/lifecycle work in
[RFC 0002](../rfcs/0002-linux-headless-sdk.md); they are deliberately not claimed as
implemented by this prototype:

- **One drawing runtime:** `ValdiWindowApplication` currently constructs a second
  `snap::drawing::Runtime`. Reuse the single configured optional drawing provider,
  including its `FontResolverWithRuntimeManager`, scheduler and finite cache limits.
- **No idle polling:** `pump()` caps waits at 16 ms for delayed work. Replace the
  single queue-listener slot with supported subscriptions and a race-safe deadline
  snapshot; wake on next-deadline changes and frame invalidation instead of polling.
- **Teardown order:** the host and SDL video currently go away before root/context
  and runtime teardown finishes. Detach subscriptions first, stop new application
  work, destroy contexts through the completion-aware teardown operation while
  pumping required queues, and retain the window until runtime/drawing cleanup
  completes. Repeated open/close tests do not prove this ordering safe.
- **Scale:** pixel density/display scale are reported but not applied. Implement
  logical layout/input/framebuffer conversions and host-driven Device dimensions
  and effective-scale notifications, including resize/display changes.

Host-owned main-thread/exit policy, disposed-queue handling, lifecycle/ASan tests,
fontconfig/system fallback, licensed deterministic text fixtures, X11/Wayland
acceptance and performance/resource measurements also remain stage-1 gates in
RFC 0001. Real editing/selection/IBus/Fcitx needs a separate editing RFC.
Accessibility, multiple windows, arm64, public keyboard/hover/custom-native-view
APIs and GPU rendering remain outside this prototype's qualification. Acceptance
assigns no maintainer, CI budget or support-tier promotion.

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

## Validation of this module

On Linux x86_64 at Valdi base `4aa98f01` (2026-11-05),
`tools/ci/linux_window_host_tests.sh` built the BCR SDL/SnapDrawing source closure
and passed the framebuffer test and all three offscreen host cycles. The test
links statically, matching `@valdi//snap_drawing:test`; it does not use system SDL.
The root calculator model and JSX regressions also pass (seven cases each).
The relocated native drawing demo builds, and both calculator launcher targets
pass configured-target analysis (`bazel cquery`); the calculator was not rebuilt
or run in this validation.
Root `bazel mod graph` contains no SDL/ALSA/X11/Wayland/xkbcommon modules, and
`bazel query //...` includes the shared/web calculator while excluding this host.

The local Arch machine needs `libxml2.so.2` for the pinned LLVM linker. Validation
supplied an extracted, checksum-verified `libxml2-legacy` library through Bazel's
`--action_env=LD_LIBRARY_PATH` and `--host_action_env=LD_LIBRARY_PATH`; this is a
local toolchain compatibility setting, not an SDL or Valdi source patch.
The new GitHub job is a separate CI run; full calculator/runtime lifecycle,
native X11/Wayland and sanitizer qualification remain the gates listed above.

## Historical native evidence

Clean SDK-consumer validation uses public runtime source
`53b70efda8c520037ecba12a7264b777d9e3f4fd` and SDK export recipe
`b37c808620d64a67a44f43823a824ea02a007f98`. The calculator and host suites
passed three complete lifecycles on each of X11, Wayland 1× and Wayland 2×.
The initial calculator run found that JSX bootstrap rejected Linux platform ID 5.
The upstream fix in [#200](https://github.com/Snapchat/Valdi/pull/200) accepts Linux
and uses the iOS class names already registered by
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

This qualifies an explicit SDK/source cohort. A full Bazel build and execution of
the calculator against the newer upstream engine remains unverified. Copied SDK compiler tools
also remain unqualified for arbitrary standalone TypeScript compilation; use the
matched source build for module generation.
