# valdi-inspect

Use this skill when you need to inspect a **live running Valdi app** — to see what components are on screen, traverse the rendered virtual node tree, capture element screenshots, or check JS heap usage.

## When to use

- User asks "what's on screen?", "what component is rendering here?", "what contexts are active?"
- You need to diagnose a layout or rendering issue in a running app
- You want to verify a component tree change after a hot-reload
- User asks about memory usage or wants a JS heap dump

## Prerequisites

The Valdi app must be **running on a connected device or emulator**. No hot-reloader required — the CLI connects directly to the app's DebuggerService.

**Port reference** — determined by how the app is built, not the OS:

| Target | Port | When |
|--------|------|------|
| iOS in-app (e.g. Snapchat) | **13592** (default) | `isStandalone = false` |
| Android in-app | **13592** (default) | `isStandalone = false` |
| macOS Valdi app | **13591** | `isStandalone = true` |
| Standalone runner / CLI runner | **13591** | `isStandalone = true` |

For **Android**, `adb forward` runs automatically. For **iOS simulator**, connect directly to `localhost`.

All commands output **JSON by default** (for AI parsing). Add `--pretty` for human-readable output.

---

## Commands

### `valdi inspect status`

Check whether the Valdi daemon is reachable and how many devices are connected.

```
valdi inspect status
valdi inspect status --pretty
```

Output: `{ connected: bool, port: number, connectedDevices: number, portName: string }`

---

### `valdi inspect devices`

List all devices connected to the Valdi daemon.

```
valdi inspect devices
valdi inspect devices --pretty
```

Output: `[{ client_id, platform, application_id }, ...]`

Use `client_id` values with `--client` in other commands. If only one device is connected, other commands auto-select it.

---

### `valdi inspect select`

Interactively choose a device and save it to `~/.valdi-inspect.json`. Subsequent commands use this device without prompting.

```
valdi inspect select
```

---

### `valdi inspect contexts`

List all active root components (contexts) on the device. A **context** is one mounted Valdi component tree — typically one per screen or overlay.

```
valdi inspect contexts
valdi inspect contexts --pretty
valdi inspect contexts --client <client_id>
```

Output: `[{ id: string, rootComponentName: string }, ...]`

The `id` is the **contextId** used by `tree` and `snapshot`. If there is only one context, commands that take a contextId will auto-select it.

---

### `valdi inspect tree [contextId]`

Get the full rendered virtual node tree for a context. If `contextId` is omitted, auto-selects the single context or prompts when there are multiple.

```
valdi inspect tree
valdi inspect tree <contextId>
valdi inspect tree <contextId> --pretty
valdi inspect tree <contextId> --max-depth <n>
```

Output: a nested tree where each node has:
- `tag`: element type (e.g. `"view"`, `"label"`, `"image"`, or a component name)
- `key`: the Valdi TSX key assigned in source
- `element.id`: numeric element ID (use this for `snapshot`)
- `element.frame`: `{ x, y, width, height }` in screen coordinates
- `element.attributes`: rendered prop values (style, value, etc.)
- `children`: array of child nodes (same shape)
- `_childrenTrimmed`: count of trimmed children when `--max-depth` is used

**Tip**: Use `--max-depth 3` or `--max-depth 4` first to get a summary, then drill in.

---

### `valdi inspect snapshot <elementId> [contextId]`

Capture a screenshot of a specific element and save it as a PNG. If `contextId` is omitted, auto-selects or prompts.

```
valdi inspect snapshot <elementId>
valdi inspect snapshot <elementId> <contextId>
valdi inspect snapshot <elementId> <contextId> --output /tmp/my-snap.png
```

Output: `{ path: "/var/folders/.../valdi-snapshot-<elementId>.png" }`

The PNG is written to disk. Read it with the `Read` tool to view it visually.

**Getting elementId**: Use `element.id` from the `tree` output (a number, e.g. `1`, `5`, `42`).

#### Using `--key` instead of numeric IDs (recommended)

Instead of looking up numeric element IDs from the tree, use the `--key` flag to resolve by component tag name or element `key` prop:

```
valdi inspect snapshot --key PreviewRoot
valdi inspect snapshot --key weather-card --port 13591
```

The `--key` flag searches the component tree for a matching tag name or `key` prop and resolves it to the element ID automatically.

**This is stable across hot-reloads** — element IDs change on every reload, but component names and `key` props stay the same. Use `--key` instead of numeric IDs to avoid re-querying the tree after each hot-reload cycle.

Custom keys can be set on any element in TSX:
```tsx
<view key="weather-card" style={styles.card}>
```

---

### `valdi inspect heap`

Dump JS heap statistics for the connected device.

```
valdi inspect heap
valdi inspect heap --pretty
valdi inspect heap --gc
```

Options:
- `--gc`: run garbage collection before dumping (more accurate free memory reading)

Output: `{ memoryUsageBytes: number, heapDumpJSON: string }`

`heapDumpJSON` is a serialized heap snapshot in V8 format. Note: heap dumps can crash small/toy apps — this works best on full production apps.

---

## Common workflows

### "What is rendering on screen right now?"

```
valdi inspect tree --pretty
```

(contextId is auto-selected if there's only one)

### "Show me what a specific component looks like"

1. Run `valdi inspect tree --pretty` to find the node — note its `element.id`
2. Run `valdi inspect snapshot <element.id>` to capture it
3. Read the PNG with the `Read` tool

### "Diagnose a layout issue at position X,Y"

Run `valdi inspect tree --pretty` and look for `element.frame` values overlapping with the target position.

### "Check memory usage"

```
valdi inspect heap --pretty
```

### "Verify a hot-reload applied correctly"

Run `valdi inspect tree` before and after the edit, compare the JSON output.

---

## Options reference

| Option | Commands | Description |
|--------|----------|-------------|
| `--port <n>` | all | Daemon TCP port (default: 13592 mobile, 13591 standalone) |
| `--client <id>` | contexts, tree, snapshot, heap | Target a specific connected device |
| `--pretty` | devices, status, contexts, tree, heap | Human-readable output |
| `--max-depth <n>` | tree | Trim tree to N levels deep |
| `--output <path>` | snapshot | Custom PNG output path |
| `--gc` | heap | GC before heap dump |

---

## Troubleshooting

**"Valdi daemon not running on port 13592"** — Make sure the app is running. macOS Valdi apps and standalone runners use port 13591: add `--port 13591`.

**"No devices connected"** — Make sure the Valdi app is running. For Android, ensure a device or emulator is connected via `adb`.

**"Timeout waiting for device response"** — The app may be frozen or the JS runtime may be busy. Try again after a moment.
