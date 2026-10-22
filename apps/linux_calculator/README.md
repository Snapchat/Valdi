# Linux Valdi calculator

A standalone four-function calculator rendered by a compiled Valdi TSX component
through `ValdiLayer`, SnapDrawing and the optional SDL3 Linux window host.
SnapDrawing rasterizes on the CPU in this baseline; SDL owns window-surface
presentation. This example does not create a SnapDrawing GPU context.
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

From this directory, compile the pure calculation model with TypeScript 5.3.3
and run its Node tests:

```sh
model_output=$(mktemp -d)
npx --yes --package=typescript@5.3.3 tsc src/Calculator.ts \
  --outDir "$model_output" --target ES2020 --module commonjs --strict --skipLibCheck
node test/model.test.cjs "$model_output/Calculator.js"
rm -rf "$model_output"
```

The native executable accepts `--self-test [snapshot.bmp]` after the module/font
arguments. It waits for real component layout, injects SDL mouse events at the
reported key centers, verifies `12 + 3 = 15`, then exercises keyboard input,
clear/backspace, divide-by-zero recovery and resizing while preserving the
result. It creates/destroys the complete application three times and checks the
component's disconnect callback on teardown. The snapshot is the actual SDL
window surface after resizing, not a recreated UI or render mockup.

From the repository root, run the calculator on private Xvfb and headless Weston
displays, including 1× and 2× Wayland pixels:

```sh
bash valdi_linux_window/test/run-displays.sh \
  bazel-bin/apps/linux_calculator/calculator /tmp/calculator-results \
  /tmp/valdi-calculator-modules /path/to/font.ttf --self-test
```

See the host notes for compositor prerequisites. The test harness appends a
snapshot path for each display and cleans up its private display processes. The
tests do not measure performance,
prove physical GPU behavior, audit leaks, validate real IMEs, or establish a
desktop accessibility bridge.

![Calculator on a headless Wayland output at 2×](docs/calculator-wayland-2x.png)

This screenshot comes from the retained historical binary-SDK validation.
It does not establish clean current-upstream build compatibility.
