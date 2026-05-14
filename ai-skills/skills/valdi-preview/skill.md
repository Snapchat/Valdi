# valdi-preview

Use this skill to **understand and visually preview a Valdi module or component** — read its API, render it in the preview harness app, inspect the view hierarchy, and capture a screenshot.

## When to use

- You need to understand what a Valdi component looks like before using it
- You want to see a component's rendered output to verify your code
- User asks "what does X component look like?" or "show me how X renders"
- You're building a UI and need to evaluate which widget to use
- You want to explore a component's API (viewModel, slots, styling)

## Overview

The workflow has two phases:

1. **Static analysis** — read source files to understand the component's API
2. **Live preview** (optional) — render it in the preview harness, inspect the tree, screenshot it

You can stop after phase 1 if you only need API understanding. Phase 2 requires a running app.

---

## Phase 1: Understand the component

### Step 1: Find the module source

Given a component name or module path, locate its source files:

```bash
# Find by component class name
grep -r "export class CoreButton" --include="*.tsx" --include="*.ts"

# Find by module name in BUILD.bazel
grep -r "name = \"valdi_widgets_core\"" --include="BUILD.bazel"
```

### Step 2: Read the API surface

Read the component's main `.tsx` file and look for:

1. **ViewModel interface** — the props the component accepts. Look for `@ViewModel` JSDoc tag:
   ```typescript
   /**
    * @ViewModel
    */
   export interface CoreButtonViewModel {
     title: string;
     onPress: () => void;
     disabled?: boolean;
   }
   ```

2. **Component class** — the rendering logic. Look for `@Component` JSDoc tag and the `onRender()` method.

3. **Slots** — child content areas. Look for `AnyRenderFunction` or `$slot` usage in the viewModel or render.

4. **JSDoc** — descriptions, `@example` tags, parameter docs.

5. **State interface** — if it's a `StatefulComponent`, read the state shape.

### Step 3: Find usage examples

Search the codebase for real usage of the component:

```bash
# Find files that import this component
grep -r "import.*CoreButton" --include="*.tsx" --include="*.ts" -l

# Find JSX usage
grep -r "<CoreButton" --include="*.tsx"
```

Pick 2-3 representative usages to understand common prop combinations.

### Step 4: Summarize

Report to the user:
- Component name and location
- ViewModel props (required and optional)
- What it renders (layout structure from `onRender()`)
- How it's typically used (from real examples)

---

## Phase 2: Live preview

### Option A: `valdi preview` CLI (recommended)

The fastest path — automatically generates the preview harness and builds:

```bash
# By import path
valdi preview widgets/src/components/button/Checkbox

# By file path
valdi preview ./valdi_modules/widgets/src/components/button/Checkbox.tsx
```

This command:
1. Resolves the component file
2. Parses the ViewModel to extract props
3. Generates `apps/preview/PreviewRoot.tsx` with sample props and interactive state
4. Generates `apps/preview/BUILD.bazel` with correct deps
5. Generates `apps/preview/tsconfig.json` with correct path mappings
6. Builds and launches `//apps/preview:preview_app_macos`

First build takes 2-5 minutes; subsequent builds ~5-15 seconds.

### Option B: Manual harness setup

For full control or when `valdi preview` doesn't support your use case.

Requires a running Valdi app. If no app is running, tell the user to start one first.

### Step 1: Edit the preview harness

The preview harness app lives at `apps/preview/` in the Valdi repo. It has three files:

| File | Purpose |
|------|---------|
| `PreviewRoot.tsx` | Root component — **edit this** to render your target |
| `BUILD.bazel` | Module deps — **add deps here** if the target is in another module |
| `tsconfig.json` | Path mappings — **add path entries** for new module deps |

**Edit `PreviewRoot.tsx`** to import and render the target component:

```typescript
import { Component } from 'valdi_core/src/Component';
import { CoreButton } from 'valdi_widgets_core/src/components/button/CoreButton';

/**
 * @ViewModel
 * @ExportModel
 */
export interface ViewModel {}

/**
 * @Context
 * @ExportModel
 */
export interface ComponentContext {}

/**
 * @Component
 * @ExportModel
 */
export class PreviewRoot extends Component<ViewModel, ComponentContext> {
  onRender(): void {
    <view backgroundColor="#ffffff" width="100%" height="100%" padding={24}
          alignItems="center" justifyContent="center">
      <CoreButton title="Example Button" onPress={() => {}} />
    </view>;
  }
}
```

