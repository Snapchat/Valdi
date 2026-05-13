# valdi-mock-to-module

Turn a screenshot or design mock into a running Valdi module. Two-phase pipeline: get something visible fast (Phase 1), then refine with parallel specialist agents and pixel diff (Phase 2).

## When to use

- User provides a screenshot or mock and says "build this" or "turn this into a Valdi component"
- User wants to go from visual design to running code
- User invokes `/valdi-mock-to-module`

## Prerequisites

- A Valdi workspace (Valdi repo, Valdi_Widgets, or any repo with `valdi_modules/`)
- The `valdi` CLI installed and on PATH
- For live preview: macOS (uses `valdi_application` -> `_macos` target)
- Pillow for pixel diff: `pip3 install Pillow`

---

## CRITICAL RULES

These rules were learned the hard way. Violating them wastes time.

### 1. NEVER kill the running app — ever

**Never** kill the macOS preview app or port 13591. The user keeps the app running across sessions. Killing it forces a slow rebuild/relaunch and loses window positioning.

At the start of the pipeline, only kill **hot-reloader processes** (valdi_compiler, valdi_companion, run_hotreloader, "valdi hotreload", port 9010). **Never** kill port 13591 or the app process.

During iteration, don't kill anything — just edit files and let hot-reload push changes.

### 2. Check hot-reload log after EVERY edit

After EVERY file edit, immediately check compilation:

```bash
tail -5 /tmp/hotreload.log
```

- ✅ `Recompilation pass finished` with recent timestamp → success
- ❌ Error in YOUR file → fix before making more edits
- ⚠️ Errors in OTHER modules → ignore (pre-existing)

### 3. Batch edits, then check once

Apply ALL specialist agent fixes in a single batch edit. Check the hot-reload log once after the batch. Then take one snapshot + diff. This is much faster than one-at-a-time edits. Only split edits if a batch fails to compile — then bisect to find the broken change.

### 4. Phone-width frame for accurate proportions

PreviewRoot MUST wrap the component in a fixed 390px-wide container. Without this, the desktop window stretches the layout wider than a phone screen, making pixel diff useless.

```tsx
onRender(): void {
  <view key="PreviewRoot" style={styles.root}>
    <view style={styles.phoneFrame}>
      <YourComponent />
    </view>
  </view>;
}

const styles = {
  root: new Style<View>({
    width: '100%', height: '100%',
    backgroundColor: '#333333', alignItems: 'center',
  }),
  phoneFrame: new Style<View>({
    width: '390', height: '100%', backgroundColor: '#FFFFFF',
  }),
};
```

### 5. BUILD.bazel dep changes require a full recompile

If you change `BUILD.bazel` (e.g. adding a new dependency), you **must kill the app and do a full rebuild**. Hot-reload only recompiles TypeScript within the existing dependency graph — the running binary doesn't have the new dependency's compiled code.

After any BUILD.bazel dep change:
1. Kill hot-reloader processes
2. Kill the app
3. Rebuild and relaunch
4. Restart hot-reload

**Plan your deps up front** to avoid mid-iteration rebuilds.

---

## Pipeline overview

```
Mock image
    |
    v
[Phase 1: Bare-bones]
    |-- 1. Breakdown (analyze layout)
    |-- 2. Generate (write TSX — rough structure, placeholder styles)
    |-- 3. Build + launch + hot-reload
    |-- 4. First snapshot + pixel diff
    |
    v
[Phase 2: Parallel refinement]
    |-- Spin up 3 specialist agents (research only, no edits):
    |   |-- Text Agent      (fonts, sizes, colors, content, positioning)
    |   |-- Icons Agent      (icon selection, stroke/fill, tints)
    |   |-- Widgets Agent    (widget library replacements for raw primitives)
    |
    |-- Diff Coordinator (main agent):
    |   |-- Batch all specialist fixes into a single edit
    |   |-- Check hot-reload once after the batch
    |   |-- Re-diff with --auto-mask after each batch
    |   |-- Repeat until MATCH or 5 iterations
    |
    v
  Done — present final render + diff
```

**Phase 1 goal**: Something visible on screen fast. Don't perfect anything.

**Phase 2 goal**: Converge on the mock. Specialists research in parallel; main agent applies edits.

---

## Step 1: Breakdown

Analyze the mock image. **Follow the `/valdi-mock-breakdown` skill**.

