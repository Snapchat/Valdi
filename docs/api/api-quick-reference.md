# Native Template Elements - Quick Reference

This is a quick reference guide for the most commonly used properties of native template elements. For complete API documentation, see the [API Reference](api-reference-elements.md).

## Common to All Elements

```tsx
// Layout & Sizing
width={200}              // or "50%" or "auto"
height={100}             // or "50%" or "auto"
minWidth={50}
maxWidth={500}
aspectRatio={1.5}        // width/height ratio

// Positioning
position="absolute"      // or "relative" (default)
top={10}
left={10}
right={10}
bottom={10}

// Spacing
margin={10}              // or "10 20 10 20" or "10"
padding={10}
marginTop={5}
paddingLeft={10}

// Flexbox
flexDirection="row"      // or "column" (default), "row-reverse", "column-reverse"
justifyContent="center"  // or "flex-start", "flex-end", "space-between", "space-around"
alignItems="center"      // or "stretch" (default), "flex-start", "flex-end", "baseline"
flexGrow={1}
flexShrink={1}
flexWrap="wrap"

// Lifecycle
onLayout={this.handleLayout}
onVisibilityChanged={this.handleVisibilityChanged}
ref={myElementRef}
key="unique-key"
id="global-id"

// Performance
lazy={true}              // Shorthand for lazyLayout + limitToViewport
lazyLayout={true}
limitToViewport={true}

// Accessibility
accessibilityLabel="Tap to continue"
accessibilityHint="Opens the next screen"
accessibilityCategory="button"
```

---

## `<layout>`

**Use for:** Invisible containers that don't need native views (best performance).

```tsx
<layout
  width="100%"
  height={200}
  flexDirection="row"
  justifyContent="space-between"
  padding={10}
>
  {/* children */}
</layout>
```

---

## `<view>`

**Use for:** Visible containers that need background, borders, or gesture handling.

```tsx
<view
  backgroundColor="#ffffff"
  borderRadius={8}
  borderWidth={1}
  borderColor="#dddddd"
  opacity={0.9}
  boxShadow="0 2 10 rgba(0,0,0,0.1)"
  
  // Gestures
  onTap={this.handleTap}
  onLongPress={this.handleLongPress}
  onDrag={this.handleDrag}
  touchEnabled={true}
  
  // Transform
  scaleX={1.2}
  scaleY={1.2}
  rotation={0.5}  // radians
  translationX={10}
  translationY={10}
>
  {/* children */}
</view>
```

---

## `<scroll>`

**Use for:** Scrollable containers.

```tsx
<scroll
  horizontal={false}     // true for horizontal scrolling
  height={400}
  width="100%"
  
  // Scroll events
  onScroll={this.handleScroll}
  onScrollEnd={this.handleScrollEnd}
  onDragStart={this.handleDragStart}
  
  // Behavior
  bounces={true}
  pagingEnabled={false}
  scrollEnabled={true}
  showsVerticalScrollIndicator={true}
  dismissKeyboardOnDrag={false}
  
  // Viewport extensions (for preloading)
  viewportExtensionTop={100}
  viewportExtensionBottom={100}
>
  {/* scrollable children */}
</scroll>

// Programmatic scrolling (via ref):
myScrollRef.setAttribute('contentOffsetY', 500);
myScrollRef.setAttribute('contentOffsetAnimated', true);
```

---

## `<image>`

**Use for:** Displaying images from assets or URLs.

```tsx
<image
  src={res.myImage}      // or "https://example.com/image.png"
  width={200}
  height={200}
  
  // Display
  objectFit="cover"      // or "fill", "contain", "none"
  tint="#ff0000"
  flipOnRtl={false}
  
  // Transform
  contentScaleX={1}
  contentScaleY={1}
  contentRotation={0}    // radians
  
  // Callbacks
  onAssetLoad={this.handleAssetLoad}
  onImageDecoded={this.handleImageDecoded}
  
  // Filters
  filter={myImageFilter}
/>
```

---

## `<video>`

**Use for:** Video playback.

