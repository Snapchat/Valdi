---
name: valdi-tsx
description: "Valdi TypeScript/TSX component authoring patterns including StatefulComponent lifecycle (onCreate, onViewModelUpdate, onDestroy), state management with setState, viewModel props, type-safe Style styling, provider dependency injection, event handling (onTap, onPress), and common anti-patterns versus React. Use when writing or reviewing .tsx component files in /src/valdi_modules/, /apps/, /modules/, or /npm_modules/ directories."
---

# Valdi TypeScript/TSX Component Rules

**Applies to**: TypeScript and TSX files in `/src/valdi_modules/`, `/apps/`, `/modules/`, `/npm_modules/`

## 🚨 CRITICAL: Valdi is NOT React!

**AI assistants frequently suggest React patterns that DON'T EXIST in Valdi.** Despite using TSX/JSX syntax, Valdi compiles to native code.

### Most Common Mistakes

```typescript
// ❌ NEVER use React hooks (don't exist!)
const [count, setCount] = useState(0);  // ❌
useEffect(() => { ... }, []);           // ❌

// ❌ NEVER use functional components (don't exist!)
const MyComponent = () => <view />;     // ❌

// ❌ Common hallucinations
this.props.title;           // Should be: this.viewModel.title
this.markNeedsRender();     // Doesn't exist! Use setState()
onMount() { }               // Should be: onCreate()
return <view />;            // onRender() returns void!
```

> **📖 Full list**: See `/AGENTS.md` → "AI Anti-Hallucination" section for comprehensive examples

### ✅ Correct Valdi Patterns

```typescript
import { StatefulComponent } from 'valdi_core/src/Component';

class MyComponent extends StatefulComponent<ViewModel, State> {
  state = { count: 0 };
  
  onCreate() { }                           // Component created
  onViewModelUpdate(prev: ViewModel) { }   // Props changed
  onDestroy() { }                          // Before removal
  
  handleClick = () => {
    this.setState({ count: this.state.count + 1 });  // Auto re-renders
  };
  
  onRender() {  // Returns void, not JSX!
    <button title={`Count: ${this.state.count}`} onPress={this.handleClick} />;
  }
}
```

## Quick Reference

| What | React | Valdi |
|------|-------|-------|
| **Component** | Function or class | Class only (Component or StatefulComponent) |
| **State** | `useState(0)` | `state = { count: 0 }` + `setState()` |
| **Props** | `this.props.title` | `this.viewModel.title` |
| **Mount** | `useEffect(() => {}, [])` | `onCreate()` |
| **Update** | `useEffect(() => {}, [dep])` | `onViewModelUpdate(prev)` |
| **Unmount** | `useEffect(() => () => {}, [])` | `onDestroy()` |
| **Re-render** | `setCount(...)` | `this.setState(...)` |
| **Return** | `return <view />` | `<view />;` (statement) |

## New Component Checklist

1. Create `.tsx` file in the appropriate module directory
2. Define `ViewModel` interface (add `@ViewModel @ExportModel` if native code needs it)
3. Define `State` interface if the component manages local state
4. Extend `StatefulComponent<ViewModel, State>` (or `Component<ViewModel>` for stateless)
5. Initialize `state = { ... }` with defaults
6. Implement `onRender()` — remember it returns `void`, not JSX
7. Add lifecycle methods as needed: `onCreate()`, `onViewModelUpdate()`, `onDestroy()`
8. Build and verify: `bazel build //path/to:target`
9. If build fails: check for React-isms (`useState`, `this.props`, `return <jsx>`), unsupported style properties (`gap`, `flex`, `fontSize`), or `@ExportModel` type violations (see sections below)

## Provider Pattern (Dependency Injection)

```typescript
// ✅ CORRECT - Create provider
import { createProviderComponentWithKeyName } from 'valdi_core/src/provider/createProvider';
const MyServiceProvider = createProviderComponentWithKeyName<MyService>('MyServiceProvider');

// ✅ CORRECT - Provide value
<MyServiceProvider value={myService}>
  <App />
</MyServiceProvider>

// ✅ CORRECT - Consume with HOC
import { withProviders, ProvidersValuesViewModel } from 'valdi_core/src/provider/withProviders';

interface MyViewModel extends ProvidersValuesViewModel<[MyService]> {}

class MyComponent extends Component<MyViewModel> {
  onRender() {
    const [service] = this.viewModel.providersValues;
  }
}

const MyComponentWithProvider = withProviders(MyServiceProvider)(MyComponent);
```

## Event Handling