1. **Copy mock to stable path**: `cp /path/to/mock.png /tmp/mock.png` (Desktop screenshots can disappear)
2. **Extract colors programmatically** — NEVER guess. Use the bundled `extract_colors.py`:
   ```bash
   # Auto-detect thumbnail regions (left column) + ALL colored regions (entire image)
   python3 <skill_dir>/scripts/extract_colors.py /tmp/mock.png --auto-thumbnails --auto-colors

   # Sample specific pixel colors
   python3 <skill_dir>/scripts/extract_colors.py /tmp/mock.png \
     --sample "badge:305,60" "green_dot:90,355"
   ```
   **Always use BOTH `--auto-thumbnails` AND `--auto-colors`** — thumbnails only scans the left column; colors scans the entire image for buttons, badges, and icons.
   **Watch for colored pill badges** — mocks often contain small colored pill badges (e.g., blue "New Chats", red "LIVE") that look like plain text at low resolution. The pixel diff dilutes them across segment averages. Do a manual visual pass through each row looking for colored elements beyond icons/dots. Use `--sample` to extract exact badge colors.
3. Describe the visual layout (structure, sections, components)
4. Identify which Valdi primitives and widgets to use
5. Plan the component tree before writing code

Share your analysis with the user before proceeding.

---

## Step 2: Generate the preview harness

The preview harness lives at `apps/preview/` in the workspace root.

### PreviewRoot.tsx

Write a `StatefulComponent` that renders the target UI, wrapped in a 390px phone frame:
- Empty `ViewModel` and `ComponentContext`
- All styles in a `const styles` object at the bottom
- Sample data hardcoded as constants

### BUILD.bazel

```python
load("@valdi//bzl/valdi:valdi_application.bzl", "valdi_application")
load("@valdi//bzl/valdi:valdi_module.bzl", "valdi_module")

valdi_module(
    name = "preview",
    srcs = glob(["**/*.ts", "**/*.tsx"]),
    visibility = ["//visibility:public"],
    deps = [
        "@valdi//src/valdi_modules/src/valdi/valdi_core",
        "@valdi//src/valdi_modules/src/valdi/valdi_tsx",
    ],
)

valdi_application(
    name = "preview_app",
    root_component_path = "PreviewRoot@preview/PreviewRoot",
    title = "Valdi Preview",
    deps = [":preview"],
)
```

**Workspace detection**: If `WORKSPACE` contains `workspace(name = "valdi")`, use `//` prefix. Otherwise `@valdi//`.

### tsconfig.json

For **external repos**:
```json
{
  "extends": "../../valdi_modules/_configs/base.tsconfig.json",
  "compilerOptions": { "paths": { "preview/*": ["./*"] } }
}
```

For the **Valdi repo**:
```json
{
  "extends": "../../src/valdi_modules/_configs/base.tsconfig.json",
  "compilerOptions": {
    "paths": {
      "preview/*": ["./*"],
      "valdi_core/*": ["../../src/valdi_modules/src/valdi/valdi_core/*"],
      "valdi_tsx/*": ["../../src/valdi_modules/src/valdi/valdi_tsx/*"]
    }
  }
}
```

### Style pitfalls (causes silent failures)

```tsx
// opacity must be a number, not a string
opacity: 0.3          // CORRECT
opacity: '0.3'        // WRONG — TypeScript error

// No overflow: 'hidden' on View
overflow: 'visible'   // CORRECT
overflow: 'hidden'    // WRONG — only 'visible' or 'scroll'

// Only system, system-bold, system-italic font weights
font: 'system-bold 16'         // CORRECT
font: 'system-semibold 16'     // WRONG — falls back to regular at runtime

// flexGrow is a number
flexGrow: 1           // CORRECT
flexGrow: '1'         // WRONG

// boxShadow format
boxShadow: '0 2 8 rgba(0,0,0,0.12)'  // xOffset yOffset blur color

// WRONG: <scrollview> doesn't exist
<scrollview>...</scrollview>
// CORRECT: The element is <scroll>
<scroll>...</scroll>

// WRONG: scrollDirection prop doesn't exist
<scroll scrollDirection="horizontal">
// CORRECT: Use horizontal boolean
<scroll horizontal={true}>

// WRONG: padding on Label
new Style<Label>({ padding: '10' })
// CORRECT: Label doesn't support padding — use margin
new Style<Label>({ margin: '10' })

// WRONG: importing Image from NativeTemplateElements
import { Image } from 'valdi_tsx/src/NativeTemplateElements';
// CORRECT: <image> is a JSX intrinsic, not an importable class
// Use Style<View> for image container styles

// WRONG: gap property (doesn't exist on View)
new Style<View>({ flexDirection: 'row', gap: '8' })
// CORRECT: use margin on child elements instead
// In JSX: <view marginRight={8}> or margin="0 8 0 0"

// WRONG: icon typed as string
rightIcon?: string;
<image src={vm.rightIcon} />  // Type error: Asset not assignable to string
// CORRECT: icon values are Asset type
import { Asset } from 'valdi_core/src/Asset';
rightIcon?: Asset;
```