```tsx
<video
  src="https://example.com/video.mp4"
  width={400}
  height={300}
  
  // Playback control
  volume={0.8}           // 0.0 to 1.0
  playbackRate={1}       // 0=paused, 1=playing
  seekToTime={5000}      // milliseconds
  
  // Callbacks
  onVideoLoaded={this.handleVideoLoaded}
  onBeginPlaying={this.handleBeginPlaying}
  onError={this.handleError}
  onCompleted={this.handleCompleted}
  onProgressUpdated={this.handleProgressUpdated}
/>
```

---

## `<label>`

**Use for:** Displaying text.

```tsx
<label
  value="Hello World"    // or attributedText
  
  // Text styling
  font="Montserrat-Bold 16 unscaled 16"
  color="#000000"
  textGradient="linear-gradient(#ff0000, #0000ff)"
  textShadow="rgba(0,0,0,0.1) 1 1 1 1"
  
  // Layout
  numberOfLines={2}      // 0 = unlimited
  textAlign="left"       // or "center", "right", "justified"
  textDecoration="none"  // or "underline", "dashed-underline", "dotted-underline", "strikethrough"
  lineHeight={24}        // explicit line height in points
  lineHeightMultiple={1.2} // ratio of font height
  letterSpacing={0}      // points
  textOverflow="ellipsis" // or "clip"
  
  // Auto-sizing
  adjustsFontSizeToFitWidth={false}
  minimumScaleFactor={0.5}
/>
```

---

## `<textfield>`

**Use for:** Single-line text input.

```tsx
<textfield
  value={this.state.text}
  placeholder="Enter text..."
  placeholderColor="#999999"
  
  // Keyboard
  contentType="default"  // or "email", "phoneNumber", "password", "number"
  returnKeyText="done"   // or "go", "join", "next", "search", "send"
  autocapitalization="sentences"
  autocorrection="default"
  keyboardAppearance="default"
  
  // Behavior
  enabled={true}
  textAlign="left"
  characterLimit={50}
  selectTextOnFocus={false}
  closesWhenReturnKeyPressed={true}
  
  // Callbacks
  onChange={this.handleChange}
  onEditBegin={this.handleEditBegin}
  onEditEnd={this.handleEditEnd}
  onReturn={this.handleReturn}
  onWillDelete={this.handleWillDelete}
  onSelectionChange={this.handleSelectionChange}
/>

// Programmatic focus (via ref):
myTextFieldRef.setAttribute('focused', true);
```

---

## `<textview>`

**Use for:** Multi-line text input.

```tsx
<textview
  value={this.state.text}
  placeholder="Enter text..."
  
  // Similar to textfield, plus:
  returnType="linereturn" // or any TextFieldReturnKeyText
  textGravity="top"       // or "center", "bottom"
  closesWhenReturnKeyPressed={false}
  
  // Background effect
  backgroundEffectColor="#ffffff"
  backgroundEffectBorderRadius={4}
  backgroundEffectPadding={2}
  
  onChange={this.handleChange}
/>
```

---

## `<blur>`

**Use for:** Blur effects (iOS primarily).

```tsx
<blur
  blurStyle="systemMaterial"  // Many options available
  width={300}
  height={300}
>
  {/* children rendered on top of blur */}
</blur>
```

---

## `<spinner>`

**Use for:** Loading indicators.

```tsx
<spinner
  color="#007AFF"
  width={40}
  height={40}
/>
```

---

## `<shape>`

**Use for:** Custom vector shapes.

```tsx
<shape
  path={myGeometricPath}
  width={100}
  height={100}
  
  // Stroke
  strokeWidth={2}
  strokeColor="#000000"
  strokeCap="round"      // or "butt", "square"
  strokeJoin="round"     // or "bevel", "miter"
  strokeStart={0}        // 0 to 1, animatable
  strokeEnd={1}          // 0 to 1, animatable
  
  // Fill
  fillColor="#ff0000"
/>
```

---

## `<animatedimage>`

**Use for:** Lottie animations and animated images.

```tsx
<animatedimage
  src={res.myLottie}
  width={200}
  height={200}
  
  // Animation control
  loop={true}
  advanceRate={1}        // 0=paused, 1=normal, -1=reverse
  currentTime={0}        // seconds
  animationStartTime={0} // seconds
  animationEndTime={5}   // seconds
  
  // Display
  objectFit="contain"    // or "fill", "cover", "none"
  
  // Fonts
  fontProvider={myFontProvider}
  
  // Callbacks
  onAssetLoad={this.handleAssetLoad}
  onImageDecoded={this.handleImageDecoded}
  onProgress={this.handleProgress}
/>
```