**If the component is in another module**, add it to `BUILD.bazel` deps:

```python
deps = [
    "//src/valdi_modules/src/valdi/valdi_core",
    "//src/valdi_modules/src/valdi/valdi_tsx",
    "//path/to/target_module",  # Add this
],
```

And add the path mapping to `tsconfig.json`:

```json
{
  "compilerOptions": {
    "paths": {
      "target_module/*": ["../../path/to/target_module/*"]
    }
  }
}
```

### Step 2: Build and hot-reload

**Recommended workflow** — build once, then use hot-reload for instant iteration:

```bash
# 1. Build the macOS app once
bazel build //apps/preview:preview_app_macos

# 2. Launch it directly (stays alive as background process)
open bazel-bin/apps/preview/preview_app_macos_archive-root/Valdi\ Preview.app

# 3. Start the hot-reloader
valdi hotreload --target //apps/preview:preview_app_hotreload
```

Now any edit to `PreviewRoot.tsx` is automatically recompiled and pushed to the running app in ~1s.

**Alternative** — full rebuild (slower but simpler):

```bash
# macOS (fastest for preview)
bazel run //apps/preview:preview_app_macos

# iOS simulator
bazel run //apps/preview:preview_app_ios_sim
```

### Step 3: Inspect the rendered output

Once the component is on screen, use **port 13591** for standalone macOS apps:

```bash
# Capture a screenshot by component name (stable across hot-reloads)
valdi inspect snapshot --key PreviewRoot --port 13591

# Or by a custom key prop you set on an element
valdi inspect snapshot --key my-card --port 13591

# Or by numeric element ID (from tree output — changes on each hot-reload)
valdi inspect snapshot <elementId> 2 --port 13591
```

**Prefer `--key`** — it resolves the component tag or element key to its numeric ID automatically, so you don't need to re-query the tree after every hot-reload.

If you're previewing inside a mobile app (Snapchat, etc.), use the default port 13592 instead.

Then read the PNG file to see what it looks like.

### Step 4: Iterate

If the rendering doesn't look right:

1. Adjust props, styles, or container layout in `PreviewRoot.tsx`
2. Hot-reload picks up the change automatically (~1s)
3. Re-query tree for new element IDs, then `valdi inspect snapshot`
4. Repeat until the component renders as expected

**Iteration priority** — for fastest convergence, fix in this order:
1. Font sizes (almost always too small on first pass)
2. Vertical spacing between sections
3. Text colors
4. Element sizing (padding, border radius)
5. Decorative elements (fills, shadows)

---

## Previewing multiple components

To compare several components side by side, render them all in `PreviewRoot.tsx`:

```typescript
onRender(): void {
  <view backgroundColor="#ffffff" width="100%" height="100%" padding={24}>
    <scroll height="100%" width="100%">
      <label value="CoreButton" font="system-bold 14" color="#666" marginBottom={8} />
      <CoreButton title="Primary" onPress={() => {}} />

      <view height={24} />

      <label value="PillButton" font="system-bold 14" color="#666" marginBottom={8} />
      <PillButton title="Secondary" onPress={() => {}} />
    </scroll>
  </view>;
}
```

---

## Previewing with sample data

When a component needs complex viewModel data, create realistic sample data:

```typescript
// For a component that needs a list of items
const sampleItems = [
  { id: '1', title: 'First item', subtitle: 'Description' },
  { id: '2', title: 'Second item', subtitle: 'Another one' },
  { id: '3', title: 'Third item', subtitle: 'And another' },
];

onRender(): void {
  <MyListComponent items={sampleItems} onItemTap={() => {}} />;
}
```

---

## Cleanup

After previewing, restore `PreviewRoot.tsx` to its placeholder state so the harness is ready for the next preview. Revert any deps added to `BUILD.bazel` and `tsconfig.json` unless the user wants to keep them.

---

## Troubleshooting

**"Module not found"** — You forgot to add the dep to `BUILD.bazel` or the path mapping to `tsconfig.json`.

**"Cannot resolve import"** — The tsconfig path is wrong. Check the relative path from `apps/preview/` to the target module.

**Hot-reload not picking up changes** — Stop and restart `valdi hotreload`. If that doesn't work, do a full `bazel run`.

**"Valdi daemon not running"** — The app isn't running. Build and launch it first with `bazel run`.

**Component renders blank** — Check that you're passing all required viewModel props. Read the ViewModel interface again.
