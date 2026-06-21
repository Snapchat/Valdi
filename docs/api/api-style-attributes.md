# Style Attributes Reference

This document provides a comprehensive reference for all style attributes that can be used with the `Style<T>` object in Valdi. For usage patterns and best practices, see [The Style<> Guide](../docs/core-styling.md).

## Yoga Layout Engine

Valdi's layout system is powered by [Yoga](https://github.com/facebook/yoga), Facebook's cross-platform flexbox layout engine. The available flexbox attributes and their behavior are determined by the Yoga version used.

**Current Yoga Version:**
- **Source Commit:** [`0be0e9fc0148aa609afdc31cb9670524f07050cb`](https://github.com/facebook/yoga/commit/0be0e9fc0148aa609afdc31cb9670524f07050cb)
- **Version Details:** See [`third-party/yoga/README.snap`](../../third-party/yoga/README.snap)
- **Local Source:** [`third-party/yoga/`](../../third-party/yoga/)
- **Modifications:** Custom modifications for Valdi integration (see README.snap for details)

**Key Yoga Files:**
- Enums/Types: [`third-party/yoga/src/yoga/YGEnums.h`](../../third-party/yoga/src/yoga/YGEnums.h)
- Style Properties: [`third-party/yoga/src/yoga/YGStyle.h`](../../third-party/yoga/src/yoga/YGStyle.h)
- Core API: [`third-party/yoga/src/yoga/Yoga.h`](../../third-party/yoga/src/yoga/Yoga.h)

> **Important:** When debugging layout issues or understanding flexbox behavior, refer to this specific Yoga version as behavior may differ from other Yoga versions or standard CSS flexbox implementations.

## Table of Contents

- [Yoga Layout Engine](#yoga-layout-engine)
- [Understanding Styles](#understanding-styles)
- [Common Layout Attributes](#common-layout-attributes)
  - [Using Percentages](#practical-examples-using-percentages)
- [Common View Attributes](#common-view-attributes)
- [Element-Specific Styles](#element-specific-styles)
  - [Layout Styles](#layout-styles)
  - [View Styles](#view-styles)
  - [ScrollView Styles](#scrollview-styles)
  - [ImageView Styles](#imageview-styles)
  - [VideoView Styles](#videoview-styles)
  - [Label Styles](#label-styles)
  - [TextField Styles](#textfield-styles)
  - [TextView Styles](#textview-styles)
  - [BlurView Styles](#blurview-styles)
  - [SpinnerView Styles](#spinnerview-styles)
  - [ShapeView Styles](#shapeview-styles)
  - [AnimatedImage Styles](#animatedimage-styles)
- [Style Composition](#style-composition)
- [Performance Notes](#performance-notes)
- [Practical Examples: Using Percentages](#practical-examples-using-percentages)

---

## Understanding Styles

The `Style<T>` object allows you to group multiple attributes together for reuse and cleaner code. A style can contain any attribute that the element supports, except for the `style` property itself.

```typescript
import { Style } from 'valdi_core/src/Style';
import { View } from 'valdi_tsx/src/NativeTemplateElements';

const myStyle = new Style<View>({
  backgroundColor: '#ffffff',
  padding: 16,
  borderRadius: 8,
});

// Use in JSX:
<view style={myStyle} />
```

**Key Points:**
- Styles are **typed** - `Style<View>` can only be used on `<view>` elements
- Styles can be **merged** using `Style.merge()`
- Styles can be **extended** using `.extend()`
- Styles are **overridden** by inline attributes
- Styles should be created **at initialization**, not during render

---

## Common Layout Attributes

These attributes are available on **all elements** (Layout, View, ScrollView, etc.) as they control layout and positioning.

### Size Attributes

```typescript
new Style<Layout>({
  // Dimensions - support points, percentages, or 'auto'
  width: 200,              // number (points)
  width: '50%',            // string (percentage of parent width)
  width: 'auto',           // string (automatic sizing)
  height: 100,             // number (points)
  height: '75%',           // string (percentage of parent height)
  height: 'auto',          // string (automatic sizing)
  
  // Size constraints - support points or percentages
  minWidth: 50,            // number (points)
  minWidth: '20%',         // string (percentage of parent width)
  maxWidth: 500,           // number (points)
  maxWidth: '90%',         // string (percentage of parent width)
  minHeight: 50,           // number (points)
  minHeight: '10%',        // string (percentage of parent height)
  maxHeight: 500,          // number (points)
  maxHeight: '80%',        // string (percentage of parent height)
  
  // Aspect ratio - numeric ratio only (NOT a percentage)
  aspectRatio: 1.5,        // width/height ratio (1.5 means width is 1.5x height)
  aspectRatio: 16/9,       // common video aspect ratio
})
```

**Percentage Calculation Examples:**
```typescript
// Parent container is 400pt wide x 600pt tall

// Child with width: '50%' will be 200pt wide (50% of 400pt)
// Child with height: '25%' will be 150pt tall (25% of 600pt)
// Child with margin: '10%' gets 10% of parent's width for horizontal margins (40pt)
//                    and 10% of parent's height for vertical margins (60pt)
```

### Position Attributes

```typescript
new Style<Layout>({
  // Position type
  position: 'relative',    // 'relative' | 'absolute'
  
  // Position offsets - support points or percentages
  top: 10,                 // number (points)
  top: '5%',               // string (percentage of parent height)
  right: 10,               // number (points)
  right: '5%',             // string (percentage of parent width)
  bottom: 10,              // number (points)
  bottom: '5%',            // string (percentage of parent height)
  left: 10,                // number (points)
  left: '5%',              // string (percentage of parent width)
})
```

**Position Percentage Behavior:**
- **Horizontal offsets** (`left`, `right`): Percentage is relative to parent's **width**
- **Vertical offsets** (`top`, `bottom`): Percentage is relative to parent's **height**

### Spacing Attributes

```typescript
new Style<Layout>({
  // Margin (outside spacing) - supports points or percentages
  margin: 10,              // number (points, all sides)
  margin: '5%',            // string (percentage, all sides)
  margin: '10 20',         // string (top/bottom 10pt, left/right 20pt)
  marginTop: 5,            // number (points)
  marginTop: '2%',         // string (percentage of parent height)
  marginRight: 5,          // number (points)
  marginRight: '3%',       // string (percentage of parent width)
  marginBottom: 5,         // number (points)
  marginBottom: '2%',      // string (percentage of parent height)
  marginLeft: 5,           // number (points)
  marginLeft: '3%',        // string (percentage of parent width)
  
  // Padding (inside spacing) - supports points or percentages
  padding: 10,             // number (points, all sides)
  padding: '5%',           // string (percentage, all sides)
  padding: '10 20',        // string (top/bottom 10pt, left/right 20pt)
  paddingTop: 5,           // number (points)
  paddingTop: '2%',        // string (percentage of parent height)
  paddingRight: 5,         // number (points)
  paddingRight: '3%',      // string (percentage of parent width)
  paddingBottom: 5,        // number (points)
  paddingBottom: '2%',     // string (percentage of parent height)
  paddingLeft: 5,          // number (points)
  paddingLeft: '3%',       // string (percentage of parent width)
})
```

**Spacing Percentage Behavior:**
- **Horizontal spacing** (`marginLeft`, `marginRight`, `paddingLeft`, `paddingRight`): Percentage is relative to parent's **width**
- **Vertical spacing** (`marginTop`, `marginBottom`, `paddingTop`, `paddingBottom`): Percentage is relative to parent's **height**
- **Shorthand with percentage**: `margin: '5%'` applies 5% to all sides (horizontal uses parent width, vertical uses parent height)

### Flexbox Layout Attributes

These attributes are implemented using [Yoga](https://github.com/facebook/yoga), which provides a flexbox layout engine that closely follows the [CSS Flexible Box Layout](https://www.w3.org/TR/css-flexbox-1/) specification with some additions.

```typescript
new Style<Layout>({
  // Display mode
  display: 'flex',         // 'flex' | 'none'
  overflow: 'visible',     // 'visible' | 'scroll' (Yoga also supports 'hidden')
  
  // Direction and orientation
  direction: 'inherit',    // 'inherit' | 'ltr' | 'rtl'
  flexDirection: 'column', // 'column' | 'column-reverse' | 'row' | 'row-reverse'
  
  // Flex properties for this element
  flexGrow: 1,             // number >= 0 (dimensionless, NOT a percentage)
  flexShrink: 0,           // number >= 0 (dimensionless, NOT a percentage)
  flexBasis: 'auto',       // string (automatic)
  flexBasis: 100,          // number (points)
  flexBasis: '50%',        // string (percentage of parent's main axis dimension)
  flexWrap: 'no-wrap',     // 'no-wrap' | 'wrap' | 'wrap-reverse'
  
  // Alignment (for children)
  justifyContent: 'flex-start', // 'flex-start' | 'center' | 'flex-end' | 
                                 // 'space-between' | 'space-around' | 'space-evenly'
  alignItems: 'stretch',        // 'stretch' | 'flex-start' | 'flex-end' | 
                                 // 'center' | 'baseline'
  alignContent: 'flex-start',   // Same options as justifyContent plus 'stretch'
  
  // Self alignment (override parent's alignItems)
  alignSelf: 'auto',       // 'auto' | 'flex-start' | 'flex-end' | 'center' | 
                           // 'stretch' | 'baseline' | 'space-between' | 'space-around'
})
```

#### Yoga-Specific Flexbox Behaviors

**Aspect Ratio (Yoga Extension):**
```typescript
new Style<Layout>({
  aspectRatio: 1.5,        // number (width/height ratio)
                          // This is a Yoga-specific property, not in standard flexbox
})
```

**Value Units:**
Yoga supports three types of values for dimensions and edges:
- **Points**: Absolute values (e.g., `100`)
- **Percent**: Relative to parent (e.g., `'50%'`)
- **Auto**: Automatic sizing (e.g., `'auto'`)

**Where Percentages Are Supported:**
Percentage values are calculated relative to the parent element's corresponding dimension and are supported for:
- ✅ **Size dimensions**: `width`, `height`, `minWidth`, `maxWidth`, `minHeight`, `maxHeight`
- ✅ **Spacing**: `margin*`, `padding*` (all directional variants)
- ✅ **Positioning**: `top`, `right`, `bottom`, `left`
- ✅ **Flex basis**: `flexBasis`

**Where Percentages Are NOT Supported:**
- ❌ **Border properties**: `borderWidth`, `borderRadius` (points only)
- ❌ **Flex factors**: `flexGrow`, `flexShrink` (dimensionless numbers)
- ❌ **Aspect ratio**: `aspectRatio` (numeric ratio, not percentage)
- ❌ **Transform properties**: `scaleX`, `scaleY`, `rotation`, `translationX`, `translationY` (specific units)
- ❌ **Touch area**: `touchAreaExtension*` (points only)
- ❌ **Opacity and colors**: Always specific formats
- ❌ **Font sizes**: Use point values or scaling options in font string

**Alignment Extensions:**
Yoga extends the standard flexbox `alignContent` and `alignSelf` to support `space-between` and `space-around`, which are not in the CSS flexbox spec for these properties.

**Default Values:**
Some Yoga defaults differ from CSS flexbox:
- `alignItems`: `stretch` (same as CSS)
- `justifyContent`: `flex-start` (same as CSS)
- `flexDirection`: `column` (CSS default is `row`)
- `position`: `relative` (same as CSS)

### Visibility & Performance Attributes

```typescript
new Style<Layout>({
  // Rendering control
  zIndex: 1,               // number (higher = on top)
  animationsEnabled: true, // boolean
  
  // Performance optimization
  lazy: false,             // boolean (shorthand for below)
  lazyLayout: false,       // boolean
  limitToViewport: true,   // boolean
  
  // Viewport behavior
  ignoreParentViewport: false,        // boolean
  extendViewportWithChildren: false,  // boolean
  
  // Estimated dimensions (for lazy layout)
  estimatedWidth: 100,     // number
  estimatedHeight: 100,    // number
})
```

### Accessibility Attributes

```typescript
new Style<Layout>({
  // Accessibility category
  accessibilityCategory: 'auto', // 'auto' | 'view' | 'text' | 'button' | 
                                  // 'image' | 'image-button' | 'input' | 
                                  // 'header' | 'link' | 'checkbox' | 'radio' | 
                                  // 'keyboard-key'
  
  // Navigation behavior
  accessibilityNavigation: 'auto', // 'auto' | 'passthrough' | 'leaf' | 
                                    // 'cover' | 'group' | 'ignored'
  
  // Priority
  accessibilityPriority: 0,  // number
  
  // Labels and hints
  accessibilityLabel: 'Tap here',      // string
  accessibilityHint: 'Opens menu',     // string
  accessibilityValue: '50%',           // string
  
  // States
  accessibilityStateDisabled: false,   // boolean
  accessibilityStateSelected: false,   // boolean
  accessibilityStateLiveRegion: false, // boolean
})
```

---

## Common View Attributes

These attributes are available on all **visible elements** (View, ScrollView, ImageView, Label, etc.) as they control rendering and interaction.

### Appearance Attributes

```typescript
new Style<View>({
  // Background
  background: 'linear-gradient(#ff0000, #0000ff)', // gradient string
  backgroundColor: '#ffffff',  // color string
  opacity: 1.0,                // number 0.0-1.0
  
  // Border
  border: '2 solid #000000',   // shorthand string
  borderWidth: 2,              // number or string
  borderColor: '#000000',      // color string
  borderRadius: 8,             // number or string ('8 8 0 0' for corners)
  
  // Shadow
  boxShadow: '0 2 10 rgba(0,0,0,0.1)', // string: 'x y blur color'
  
  // Clipping
  slowClipping: false,         // boolean (degrades performance)
})
```

### Gesture Attributes

```typescript
new Style<View>({
  // Touch control
  touchEnabled: true,          // boolean
  
  // Gesture states (disabled flags)
  onTapDisabled: false,        // boolean
  onDoubleTapDisabled: false,  // boolean
  onLongPressDisabled: false,  // boolean
  onDragDisabled: false,       // boolean
  onPinchDisabled: false,      // boolean
  onRotateDisabled: false,     // boolean
  
  // Gesture timing
  onTouchDelayDuration: 0,     // number (seconds)
  longPressDuration: 0.5,      // number (seconds)
  
  // Touch area extension
  touchAreaExtension: 10,      // number (all sides)
  touchAreaExtensionTop: 10,   // number
  touchAreaExtensionRight: 10, // number
  touchAreaExtensionBottom: 10,// number
  touchAreaExtensionLeft: 10,  // number
})
```

### Transform Attributes

```typescript
new Style<View>({
  // Scale
  scaleX: 1.0,                 // number
  scaleY: 1.0,                 // number
  
  // Rotation
  rotation: 0,                 // number (radians)
  
  // Translation
  translationX: 0,             // number (points, flipped in RTL)
  translationY: 0,             // number (points)
})
```

### Mask Attributes

```typescript
new Style<View>({
  maskOpacity: 1.0,            // number 0.0-1.0
  // Note: maskPath is GeometricPath, not styleable
})
```

### Platform-Specific Attributes

```typescript
new Style<View>({
  // Accessibility
  accessibilityId: 'my-button',  // string (for testing)
  
  // Scroll behavior
  canAlwaysScrollHorizontal: false,  // boolean
  canAlwaysScrollVertical: false,    // boolean
  
  // Android-specific
  filterTouchesWhenObscured: false,  // boolean
})
```

### View Lifecycle & Behavior

```typescript
new Style<View>({
  // View reuse
  allowReuse: true,            // boolean
})
```

---

## Element-Specific Styles

### Layout Styles

`Style<Layout>` can contain **only layout-related attributes** (size, position, spacing, flexbox, accessibility, lifecycle).

```typescript
import { Layout } from 'valdi_tsx/src/NativeTemplateElements';

const layoutStyle = new Style<Layout>({
  width: '100%',
  height: 'auto',
  flexDirection: 'row',
  justifyContent: 'space-between',
  alignItems: 'center',
  padding: 16,
  // Cannot use: backgroundColor, borderRadius, etc. (View-only)
});
```

**Available Attribute Groups:**
- ✅ Size Attributes
- ✅ Position Attributes
- ✅ Spacing Attributes
- ✅ Flexbox Layout Attributes
- ✅ Visibility & Performance Attributes
- ✅ Accessibility Attributes
- ❌ Appearance Attributes (View only)
- ❌ Gesture Attributes (View only)
- ❌ Transform Attributes (View only)

---

### View Styles

`Style<View>` can contain **all Layout attributes plus View-specific attributes** (appearance, gestures, transforms).

```typescript
import { View } from 'valdi_tsx/src/NativeTemplateElements';

const viewStyle = new Style<View>({
  // All Layout attributes
  width: 200,
  height: 200,
  padding: 16,
  flexDirection: 'column',
  
  // View-specific attributes
  backgroundColor: '#ffffff',
  borderRadius: 8,
  borderWidth: 1,
  borderColor: '#dddddd',
  boxShadow: '0 2 10 rgba(0,0,0,0.1)',
  opacity: 0.9,
  
  // Gestures
  touchEnabled: true,
  onTapDisabled: false,
  
  // Transforms
  scaleX: 1.0,
  scaleY: 1.0,
  rotation: 0,
});
```

**Available Attribute Groups:**
- ✅ All Layout attributes
- ✅ Appearance Attributes
- ✅ Gesture Attributes
- ✅ Transform Attributes
- ✅ Mask Attributes
- ✅ Platform-Specific Attributes

---

### ScrollView Styles

`Style<ScrollView>` can contain **all View attributes plus ScrollView-specific attributes**.

```typescript
import { ScrollView } from 'valdi_tsx/src/NativeTemplateElements';

const scrollStyle = new Style<ScrollView>({
  // All View attributes
  backgroundColor: '#f5f5f5',
  padding: 16,
  
  // ScrollView-specific attributes
  horizontal: false,
  bounces: true,
  bouncesFromDragAtStart: true,
  bouncesFromDragAtEnd: true,
  bouncesVerticalWithSmallContent: false,
  bouncesHorizontalWithSmallContent: false,
  
  pagingEnabled: false,
  scrollEnabled: true,
  
  showsVerticalScrollIndicator: true,
  showsHorizontalScrollIndicator: true,
  
  cancelsTouchesOnScroll: true,
  dismissKeyboardOnDrag: false,
  dismissKeyboardOnDragMode: 'immediate',
  
  fadingEdgeLength: 0,
  decelerationRate: 'normal',
  
  viewportExtensionTop: 0,
  viewportExtensionRight: 0,
  viewportExtensionBottom: 0,
  viewportExtensionLeft: 0,
  
  circularRatio: 0,
  
  // Note: flexDirection is NOT available (use horizontal instead)
});
```

**Available Attribute Groups:**
- ✅ All View attributes
- ✅ Scroll Behavior Attributes
- ✅ Bounce/Overscroll Attributes
- ✅ Touch/Gesture Behavior Attributes
- ✅ Visual Indicators Attributes
- ✅ Viewport Extensions Attributes
- ❌ `flexDirection` (use `horizontal` instead)

**Note:** Programmatic properties like `contentOffsetX`, `contentOffsetY`, `contentOffsetAnimated` cannot be used in styles.

---

### ImageView Styles

`Style<ImageView>` can contain **all Layout and View attributes plus ImageView-specific attributes**.

```typescript
import { ImageView } from 'valdi_tsx/src/NativeTemplateElements';

const imageStyle = new Style<ImageView>({
  // Layout attributes
  width: 200,
  height: 200,
  
  // View attributes
  backgroundColor: '#f5f5f5',
  borderRadius: 8,
  
  // ImageView-specific attributes
  objectFit: 'cover',      // 'fill' | 'contain' | 'cover' | 'none'
  tint: '#007AFF',         // color string
  flipOnRtl: false,        // boolean
  
  // Content transform
  contentScaleX: 1.0,      // number
  contentScaleY: 1.0,      // number
  contentRotation: 0,      // number (radians)
  
  // Note: src, filter, callbacks cannot be used in styles
});
```

**Available Attribute Groups:**
- ✅ All Layout attributes
- ✅ All View attributes
- ✅ Image Display Attributes
- ✅ Content Transform Attributes
- ❌ `src` (dynamic, use inline)
- ❌ `filter` (complex object)
- ❌ Callbacks (functions)

---

### VideoView Styles

`Style<VideoView>` can contain **all Layout and View attributes plus VideoView-specific attributes**.

```typescript
import { VideoView } from 'valdi_tsx/src/NativeTemplateElements';

const videoStyle = new Style<VideoView>({
  // Layout attributes
  width: 400,
  height: 300,
  
  // View attributes
  backgroundColor: '#000000',
  borderRadius: 8,
  
  // VideoView-specific attributes
  volume: 1.0,             // number 0.0-1.0
  playbackRate: 1,         // 0=paused, 1=playing
  
  // Note: src, seekToTime, callbacks cannot be used in styles
});
```

**Available Attribute Groups:**
- ✅ All Layout attributes
- ✅ All View attributes
- ✅ Volume and playback rate
- ❌ `src` (dynamic, use inline)
- ❌ `seekToTime` (dynamic control)
- ❌ Callbacks (functions)

---

### Label Styles

`Style<Label>` can contain **all Layout and View attributes plus Label-specific text attributes**.

```typescript
import { Label } from 'valdi_tsx/src/NativeTemplateElements';

const labelStyle = new Style<Label>({
  // Layout attributes
  width: 200,
  padding: 8,
  
  // View attributes
  backgroundColor: '#ffffff',
  borderRadius: 4,
  
  // Text styling
  font: 'Montserrat-Bold 16 unscaled 20',
  color: '#000000',
  textGradient: 'linear-gradient(#ff0000, #0000ff)',
  textShadow: 'rgba(0,0,0,0.1) 1 1 1 1',
  
  // Text layout
  numberOfLines: 2,        // number (0=unlimited)
  textAlign: 'left',       // 'left' | 'right' | 'center' | 'justified'
  textDecoration: 'none',  // 'none' | 'strikethrough' | 'underline' | 'dashed-underline' | 'dotted-underline'
  lineHeight: 24,          // number (points)
  lineHeightMultiple: 1.2, // number (ratio)
  letterSpacing: 0,        // number (points)
  textOverflow: 'ellipsis',// 'ellipsis' | 'clip'
  
  // Auto-sizing
  adjustsFontSizeToFitWidth: false,  // boolean
  minimumScaleFactor: 0.5,           // number
  
  // Note: value cannot be used in styles (dynamic)
});
```

**Available Attribute Groups:**
- ✅ All Layout attributes
- ✅ All View attributes
- ✅ Text Styling Attributes
- ✅ Text Layout Attributes
- ✅ Auto-sizing Attributes
- ❌ `value` (dynamic content, use inline)
- ❌ Callbacks (functions)

---

### TextField Styles

`Style<TextField>` can contain **all Label attributes plus TextField-specific input attributes**.

```typescript
import { TextField } from 'valdi_tsx/src/NativeTemplateElements';

const textFieldStyle = new Style<TextField>({
  // All Label attributes
  font: 'system 16',
  color: '#000000',
  padding: 12,
  backgroundColor: '#f5f5f5',
  borderRadius: 8,
  borderWidth: 1,
  borderColor: '#dddddd',
  
  // TextField-specific attributes
  placeholderColor: '#999999',
  tintColor: '#007AFF',    // iOS caret color
  
  // Keyboard configuration
  contentType: 'default',  // 'default' | 'phoneNumber' | 'email' | 'password' | 
                           // 'url' | 'number' | 'numberDecimal' | etc.
  returnKeyText: 'done',   // 'done' | 'go' | 'join' | 'next' | 'search' | 'send'
  autocapitalization: 'sentences',  // 'sentences' | 'words' | 'characters' | 'none'
  autocorrection: 'default',        // 'default' | 'none'
  keyboardAppearance: 'default',    // 'default' | 'dark' | 'light'
  
  // Text editing behavior
  textAlign: 'left',       // 'left' | 'center' | 'right'
  enabled: true,           // boolean
  characterLimit: 100,     // number
  selectTextOnFocus: false,// boolean
  closesWhenReturnKeyPressed: true,  // boolean
  enableInlinePredictions: false,    // boolean (iOS)
  
  // Note: value, placeholder, selection, callbacks cannot be used in styles
});
```

**Available Attribute Groups:**
- ✅ All Layout attributes
- ✅ All View attributes
- ✅ Text Styling Attributes
- ✅ Keyboard Configuration Attributes
- ✅ Text Editing Behavior Attributes
- ❌ `value`, `placeholder` (dynamic, use inline)
- ❌ `selection` (dynamic state)
- ❌ `focused` (programmatic only)
- ❌ Callbacks (functions)

---

### TextView Styles

`Style<TextView>` can contain **all TextField attributes plus TextView-specific attributes**.

```typescript
import { TextView } from 'valdi_tsx/src/NativeTemplateElements';

const textViewStyle = new Style<TextView>({
  // All TextField attributes
  font: 'system 16',
  color: '#000000',
  padding: 12,
  backgroundColor: '#f5f5f5',
  borderRadius: 8,
  
  // TextView-specific attributes
  returnType: 'linereturn',  // 'linereturn' | TextFieldReturnKeyText
  textGravity: 'top',        // 'top' | 'center' | 'bottom'
  closesWhenReturnKeyPressed: false,  // typically false for multiline
  
  // Background effect
  backgroundEffectColor: '#ffffff',
  backgroundEffectBorderRadius: 4,
  backgroundEffectPadding: 2,
});
```

**Available Attribute Groups:**
- ✅ All TextField attributes
- ✅ Return Type Configuration
- ✅ Text Gravity Attributes
- ✅ Background Effect Attributes
- ❌ `value`, `placeholder` (dynamic, use inline)
- ❌ `focused` (programmatic only)
- ❌ Callbacks (functions)

---

### BlurView Styles

`Style<BlurView>` can contain **all Layout and View attributes plus BlurView-specific attributes**.

```typescript
import { BlurView } from 'valdi_tsx/src/NativeTemplateElements';

const blurStyle = new Style<BlurView>({
  // Layout attributes
  width: 300,
  height: 300,
  
  // View attributes (limited)
  opacity: 0.9,
  borderRadius: 12,
  
  // BlurView-specific attributes
  blurStyle: 'systemMaterial',  // Many options:
                                 // 'light', 'dark', 'extraLight', 'regular', 'prominent'
                                 // 'systemUltraThinMaterial', 'systemThinMaterial',
                                 // 'systemMaterial', 'systemThickMaterial',
                                 // 'systemChromeMaterial', 'systemUltraThinMaterialLight',
                                 // etc.
});
```

**Available Attribute Groups:**
- ✅ All Layout attributes
- ✅ Limited View attributes (positioning, some appearance)
- ✅ Blur Style Attribute
- ❌ Most View appearance attributes (blur style controls appearance)

---

### SpinnerView Styles

`Style<SpinnerView>` can contain **all Layout and View attributes plus SpinnerView-specific attributes**.

```typescript
import { SpinnerView } from 'valdi_tsx/src/NativeTemplateElements';

const spinnerStyle = new Style<SpinnerView>({
  // Layout attributes
  width: 40,
  height: 40,
  
  // View attributes
  opacity: 1.0,
  
  // SpinnerView-specific attributes
  color: '#007AFF',        // color string
});
```

**Available Attribute Groups:**
- ✅ All Layout attributes
- ✅ All View attributes
- ✅ Spinner Color Attribute

---

### ShapeView Styles

`Style<ShapeView>` can contain **all Layout and View attributes plus ShapeView-specific attributes**.

```typescript
import { ShapeView } from 'valdi_tsx/src/NativeTemplateElements';

const shapeStyle = new Style<ShapeView>({
  // Layout attributes
  width: 100,
  height: 100,
  
  // View attributes
  backgroundColor: 'transparent',
  
  // ShapeView-specific attributes
  strokeWidth: 2,          // number
  strokeColor: '#000000',  // color string
  strokeCap: 'round',      // 'butt' | 'round' | 'square'
  strokeJoin: 'round',     // 'bevel' | 'miter' | 'round'
  strokeStart: 0,          // number 0-1 (animatable)
  strokeEnd: 1,            // number 0-1 (animatable)
  fillColor: '#ff0000',    // color string
  
  // Note: path is GeometricPath, cannot be used in styles (use inline)
});
```

**Available Attribute Groups:**
- ✅ All Layout attributes
- ✅ All View attributes
- ✅ Stroke Attributes
- ✅ Fill Attributes
- ❌ `path` (complex object, use inline)

---

### AnimatedImage Styles

`Style<AnimatedImage>` can contain **all Layout and View attributes plus AnimatedImage-specific attributes**.

```typescript
import { AnimatedImage } from 'valdi_tsx/src/NativeTemplateElements';

const animatedImageStyle = new Style<AnimatedImage>({
  // Layout attributes
  width: 200,
  height: 200,
  
  // View attributes
  backgroundColor: 'transparent',
  
  // AnimatedImage-specific attributes
  loop: true,              // boolean
  advanceRate: 1,          // number (0=paused, 1=normal, -1=reverse)
  objectFit: 'contain',    // 'fill' | 'contain' | 'cover' | 'none'
  
  // Animation timing
  currentTime: 0,          // number (seconds)
  animationStartTime: 0,   // number (seconds)
  animationEndTime: 5,     // number (seconds)
  
  // Note: src, fontProvider, callbacks cannot be used in styles
});
```

**Available Attribute Groups:**
- ✅ All Layout attributes
- ✅ All View attributes
- ✅ Animation Control Attributes
- ✅ Display Configuration Attributes
- ❌ `src` (dynamic, use inline)
- ❌ `fontProvider` (complex object)
- ❌ Callbacks (functions)

---

## Style Composition

### Merging Styles

Combine multiple styles into one. Later styles override earlier ones.

```typescript
const baseStyle = new Style<View>({
  width: 100,
  height: 100,
  backgroundColor: 'red',
});

const roundedStyle = new Style<View>({
  borderRadius: 8,
});

const shadowStyle = new Style<View>({
  boxShadow: '0 2 10 rgba(0,0,0,0.1)',
});

// Merge all three
const cardStyle = Style.merge(baseStyle, roundedStyle, shadowStyle);
// Result: width: 100, height: 100, backgroundColor: 'red', 
//         borderRadius: 8, boxShadow: '0 2 10 rgba(0,0,0,0.1)'
```

### Extending Styles

Create a new style based on an existing one with modifications.

```typescript
const baseStyle = new Style<View>({
  width: 100,
  height: 100,
  backgroundColor: 'red',
});

const blueStyle = baseStyle.extend({
  backgroundColor: 'blue',  // Override
  borderRadius: 8,          // Add new
});
// Result: width: 100, height: 100, backgroundColor: 'blue', borderRadius: 8
```

### Style Inheritance

Styles follow TypeScript's type system. A `Style<View>` can be used where a `Style<Layout>` is expected because View extends Layout.

```typescript
const viewStyle = new Style<View>({
  backgroundColor: 'red',
  width: 100,
});

// ✅ Works - View extends Layout
<view style={viewStyle} />

// ✅ Works - can use on layout (but backgroundColor won't apply)
<layout style={viewStyle} />  

// ❌ Type error - Label doesn't extend View directly
const labelStyle = new Style<Label>({ /* ... */ });
<view style={labelStyle} />
```

### Conditional Styles

Pre-create styles and switch between them based on state.

```typescript
const styles = {
  normal: new Style<View>({
    backgroundColor: '#ffffff',
    borderColor: '#dddddd',
  }),
  selected: new Style<View>({
    backgroundColor: '#007AFF',
    borderColor: '#0051D5',
  }),
  disabled: new Style<View>({
    backgroundColor: '#f5f5f5',
    borderColor: '#cccccc',
    opacity: 0.5,
  }),
};

// In render:
<view style={
  this.state.disabled ? styles.disabled :
  this.state.selected ? styles.selected :
  styles.normal
} />
```

---

## Performance Notes

### ✅ DO: Create Styles at Initialization

```typescript
// At module or component class level
const goodStyle = new Style<View>({
  width: 100,
  height: 100,
  backgroundColor: 'red',
});

class MyComponent extends Component {
  // Or as class property
  private buttonStyle = new Style<View>({
    padding: 12,
    borderRadius: 8,
  });
  
  onRender() {
    <view style={goodStyle} />
    <view style={this.buttonStyle} />
  }
}
```

### ❌ DON'T: Create Styles During Render

```typescript
class MyComponent extends Component {
  onRender() {
    // BAD - Creates new object every render
    const badStyle = new Style<View>({
      width: 100,
      height: 100,
    });
    
    // BAD - Expensive operations during render
    const merged = Style.merge(style1, style2);
    const extended = style1.extend({ backgroundColor: 'red' });
    
    <view style={badStyle} />
  }
}
```

### ✅ DO: Use Inline Attributes for Dynamic Values

```typescript
const staticStyle = new Style<View>({
  width: 100,
  height: 100,
  borderRadius: 8,
});

class MyComponent extends Component {
  onRender() {
    // Good - Style is static, dynamic value is inline
    <view 
      style={staticStyle} 
      backgroundColor={this.state.isActive ? 'blue' : 'gray'} 
    />
  }
}
```

### ✅ DO: Pre-create Variant Styles

```typescript
const baseButton = new Style<View>({
  padding: 12,
  borderRadius: 8,
});

const styles = {
  primary: baseButton.extend({ backgroundColor: '#007AFF' }),
  secondary: baseButton.extend({ backgroundColor: '#5856D6' }),
  danger: baseButton.extend({ backgroundColor: '#FF3B30' }),
};

// Use in render - no style creation happens
<view style={this.props.variant === 'primary' ? styles.primary : styles.secondary} />
```

### Style vs Inline Attribute Priority

Inline attributes **always override** style attributes.

```typescript
const myStyle = new Style<View>({
  backgroundColor: 'red',
  width: 100,
});

// backgroundColor will be 'blue', width will be 100
<view style={myStyle} backgroundColor='blue' />
```

### What Can't Be Styled

These properties cannot be included in styles and must be used inline:

- **Callbacks/Functions**: `onChange`, `onTap`, `onLayout`, etc.
- **Complex Objects**: `path`, `filter`, `fontProvider`, etc.
- **Dynamic Content**: `value`, `src`, `placeholder` (in most cases)
- **Refs**: `ref` property
- **Programmatic Properties**: `focused`, `contentOffsetX`, etc.

---

## Quick Reference by Category

### Layout & Positioning
- Size: `width`, `height`, `minWidth`, `maxWidth`, `minHeight`, `maxHeight`, `aspectRatio`
- Position: `position`, `top`, `right`, `bottom`, `left`
- Spacing: `margin*`, `padding*`
- Flexbox: `flexDirection`, `justifyContent`, `alignItems`, `alignContent`, `alignSelf`, `flexGrow`, `flexShrink`, `flexBasis`, `flexWrap`
- Display: `display`, `overflow`, `zIndex`

### Appearance
- Background: `background`, `backgroundColor`, `opacity`
- Border: `border`, `borderWidth`, `borderColor`, `borderRadius`
- Shadow: `boxShadow`
- Clipping: `slowClipping`

### Text (Label, TextField, TextView)
- Font: `font`, `color`, `textGradient`, `textShadow`
- Layout: `numberOfLines`, `textAlign`, `textDecoration`, `lineHeight`, `letterSpacing`, `textOverflow`
- Auto-size: `adjustsFontSizeToFitWidth`, `minimumScaleFactor`
- Input: `placeholderColor`, `tintColor`, `contentType`, `returnKeyText`, `autocapitalization`, `autocorrection`

### Gestures
- Control: `touchEnabled`, `touchAreaExtension*`
- Disabled: `onTapDisabled`, `onDoubleTapDisabled`, `onLongPressDisabled`, `onDragDisabled`, `onPinchDisabled`, `onRotateDisabled`
- Timing: `onTouchDelayDuration`, `longPressDuration`

### Transforms
- Scale: `scaleX`, `scaleY`
- Rotate: `rotation`
- Translate: `translationX`, `translationY`

### Accessibility
- Category: `accessibilityCategory`, `accessibilityNavigation`, `accessibilityPriority`
- Labels: `accessibilityLabel`, `accessibilityHint`, `accessibilityValue`
- States: `accessibilityStateDisabled`, `accessibilityStateSelected`, `accessibilityStateLiveRegion`
- Testing: `accessibilityId`

### Performance
- Lazy: `lazy`, `lazyLayout`, `limitToViewport`
- Viewport: `ignoreParentViewport`, `extendViewportWithChildren`
- Estimates: `estimatedWidth`, `estimatedHeight`
- Animation: `animationsEnabled`

---

## Practical Examples: Using Percentages

### Responsive Layout with Percentages

```typescript
const styles = {
  // Full-width container
  container: new Style<View>({
    width: '100%',           // Takes full width of parent
    padding: '5%',           // 5% padding on all sides
    backgroundColor: '#f5f5f5',
  }),
  
  // Two-column layout (side by side)
  column: new Style<View>({
    width: '48%',            // Just under 50% to allow for gap
    margin: '1%',            // Creates spacing between columns
    backgroundColor: '#ffffff',
  }),
  
  // Centered content with max width
  content: new Style<Layout>({
    width: '90%',            // 90% of parent width
    maxWidth: 600,           // But never more than 600pt
    marginLeft: '5%',        // Center with left margin
    marginRight: '5%',       // Center with right margin
  }),
  
  // Card with percentage padding
  card: new Style<View>({
    width: '100%',
    padding: '4%',           // Responsive padding
    borderRadius: 8,
    boxShadow: '0 2 10 rgba(0,0,0,0.1)',
  }),
  
  // Absolutely positioned overlay
  overlay: new Style<View>({
    position: 'absolute',
    top: '10%',              // 10% from top
    left: '10%',             // 10% from left
    width: '80%',            // 80% of parent width
    height: '80%',           // 80% of parent height
    backgroundColor: 'rgba(0,0,0,0.5)',
  }),
};
```

### Flexbox with Percentage Basis

```typescript
const flexStyles = {
  container: new Style<Layout>({
    width: '100%',
    flexDirection: 'row',
    justifyContent: 'space-between',
  }),
  
  // Flexible items with percentage basis
  sidebar: new Style<Layout>({
    flexBasis: '30%',        // Takes 30% of container width
    flexGrow: 0,             // Won't grow
    flexShrink: 1,           // Can shrink if needed
  }),
  
  mainContent: new Style<Layout>({
    flexBasis: '65%',        // Takes 65% of container width
    flexGrow: 1,             // Can grow to fill space
    flexShrink: 1,           // Can shrink if needed
  }),
};
```

### Avoiding Common Percentage Mistakes

```typescript
// ❌ WRONG - These don't support percentages
const wrongStyles = new Style<View>({
  borderRadius: '50%',     // ❌ borderRadius only accepts points
  opacity: '50%',          // ❌ opacity is 0.0-1.0, not percentage
  scaleX: '150%',          // ❌ scale is a number ratio, not percentage
  rotation: '90%',         // ❌ rotation is in radians
  aspectRatio: '16/9',     // ❌ aspectRatio is a number (16/9), not a string
});

// ✅ CORRECT - Proper usage
const correctStyles = new Style<View>({
  borderRadius: 8,         // ✅ Points only
  opacity: 0.5,            // ✅ 0.0-1.0 scale
  scaleX: 1.5,             // ✅ Numeric ratio (150% = 1.5)
  rotation: Math.PI / 2,   // ✅ Radians (90° = π/2)
  aspectRatio: 16/9,       // ✅ Numeric ratio
});
```

### Mixing Percentages and Fixed Values

```typescript
const mixedStyles = {
  // Fixed header with percentage width
  header: new Style<View>({
    width: '100%',           // Full width
    height: 60,              // Fixed height in points
    paddingLeft: 20,         // Fixed padding
    paddingRight: 20,        // Fixed padding
    backgroundColor: '#ffffff',
  }),
  
  // Responsive grid items
  gridItem: new Style<View>({
    width: '31%',            // Percentage width for 3 columns
    height: 150,             // Fixed height
    margin: '1%',            // Percentage margin for spacing
    borderRadius: 8,         // Fixed border radius
  }),
  
  // Combination for centering
  centered: new Style<Layout>({
    width: '80%',            // Percentage width
    maxWidth: 400,           // Fixed maximum
    marginLeft: '10%',       // Percentage to center
    marginRight: '10%',      // Percentage to center
  }),
};
```

---

## See Also

- [Complete API Reference](api-reference-elements.md) - Full element documentation
- [The Style<> Guide](../docs/core-styling.md) - Usage patterns and best practices
- [Quick Reference](api-quick-reference.md) - Common properties lookup
- [Core Flexbox](../docs/core-flexbox.md) - Understanding flexbox layout
