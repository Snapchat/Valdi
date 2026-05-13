# Valdi Component Deduplication

Find and extract duplicated UI patterns across Valdi modules into shared components.

## When to use

When you want to identify components that multiple modules have independently implemented, then extract them into a shared library so future modules can reuse them instead of reinventing.

## Running the analysis

```bash
# Full scan of all modules
valdi analyze duplicates

# Scope to one module
valdi analyze duplicates --module my_module

# Machine-readable output
valdi analyze duplicates --json

# Only check for a specific pattern
valdi analyze duplicates --pattern async

# Custom scan roots and shared libraries
valdi analyze duplicates --scan-root src/modules --shared-lib valdi_widgets
valdi analyze duplicates --scan-root src/modules --shared-lib my_shared_lib --shared-lib another_lib
```

### CLI flags

| Flag | Effect |
|---|---|
| `--module <name>` | Scope to one module |
| `--json` | Machine-readable JSON output |
| `--threshold <0-1>` | Similarity threshold (default 0.6) |
| `--pattern <name>` | Only detect: `async`, `button`, `shimmer` |
| `--output <path>` | Write JSON report to file |
| `--verbose` | Show per-file detail |
| `--scan-root <path>` | Root directory to scan (repeatable) |
| `--shared-lib <name>` | Shared component library name (repeatable) |

## Understanding the report

The report has two sections:

**Known Patterns** — pre-built detectors for the most common duplication:
- **Async State Container**: Components that implement loading→error/content state machines
- **Button Wrapper**: Thin wrappers around a shared button with hardcoded sizing/coloring
- **Shimmer / Skeleton Layout**: Custom loading placeholder components

**Similar Clusters** — generic similarity analysis that finds groups of components with overlapping ViewModel props, render trees, and import signatures.

Each cluster shows:
- Number of files and average similarity score
- Shared ViewModel props across the cluster
- A suggested extraction approach

## Extraction workflow

Once you identify a cluster worth extracting:

### 1. Read the candidates

