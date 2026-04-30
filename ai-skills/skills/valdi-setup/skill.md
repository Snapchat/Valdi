# Valdi Module Setup

## BUILD.bazel — valdi_module()

```python
load("//bzl/valdi:valdi_module.bzl", "valdi_module")

valdi_module(
    name = "my_module",              # Must match the directory name exactly
    srcs = glob([
        "src/**/*.ts",
        "src/**/*.tsx",
    ]) + [
        "tsconfig.json",             # tsconfig must be listed in srcs
    ],
    android_output_target = "release",
    ios_module_name = "SCCMyModule", # SCC prefix + PascalCase module name
    ios_output_target = "release",
    visibility = ["//visibility:public"],
    deps = [
        "//src/valdi_modules/src/valdi/valdi_core",
        "//src/valdi_modules/src/valdi/valdi_tsx",
        # Add as needed — see dependency table below
    ],
)
```

**Common mistakes that cause build failures:**

- `name` must equal the Bazel package directory name. The Valdi compiler derives
  module identity from this — mismatches cause a build error.
- `tsconfig.json` must be in `srcs`. It won't be picked up automatically even if
  it's in the directory.
- Missing a dep (e.g. using `HTTPClient` without `valdi_http`) produces a TypeScript
  path resolution error, not a missing import error — can be confusing.

## tsconfig.json

```json
{
    "extends": "../../../../src/valdi_modules/src/valdi/_configs/base.tsconfig.json"
}
```

Adjust the `../../../../` prefix to match how many directories deep your module is
from the repo root. The base config sets up `paths` aliases so `'valdi_core/src/...'`
imports resolve correctly.

## Dependency Table

| You're using | Add this dep |
|---|---|
| `Component`, `StatefulComponent`, `Style`, providers, `CancelablePromise` | `//src/valdi_modules/src/valdi/valdi_core` |
| JSX elements (`<view>`, `<label>`, etc.), `NativeTemplateElements` | `//src/valdi_modules/src/valdi/valdi_tsx` |
| `HTTPClient`, `HTTPResponse` | `//src/valdi_modules/src/valdi/valdi_http` |
| `PersistentStore` | `//src/valdi_modules/src/valdi/persistence` |
| Pre-built UI components (buttons, cards, etc.) | `//src/valdi_modules/src/valdi/valdi_widgets_core` |

## ios_module_name Convention

Must start with `SCC` followed by the module name in PascalCase:

| Directory name | ios_module_name |
|---|---|
| `chat_thread` | `SCCChatThread` |
| `profile_editor` | `SCCProfileEditor` |
| `story_viewer` | `SCCStoryViewer` |

This becomes the Swift module name on iOS. Conflicts with other `SCCXxx` modules in
the same app target will cause a linker error.

## New Component File Template

```typescript
import { Component } from 'valdi_core/src/Component';

interface MyComponentViewModel {
  // viewModel properties
}

export class MyComponent extends Component<MyComponentViewModel> {
  onRender(): void {
    <view>
      <label value={this.viewModel.someText} />;
    </view>;
  }
}
```

For stateful components:

```typescript
import { StatefulComponent } from 'valdi_core/src/Component';

interface MyState {
  // state properties
}

export class MyComponent extends StatefulComponent<MyComponentViewModel, MyState> {
  state: MyState = { /* initial values */ };

  onRender(): void {
    // ...
  }
}
```

## Registering in an App Target

Your module must be added as a dependency of an application target to be compiled
and linked. The exact location depends on your project — look for the list of
Valdi module deps in the app's `BUILD.bazel` (often a `VALDI_MODULES` list or
similar) and add your module:

```python
"//path/to/my_module:my_module",
```

Without this, the module will not be compiled or bundled — the app will fail at
runtime with "No item named '...' in module '...'" even though the build itself
may succeed.

## Building

```bash
bazel build //path/to/my_module:my_module
```

## Hot Reload

```bash
valdi hotreload
```

Run from your module directory. The CLI watches for file changes, recompiles, and
pushes the updated module to a connected simulator or device over USB (or network
with `--network`).

```bash
valdi hotreload --network   # Discover device over Wi-Fi instead of USB
```

If hot reload stops reflecting changes, stop with `Ctrl+C` and restart — the CLI
will clean stale build artifacts automatically.
