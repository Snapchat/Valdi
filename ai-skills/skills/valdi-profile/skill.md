# valdi-profile

Use this skill when you need to capture a **CPU profile from a running Valdi app** — to diagnose slow renders, identify hot functions, or understand JS thread activity.

## When to use

- User asks "why is this component slow?", "what's taking so long?", "profile this interaction"
- You want to measure the JS cost of a specific UI action
- You need a flamegraph or call-tree of the Hermes JS runtime

## Prerequisites

### 1. Build the app with Hermes

`valdi profile` requires the app to be built with the **Hermes JS engine**. The default engine is QuickJS — you must opt in:

```
valdi install android --bazel_args="--@valdi//bzl/valdi:js_engine=hermes --@valdi//bzl/valdi:js_bytecode_format=hermes"
valdi install ios     --bazel_args="--@valdi//bzl/valdi:js_engine=hermes --@valdi//bzl/valdi:js_bytecode_format=hermes"
```

If you try to profile a QuickJS build you will get:
```
Hermes debug socket not found on port 13595.
```

### 2. Run the app with the hot-reloader

The app must be **running on a connected device or emulator**, and the **hot-reloader must be running from the project directory**:

```
valdi hotreload android
valdi hotreload ios
```

The hot-reloader creates the `port 13595` tunnel to the Hermes debug server (which starts on a random port inside the app). Without it, port 13595 is not reachable.

> **Note**: The hot-reloader does not need to compile successfully. Even if it prints TypeScript errors, it still sets up the port-13595 tunnel and the profile command will work.

All commands output **JSON by default** (for AI parsing). Human-readable output goes to stderr.

---

## Commands

### `valdi profile capture`

Capture a CPU profile. Stops on Ctrl+C (interactive) or after `--duration` seconds.

```
valdi profile capture
valdi profile capture --duration 10
valdi profile capture --duration 5 --output /tmp/my-profile.cpuprofile
```

Output: `{ "path": "/tmp/valdi-profile-<timestamp>.cpuprofile" }`

The `.cpuprofile` file is standard Chrome DevTools CPU profile format.

**Options:**

| Option | Description |
|--------|-------------|
| `--duration <s>` | Profile for N seconds then stop (omit to stop with Ctrl+C) |
| `--output <path>` | Custom output path (default: `/tmp/valdi-profile-<timestamp>.cpuprofile`) |
| `--port <n>` | Hermes debug port (default: 13595) |
| `--context <id>` | JS context ID (auto-selected if only one; prompts if multiple) |

---

## Common workflows

### "Profile a slow render"

1. Start the app and run `valdi hotreload android` (or `ios`)
2. Navigate to the screen in the app
3. Run: `valdi profile capture --duration 5`
4. Trigger the slow interaction during the 5-second window
5. Open the output file in Chrome DevTools or Speedscope

### "Find what's running on the JS thread"

```
valdi profile capture --duration 3
```

Open the `.cpuprofile` in Chrome DevTools → Performance → Load file.

### "Interactive profiling session"

```
valdi profile capture
# ... trigger interactions ...
# press Ctrl+C to stop
```

---

## Opening the output

The `.cpuprofile` file is compatible with several viewers:

**Chrome DevTools** (best flamegraph):
1. Open Chrome → DevTools → Performance tab
2. Click the upload icon → select the `.cpuprofile` file

**Speedscope** (fast, shareable):
- Open [speedscope.app](https://speedscope.app) → drag and drop the file

**VS Code** (integrated):
- Install "JS Profile Visualizer" extension
- Open the `.cpuprofile` file directly

---

## Troubleshooting

**"Hermes debug socket not found on port 13595"** — Either the app was built with QuickJS (the default), or the hot-reloader is not running. Check both prerequisites above.

**"No debuggable JS contexts found"** — The app is running but Hermes hasn't finished initialising. Wait a moment and retry.

**"Timeout waiting for response to Profiler.stop"** — The profile data is large (long capture on a busy app). The CLI waits up to 60 seconds; if this still times out, use a shorter `--duration`.

**Profile appears empty or shows only `[root]` in Speedscope/Chrome DevTools** — The app was idle during the capture (waiting for input, no JS activity). Trigger the interaction you want to profile *during* the capture window. The snake game, for example, only shows JS activity when the snake is actively moving.

**Physical iOS device** — Not supported in V1. Hermes debugging on a physical iOS device requires idb tunnelling. Use an iOS simulator instead.
