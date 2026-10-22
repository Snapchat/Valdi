# Linux Valdi calculator

A standalone four-function calculator rendered by a compiled Valdi TSX component
through `ValdiLayer`, SnapDrawing and the optional SDL3 Linux window host.
The component uses ordinary Valdi state/layout/tap handlers. Its native context
only supplies desktop keyboard input and state/layout observations for tests.
It needs no application-specific shell, service framework or native view toolkit.

The calculation model supports digits, decimals, sign, percent, clear, backspace,
operator replacement, immediate-execution chaining and non-finite-result recovery.
This is a small integration example, not a scientific calculator. It never
evaluates an expression as JavaScript source.

## Build and run

Install SDL3 >= 3.2 development headers/libraries and pkg-config. See the
[host build notes](../../valdi_linux_window/README.md) for the experimental system
dependency binding and current source-build qualification limits.

```sh
bazel build //apps/linux_calculator:calculator
mkdir -p /tmp/valdi-calculator-modules
bazel-bin/apps/linux_calculator/calculator /tmp/valdi-calculator-modules /path/to/font.ttf
```

The Bazel target statically registers its compiled module resources, so its
extra module-search directory may be empty. Pass a real font file; it is
registered as the `Window` family rather than relying on platform font discovery.
Keyboard digits/operators, Enter, Escape and Backspace supplement button taps.
Keyboard commands use SDL key events; no text-input/IME or editing claim is made.

## Verification

The pure calculation tests compile `src/Calculator.ts` to CommonJS and run:

```sh
node test/model.test.cjs /absolute/path/to/compiled/Calculator.js
```

The native executable accepts `--self-test [snapshot.bmp]` after the module/font
arguments. It waits for real component layout, injects SDL mouse events at the
reported key centers, verifies `12 + 3 = 15`, then exercises keyboard input,
clear/backspace, divide-by-zero recovery and resizing while preserving the
result. It creates/destroys the complete application three times and checks the
component's disconnect callback on teardown. The snapshot is the actual SDL
window surface after resizing, not a recreated UI or render mockup.

Use private Xvfb/headless Weston displays as described in the host tests. The
tests cover both 1× and 2× Wayland pixels. They do not measure performance,
prove physical GPU behavior, audit leaks, validate real IMEs, or establish a
desktop accessibility bridge.
