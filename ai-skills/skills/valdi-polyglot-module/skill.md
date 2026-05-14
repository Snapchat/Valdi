# Polyglot Module Rules

**Applies to**: `**/BUILD.bazel` and source files in modules with platform-specific implementations (`android/`, `ios/`, `macos/`, `web/` directories).

## Module Structure

A polyglot module contains platform-native implementations alongside Valdi TSX source:

```
my_module/
  src/           # Valdi TSX components (compiled by Valdi compiler)
  android/       # Kotlin/Java native implementation
  ios/           # Objective-C native implementation
  macos/         # Objective-C native implementation (macOS desktop)
  web/           # TypeScript web implementation (compiled by ts_project, NOT Valdi compiler)
  strings/       # Localization
  module.yaml
  tsconfig.json
  BUILD.bazel
```

## BUILD.bazel Pattern

```python
load("@aspect_rules_ts//ts:defs.bzl", "ts_project")

# web_deps MUST be a ts_project — never use filegroup for web code.
ts_project(
    name = "my_module_web",
    srcs = glob(
        [
            "web/**/*.ts",
            "src/**/*.d.ts",       # include if web code imports module type declarations
        ],
        exclude = ["web/**/*.d.ts"],  # exclude to avoid TS5055 output collision with composite
    ),
    allow_js = True,
    composite = True,
    transpiler = "tsc",            # required — aspect_rules_ts does not default it
    tsconfig = "web/tsconfig.json",
    visibility = ["//visibility:public"],
)

valdi_module(
    name = "my_module",
    srcs = glob(["src/**/*.ts", "src/**/*.tsx"]) + ["tsconfig.json"],
    android_deps = [":android_impl"],
    ios_deps = [":ios_impl"],
    macos_deps = [":macos_impl"],
    web_deps = [":my_module_web"],
    deps = [...],
)
```

- `web_deps`: makes `web/` outputs available to the web bundle. Any file in `web_deps` that exports `webPolyglotViews` is automatically registered with the `WebViewClassRegistry` at bundle time — no extra configuration needed.
- After changing `module.yaml` deps, run: `./scripts/regenerate_valdi_modules_build_bazel_files.sh`

## Web Polyglot Entry Pattern

Web entry files export view factories that are auto-registered with the `WebViewClassRegistry` at bundle time. Use typed TypeScript with an `AttributeHandler` interface:

```typescript
interface AttributeHandler {
  changeAttribute(name: string, value: unknown): void;
}

type ViewFactory = (container: HTMLElement) => AttributeHandler;

function createMyViewFactory(): ViewFactory {
  return (container: HTMLElement): AttributeHandler => {
    const element = document.createElement('div');
    container.appendChild(element);

    return {
      changeAttribute(name: string, value: unknown): void {
        if (name === 'myAttribute' && typeof value === 'number') {
          element.textContent = String(value);
        }
      },
    };
  };
}

// Keys must match the webClass attribute in <custom-view webClass="MyCustomViewClass">
export const webPolyglotViews: Record<string, ViewFactory> = {
  MyCustomViewClass: createMyViewFactory(),
};
```

### Web `tsconfig.json`