---

## Step 3: Build, launch, and set up hot-reload

### Kill stale processes first (ONLY at start, not during iteration)

```bash
pkill -f valdi_compiler
pkill -f valdi_companion
pkill -f run_hotreloader
pkill -f "valdi hotreload"
lsof -ti:9010 | xargs kill
# NEVER kill port 13591 — that's the running app. Leave it open.
```

### First build

```bash
cd <workspace_root>

# External repos:
bazel build //apps/preview:preview_app_macos
open bazel-bin/apps/preview/preview_app_macos_archive-root/Valdi\ Preview.app

# Start hot-reload
valdi hotreload --target //apps/preview:preview_app_hotreload > /tmp/hotreload.log 2>&1 &
```

**CRITICAL: Never use `bazel run` with hot-reload.** `bazel run` holds the bazel server lock, blocking hot-reload. Use `bazel build` + `open` separately.

Wait for the app to connect:
```bash
valdi inspect status --port 13591
# Should show {"connected":true}
```

---

## Step 4: Capture a screenshot

```bash
valdi inspect snapshot --key PreviewRoot --port 13591
```

The `--key` flag resolves a component tag name or element `key` prop to its element ID automatically. **Stable across hot-reloads** — no need to re-query the tree.

**Port 13591** = standalone app. Port 13592 = mobile/in-app.

Read the PNG file to view the rendered output.

---

## Step 5: Pixel diff

**Generate a real pixel diff** — don't eyeball. Use the bundled scripts:

```bash
# One-shot: capture snapshot + generate diff (ALWAYS use --auto-mask)
bash <skill_dir>/scripts/snapshot-and-diff.sh /tmp/mock.png --key <ComponentKey> --port 13591

# Or just the diff (if you already have a snapshot):
python3 <skill_dir>/scripts/diff.py /tmp/mock.png /path/to/snapshot.png --auto-mask

# For iOS simulator screenshots, also mask the status bar:
python3 <skill_dir>/scripts/diff.py /tmp/mock.png /path/to/snapshot.png --auto-mask --mask "0,0,400,30"

# Auto-watch mode: monitors hot-reload and auto-diffs on each recompile
bash <skill_dir>/scripts/watch-and-diff.sh /tmp/mock.png --key <ComponentKey> --port 13591
```

**ALWAYS use `--auto-mask`** on every diff. This auto-detects avatar/thumbnail/Bitmoji regions and excludes them from the match score. These are image-based content that the component renders as colored placeholder circles — penalizing the score for them is misleading and makes progress harder to track. For iOS simulator screenshots, also mask the status bar (`"0,0,400,30"` in resized coords) since the component can't reproduce the clock/battery/signal UI.

Output:
- `/tmp/mock-vs-render-diff.png` — side-by-side: mock | render | amplified diff | shadow diff
- Match percentage (pixels within threshold)
- `--mask` regions shown as red-tinted overlays on all panels (exclude thumbnails/photos from diff)
- `--segments N` adds per-region breakdown showing which vertical band needs the most work
- Shadow match percentage reported alongside pixel match percentage

**IMPORTANT: Always show the user the diff.** After every pixel diff, use the Read tool to display three images:
1. The mock image (`/tmp/mock.png`)
2. The render snapshot
3. The diff image (`/tmp/mock-vs-render-diff.png`)
4. **Open the diff image** so it's visible on screen: `open /tmp/mock-vs-render-diff.png`

**Bright spots in the third panel = areas that don't match.** This catches background color mismatches, spacing differences, and drop shadow issues that eyeballing misses.

---

## Step 6: Parallel refinement agents

After the first diff, spin up **three specialist agents in parallel** (research only, no edits).

### Specialist memory (cross-iteration persistence)

Specialists lose context between iterations. Each agent reads/appends to `/tmp/mock-specialist-findings.json` to avoid re-researching resolved issues.

Initialize before first run:
```bash
echo '{"text": [], "icons": [], "widgets": [], "iteration": 1}' > /tmp/mock-specialist-findings.json
```

Include in every specialist prompt:
```
MEMORY: Read /tmp/mock-specialist-findings.json if it exists. Skip RESOLVED issues.
After analysis, append new findings under your key. Mark fixed issues RESOLVED.
```

### Text Agent
Audits fonts, sizes, weights, colors, content, positioning. Checks every label against the mock.

### Icons Agent
Audits icon names, stroke vs fill, tints, sizes, icon circle backgrounds. If the project has an icon library, **use `find_icon.py`** for fuzzy matching:
```bash
python3 <skill_dir>/scripts/find_icon.py "chat bubble" --icons-file <path>/Icons.tsx
```

