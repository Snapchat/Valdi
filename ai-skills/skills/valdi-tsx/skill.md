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

**Important**: Valdi doesn't support `addEventListener`, `keydown`, or other global DOM events. Use element-specific callbacks like `onTap`, `onPress`, `onChange`, etc. For keyboard input on macOS desktop, use a polyglot `<custom-view>` with a native NSView that captures `keyDown:` events and forwards them via a bound callback attribute.

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

**Important**: Always use `this.setTimeoutDisposable()` in components. It automatically cleans up when the component is destroyed, preventing memory leaks.

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

### Spacing: Padding & Margin

```typescript
// ✅ CORRECT - Valdi spacing syntax
new Style<View>({
  // Single value - all sides
  padding: 10,
  margin: 5,
  
  // String shorthand - vertical horizontal
  padding: '10 20',    // 10pt top/bottom, 20pt left/right
  margin: '5 10',
  
  // Individual sides
  paddingTop: 5,
  paddingRight: 10,
  paddingBottom: 5,
  paddingLeft: 10,
  
  // Percentages (relative to parent)
  padding: '5%',       // 5% of parent width/height
  marginLeft: '10%',   // 10% of parent width
})

// ❌ WRONG - These don't exist in Valdi
new Style<View>({
  gap: 10,                  // ❌ Use margin on children
  paddingHorizontal: 20,    // ❌ Use padding: '0 20'
  paddingVertical: 10,      // ❌ Use padding: '10 0'
  paddingInline: 15,        // ❌ Doesn't exist
})

// ❌ WRONG - gap property doesn't exist on View
new Style<View>({ flexDirection: 'row', gap: '8' })
// ✅ CORRECT - use margin on child elements instead
// In JSX: <view marginRight={8}> or margin="0 8 0 0"
```

### Layout: Flexbox (Yoga)

```typescript
// ✅ CORRECT - Valdi uses Yoga flexbox
new Style<View>({
  // Container properties
  flexDirection: 'row',          // 'row' | 'column' | 'row-reverse' | 'column-reverse'
  justifyContent: 'center',      // 'flex-start' | 'center' | 'flex-end' | 'space-between' | 'space-around' | 'space-evenly'
  alignItems: 'center',          // 'flex-start' | 'center' | 'flex-end' | 'stretch' | 'baseline'
  alignContent: 'flex-start',    // For multi-line flex containers
  flexWrap: 'wrap',              // 'wrap' | 'nowrap' | 'wrap-reverse'
  
  // Child properties
  flexGrow: 1,                   // Grow to fill space (NOTE: use flexGrow, not flex)
  flexShrink: 1,                 // How much to shrink
  flexBasis: 100,                // Base size before flex
  alignSelf: 'center',           // Override parent's alignItems
})

// ❌ WRONG - These don't exist
new Style<View>({
  display: 'grid',               // ❌ Only 'flex' supported
  gridTemplateColumns: '1fr 1fr', // ❌ No CSS Grid
  flex: 1,                       // ❌ Use flexGrow: 1 instead!
})
```

### Position & Size

```typescript
// ✅ CORRECT - Positioning
new Style<View>({
  // Size
  width: 200,           // Points
  width: '50%',         // Percentage of parent
  width: 'auto',        // Auto-size
  height: 100,
  minWidth: 50,
  maxWidth: 500,
  aspectRatio: 16/9,    // Width:height ratio
  
  // Position
  position: 'relative', // 'relative' | 'absolute'
  top: 10,
  right: 10,
  bottom: 10,
  left: 10,
})
```

### Common Properties

```typescript
// ✅ CORRECT - Frequently used properties
new Style<View>({
  backgroundColor: '#fff',
  opacity: 0.8,
  
  // Borders
  borderRadius: 8,
  borderWidth: 1,
  borderColor: '#ccc',
  borderTopWidth: 2,
  
  // Shadow
  boxShadow: '0 2 4 rgba(0, 0, 0, 0.1)',
  
  // Overflow — only 'visible' | 'scroll' (NOT 'hidden'!)
  overflow: 'scroll',   // 'visible' | 'scroll'
})
```

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

## Common Mistakes to Avoid

1. **Returning JSX from onRender()** - It returns void, JSX is a statement
2. **Forgetting setState()** - Direct mutation won't trigger re-render
3. **Using this.props** - Should be this.viewModel
4. **Wrong lifecycle names** - onCreate/onViewModelUpdate/onDestroy (not mount/update/unmount)
5. **Suggesting scheduleRender()** - Deprecated, use StatefulComponent + setState()
6. **Using addEventListener** - Use element callbacks like onTap, onPress, onChange
7. **Using setInterval/setTimeout directly** - Use this.setTimeoutDisposable()
8. **Using CSS properties that don't exist** - No gap, paddingHorizontal, paddingVertical; use margin on children for gap
9. **Using `flex: 1`** - `flex` doesn't exist on `View`; use `flexGrow: 1` instead
10. **Using `fontSize` on Label** - Labels use `font: 'system 20'` (string), not `fontSize`
11. **Typing SIGIcon values as `string`** - `SIGIcon.cameraStroke` etc. return `Asset`, not `string`; use `import { Asset } from 'valdi_core/src/Asset'` for ViewModel fields that store icon references
12. **Using `overflow: 'hidden'`** - `View` only accepts `'visible' | 'scroll'`; remove overflow or use 'scroll'
13. **Using type aliases in `@ExportModel` ViewModels** - Only primitives and other `@ExportModel` types are allowed
14. **Importing `Shape` instead of `ShapeView`** - `Shape` is not exported; use `import { ShapeView } from 'valdi_tsx/src/NativeTemplateElements'` and `new Style<ShapeView>({...})`
15. **Using per-side border properties** - No `borderRight`, `borderRightWidth`, etc. Only `borderWidth`, `borderColor`, `borderRadius` exist. Use a thin `<view>` as a divider instead.
16. **Using `font: 'system-semibold 16'`** - Only `system` (regular) and `system-bold` are reliably available. Use `system-bold` for semibold.
17. **ViewModel/Context name collisions** - When a module has multiple components, each exported `ViewModel` and `ComponentContext` must have a unique name (e.g. `WeatherCardViewModel` not just `ViewModel`), or the compiler will emit conflicting platform types.
18. **Using `flexDirection` on ScrollView** - ScrollView doesn't support flexDirection; it scrolls vertically by default

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
