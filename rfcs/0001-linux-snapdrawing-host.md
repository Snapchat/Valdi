# RFC 0001: Optional Linux window host for SnapDrawing

- Status: Draft
- Author(s): @sp-ben-dodson
- Discussion: This pull request.
- Implementation: None yet.

## Summary

Add an opt-in Linux application host that renders Valdi components through SnapDrawing in an SDL3 window on Wayland and X11. Start with CPU raster presentation and an early text-input probe; stage full editing and GPU rendering separately. Text editing is an anticipated requirement, so composition, focus and caret integration shape the host from the start. This RFC proposes the design and test strategy for review.

## Motivation

Linux developers need a reusable graphical application target. The existing [Linux bootstrap](https://github.com/Snapchat/Valdi/blob/53b70efda8c520037ecba12a7264b777d9e3f4fd/valdi/src/valdi/linux/Bootstrap/ValdiLinuxMain.cpp) runs scripts and a queue without creating a window. A shared graphical host would let applications reuse Valdi and SnapDrawing while keeping application-specific services separate from window/rendering infrastructure.

## Detailed design

Provide a separate opt-in Bazel target/macro, provisionally `valdi_linux_window_application`, taking a root component and initial logical window extent. Preserve existing CLI behavior. Reuse Valdi runtime/resource loading, a SnapDrawing view-manager context, `ValdiLayer` and `LayerRoot`.

SDL3 owns window creation, platform events and presentation. The adapter drains Valdi's main queue without blocking event processing, schedules invalidated frames and maintains logical size, framebuffer size and scale. Keep window/text-input operations on the main thread; preserve existing SnapDrawing thread contracts when introducing asynchronous drawing. Bound framebuffer allocations, queued work and caches, and cancel callbacks before teardown.

Implement in stages after RFC acceptance:

1. **Raster host:** one resizable window with labels, an image, tap interaction and animation. Render through `BitmapGraphicsContext` into bounded pixel storage and present through an SDL window surface. Include a composition/commit, focus and caret-area probe.
2. **Editing:** connect a Valdi editable-text example with selection, clipboard, preedit/commit and explicit UTF-8/UTF-16 offset conversion. SDL's [editing events](https://wiki.libsdl.org/SDL3/SDL_TextEditingEvent) and [text-input API](https://wiki.libsdl.org/SDL3/SDL_StartTextInput) provide platform inputs; Valdi still owns editing behavior. Accessibility requires a separate semantic bridge and acceptance gate.
3. **GPU presentation/rendering:** evaluate after the raster baseline. The inspected [SnapDrawing build](https://github.com/Snapchat/Valdi/blob/53b70efda8c520037ecba12a7264b777d9e3f4fd/snap_drawing/BUILD.bazel) selects Ganesh GL/EGL for Android, not the default Linux path. Linux GPU compatibility and performance remain unproven. Specify context, surface, synchronization and swap ownership before adding that backend.

Keep SDL3 and its reviewed, pinned dependency closure confined to the optional Linux target. Use upstream Bazel/Bzlmod conventions, preserve third-party notices and test an external consumer. Initially validate Linux x86_64; make further architecture/support commitments explicitly. iOS, Android, macOS, web and headless targets retain their existing implementations.

The host runs on ordinary Linux desktops. Application-specific launching, permissions, storage, packaging and custom compositor protocols remain outside the host.

## Automated test strategy

Proposed jobs below are additions, not existing passing coverage. Each implementation stage lands with its tests.

| Coverage | CI execution and regression caught |
|---|---|
| Event/scheduling unit tests | Linux unit job: deterministic clocks and events catch lost invalidation, busy loops, scale-coordinate errors, stale callbacks and allocation-bound violations. |
| Text adapter tests | Linux unit/component job: synthetic preedit/commit, cancellation, focus changes, Unicode offsets, selection and caret updates catch duplicated commits and incorrect state transitions. These do not establish native IME interoperability. |
| Raster/window integration | New Linux Xvfb and headless-Wayland jobs: deterministic font/pixel comparisons, tap, resize, 1×/1.25×/2× scale, restore and repeated close catch rendering, targeting and resource-lifecycle regressions. Missing required backends fail the job. |
| Build isolation | External Bazel consumer and existing platform CI verify the opt-in target and prevent SDL dependencies from entering CLI/headless or mobile builds. |

Later GPU changes require software-GPU CI where supported, image parity checks and dedicated hardware evidence. Native IBus/Fcitx composition/candidate placement, physical input and screen-reader behavior require recorded manual acceptance because synthetic event injection cannot prove system integration. Stage-one success does not qualify a complete desktop/editor release. Record timings and resource counts; no speed or working-rendering claim is made by this RFC.

## Compatibility

Additive and opt-in; existing applications need no migration or deprecation. Linux remains at its current support tier until maintainers explicitly change it after acceptance evidence. Proposed API names and the SDL dependency/build policy remain subject to review.

## Alternatives considered

- **GLFW:** smaller window/context API and originally suggested for evaluation. Its inspected API leaves general composition/caret integration to additional platform work. Prefer it only with a concrete upstream rationale and a complete text-input plan.
- **GTK4:** useful toolkit input/accessibility infrastructure, with broader toolkit integration and dependency implications. Custom-drawn content still needs semantic adapters.
- **Direct Wayland/EGL:** greater protocol control, with more window/buffer/input machinery and a separate X11 adapter to maintain.

## Drawbacks and risks

Adds a dependency and Linux maintenance/CI work. CPU rasterization is a bounded first milestone, not a performance promise. SDL APIs alone do not establish complete text editing or accessibility. Raster API availability, Linux fonts, backend interoperability and eventual GPU support need verification.

## Unresolved questions

- Who owns ongoing Linux host maintenance and CI, and what milestone warrants a supported application-target tier?
- Which SDL3 pin and Bazel dependency approach should upstream adopt?
- Which existing Valdi editing/accessibility hooks can be reused, and which need narrowly scoped extensions?
- What additional automated/native IME coverage do maintainers require before accepting the editing milestone?