The `web/tsconfig.json` must be standalone — do not extend the module-level tsconfig (it has Valdi path mappings that don't apply to web builds):

```json
{
  "compilerOptions": {
    "target": "ES2016",
    "module": "commonjs",
    "strict": true,
    "composite": true,
    "allowJs": true,
    "lib": ["dom", "ES2019"]
  }
}
```

### Key constraints for `web/` files
- Compiled by `tsc` via `ts_project`, **not** the Valdi compiler
- Do **NOT** use Valdi imports (`valdi_core/`, `valdi_tsx/`) — web files run in the browser, not the Valdi runtime
- Do **NOT** manually call `globalThis.moduleLoader.resolveRequire()` — build system handles registration
- The `web/tsconfig.json` must be standalone with `lib: ["dom", "ES2019"]` and `module: "commonjs"`
- Always use `transpiler = "tsc"` in the `ts_project` rule — aspect_rules_ts requires explicit transpiler selection

## Canonical Example

See `valdi_polyglot` module for a complete working example with all four platforms:
- `src/composer_modules/src/composer/valdi_polyglot/` (in the Snap monorepo)
- Or the equivalent path in your project

## macOS Native View with Attribute Binding

macOS `<custom-view>` implementations can receive callback attributes from TSX via `SCValdiMacOSFunction`. This is how you bridge events (like keyboard input) from native AppKit views back to Valdi components.

### Native Side (Objective-C)

```objc
// macos/SCMyView.h
#import <AppKit/AppKit.h>
@class SCValdiMacOSAttributesBinder;

@interface SCMyView : NSView
+ (void)bindAttributes:(SCValdiMacOSAttributesBinder *)attributesBinder;
@end

// macos/SCMyView.m
#import "SCMyView.h"
#import "valdi/macos/SCValdiMacOSAttributesBinder.h"
#import "valdi/macos/SCValdiMacOSFunction.h"

@interface SCMyView () {
    SCValdiMacOSFunction *_onEvent;
}
@end

@implementation SCMyView

- (void)valdi_setOnEvent:(id)value {
    _onEvent = value;
}

// Call the Valdi callback with a dictionary parameter:
// [_onEvent performWithParameters:@[@{@"key": @"ArrowUp"}]];

+ (void)bindAttributes:(SCValdiMacOSAttributesBinder *)attributesBinder {
    [attributesBinder bindUntypedAttribute:@"onEvent"
                invalidateLayoutOnChange:NO
                                selector:@selector(valdi_setOnEvent:)];
}
@end
```

### TSX Side

```typescript
<custom-view
  macosClass='SCMyView'
  onEvent={this.handleEvent}
  width={200}
  height={200}
>
  {/* child elements render inside the custom-view */}
</custom-view>
```

### BUILD.bazel

```python
objc_library(
    name = "macos_impl",
    srcs = glob(["macos/**/*.m"]),
    hdrs = glob(["macos/**/*.h"]),
    copts = ["-I."],
    sdk_frameworks = ["AppKit"],
    visibility = ["//visibility:public"],
    deps = ["@valdi//valdi:valdi_macos_desktop_lib"],
)
```

### Key Points

- The attribute name in `bindUntypedAttribute:` must match the TSX attribute name exactly (e.g. `"onEvent"` ↔ `onEvent={...}`)
- Callback values arrive as `SCValdiMacOSFunction` objects — call `[callback performWithParameters:@[...]]` to invoke the Valdi/JS function
- Parameters are passed as an `NSArray` of `NSDictionary`/`NSString`/`NSNumber` — they're auto-converted to JS objects
- **The custom-view must have non-zero dimensions** to participate in the responder chain (for keyboard focus, etc.)
- For keyboard input: override `acceptsFirstResponder` → `YES`, `keyDown:`, and call `[self.window makeFirstResponder:self]` in `viewDidMoveToWindow`
- Add `bazel_build_file_generation_disabled: true` to `module.yaml` when hand-maintaining the BUILD file (prevents the regenerate script from overwriting it)

## Common Mistakes

- Putting `web/` files in `srcs` — they must go through `ts_project` + `web_deps`, not the Valdi compiler
- Missing platform `_deps` — each platform impl must be wired via `android_deps`, `ios_deps`, `macos_deps`
- Using Valdi imports in `web/` — they don't exist in the browser bundle
- Creating a 0×0 custom-view for keyboard input — macOS won't give it first responder status; wrap visible content inside it instead
- Forgetting `bazel_build_file_generation_disabled: true` in module.yaml — the regenerate script will overwrite your hand-maintained BUILD and drop the `macos_deps`/`ios_deps`