---

## `<slot>`

**Use for:** Content projection in components.

```tsx
// In component:
<layout>
  <slot name="header" />
  <slot /> {/* default slot */}
  <slot name="footer" />
</layout>

// When using component:
<MyComponent>
  <layout slot="header">Header content</layout>
  <layout>Default content</layout>
  <layout slot="footer">Footer content</layout>
</MyComponent>
```

---

## ⚠️ Important: Event Handlers

**Never use inline anonymous functions in TSX.** Always define event handlers as component methods.

**❌ DON'T do this:**

```tsx
class MyComponent extends Component {
  onRender() {
    <view onTap={() => {
      // This creates a new function on every render!
      console.log('tapped');
    }} />
  }
}
```

**✅ DO this instead:**

```tsx
class MyComponent extends Component {
  onRender() {
    <view onTap={this.handleTap} />
  }

  private handleTap = () => {
    console.log('tapped');
  }
}
```

**Why?** Inline functions create a new function instance on every render, which:
- Causes unnecessary re-renders and performance issues
- Breaks the diff algorithm's ability to detect unchanged callbacks
- Triggers updates to the backing native view on every render

> **See also:** [Performance Optimization](../docs/performance-optimization.md#using-callbacks-in-elements) for more details.

---

## Common Patterns

### Centered Content

```tsx
<layout width="100%" height="100%" justifyContent="center" alignItems="center">
  <label value="Centered!" />
</layout>
```

### Card with Shadow

```tsx
<view
  backgroundColor="#ffffff"
  borderRadius={12}
  boxShadow="0 4 20 rgba(0,0,0,0.15)"
  padding={16}
  margin={16}
>
  {/* card content */}
</view>
```

### Horizontal Scroll

```tsx
<scroll horizontal={true} height={200} width="100%">
  <layout flexDirection="row">
    {items.map(item => (
      <view key={item.id} width={150} height={150} margin={10}>
        {/* item content */}
      </view>
    ))}
  </layout>
</scroll>
```

### Web Content

```tsx
<webview
  width="100%"
  height={400}
  controller={this.webViewController}
/>
```

Create the controller with `WebView.createController()` from `valdi_webview`, then call controller methods such as `load`, `reload`, and `goBack` imperatively.

### Form Input

```tsx
class EmailForm extends Component {
  onRender() {
    <layout padding={20}>
      <label value="Email" marginBottom={8} />
      <textfield
        contentType="email"
        placeholder="your@email.com"
        backgroundColor="#f5f5f5"
        borderRadius={8}
        padding={12}
        onChange={this.handleEmailChange}
      />
    </layout>
  }

  private handleEmailChange = (event) => {
    this.setState({ email: event.text });
  }
}
```

---

## TypeScript Types

### Events

```typescript
import { TouchEvent, DragEvent, ScrollEvent } from 'valdi_tsx/src/GestureEvents';
import { ElementFrame } from 'valdi_tsx/src/Geometry';
import { EditTextEvent, EditTextEndEvent } from 'valdi_tsx/src/NativeTemplateElements';
```

### Refs

```typescript
import { IRenderedElementHolder } from 'valdi_tsx/src/IRenderedElementHolder';

private myViewRef = new IRenderedElementHolder<View>();

// In render:
<view ref={this.myViewRef} />

// Later:
const element = this.myViewRef.getElement();
if (element) {
  element.setAttribute('backgroundColor', '#ff0000');
}
```

---

## See Also

- [Complete API Reference](api-reference-elements.md) - Full documentation with all properties
- [Core Views Guide](../docs/core-views.md) - Layout and view best practices
- [Core Flexbox](../docs/core-flexbox.md) - Understanding flexbox layout
- [Core Images](../docs/core-images.md) - Working with images
- [Core Scrolls](../docs/core-scrolls.md) - ScrollView patterns
- [Core Text](../docs/core-text.md) - Text and input components