```typescript
// ✅ CORRECT - Use onTap for interactive elements
<view onTap={this.handleClick}>
  <label value="Click me" />
</view>

<button title="Press me" onPress={this.handleAction} />

// ❌ WRONG - No global keyboard events
window.addEventListener('keydown', ...);  // Doesn't work!
document.addEventListener('click', ...);  // Doesn't work!

// ✅ CORRECT - For text input, use TextField callbacks
<textfield 
  value={this.state.text}
  onChange={this.handleTextChange}
  onEditEnd={this.handleSubmit}
/>

// ✅ CORRECT - For keyboard input on macOS desktop, use a polyglot <custom-view>
// (see valdi-polyglot-module and valdi-custom-view skills for full pattern)
{Device.isDesktop() && (
  <custom-view macosClass='SCKeyboardView' onKeyDown={this.handleKeyDown} width={200} height={200}>
    {/* wrap visible content so the view has non-zero size for first responder */}
  </custom-view>
)}
```

## Timers and Scheduling

```typescript
// ✅ CORRECT - Use component's setTimeoutDisposable
class MyComponent extends StatefulComponent<ViewModel, State> {
  onCreate() {
    // Timer auto-cancels when component destroys
    this.setTimeoutDisposable(() => {
      console.log('Delayed action');
    }, 1000);
  }
  
  // ✅ CORRECT - Recurring task pattern (use recursive setTimeout)
  private scheduleLoop() {
    this.setTimeoutDisposable(() => {
      this.doSomething();
      this.scheduleLoop();  // Schedule next iteration
    }, 100);
  }
}

// ❌ WRONG - Don't use setInterval directly
setInterval(() => { ... }, 100);  // Won't auto-cleanup!

// ❌ WRONG - Don't use setTimeout directly
setTimeout(() => { ... }, 100);  // Won't auto-cleanup!
```

## Styling

### Basic Style Usage

```typescript
import { Style } from 'valdi_core/src/Style';
import { View, Label } from 'valdi_tsx/src/NativeTemplateElements';
import { systemBoldFont } from 'valdi_core/src/SystemFont';

// ✅ CORRECT - Type-safe styles
const styles = {
  // Style<View> can only be used on <view> elements
  container: new Style<View>({
    backgroundColor: '#fff',
    padding: 10,
    borderRadius: 8,
    flexDirection: 'row',
    justifyContent: 'center',
    alignItems: 'center',
  }),
  
  // Style<Label> can only be used on <label> elements
  // Label uses font (string) NOT fontSize. Format: 'FontName Size [scaling] [maxSize]'
  title: new Style<Label>({
    color: '#000',
    font: 'system 20',         // size via font string, NOT fontSize!
    // font: systemBoldFont(20),  // or use SystemFont helper
  }),
};

// Use in render
onRender() {
  <view style={styles.container}>
    <label style={styles.title} value="Hello" />
  </view>;
}
```

### Font weights

Only two system font weights are available:
- `'system 16'` — regular weight
- `'system-bold 16'` — bold weight

**No other weights exist.** `system-semibold`, `system-light`, `system-medium` etc. will cause build errors. If you need semibold, use `system-bold` instead.

### ScrollView restrictions

`<scroll>` (ScrollView) does **not** support `flexDirection`. It always scrolls vertically. Do not set `flexDirection: 'column'` on a ScrollView style — it will cause a build error.

```typescript
// ❌ WRONG - flexDirection not valid on ScrollView
new Style<ScrollView>({ flexDirection: 'column' })

// ✅ CORRECT - ScrollView scrolls vertically by default, no flexDirection needed
new Style<ScrollView>({ width: '100%', height: '100%' })
```

### Style Composition

```typescript
// ✅ CORRECT - Merge multiple styles
const combined = Style.merge(styles.base, styles.primary);

// ✅ CORRECT - Extend a style with overrides
const largeButton = styles.button.extend({
  width: 200,
  height: 60,
});

// ✅ CORRECT - Dynamic styling with extend
<view style={styles.container.extend({
  backgroundColor: isActive ? 'blue' : 'gray',
})} />

// ❌ WRONG - Can't merge incompatible types
Style.merge(styles.viewStyle, styles.labelStyle);  // Type error!
```

### Spacing, Layout, and Positioning

Key rules for spacing and layout (Valdi uses Yoga flexbox):

- **Padding/margin**: `padding: 10`, `padding: '10 20'` (vertical horizontal), or individual sides (`paddingTop`, etc.)
- **No `gap`**: Use margin on children instead. No `paddingHorizontal`/`paddingVertical` — use string shorthand
- **Flexbox**: Use `flexGrow: 1` (not `flex: 1`), standard `flexDirection`, `justifyContent`, `alignItems`
- **No CSS Grid**: Only flexbox layout is supported
- **Position**: `position: 'relative' | 'absolute'` with `top`/`right`/`bottom`/`left`
- **Size**: Points (`width: 200`), percentage (`width: '50%'`), or `'auto'`
- **Borders**: `borderRadius`, `borderWidth`, `borderColor` only — no per-side border properties (`borderRight` etc.)
- **Overflow**: Only `'visible' | 'scroll'` — `'hidden'` does not exist
- **Shadow**: `boxShadow: '0 2 4 rgba(0, 0, 0, 0.1)'`