### Widgets Agent
Finds widget library components that could replace raw primitive constructions. Check the workspace for a shared widget/component library and use those instead of hand-building with `<view>` + `<label>`.

Each agent outputs structured fix recommendations with specific code changes.

---

## Step 7: Apply fixes (diff coordinator)

You are the diff coordinator. After the specialist agents report back:

1. **Triage**: Merge all three reports into a prioritized fix list. Group by visual impact.
2. **Batch all fixes**: Apply ALL fixes from the specialist reports in a single edit. Then check hot-reload log once: `tail -5 /tmp/hotreload.log` — confirm `Recompilation pass finished` with a recent timestamp, no errors in your file. Pre-existing errors in other modules can be ignored. If there's an error, bisect to find the broken change.
3. **Show the pixel diff after each batch**: Run the diff script with `--auto-mask --segments 4`. **ALWAYS use `--auto-mask`** to exclude avatar/thumbnail regions:
   ```bash
   bash <skill_dir>/scripts/snapshot-and-diff.sh /tmp/mock.png --key <ComponentKey> --port 13591
   # Or with auto-masking and segments:
   python3 <skill_dir>/scripts/diff.py /tmp/mock.png <snapshot_path> --segments 4 --auto-mask --mask "0,0,400,30"
   ```
   Then **Read all three images** (mock, render, diff) to show the side-by-side comparison, and **open the diff image**: `open /tmp/mock-vs-render-diff.png`. The per-region breakdown tells you exactly which band of the screen needs the most work.
4. **Read the new diff image**: Use the per-region breakdown to identify which vertical band has the lowest match %. Send that info back to the relevant specialist agent for targeted fixes.
5. **Update specialist memory**: Increment iteration counter before re-launching specialists
6. **Repeat** until match > 85% or 5 iterations

### Diminishing returns (~84% ceiling)

After ~84% match on complex layouts, component tweaks yield <0.5% improvement. Remaining differences come from aspect ratio distortion (cumulative vertical drift), shadow differences, and anti-aliasing. Focus on **semantic accuracy** (correct elements, badges, colors) over pixel perfection. Don't try preserving the render's aspect ratio in the diff — forced resize to the mock's aspect ratio is correct (preserving was tested and worse).

### Specialist semantic gap

The pixel diff misses small semantic differences (e.g., gray icon vs blue badge) because segment averages dilute small features. Specialists should **compare element-by-element** against the mock, not just audit broad categories.

### Fix priority order

1. **Structure** — wrong layout, missing sections
2. **Widget replacements** — widget library components fix shadows/spacing/fonts in one shot
3. **Text** — content, font sizes, weights, colors
4. **Icons** — icon names, stroke/fill, tints
5. **Colors/shadows** — backgrounds, box shadows, borders
6. **Fine-tuning** — sub-pixel spacing, opacity

---

## Troubleshooting

**Build fails with "module not found"** -- Missing dep in BUILD.bazel.

**`valdi inspect` connects to wrong device** -- adb may forward port 13591. Run `adb forward --remove-all`.

**Component renders blank** -- Check `onRender()` renders elements (not returns them).

**Hot-reload not compiling** -- Check `tail -20 /tmp/hotreload.log`. Fix TypeScript errors in your file.

**`opacity: '0.3'` error** -- Must be a number: `opacity: 0.3`.

**`overflow: 'hidden'` error** -- Only `'visible'` or `'scroll'` supported.

**`font: 'system-semibold'` renders as regular** -- Only `system`, `system-bold`, `system-italic` work natively.

**Pixel diff shows stretched layout** -- Missing 390px phone frame in PreviewRoot.

**`<scrollview>` build error** — The element is `<scroll>`, not `<scrollview>`. Valdi uses `<scroll>`.

**`scrollDirection` build error** — Use `horizontal={true}` on `<scroll>`, not `scrollDirection="horizontal"`.

**`padding` on Label build error** — `Label` doesn't support `padding`. Use `margin` instead.

**`Image` import error** — `<image>` is a JSX intrinsic, not an importable class. Don't import Image.

**Widget component crashes** — Some widget components only accept a subset of layout props. Check the component's ViewModel interface for available props before passing layout properties.

**ErrorBoundary stuck after hot-reload** — If an error persists after fixing code, kill and relaunch the app. The ErrorBoundary caches error state that hot-reload can't clear.

**snapshot-and-diff.sh hangs on context selector** — When the app has multiple Valdi contexts, pass `--context <device_idx> <context_idx>` to bypass the interactive selector.