Read each file in the cluster. Identify:
- The shared ViewModel props (these become the new component's ViewModel)
- The shared render structure (this becomes the new component's `onRender`)
- Module-specific variations (these become optional props or render callbacks)

### 2. Check for an existing shared component to extend

Before creating anything new, search the shared library for a component that already solves part of the problem. Often the right fix is adding props, a preset enum, or a render callback to an existing component rather than building from scratch.

```bash
# Search the shared library for related components
grep -rl "Button\|Shimmer\|AsyncContent" <your-shared-lib>/src/components/
```

If an existing component covers 70%+ of the use cases, extend it:
- Add optional props for the missing behavior
- Add a preset or variant enum if consumers just need different default combos
- Add a render callback slot if consumers need to inject custom UI

Only create a new component when nothing in the shared library is close enough to extend.

### 3. Design or extend the shared component

**Extending an existing component** (preferred):

```typescript
// Add a preset to an existing button component
export enum CoreButtonPreset {
  PRIMARY_CTA,
  SECONDARY_ACTION,
  PILL_ACTION,
}

// Add an optional prop to the existing ViewModel
interface CoreButtonViewModel {
  // ... existing props ...
  preset?: CoreButtonPreset;
}
```

**Creating a new component** (when nothing exists to extend):

```typescript
import { Component } from 'valdi_core/src/Component';

interface SharedComponentViewModel {
  commonProp: string;
  optionalVariant?: VariantType;
  renderCustomSection?: () => void;
}

export class SharedComponent extends Component<SharedComponentViewModel> {
  onRender(): void {
    <view>
      <label value={this.viewModel.commonProp} />;
      {this.viewModel.renderCustomSection?.()};
    </view>;
  }
}
```

### 4. Place it in the shared library

If you created a new component, place it in your project's shared component library
(e.g. `<shared-lib>/src/components/<category>/`). Follow your project's conventions
for directory structure and naming.

### 5. Migrate consumers

For each file in the cluster:
1. Replace the local component class with an import of the shared one
2. Map the old ViewModel props to the new shared ViewModel
3. Move module-specific rendering into render callbacks
4. Delete the old local component file
5. Run `valdi agent-check --module <name>` to verify

### 6. Verify

```bash
# Re-run analysis — the cluster should shrink or disappear
valdi analyze duplicates --pattern async

# Build and test affected modules
valdi agent-check --module module_a
valdi agent-check --module module_b
```

## Common extraction patterns

### Async State Container

The most common duplication. Components that manage loading→loaded/error states:

```typescript
import { StatefulComponent } from 'valdi_core/src/Component';
import { promiseToCancelablePromise } from 'valdi_core/src/CancelablePromise';

interface AsyncStateContainerViewModel<T> {
  resultProvider: () => PromiseLike<T>;
  renderLoading: () => void;
  renderResult: (data: T) => void;
  renderError: (error: string, retry: () => void) => void;
  renderEmpty?: () => void;
}

type AsyncState<T> = { phase: 'loading' } | { phase: 'result'; data: T } | { phase: 'error'; message: string };

export class AsyncStateContainer<T> extends StatefulComponent<AsyncStateContainerViewModel<T>, { state: AsyncState<T> }> {
  state = { state: { phase: 'loading' } as AsyncState<T> };

  onCreate(): void {
    this.load();
  }

  onRender(): void {
    const s = this.state.state;
    switch (s.phase) {
      case 'loading': this.viewModel.renderLoading(); break;
      case 'result': this.viewModel.renderResult(s.data); break;
      case 'error': this.viewModel.renderError(s.message, () => this.load()); break;
    }
  }

  private load(): void {
    this.setState({ state: { phase: 'loading' } });
    const cancelable = promiseToCancelablePromise(
      Promise.resolve(this.viewModel.resultProvider()),
      () => {},
    );
    this.registerDisposable(() => cancelable.cancel?.());
    cancelable.then(
      data => { this.setState({ state: { phase: 'result', data } }); },
      error => { this.setState({ state: { phase: 'error', message: (error as Error).message } }); },
    );
  }
}
```

### Button Presets

Instead of per-module button wrappers, use preset enums:

```typescript
// Instead of a new wrapper component, add presets to the existing shared button
enum ButtonPreset {
  PRIMARY_CTA,
  SECONDARY_ACTION,
  PILL_ACTION,
}

function presetProps(preset: ButtonPreset): { sizing: ButtonSizing; coloring: ButtonColoring } {
  switch (preset) {
    case ButtonPreset.PRIMARY_CTA: return { sizing: ButtonSizing.XL, coloring: ButtonColoring.PRIMARY_BRAND };
    case ButtonPreset.SECONDARY_ACTION: return { sizing: ButtonSizing.LARGE, coloring: ButtonColoring.SECONDARY };
    case ButtonPreset.PILL_ACTION: return { sizing: ButtonSizing.SMALL, coloring: ButtonColoring.TERTIARY };
  }
}
```

### Shimmer Layout

Configurable skeleton loading placeholder:

```typescript
import { Component } from 'valdi_core/src/Component';

interface ShimmerRow {
  widthRatio: number;
  height: number;
  borderRadius?: number;
}

interface ShimmerLayoutViewModel {
  rows: ShimmerRow[];
  spacing?: number;
}

export class ShimmerLayout extends Component<ShimmerLayoutViewModel> {
  onRender(): void {
    const spacing = this.viewModel.spacing ?? 8;
    <view>
      {this.viewModel.rows.forEach((row, i) => {
        <AnimationShimmer
          width={`${row.widthRatio * 100}%`}
          height={row.height}
          borderRadius={row.borderRadius ?? 4}
          marginTop={i > 0 ? spacing : 0}
        />;
      })}
    </view>;
  }
}
```

## Tips

- Start with the highest-severity known patterns — they have the most impact
- Extract the simplest shared interface first, then iterate to handle edge cases
- Use render callbacks for module-specific UI instead of trying to generalize everything
- Run `valdi analyze duplicates --json --output report.json` to save a baseline, then re-run after extraction to measure impact