> See `/docs/api/api-style-attributes.md` for the complete 1290+ property reference.

### Type Safety

```typescript
// ✅ CORRECT - Style types match element types
const viewStyle = new Style<View>({ backgroundColor: 'red' });
const labelStyle = new Style<Label>({ color: 'blue' });

<view style={viewStyle} />      // ✅ Works
<label style={labelStyle} />    // ✅ Works

// ❌ WRONG - Type mismatch
<label style={viewStyle} />     // ❌ Type error!
<view style={labelStyle} />     // ❌ Type error!

// ✅ CORRECT - Layout styles work on any layout element
const layoutStyle = new Style<Layout>({ padding: 10 });
<view style={layoutStyle} />    // ✅ view extends Layout
<label style={layoutStyle} />   // ✅ label extends Layout
```

> **📖 Complete reference**: See `/docs/api/api-style-attributes.md` for all 1290+ style properties
> 
> **📖 Best practices**: See `/docs/docs/core-styling.md` for styling patterns and examples

## @ExportModel ViewModel Restrictions

Interfaces annotated with `@ViewModel @ExportModel` are exported to native code. The Valdi compiler can only export **primitive types** (`string`, `number`, `boolean`) and other `@ExportModel`-annotated interfaces. Custom type aliases (e.g. `type Direction = 'UP' | 'DOWN'`) are **not supported** in exported ViewModels.

```typescript
// ❌ WRONG — type alias in @ExportModel ViewModel
type Direction = 'UP' | 'DOWN' | 'LEFT' | 'RIGHT';

/** @ViewModel @ExportModel */
interface GameViewModel {
  initialDirection?: Direction;  // ❌ Compiler error: "Unrecognized type"
}

// ✅ CORRECT — keep custom types in State (internal), not ViewModel (exported)
/** @ViewModel @ExportModel */
interface GameViewModel {}  // Only export what native code needs

interface GameState {
  direction: Direction;  // ✅ Fine — State is not exported
}
```

## Additional Pitfalls

Beyond the anti-patterns shown inline above, watch for these less obvious mistakes:

- **SIGIcon is Asset, not string** — `SIGIcon.cameraStroke` returns `Asset`; use `import { Asset } from 'valdi_core/src/Asset'` for ViewModel fields storing icon references
- **Import ShapeView, not Shape** — `Shape` is not exported; use `import { ShapeView } from 'valdi_tsx/src/NativeTemplateElements'`
- **No per-side border properties** — `borderRight`, `borderRightWidth` etc. do not exist; use a thin `<view>` divider instead
- **ViewModel/Context name collisions** — each exported `ViewModel` and `ComponentContext` must have a unique name across the module (e.g. `WeatherCardViewModel`, not just `ViewModel`)

## Platform Detection

Use `Device` for platform-conditional rendering:

```typescript
import { Device } from 'valdi_core/src/Device';

class MyComponent extends Component<MyViewModel> {
  onRender(): void {
    <view>
      {Device.isIOS() && <IOSOnlyView />}
      {Device.isAndroid() && <AndroidOnlyView />}
      {Device.isDesktop() && <DesktopOnlyView />}
      {Device.isWeb() && <WebOnlyView />}
    </view>;
  }
}
```

**Available guards:** `Device.isIOS()`, `Device.isAndroid()`, `Device.isDesktop()`, `Device.isWeb()`

Use platform guards before using `<custom-view>` elements that don't have implementations on all platforms. `Device.isDesktop()` is true for macOS desktop apps (the preview/standalone app). There is no `Device.isMacOS()` — use `Device.isDesktop()` instead.

## Imports

```typescript
// ✅ CORRECT imports
import { Component, StatefulComponent } from 'valdi_core/src/Component';
import { Device } from 'valdi_core/src/Device';
import { systemFont } from 'valdi_core/src/SystemFont';
import { Style } from 'valdi_core/src/Style';

// ❌ WRONG - React imports don't exist
import React from 'react';  // Error!
import { useState } from 'react';  // Error!
```

## More Information

- **Full anti-hallucination guide**: `/AGENTS.md` (comprehensive React vs Valdi comparison)
- **AI tooling**: `/docs/docs/ai-tooling.md`
- **Provider pattern**: `/docs/docs/advanced-provider.md`
- **Valdi GitHub**: https://github.com/Snapchat/Valdi
