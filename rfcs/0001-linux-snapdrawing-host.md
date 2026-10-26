# RFC 0001: Optional Linux window host for SnapDrawing

- Status: Draft; final number assigned at acceptance.
- Author(s): @sp-ben-dodson
- Discussion: [This pull request](https://github.com/Snapchat/Valdi/pull/196), following the Linux support discussion in [#6](https://github.com/Snapchat/Valdi/issues/6).
- Implementation: [#198](https://github.com/Snapchat/Valdi/pull/198) (window prototype) and [#199](https://github.com/Snapchat/Valdi/pull/199) (Device/theme prototype); neither establishes the acceptance gates below.

## Summary

Add an opt-in Linux application host that renders Valdi components through SnapDrawing in one SDL3 window on Wayland and X11. Stage 1 provides CPU raster presentation and a text-input probe. Full editing requires a separate RFC for shared SnapDrawing work; GPU rendering follows later. The host consumes the drawing-free embedding core proposed in [#197](https://github.com/Snapchat/Valdi/pull/197) and adds an optional drawing provider. This RFC proposes contracts and tests; it does not claim they already exist in the prototypes.

## Motivation

Linux developers need a reusable graphical application target. The existing [Linux bootstrap](https://github.com/Snapchat/Valdi/blob/ede1462368bbc233c507dd7136a3602d457a56b8/valdi/src/valdi/linux/Bootstrap/ValdiLinuxMain.cpp) runs scripts and a queue without creating a window. A shared host would let applications reuse Valdi and SnapDrawing while keeping application services separate from window/rendering infrastructure.

## Detailed design

**Shared core and drawing provider.** RFC #197 owns drawing-free runtime/resource loading, host-controlled lifetime and exit, supported queue subscriptions, and completion-aware teardown. This host uses those seams rather than maintaining a second bootstrap. Graphical work can progress once these shared-core seams are available, independently of the SDK’s separate export/packaging milestones. Required shared changes, with regressions for existing consumers, are:

- Accept JSX platform 5 and match native class precedence: `linuxClass → macosClass → iosClass`, including explicit overrides and each fallback. Current `JSXBootstrap.ts` rejects Linux, while native `ViewNode` and `DefaultAttributes` already recognize it.
- Add supported main-queue subscriptions without replacing `StandaloneExitCoordinator`'s listener. Expose the next task deadline and wake the SDL loop when work/deadlines change; post an SDL event from `LayerRootListener::onNeedsProcessFrame`. Wait for events or the next deadline, without periodic idle polling or lost wakeups.
- Make Device system identity (`linux`), window dimensions, effective scale and change notifications host-driven. Layout, Device dimensions and input use application logical units; framebuffer size uses pixels. Reuse the host-driven Device/theme path explored in #199.
- Reuse the optional drawing runtime/provider, including `FontResolverWithRuntimeManager`, which `ValdiStandaloneRuntime` already creates lazily. Do not construct a second drawing runtime without its font resolver.

**Threading and lifetime.** The SDL thread is Valdi's main thread. It drains `StandaloneMainQueue` and owns `LayerRoot`, `processFrame`, input and all SDL video calls. JS runs on its dispatch queue. Stage 1 draws synchronously on the main thread. Future asynchronous drawing requires an SDL-backed `IFrameScheduler`/`DrawLooper` design and tests. Bound framebuffer allocations, queued work and caches.

Windowed mode explicitly keeps the host alive while idle, without changing CLI idle-exit defaults. JS `exit(code)` requests an orderly host close and preserves the exit code; window close uses the same teardown path. A disposed main queue ends the normal event pump. Cleanup queues must remain usable until teardown completion; unexpected disposal that prevents completion is a reported failure. Minimize pauses rendering and forwards `applicationWillPause`/`applicationDidResume`; future asynchronous drawing also uses DrawLooper background hooks. Occlusion does so only when the backend reports it reliably. Focus loss alone does not suspend JS.

Teardown is ordered and idempotent:

1. Stop input/frame production and reject new application work.
2. Detach root/queue subscriptions and cancel host callbacks, keeping the SDL window/resources alive. Listener/pump detachment must be separate from window destruction.
3. Call `root->destroy()`.
4. Release all host-owned `ValdiLayer` references and initiate component/context destruction.
5. Pump admitted teardown work until explicit context-destruction completion, with a bounded timeout and reported failure path. An empty-queue snapshot is not completion; a timeout must not continue normal destruction with live references.
6. Release retained JS values/bridge callbacks, then tear down the Valdi runtime.
7. Release the drawing runtime.
8. Destroy the SDL window, then quit SDL.

No externally retained layer or JS value may outlive the runtime. `ValdiLayer` stores a raw `Runtime*` and retains `ViewManagerContext` through a `Ref`; both still require this ordering. Tests must exercise delayed JS/main-queue destruction and callbacks racing close.

**Scale and fonts.** With SDL pixel density `p` and [window display scale](https://wiki.libsdl.org/SDL3/SDL_GetWindowDisplayScale) `s` (which already incorporates density), logical extent is `pixelExtent / s`, input coordinates are `SDLWindowCoordinates * p / s`, and logical content renders at scale `s`. Recompute and notify Device on resize/pixel-size/display-scale changes. This handles Wayland density and X11 content scaling without multiplying density twice. Reject invalid scales and dimensions before allocating. Initial native-display acceptance covers 1×/2×; fractional-coordinate unit tests do not establish fractional-scale desktop interoperability.

Select `@skia//:fontmgr_fontconfig` only in the optional graphical closure for system discovery, retaining the runtime font resolver for application fonts. Vendor pinned Noto Sans fixtures with hashes and their license for deterministic snapshots. Pin CI's fallback font set, test CJK/emoji fallback and missing-glyph behavior explicitly, and report unavailable required fonts as failures. A single supplied font/weight is prototype coverage; color emoji remains unqualified until separately tested.

**Build and application selection.** Put the host in a separately rooted Bazel module, provisionally `linux_window_host/`, depending on `@valdi` and the pinned BCR [`sdl3` 3.4.18 module](https://github.com/bazelbuild/bazel-central-registry/blob/main/modules/sdl3/3.4.18/MODULE.bazel). Its SDL/X11/Wayland/xkbcommon/audio and fontconfig dependencies must not be registered or resolved by ordinary `@valdi` consumers. Replace prototype root-module `pkg-config` discovery with the pinned dependency graph. Preserve [SDL's zlib notice](https://github.com/libsdl-org/SDL/blob/release-3.4.18/LICENSE.txt); the registry declares ALSA headers while SDL loads the runtime library by soname. Document the actual configured build/runtime closure, notices and licenses, including dynamically loaded libraries.

Provide an experimental companion macro, provisionally `valdi_linux_window_application`, in that separate module, taking a root component and initial logical extent. This keeps host loading explicit: existing `valdi_application` already generates a Linux target and retains its current default. A distinct window-host tag and explicit experimental CLI window mode select the companion target. Mark the macro, mode and C++ facade (`ValdiWindowApplication`/`SDLWindowHost`) experimental under [VERSIONING.md](../VERSIONING.md); implementation must update `docs/docs/choosing-valdi.md` to distinguish an experimental window host from supported Linux app availability. Debugger and hot reload are off by default; any enabled debugger binds only to loopback.

Implement after acceptance:

1. **Raster host:** one resizable window with labels, an image, single-pointer tap, wheel scrolling and animation. Render through `BitmapGraphicsContext` into bounded pixels and present through an SDL window surface. Include a native composition/commit, focus and caret-area probe; this probe is not an editor.
2. **Editing, separately specified:** current `EditableTextLayer` displays text but does not implement caret, selection, focus and IME editing. A separate RFC must define shared SnapDrawing editing, clipboard/preedit/commit and UTF-8/UTF-16 offsets, with Android/iOS regressions and real IBus/Fcitx acceptance. Accessibility needs its own semantic bridge/gate.
3. **GPU presentation/rendering:** evaluate after the raster baseline with explicit context, surface, synchronization and swap ownership, software-GPU CI and dedicated hardware evidence. GPU rendering is outside stages 1–2.

Initial qualification is Linux x86_64; this is a Valdi host/toolchain qualification boundary, not an SDL arm64 limitation. Multiple windows, arm64 qualification, public keyboard/hover APIs, cursor/multi-pointer support and supported native-custom-view registration are outside this RFC. Internal C++ layer factories are not a supported custom-view contract. Application launching, permissions, storage, packaging, portals and custom compositor protocols remain application concerns.

## Automated test strategy

These are proposed additions, not existing passing coverage. Each implementation stage lands with its tests; acceptance requires maintainer agreement on the strategy.

| Coverage | CI execution and regression caught |
|---|---|
| Shared-core regressions | Existing `linux-ci` `test-modules`/`test-cpp` gates: JSX platform-5 overrides/fallbacks, Device Linux identity/resize/scale/theme notifications, queue subscription/deadline changes and host exit policy. Preserve current platform cases. New coverage must not depend on Linux `//valdi:test_runtime`, whose teardown failure is tracked separately. |
| Host scheduling/lifecycle units | Deterministic clocks/events catch lost invalidation, idle busy loops, scale/input errors, stale callbacks, allocation-bound violations and delayed destruction. Assert idle keepalive, JS exit code/window close, disposed-queue termination, repeated/idempotent close, teardown completion and timeout behavior. |
| Raster/window integration | Proposed `tools/ci/linux_window_host_tests.sh`, run as a new `linux-window-host` task in the existing `linux-ci` matrix on `ubuntu-24.04`. The script sets up Xvfb and headless Weston/pixman with Mesa softpipe, then tests drawing, tap/wheel, resize, minimize/restore, repeated close and scale/input parity at 1×/2× on each backend. Missing required backends fail the job. QuickJS is required initially; Hermes needs a separately named qualification. |
| Fonts/pixels | Linux-specific baselines with vendored, pinned fonts; reuse `apps/snapshot_tests` tooling where practical. Exact geometry/solid-color assertions catch raster/scale defects. Initial proposed text tolerance: at most 2/255 per channel on at most 0.1% of image pixels, subject to review/calibration on the pinned runner. Record expected/actual/diff images, font/scale/backend metadata and logs; upload them on failure. Separate fallback fixtures catch CJK/emoji and missing-glyph regressions. |
| Build isolation | Extend `tools/ci/test_external_build_graph.sh` with an external source consumer and fail-closed dependency/`aquery` assertions: no SDL or graphical host dependency/action enters `//valdi:valdi_linux` or mobile targets. Queries failing analysis must fail these new assertions. Build the opt-in host from its own module and retain existing platform CI. |
| Text probe and future editing | Stage 1 synthetic preedit/commit, cancellation, focus/caret-area and offset tests catch duplicate commits and adapter-state errors. The separate editing RFC must add shared-core Android/iOS tests and native IBus/Fcitx acceptance before claiming editor support. Synthetic events do not qualify system IME or accessibility interoperability. |

Wire behavior through `tools/ci` scripts so both CI systems execute it. Extend both pull-request/push filters in `.github/workflows/bzl-changes.yml` for the nested host module/lock/build files, host/shared source, test fonts/baselines and CI scripts. Start the graphical task non-blocking while existing core regression gates remain blocking. Proposed limits are 45 minutes for the build/job and five minutes for the display harness; these are unmeasured caps. Record cold/warm duration, idle wakeups and resource counts on four-core runners before agreeing a blocking gate or ongoing CI budget.

Native IBus/Fcitx composition/candidate placement, physical input and screen-reader behavior require recorded manual acceptance because injection cannot prove system integration. Stage-one success qualifies only its stated raster/input scope.

## Compatibility

Additive and opt-in; existing applications need no migration or deprecation. iOS, Android, macOS, web and headless targets retain their existing implementations and dependency closures, with regression coverage for shared changes. Linux remains at its current development-only tier until maintainers explicitly change it after evidence. Acceptance assigns no maintainer, CI budget, implementation commitment or support-tier promotion. Final numbering follows the RFC acceptance process.

## Alternatives considered

- **GLFW:** Simon suggested investigating it during a discussion about hosting SnapDrawing in a window. It was a candidate to research; SDL3 is this RFC's proposal given anticipated composition/caret integration via its text-input APIs. Choosing GLFW would need an equivalent concrete text-input plan.
- **GTK4:** useful toolkit input/accessibility infrastructure, with broader integration and dependencies. Custom-drawn content still needs semantic adapters.
- **Direct Wayland/EGL:** greater protocol control, with more window/buffer/input machinery and a separate X11 adapter to maintain.

## Drawbacks and risks

Adds Linux maintenance, CI cost and an optional dependency closure. CPU SnapDrawing rasterization is not a performance promise or a dependency-free presentation path: SDL's Wayland surface may still require EGL/Mesa. The prototype uses Mesa softpipe because of an llvmpipe/unwind conflict, which remains a qualification risk. Fontconfig/system fallback, Linux pixel stability, backend/scale interoperability, resource bounds and eventual GPU support need evidence. SDL APIs alone do not provide shared editing or accessibility. Experimental APIs and proposed CI tolerances/budgets may change during implementation review.

## Unresolved questions

- Who owns ongoing Linux host maintenance/CI, and what evidence warrants a supported application-target tier?
- Do maintainers accept the proposed module boundary, initial non-blocking task, measured-budget process and pixel tolerance after baseline review?
- Which shared editing/accessibility extensions and additional native IME gates should the separate RFC require?
