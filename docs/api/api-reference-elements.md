# API Reference

This document provides a comprehensive reference for all native template elements and their properties, methods, and types.

## Table of Contents

- [Layout](#layout)
- [View](#view)
- [ScrollView](#scrollview)
- [ImageView](#imageview)
- [WebView](#webview)
- [VideoView](#videoview)
- [Label](#label)
- [TextField](#textfield)
- [TextView](#textview)
- [BlurView](#blurview)
- [SpinnerView](#spinnerview)
- [ShapeView](#shapeview)
- [AnimatedImage](#animatedimage)
- [Slot](#slot)
- [Common Types](#common-types)

> **Note:** Layout and positioning attributes are powered by [Yoga](https://github.com/facebook/yoga) (commit [`0be0e9fc`](https://github.com/facebook/yoga/commit/0be0e9fc0148aa609afdc31cb9670524f07050cb)). See [Style Attributes Reference](api-style-attributes.md#yoga-layout-engine) for Yoga version details and flexbox behavior.

---

## Layout

**JSX Element:** `<layout>`

Represents a node in the layout tree. Use a `<layout>` instead of `<view>` whenever possible, since these nodes will not be backed by a platform view when rendering, making them more performant.

### Properties

#### Layout Attributes

##### Lifecycle Callbacks

**`onLayout`**: `(frame: ElementFrame) => void`
- Called whenever the calculated frame for the element has changed.

**`onVisibilityChanged`**: `(isVisible: boolean, eventTime: EventTime) => void`
- Called whenever the visibility for the element has changed.

**`onViewportChanged`**: `(viewport: ElementFrame, frame: ElementFrame, eventTime: EventTime) => void`
- Called whenever the calculated viewport for the element has changed.
- The viewport represents the rectangle area within the element's frame that is visible.

**`onLayoutComplete`**: `() => void`
- Called after a layout pass has completed, if this node or any of its children were updated.

**`onMeasure`**: `OnMeasureFunc`
- If set, the node will be set as a lazyLayout, and the given measure callback will be called whenever the node needs to be measured.
- The callback should return a MeasuredSize tuple representing how big the node should be.
- Type: `(width: number, widthMode: MeasureMode, height: number, heightMode: MeasureMode) => MeasuredSize`

##### Performance Optimization

**`lazyLayout`**: `boolean`
- Whether the layout starting from this element should be lazy.
- A lazy layout is disconnected from its parent layout; its children cannot impact the layout of the parent.
- Setting this to true will defer layout calculation for all the children until this layout is determined to be visible.
- This can help with performance when the layout is within a ScrollView.
- Default: `false`

**`limitToViewport`**: `boolean`
- Whether the backing native View instance should only be created if the element is visible within the viewport.
- Setting this to true can help with performance when the view is within a ScrollView.
- Default: `true`

**`lazy`**: `boolean`
- Shorthand for lazyComponent, lazyLayout and limitToViewport.
- Default: `false`

**`estimatedWidth`**: `number`
- If set, the node will use this value as the base width that the node should measure at before the children are inserted.
- This is typically used on nodes that represent an empty placeholder that are later rendered with children.

**`estimatedHeight`**: `number`
- If set, the node will use this value as the base height that the node should measure at before the children are inserted.

##### Identity & References

**`id`**: `string`
- Used to uniquely identify an element in the global valdi context.

**`key`**: `string`
- A key to uniquely identify the element.
- If it is not provided, the framework will generate one based on the index in which the element is being rendered.

**`ref`**: `IRenderedElementHolder<this>`
- Sets an element reference holder, which will keep track of the rendered elements.

##### Visual Properties

**`animationsEnabled`**: `boolean`
- Can be used to disable animations for this element and its children.
- Default: `true`

**`zIndex`**: `number`
- Loosely, zIndex specifies the order in which siblings are layered on top of each other.
- Higher zIndex will mean that an element will be rendered "on top" of its siblings.
- Specifying a zIndex will make the framework sort this element and its siblings before rendering the view tree.

**`class`**: `string`
- Sets a CSS class to use from the associated CSS document.
- You typically would use the style property directly instead.

**`ignoreParentViewport`**: `boolean`
- Whether the calculated viewport for this element should not be intersected with the viewport of the parent.
- This allows a child to have a viewport bigger than its parent.

#### Size Properties

**`width`**: `CSSValue` (string | number)
- Specifies the width of an element's content area.
- `auto` (default): width for the element based on its content.
- points: Defines the width in absolute points.
- percentage: Defines the width in percentage of its parent's width.

**`height`**: `CSSValue`
- Specifies the height of an element's content area.
- `auto` (default): height for the element based on its content.
- points: Defines the height in absolute points.
- percentage: Defines the height in percentage of its parent's height.

**`minWidth`**: `CSSValue`
- Specifies the minimum width of an element's content area.

**`minHeight`**: `CSSValue`
- Specifies the minimum height of an element's content area.

**`maxWidth`**: `CSSValue`
- Specifies the maximum width of an element's content area.

**`maxHeight`**: `CSSValue`
- Specifies the maximum height of an element's content area.

**`aspectRatio`**: `number`
- Specifies the aspect ratio of an element's content area.
- Defined as the ratio between the width and the height of a node.
- e.g. if a node has an aspect ratio of 2 then its width is twice the size of its height.

#### Position Properties

**`position`**: `'relative' | 'absolute'`
- The position type of an element defines how it is positioned within its parent.
- `relative` (default): positioned according to the normal flow of the layout, then offset relative to that position based on the values of top, right, bottom, and left.
- `absolute`: doesn't take part in the normal layout flow. Position is determined based on the top, right, bottom, and left values.

**`top`**: `CSSValue`
- Used with `position` to offset the element from its normal position or parent.

**`right`**: `CSSValue`
- Used with `position` to offset the element from its normal position or parent.

**`bottom`**: `CSSValue`
- Used with `position` to offset the element from its normal position or parent.

**`left`**: `CSSValue`
- Used with `position` to offset the element from its normal position or parent.

#### Spacing Properties

**`margin`**: `string | number`
- Margin affects the spacing around the outside of a node.
- A node with margin will offset itself from the bounds of its parent but also offset the location of any siblings.

**`marginTop`**: `CSSValue`

**`marginRight`**: `CSSValue`

**`marginBottom`**: `CSSValue`

**`marginLeft`**: `CSSValue`

**`padding`**: `string | number`
- Padding affects the size of the node it is applied to.
- Padding will not add to the total size of an element if it has an explicit size set.

**`paddingTop`**: `CSSValue`

**`paddingRight`**: `CSSValue`

**`paddingBottom`**: `CSSValue`

**`paddingLeft`**: `CSSValue`

#### Flexbox Properties

**`display`**: `'flex' | 'none'`
- Choose the display mode.
- `flex`: use flexbox layout system
- `none`: do not display this element
- Default: `'flex'`

**`overflow`**: `'visible' | 'scroll'`
- Specifies how the parent will react to its children overflowing its boundaries.
- `visible`: overflowing children elements will stretch the parent container
- `scroll`: the parent container's size will not be affected by the children element's bounds
- Default for layout elements: `'visible'`
- Default for scroll elements: `'scroll'`

**`direction`**: `'inherit' | 'ltr' | 'rtl'`
- Layout direction specifies the direction in which children and text in a hierarchy should be laid out.
- `inherit` (default): Use the parent's direction value
- `ltr`: Text and children laid out from left to right
- `rtl`: Text and children laid out from right to left
- Default: `'inherit'`

**`flexDirection`**: `'column' | 'column-reverse' | 'row' | 'row-reverse'`
- Controls the direction in which children of a node are laid out.
- `column` (default): Align children from top to bottom
- `column-reverse`: Align children from bottom to top
- `row`: Align children from left to right
- `row-reverse`: Align children from right to left
- Default: `'column'`

**`flexWrap`**: `'no-wrap' | 'wrap' | 'wrap-reverse'`
- Set on containers and controls what happens when children overflow the size of the container along the main axis.
- Default: children are forced into a single line
- `wrap`: items are wrapped into multiple lines along the main axis if needed
- `wrap-reverse`: same as wrap but the order of the lines is reversed

**`justifyContent`**: `'flex-start' | 'center' | 'flex-end' | 'space-between' | 'space-around' | 'space-evenly'`
- Describes how to align children within the main axis of their container.
- `flex-start` (default): Align children to the start of the container's main axis
- `flex-end`: Align children to the end
- `center`: Align children in the center
- `space-between`: Evenly space children, distributing remaining space between them
- `space-around`: Evenly space children, distributing remaining space around them
- `space-evenly`: Evenly distributed with equal spacing
- Default: `'flex-start'`

**`alignContent`**: `'flex-start' | 'flex-end' | 'stretch' | 'center' | 'space-between' | 'space-around'`
- Defines the distribution of lines along the cross-axis.
- Only has effect when items are wrapped to multiple lines using `flexWrap`.
- Default: `'flex-start'`

**`alignItems`**: `'stretch' | 'flex-start' | 'flex-end' | 'center' | 'baseline'`
- Describes how to align children along the cross axis of their container.
- `stretch` (default): Stretch children to match the height of the container's cross axis
- `flex-start`: Align children to the start of the container's cross axis
- `flex-end`: Align children to the end
- `center`: Align children in the center
- `baseline`: Align children along a common baseline
- Default: `'stretch'`

**`alignSelf`**: `'auto' | 'flex-start' | 'flex-end' | 'center' | 'stretch' | 'baseline'`
- Overrides the parent's `alignItems` for this specific child.

**`flexGrow`**: `number`
- Describes how any space within a container should be distributed among its children along the main axis.
- Accepts any floating point value >= 0
- Default: `0`

**`flexShrink`**: `number`
- Describes how to shrink children along the main axis when the total size of the children overflow the size of the container.
- Accepts any floating point value >= 0
- Default: `0`

**`flexBasis`**: `CSSValue`
- Provides the default size of an item along the main axis.
- Similar to setting width (for row) or height (for column).

**`extendViewportWithChildren`**: `boolean`
- Whether the calculated viewport for the element should be potentially extended by taking into account the space of all the children.
- By default, a child element outside the bounds of the parent element is considered invisible.
- When this flag is true, the bounds of the parent will be extended such that the children are always visible.
- This flag should be used rarely and in very specific circumstances.
- Default: `false`

#### Accessibility Properties

**`accessibilityCategory`**: `'auto' | 'view' | 'text' | 'button' | 'image' | 'image-button' | 'input' | 'header' | 'link' | 'checkbox' | 'radio' | 'keyboard-key'`
- Specify meta information that can then be used by accessibility technologies.
- Default: `'auto'`

**`accessibilityNavigation`**: `'auto' | 'passthrough' | 'leaf' | 'cover' | 'group' | 'ignored'`
- Specify the way the element should behave during navigation by accessibility technologies.
- `auto`: automatically chosen depending on the element
- `passthrough`: element is not focusable, but its children may be accessed
- `leaf`: element is fully focusable and interactive but none of its children are navigatable
- `cover`: element is first fully focusable and interactive and afterward its children can also be accessed
- `group`: element may be announced but not focused and its children may also be accessed
- `ignored`: element and all its children will be ignored, unnavigatable and unfocusable
- Default: `'auto'`

**`accessibilityPriority`**: `number | AccessibilityPriority`
- Specify the local priority of the element (compared to its direct sibling) for accessibility navigation sequential ordering.
- All sibling elements are sorted by descending priority during accessibility navigation.
- Default: `0`

**`accessibilityLabel`**: `string`
- Defines the information being displayed.
- When an element is accessed by accessibility technologies, VoiceOver/TalkBack will read this string.

**`accessibilityHint`**: `string`
- Defines the purpose of this element.
- Additional hint that helps users understand what will happen when they perform an action on the accessibility element.

**`accessibilityValue`**: `string`
- For elements that intrinsically contain information (such as textinputs or sliders).
- Especially when it may be dynamic, we need to provide its current value.

**`accessibilityStateDisabled`**: `boolean`
- For dynamic and interactive elements, indicate that the element is temporarily disabled.
- Default: `false`

**`accessibilityStateSelected`**: `boolean`
- For dynamic and interactive elements, set the current selection status.
- Default: `false`

**`accessibilityStateLiveRegion`**: `boolean`
- For dynamic and interactive elements, indicate that the element frequently updates its label, value or children.
- Default: `false`

#### Styling

**`style`**: `IStyle<Layout>`
- Styling object allows setting multiple attributes at once.
- See [Layout Style Attributes](api-style-attributes.md#layout-styles) for a complete list of attributes that can be used in `Style<Layout>`.

---

## View

**JSX Element:** `<view>`

**iOS Native:** `SCValdiView`  
**Android Native:** `com.snap.valdi.views.ValdiView`

Represents a node in the layout tree backed by a native platform view. Use a `<layout>` instead of `<view>` whenever possible for better performance.

### Properties

All properties from [Layout](#layout), plus:

#### Lifecycle Callbacks

**`onViewCreate`**: `() => void`
- Called whenever the backing native view is created.

**`onViewDestroy`**: `() => void`
- Called whenever the backing native view is destroyed.

**`onViewChange`**: `(nativeView: NativeView | undefined) => void`
- Called whenever the backing native view is created or destroyed.
- @deprecated use `IRenderedElement.getNativeNode`

**`allowReuse`**: `boolean`
- Whether the backing native View instance can be re-used.
- By default, View instances are re-used across screens.
- By setting this to false, the view instance will be guaranteed to be new and not re-used from the view pool.
- Default: `true`

#### Appearance

**`background`**: `string`
- Sets the background of the view.
- Use `"color1"` to set a color.
- Use `"linear-gradient(color1, color2, color3...)"` to set a gradient with evenly-spaced stops.
- Use `"linear-gradient(color1 stop1, color2 stop2, color3 stop3...)"` to set a gradient with custom stops.

**`backgroundColor`**: `Color` (string)
- Sets the background color of the view.
- `undefined` sets a clear color.

**`opacity`**: `number`
- Sets the opacity of the view.
- Accepts values: [0.0 - 1.0]
- Note: Making a view non-opaque will require layer blending.

**`slowClipping`**: `boolean`
- Enables clipping of the view's contents based on the view's borders.
- Warning: degrades performance, avoid using if possible.
- Default: `false`

#### Border Properties

**`border`**: `string`
- Short-hand for all the borderXXX attributes.

**`borderWidth`**: `CSSValue`
- Sets the width of an element's border.
- Note: make sure to set the borderColor
- Default: `0`

**`borderColor`**: `Color`
- Sets the color of an element's four borders.
- Note: make sure to set the borderWidth for the border to be visible
- Default: `black`

**`borderRadius`**: `CSSValue`
- Defines the radius of the element's corners.
- Can specify radius on a per-corner basis: `"5 10 0 20"` (top/left/right/bottom)
- Or set all corners at once: `"10"`
- Default: `0`

**`boxShadow`**: `string`
- Add a shadow to the view.
- Syntax: `'{xOffset} {yOffset} {shadowOverflow} {color}'`
- All number values are interpreted as points.
- Example: `"0 2 10 rgba(0, 0, 0, 0.1)"`

#### Gesture Properties

**`touchEnabled`**: `boolean`
- Set this to `false` to disable any user interactions on this view and its children.
- Default: `true`

**`hitTest`**: `(event: TouchEvent) => boolean`
- Determines if a given view can capture any touches.

**`onTouch`**: `(event: TouchEvent) => void`
- Event handler called on every touch event captured by this view (started, changed, ended).

**`onTouchStart`**: `(event: TouchEvent) => void`
- Event handler called on the 'started' touch events captured by this view.

**`onTouchEnd`**: `(event: TouchEvent) => void`
- Event handler called on the 'ended' touch events captured by this view.

**`onTouchDelayDuration`**: `number`
- Specifies the minimum duration, in seconds, for an onTouch event to trigger.
- When not set, will be zero.

##### Tap Gestures

**`onTapDisabled`**: `boolean`
- Set to `true` to disable tap completely.
- Default: `false`

**`onTap`**: `(event: TouchEvent) => void`
- The handler that will be called when the user performs a tap gesture on this view.

**`onTapPredicate`**: `(event: TouchEvent) => boolean`
- The predicate used to decide whether the tap gesture should be recognized.
- Prefer using `onTapDisabled` if the decision can be made without TouchEvent data.

##### Double Tap Gestures

**`onDoubleTapDisabled`**: `boolean`
- Set to `true` to disable double tap completely.
- Default: `false`

**`onDoubleTap`**: `(event: TouchEvent) => void`
- Handler called when the user performs a double tap gesture.

**`onDoubleTapPredicate`**: `(event: TouchEvent) => boolean`
- Predicate to decide whether the double tap gesture should be recognized.

##### Long Press Gestures

**`longPressDuration`**: `number`
- Specifies the minimum duration, in seconds, for a long press to trigger.
- When not set, will use a platform provided default.

**`onLongPressDisabled`**: `boolean`
- Set to `true` to disable long press completely.
- Default: `false`

**`onLongPress`**: `(event: TouchEvent) => void`
- Handler called when the user performs a long press gesture.

**`onLongPressPredicate`**: `(event: TouchEvent) => boolean`
- Predicate to decide whether the long press gesture should be recognized.

##### Drag Gestures

**`onDragDisabled`**: `boolean`
- Set to `true` to disable drag completely.
- Default: `false`

**`onDrag`**: `(event: DragEvent) => void`
- Handler called when the user performs a dragging gesture that started on this view.

**`onDragPredicate`**: `(event: DragEvent) => boolean`
- Predicate to decide whether the drag gesture should be recognized.

##### Pinch Gestures

**`onPinchDisabled`**: `boolean`
- Set to `true` to disable pinch completely.
- Default: `false`

**`onPinch`**: `(event: PinchEvent) => void`
- Handler called when the user performs a pinch gesture on this view.

**`onPinchPredicate`**: `(event: PinchEvent) => boolean`
- Predicate to decide whether the pinch gesture should be recognized.

##### Rotate Gestures

**`onRotateDisabled`**: `boolean`
- Set to `true` to disable rotate completely.
- Default: `false`

**`onRotate`**: `(event: RotateEvent) => void`
- Handler called when the user performs a rotate gesture on this view.

**`onRotatePredicate`**: `(event: RotateEvent) => boolean`
- Predicate to decide whether the rotate gesture should be recognized.

#### Touch Area Extension

**`touchAreaExtension`**: `number`
- Can be used to increase the view's touch target area.

**`touchAreaExtensionTop`**: `number`

**`touchAreaExtensionRight`**: `number`

**`touchAreaExtensionBottom`**: `number`

**`touchAreaExtensionLeft`**: `number`

#### Transform Properties

**`scaleX`**: `number`
- Specifies the horizontal scale component of the affine transformation to be applied to the view.

**`scaleY`**: `number`
- Specifies the vertical scale component of the affine transformation to be applied to the view.

**`rotation`**: `number`
- Specifies the rotation component in angle radians of the affine transformation to be applied to the view.

**`translationX`**: `number`
- Specifies the horizontal translation component of the affine transformation to be applied to the view.
- Note: When the device is in RTL mode, the applied translationX value will be flipped.

**`translationY`**: `number`
- Specifies the vertical translation component of the affine transformation to be applied to the view.

#### Mask Properties

**`maskOpacity`**: `number`
- Set an opacity to use on mask.
- The opacity defines how much the mask should "erase" pixels that match the maskPath.
- Opacity of 1 will make all the pixels matching the path transparent.
- Default: `1`

**`maskPath`**: `GeometricPath`
- Set a geometric path to use as a mask on the given view.
- Pixels that are within the given path will be turned transparent relative to the maskOpacity.

#### Platform-Specific

**`accessibilityId`**: `string`
- Sets the view's accessibility identifier.
- Commonly used to identify UI elements in UI tests.

**`canAlwaysScrollHorizontal`**: `boolean`
- Forces the platform surface method canScroll to always return true for horizontal touch events.
- This property can be used to ensure that any platform-specific gesture handlers can be ignored when a valdi module would capture the event.

**`canAlwaysScrollVertical`**: `boolean`
- Forces the platform surface method canScroll to always return true for vertical touch events.

**`filterTouchesWhenObscured`**: `boolean`
- Optionally set filterTouchesWhenObscured for payment sensitive button on Android.
- Not used for iOS.

#### Styling

**`style`**: `IStyle<View | Layout>`
- Styling object allows setting multiple attributes at once.
- See [View Style Attributes](api-style-attributes.md#view-styles) for a complete list of attributes that can be used in `Style<View>`.

---

## ScrollView

**JSX Element:** `<scroll>`

**iOS Native:** `SCValdiScrollView`  
**Android Native:** `com.snap.valdi.views.ValdiScrollView`

A view that provides scrolling functionality for its children.

### Properties

All properties from [View](#view), except `flexDirection` (which is replaced by `horizontal`), plus:

#### Scroll Event Callbacks

**`onScroll`**: `(event: ScrollEvent) => void`
- Called when the content offset of the scrollview changed.

**`onScrollEnd`**: `(event: ScrollEndEvent) => void`
- Called when the content offset of the scrollview has settled.

**`onDragStart`**: `(event: ScrollEvent) => void`
- Called when the user starts dragging the scroll content.

**`onDragEnding`**: `(event: ScrollDragEndingEvent) => ScrollOffset | undefined`
- Called synchronously when the ScrollView will end dragging.
- The called function can return a scroll offset that should be used to replace the content offset where the scroll view should settle.

**`onDragEnd`**: `(event: ScrollDragEndEvent) => void`
- Called when the ScrollView ended dragging.
- This will be called right after onDragEnding() is called, and will be called asynchronously.
- The scroll view might still be animating its scroll at this point.

**`onContentSizeChange`**: `(event: ContentSizeChangeEvent) => void`
- Called whenever the content size of the scroll view has changed.

#### Scroll Behavior

**`horizontal`**: `boolean`
- When enabled, the scroll view will allow horizontal scrolling instead of vertical.
- Note: this attribute replaces flexDirection.
- When enabled, children will be laid out as if flexDirection="row" was set.
- When disabled, children will be laid out as if flexDirection="column" was set.
- Default: `false`

**`flexDirection`**: `never`
- FlexDirection is not available on scroll views.
- It can be set through the "horizontal" attribute.

**`scrollEnabled`**: `boolean`
- When enabled, the scroll view can be scrolled by the user.
- Default: `true`

**`pagingEnabled`**: `boolean`
- When enabled, the scroll content offset will always settle on a multiple of the scrollview's size.
- Default: `false`

#### Bounce/Overscroll

**`bounces`**: `boolean`
- Configure whether or not there should be a visual effect when the user tries to scroll past the scroll boundaries.
- On iOS: this visually looks like letting the user "bounce" on the edge of the scroll view.
- On Android: this looks like a glow or squish effect that follows the finger or a bounce if using skia.
- Default: `true`

**`bouncesFromDragAtStart`**: `boolean`
- Allow bouncing by dragging even when the content offset is already at the minimum.
- Default: `true`

**`bouncesFromDragAtEnd`**: `boolean`
- Allow bouncing by dragging even when the content offset is already at the maximum.
- Default: `true`

**`bouncesVerticalWithSmallContent`**: `boolean`
- Allow bouncing even when the vertical content size is smaller than the scroll itself.
- Default: `false`

**`bouncesHorizontalWithSmallContent`**: `boolean`
- Allow bouncing even when the horizontal content size is smaller than the scroll itself.
- Default: `false`

#### Touch/Gesture Behavior

**`cancelsTouchesOnScroll`**: `boolean`
- Cancels all active touch gestures (from onTouch) of the scroll view's children when dragging the scroll view content.
- Default: `true`

**`dismissKeyboardOnDrag`**: `boolean`
- If the keyboard is open, close it when we start scrolling.
- Default: `false`

**`dismissKeyboardOnDragMode`**: `'immediate' | 'touch-exit-below'`
- When dismissKeyboardOnDrag is `true`, this changes the behavior of when the keyboard is dismissed.
- `immediate`: keyboard is dismissed as soon as scrolling begins
- `touch-exit-below`: keyboard is dismissed when the scroll drag gesture touches leave the lower boundary of the scroll bounds
- Default: `'immediate'`

#### Visual Indicators

**`showsVerticalScrollIndicator`**: `boolean`
- Shows the scroll indicator when scrolling vertically.
- Default: `true`

**`showsHorizontalScrollIndicator`**: `boolean`
- Shows the scroll indicator when scrolling horizontally.
- Default: `true`

**`fadingEdgeLength`**: `number`
- Create a fade effect on the content of the scroll view when scrolling is possible.
- This attribute can control the size of the effect.
- Default: `0`

#### Viewport Extensions

**`viewportExtensionTop`**: `number`
- Can be used to extend the visible viewport of the scroll element.
- When scrolling, this will cause child elements to be rendered potentially earlier (if positive) or later (if negative).
- Default: `0`

**`viewportExtensionRight`**: `number`
- Extends the visible viewport on the right side.
- Default: `0`

**`viewportExtensionBottom`**: `number`
- Extends the visible viewport on the bottom side.
- Default: `0`

**`viewportExtensionLeft`**: `number`
- Extends the visible viewport on the left side.
- Default: `0`

#### Advanced Features

**`circularRatio`**: `number`
- Enables circular scroll by specifying the ratio between the calculated content size of the scroll element to the size of a single scrollable page.
- For circular scroll to work properly, the children elements must be re-rendered at least twice, in which case `circularRatio` should be set to 2.
- Default: `0` (scroll is not circular)

**`decelerationRate`**: `'normal' | 'fast'`
- [iOS-Only] Defines the rate at which the scroll view decelerates after a fling gesture.
- Default: `'normal'`

**`scrollPerfLoggerBridge`**: `IScrollPerfLoggerBridge`
- Set this to enable scroll performance logging with given attribution parameters.
- To get a value of this type, see `AttributedCallsite.forScrollPerfLogging`

#### Interactive Properties (Programmatic Only)

These properties should not be used in TSX tags, only for programmatic attribute manipulation:

**`contentOffsetX`**: `number`
- Can be programmatically set to change the current horizontal scroll offset.

**`contentOffsetY`**: `number`
- Can be programmatically set to change the current vertical scroll offset.

**`contentOffsetAnimated`**: `boolean`
- When set to true, any programmatic change to the contentOffset will be animated.

**`staticContentWidth`**: `number`
- When set, the scrollable content width will be based on this value for operations like snapping and scrolling.
- This will bypass the automatic measurement.

**`staticContentHeight`**: `number`
- When set, the scrollable content height will be based on this value for operations like snapping and scrolling.
- This will bypass the automatic measurement.

#### Styling

**`style`**: `IStyle<ScrollView | View | Layout>`
- See [ScrollView Style Attributes](api-style-attributes.md#scrollview-styles) for a complete list of attributes that can be used in `Style<ScrollView>`.

---

## ImageView

**JSX Element:** `<image>`

**iOS Native:** `SCValdiImageView`  
**Android Native:** `com.snap.valdi.views.ValdiImageView`

A view for displaying images from local assets or remote URLs.

### Properties

All properties from [Layout Attributes](#layout), plus:

#### Image Source

**`src`**: `string | Asset`
- Specify the image asset or url to be rendered within the image's bounds.
- Can be a URL string or an Asset object.
- Default: `undefined`

#### Image Display

**`objectFit`**: `'fill' | 'contain' | 'cover' | 'none'`
- Define how the image should be resized within the image view's bounds.
- Useful when the aspect ratio of the image bounds may be different than the image asset content's aspect ratio.
- `fill`: stretch the image to fill the image view's bounds
- `contain`: conserve aspect ratio and scale to fit within bounds (can leave blank space)
- `cover`: conserve aspect ratio and scale to fill bounds (can crop parts of the image)
- `none`: conserve aspect ratio and don't scale to fit (just rendered in center)
- Default: `'fill'`

**`tint`**: `Color`
- Apply a color tint on every pixel of the image.
- Default: `undefined`

**`flipOnRtl`**: `boolean`
- When the current layout is in RTL, horizontally-mirror-flip the image's content.
- Default: `false`

#### Content Transform

**`contentScaleX`**: `number`
- Scale horizontally the content of the image within the image's bounds.
- Default: `1`

**`contentScaleY`**: `number`
- Scale vertically the content of the image within the image's bounds.
- Default: `1`

**`contentRotation`**: `number`
- Rotate the content of the image in radians.
- Default: `0`

#### Filters

**`filter`**: `ImageFilter`
- A post processing filter to apply on the image.

#### Callbacks

**`onAssetLoad`**: `(success: boolean, errorMessage?: string) => void`
- Called when the loaded image asset has either successfully loaded or failed to load.

**`onImageDecoded`**: `(width: number, height: number) => void`
- Called when the image has been loaded and we have the dimensions.
- Note: dimensions returned are of the raw image which may be different than the view.

#### View Properties

All view properties from [View](#view) including:
- Lifecycle callbacks (`onViewCreate`, `onViewDestroy`)
- Appearance properties (`backgroundColor`, `opacity`, `border`, etc.)
- Gesture handlers (tap, drag, etc.)
- Transform properties

#### Styling

**`style`**: `IStyle<ImageView | View | Layout>`
- See [ImageView Style Attributes](api-style-attributes.md#imageview-styles) for a complete list of attributes that can be used in `Style<ImageView>`.

---

## WebView

**JSX Element:** `<webview>`

**iOS Native:** `SCValdiWebView`
**Android Native:** `com.snap.valdi.modules.webview.ValdiWebView`

A host element for a native webview controller created by the `valdi_webview` module.

### Properties

All properties from [Layout Attributes](#layout), plus:

#### Controller

**`controller`**: `IWebViewNativeController`
- Native webview controller created by `WebView.createController()` from `valdi_webview`.
- The controller owns the platform webview and is attached to this host element when the attribute is applied.

#### Styling

**`style`**: `IStyle<WebViewElement | View | Layout>`
- See [View Style Attributes](api-style-attributes.md#view-styles) for common view styling attributes.

---

## VideoView

**JSX Element:** `<video>`

**iOS Native:** `SCValdiVideoView`  
**Android Native:** `com.snap.valdi.views.ValdiVideoView`

A view for displaying and controlling video playback.

### Properties

All properties from [Layout Attributes](#layout), plus:

#### Video Source

**`src`**: `string | Asset`
- Specify the video asset or url to be rendered within the view's bounds.
- Default: `undefined`

#### Playback Control

**`volume`**: `number`
- Float between 0 and 1.

**`playbackRate`**: `number`
- `0` is paused
- `1` is playing
- Default: paused

**`seekToTime`**: `number`
- In milliseconds.

#### Callbacks

**`onVideoLoaded`**: `(duration: number) => void`
- A callback to be called when the video is loaded.
- Hands back the length of the video in milliseconds.

**`onBeginPlaying`**: `() => void`
- Callback for when the video begins playing.

**`onError`**: `(error: string) => void`
- Callback to be called when there's an error.

**`onCompleted`**: `() => void`
- Callback to be called when the video completes.

**`onProgressUpdated`**: `(time: number, duration: number) => void`
- Callback called when the video progress updates.
- `time` is in milliseconds
- `duration` is in milliseconds
- Frequency may differ from platform to platform.

#### View Properties

All view properties from [View](#view).

#### Styling

**`style`**: `IStyle<VideoView | View | Layout>`
- See [VideoView Style Attributes](api-style-attributes.md#videoview-styles) for a complete list of attributes that can be used in `Style<VideoView>`.

---

## Label

**JSX Element:** `<label>`

**iOS Native:** `SCValdiLabel`  
**Android Native:** `android.widget.TextView`

A view for displaying text.

### Properties

All properties from [Layout Attributes](#layout), plus:

#### Text Content

**`value`**: `string | AttributedText`
- The text value of the label.
- Use the `AttributedTextBuilder` class to set a value composed of multiple strings with different text attributes.

#### Text Styling

**`font`**: `string`
- Set the font used to render the text characters.
- This attribute must be a string with 4 parts separated by space:
  1. required: the name of the font (including the font's weight)
  2. required: the size of the font
  3. optional: the scaling type (or 'unscaled' for no scaling)
  4. optional: the maximum size of the font after scaling
- Example: `'Montserrat-Bold 16 unscaled 16'`
- Default: `'system 12'`

**`color`**: `Color`
- Set the rendering color for the text characters.
- Default: `black`

**`textGradient`**: `string`
- Set the Gradient color for the text string. Overrides color setting.
- Use `"linear-gradient(color1, color2, color3...)"` for evenly-spaced stops.
- Use `"linear-gradient(color1 stop1, color2 stop2, color3 stop3...)"` for custom stops.

**`textShadow`**: `string`
- Add a shadow to the text.
- Syntax: `'{color} {radius} {opacity} {offsetX} {offsetY}'`
- All number values are interpreted as points.
- Note: TextShadow and BoxShadow are mutually exclusive.
- Example: `'rgba(0, 0, 0, 0.1) 1 1 1 1'`

#### Text Layout

**`numberOfLines`**: `number`
- This property controls the maximum number of lines to use to fit the label's text into its bounding rectangle.
- To remove any maximum limit, and use as many lines as needed, set the value to 0.
- Default: `1`

**`textAlign`**: `'left' | 'right' | 'center' | 'justified'`
- Set the horizontal alignment behavior for the label's text.
- Note: it will automatically invert in RTL locales:
  - In RTL locales, "left" will align text to the right of the label's bounds
  - In RTL locales, "right" will align text to the left of the label's bounds
- Default: `'left'`

**`textDecoration`**: `'none' | 'strikethrough' | 'underline' | 'dashed-underline' | 'dotted-underline'`
- Optionally adds a visual decoration effect to the label's text.
- Default: `undefined`

**`lineHeight`**: `number`
- Explicit rendering size of each line of the label, in points.
- When both `lineHeight` and `lineHeightMultiple` are provided, `lineHeight` takes precedence.
- Default: `undefined`

**`lineHeightMultiple`**: `number`
- Rendering size of each line of the label, this value is a ratio of the font height.
- If the `lineHeightMultiple` value is above 1, spacing is added on top of each line of the text.
- Example: A value of 2 will double the height of each line.
- Default: `1`

**`letterSpacing`**: `number`
- Extra spacing added at the end of each character, in points.
- Note: negative values will shrink the space between characters.
- Default: `0`

**`textOverflow`**: `'ellipsis' | 'clip'`
- Controls how hidden text overflow content is signaled to users.
- Default: `'ellipsis'`

#### Text Selection

**`selectable`**: `boolean`
- Enables text selection and copy support for the label.
- Default: `false`

**`selection`**: `[number, number]`
- First index for start of selection.
- Second index for end of selection.
- Set both to the same value to place the selection at a single position.

**`onSelectionChange`**: `(event: EditTextEvent) => void`
- Callback called when the label selection is changed.
- The event contains the current text value and selected indexes.

#### Auto-sizing

**`adjustsFontSizeToFitWidth`**: `boolean`
- If this property is `true`, and the text exceeds the label's bounding rectangle, the label reduces the font size until the text fits or it has scaled down to the minimum font size.
- If you set this to `true`, be sure to also set an appropriate minimum font scale with `minimumScaleFactor`.
- Notes:
  - This autoshrinking behavior is mostly intended for use with a single-line label when numberOfLines == 1.
  - This will also work when for multiline using numberOfLines > 0 but the behavior will be much less predictable.
  - Specifying the label's width/height/minHeight will make the auto-shrink behavior much more predictable.
- Default: `false`

**`minimumScaleFactor`**: `number`
- If `adjustsFontSizeToFitWidth` is `true`, use this property to specify the smallest multiplier for the current font size that yields an acceptable font size for the label's text.
- Default: `0`

#### View Properties

All view properties from [View](#view).

#### Styling

**`style`**: `IStyle<Label | View | Layout>`
- See [Label Style Attributes](api-style-attributes.md#label-styles) for a complete list of attributes that can be used in `Style<Label>`.

---

## TextField

**JSX Element:** `<textfield>`

**iOS Native:** `SCValdiTextField`  
**Android Native:** `com.snap.valdi.views.ValditText`

A single-line text input field.

### Properties

All properties from [Layout Attributes](#layout), plus:

#### Text Content & Styling

All text properties from [Label](#label) including `value`, `font`, `color`, `textGradient`, `textShadow`.

#### Text Input Configuration

**`placeholder`**: `string`
- The string that is displayed when there is no other text in the text field.

**`placeholderColor`**: `Color`
- Sets the foreground color of the placeholder text.

**`tintColor`**: `Color`
- [iOS-Only] Setting the tintColor will change the color of the editing caret and drag handles of the text field.

**`textAlign`**: `'left' | 'center' | 'right'`
- Set the text alignment within the typing box of the text input.
- Default: `'left'`

**`enabled`**: `boolean`
- Set whether the text input is editable or not.
- Default: `true`

#### Keyboard Configuration

**`contentType`**: `'default' | 'phoneNumber' | 'email' | 'password' | 'url' | 'number' | 'numberDecimal' | 'numberDecimalSigned' | 'passwordNumber' | 'passwordVisible' | 'noSuggestions'`
- The content type identifies what keyboard keys and capabilities are available on the input and which ones appear by default.
- Default: `'default'`

**`returnKeyText`**: `'done' | 'go' | 'join' | 'next' | 'search' | 'send' | 'continue'`
- Setting this property changes the visible title of the Return key.
- Note: This might not actually be text, e.g. on Android 10 setting this to 'search' will make the return key display a search glass icon.
- Default: `'done'`

**`autocapitalization`**: `'sentences' | 'words' | 'characters' | 'none'`
- Determines at what times the Shift key is automatically pressed, thereby making the typed character a capital letter.
- Default: `'sentences'`

**`autocorrection`**: `'default' | 'none'`
- Determines whether autocorrection is enabled or disabled during typing.
- Default: `'default'`

**`keyboardAppearance`**: `'default' | 'dark' | 'light'`
- [iOS-Only] Force keyboard appearance to dark/light (ignoring the current system appearance).

**`enableInlinePredictions`**: `boolean`
- Whether to enable inline predictions for the text input.
- Note: This is only relevant on iOS. No-op if used on Android.
- Default: `false`

#### Text Editing Behavior

**`characterLimit`**: `number`
- Determines the maximum number of characters allowed to be entered into the text field.
- Note: an undefined or negative value means there is no limit.
- Default: `undefined`

**`selectTextOnFocus`**: `boolean`
- Whether to select the contents of the textfield when it becomes focused.
- Default: `false`

**`closesWhenReturnKeyPressed`**: `boolean`
- Dismiss keyboard when the return key is pressed.
- Is enabled by default.
- Default for textfield elements: `true`
- Default for textview elements: `false`

#### Selection

**`selectable`**: `boolean`
- Allows users to select and copy text input text through the native text selection UI when the text input is not editable.
- Default: `true`

**`selection`**: `[number, number]`
- Selection for the text field.
- First index for start of selection
- Second index for end of selection
- Set both to the same to select at a single position

#### Callbacks

**`onChange`**: `(event: EditTextEvent) => void`
- Callback that will get called whenever the user changes the text value of the text field.
- The event parameter contains the current text value of the text field.

**`onWillChange`**: `(event: EditTextEvent) => EditTextEvent | undefined`
- Callback called when the text value is about to change.
- The event parameter contains the current text value before any change.
- Must return the new text value expected to replace the last text value.

**`onEditBegin`**: `(event: EditTextBeginEvent) => void`
- Callback called when the user starts editing the textfield (i.e. when the text field becomes focused).
- The event parameter contains the current text value of the text field.

**`onEditEnd`**: `(event: EditTextEndEvent) => void`
- Callback called when the user stops editing the textfield (i.e. when the text field stops being focused).
- The event parameter contains the current text value and the reason for ending.

**`onReturn`**: `(event: EditTextEvent) => void`
- Callback called when the user taps the return key.
- The event parameter contains the current text value of the text field.

**`onWillDelete`**: `(event: EditTextEvent) => void`
- Callback called when the user presses the delete key on the keyboard.
- The event parameter contains the current text value before characters are deleted.
- Note: on android some soft-keyboard may only send this when pressing delete on an empty textfield.

**`onSelectionChange`**: `(event: EditTextEvent) => void`
- Callback called when the selection is changed.
- The event parameter contains the current text value and the selected indexes.

#### Interactive Properties (Programmatic Only)

**`focused`**: `boolean`
- Can be programmatically set to change the current focus state of the textfield.
- Changing this to true will make this textfield become the currently edited one.
- Example:
  - On iOS this will make the view request/resign the first responder
  - On Android this will make the view become focused/unfocused and open/close the keyboard

#### View Properties

All view properties from [View](#view).

#### Styling

**`style`**: `IStyle<TextField | View | Layout>`
- See [TextField Style Attributes](api-style-attributes.md#textfield-styles) for a complete list of attributes that can be used in `Style<TextField>`.

---

## TextView

**JSX Element:** `<textview>`

**iOS Native:** `SCValdiTextView`  
**Android Native:** `com.snap.valdi.views.ValdiEditTextMultiline`

A multi-line text input field.

### Properties

All properties from [TextField](#textfield), except `returnKeyText` is replaced with `returnType`, plus:

#### Return Key Configuration

**`returnType`**: `'linereturn' | 'done' | 'go' | 'join' | 'next' | 'search' | 'send' | 'continue'`
- Setting this property changes the visible title of the Return key.
- Setting this property will also impact the behavior of the return key.
- `linereturn` will let users add line returns, but any other value will be constrained to single line text.
- Consider using this attribute in combination with `closesWhenReturnKeyPressed`.
- Default: `'linereturn'`

#### Text Gravity

**`textGravity`**: `'top' | 'center' | 'bottom'`
- Set the text gravity/alignment vertically within the text view.
- Default: `'center'`

#### Text Rendering

**`numberOfLines`**: `number`
- Controls the maximum number of visible text lines.
- Set to `0` to remove the limit and allow as many lines as needed.
- Default: `0`

**`textOverflow`**: `'ellipsis' | 'clip'`
- Controls how hidden text overflow content is signaled to users.

**`textDecoration`**: `'none' | 'strikethrough' | 'underline' | 'dashed-underline' | 'dotted-underline'`
- Optionally adds a visual decoration effect to the text view's text.

**`lineHeight`**: `number`
- Explicit rendering size of each line of the text view, in points.
- When both `lineHeight` and `lineHeightMultiple` are provided, `lineHeight` takes precedence.

**`lineHeightMultiple`**: `number`
- Rendering size of each line of the text view as a multiple of the font height.
- Default: `1`

**`customUnderlineStyle`**: `string`
- Overrides underline drawing geometry for underlined text ranges.
- Format: `"height onWidth offWidth offset"`.

**`textGradient`**: `string`
- Sets a gradient color for the text string.

#### Text Selection

**`selectable`**: `boolean`
- Allows users to select and copy text view text through the native text selection UI.
- Default: `true`

#### Background Effect

**`backgroundEffectColor`**: `Color`
- Set the color for the background effect of the text view.
- This background is drawn behind each line fragment of the text, wrapping each line together making a cohesive background that follows the text shape.
- Default: `clear`

**`backgroundEffectBorderRadius`**: `number`
- Set the background effect border radius.
- Default: `0`

**`backgroundEffectPadding`**: `number`
- Set the background effect padding.
- This padding is applied only around the exterior edge of the text as a whole (it does not apply to the space between lines of text).
- Default: `0`

#### Interactive Properties (Programmatic Only)

**`focused`**: `boolean`
- Can be programmatically set to change the current focus state of the textview.

#### Styling

**`style`**: `IStyle<TextView | View | Layout>`
- See [TextView Style Attributes](api-style-attributes.md#textview-styles) for a complete list of attributes that can be used in `Style<TextView>`.

---

## BlurView

**JSX Element:** `<blur>`

**iOS Native:** `SCValdiBlurView`  
**Android Native:** `com.snap.valdi.views.ValdiView`

A view that applies a blur effect to content behind it.

### Properties

All properties from [Layout Attributes](#layout) and [View Attributes](#view) (appearance and gestures), plus:

#### Blur Configuration

**`blurStyle`**: `BlurStyle`
- The style of blur effect to apply.
- Possible values:
  - `'light'`
  - `'dark'`
  - `'extraLight'`
  - `'regular'`
  - `'prominent'`
  - `'systemUltraThinMaterial'`
  - `'systemThinMaterial'`
  - `'systemMaterial'`
  - `'systemThickMaterial'`
  - `'systemChromeMaterial'`
  - `'systemUltraThinMaterialLight'`
  - `'systemThinMaterialLight'`
  - `'systemMaterialLight'`
  - `'systemThickMaterialLight'`
  - `'systemChromeMaterialLight'`
  - `'systemUltraThinMaterialDark'`
  - `'systemThinMaterialDark'`
  - `'systemMaterialDark'`
  - `'systemThickMaterialDark'`
  - `'systemChromeMaterialDark'`

#### Container Properties

BlurView extends ContainerTemplateElement, so it can have children.

#### Styling

**`style`**: `IStyle<BlurView | View | Layout>`
- See [BlurView Style Attributes](api-style-attributes.md#blurview-styles) for a complete list of attributes that can be used in `Style<BlurView>`.

---

## SpinnerView

**JSX Element:** `<spinner>`

**iOS Native:** `SCValdiSpinnerView`  
**Android Native:** `com.snap.valdi.views.ValdiSpinnerView`

A view that displays a loading spinner.

### Properties

All properties from [Layout Attributes](#layout) and [View](#view), plus:

#### Spinner Configuration

**`color`**: `Color`
- Color of the spinning shape.

#### Styling

**`style`**: `IStyle<SpinnerView | View | Layout>`
- See [SpinnerView Style Attributes](api-style-attributes.md#spinnerview-styles) for a complete list of attributes that can be used in `Style<SpinnerView>`.

---

## ShapeView

**JSX Element:** `<shape>`

**iOS Native:** `SCValdiShapeView`  
**Android Native:** `com.snap.valdi.views.ShapeView`

A view for drawing custom shapes using geometric paths.

### Properties

All properties from [Layout Attributes](#layout) and [View](#view), plus:

#### Shape Configuration

**`path`**: `GeometricPath`
- The GeometricPath object representing the path to draw.

#### Stroke Properties

**`strokeWidth`**: `number`
- Defines the thickness of the shape's stroked path.

**`strokeColor`**: `Color`
- The color that the stroked path is drawn with.

**`strokeCap`**: `'butt' | 'round' | 'square'`
- The stroke cap specifies the shape of the endpoints of an open path when stroked.

**`strokeJoin`**: `'bevel' | 'miter' | 'round'`
- The stroke join specifies the shape of the joints between connected segments of a stroked path.

**`strokeStart`**: `number`
- The relative location at which to begin stroking the path.
- Default value is 0. Animatable.

**`strokeEnd`**: `number`
- The relative location at which to stop stroking the path.
- Default value is 1. Animatable.

#### Fill Properties

**`fillColor`**: `Color`
- The color that the shape's enclosed area is filled with.

#### Styling

**`style`**: `IStyle<ShapeView | View | Layout>`
- See [ShapeView Style Attributes](api-style-attributes.md#shapeview-styles) for a complete list of attributes that can be used in `Style<ShapeView>`.

---

## AnimatedImage

**JSX Element:** `<animatedimage>`

**iOS Native:** `SCValdiAnimatedContentView`  
**Android Native:** `com.snap.valdi.views.AnimatedImageView`

A view for displaying animated images including Lottie animations, animated WebP, and other formats.

### Properties

All properties from [Layout Attributes](#layout) and [View](#view), plus:

#### Image Source

**`src`**: `string | Asset`
- Specify the image asset or url to be rendered within the view's bounds.
- Default: `undefined` (nothing rendered by default)

#### Animation Control

**`loop`**: `boolean`
- Specify whether the image animation should loop back to the beginning when reaching the end.

**`advanceRate`**: `number`
- Sets the speed ratio at which the image animation should run.
- `0` is paused
- `1` means the animation runs at normal speed
- `0.5` at half speed
- `-1` the animation will run in reverse
- Default: `0`

**`currentTime`**: `number`
- Set the current time in seconds for the image animation as an offset from animationStartTime.

**`animationStartTime`**: `number`
- Set the start time in seconds for the image animation.
- Animation will be clamped between start and end time.
- Default: `0`

**`animationEndTime`**: `number`
- Set the end time in seconds for the image animation.
- Animation will be clamped between start and end time.
- Default: animation duration

#### Display Configuration

**`objectFit`**: `'fill' | 'contain' | 'cover' | 'none'`
- Define how the animatedimage should be resized within the view's bounds.
- Note that the default is different from the image element.
- `fill`: stretch the image to fill the view's bounds
- `contain`: conserve aspect ratio and scale to fit within bounds (can leave blank space)
- `cover`: conserve aspect ratio and scale to fill bounds (can crop parts)
- `none`: conserve aspect ratio and don't scale to fit (just rendered in center)
- Default: `'contain'`

#### Font Provider

**`fontProvider`**: `IFontProvider`
- Set a font provider that the lottie element will use to resolve fonts within its scenes.

#### Callbacks

**`onAssetLoad`**: `(success: boolean, errorMessage?: string) => void`
- Called when the loaded animatedimage asset has either successfully loaded or failed to load.

**`onImageDecoded`**: `(width: number, height: number) => void`
- Called when the animatedimage has been loaded and we have the dimensions.
- Note: dimensions returned are of the raw image which may be different than the view.

**`onProgress`**: `(event: AnimatedImageOnProgressEvent) => void`
- Called when the animation progresses.
- Event contains:
  - `time`: number (current time in seconds)
  - `duration`: number (duration of the animation in seconds)

#### Styling

**`style`**: `IStyle<AnimatedImage | View | Layout>`
- See [AnimatedImage Style Attributes](api-style-attributes.md#animatedimage-styles) for a complete list of attributes that can be used in `Style<AnimatedImage>`.

---

## Slot

**JSX Element:** `<slot>`

Slots are used for content projection, allowing a component to define placeholders that can be filled with content.

### Properties

**`key`**: `string`
- A key to uniquely identify the element.
- If not provided, the framework will generate one based on the index.

**`name`**: `string`
- The unique name of the Slot.
- If not provided, it would be considered to be named 'default'.

**`ref`**: `IRenderedElementHolder<unknown>`
- Sets an element reference holder, which will keep track of the elements rendered at the root of this slot.

---

## Common Types

### EditTextEvent

Interface for text editing events.

```typescript
interface EditTextEvent {
  text: string;
  selectionStart: number;
  selectionEnd: number;
}
```

### EditTextBeginEvent

Extends `EditTextEvent` with no additional properties.

### EditTextEndEvent

```typescript
interface EditTextEndEvent extends EditTextEvent {
  reason: EditTextUnfocusReason;
}
```

### EditTextUnfocusReason

```typescript
enum EditTextUnfocusReason {
  Unknown = 0,
  ReturnKeyPress = 1,
  DismissKeyPress = 2,
}
```

### MeasureMode

```typescript
enum MeasureMode {
  Unspecified = 0,
  Exactly = 1,
  AtMost = 2,
}
```

### MeasuredSize

```typescript
type MeasuredSize = [number, number];
```

### OnMeasureFunc

```typescript
type OnMeasureFunc = (
  width: number,
  widthMode: MeasureMode,
  height: number,
  heightMode: MeasureMode,
) => MeasuredSize;
```

### CSSValue

```typescript
type CSSValue = string | number;
```

### Color

```typescript
type Color = string;
```

### LabelValue

```typescript
type LabelValue = string | AttributedText;
```

### Gesture Types

#### GestureHandler

```typescript
type GestureHandler<Event> = (event: Event) => void;
```

#### GesturePredicate

```typescript
type GesturePredicate<Event> = (event: Event) => boolean;
```

### Callback Types

#### ImageOnAssetLoadCallback

```typescript
type ImageOnAssetLoadCallback = (success: boolean, errorMessage?: string) => void;
```

#### ImageOnImageDecodedCallback

```typescript
type ImageOnImageDecodedCallback = (width: number, height: number) => void;
```

#### AnimatedImageOnProgressCallback

```typescript
type AnimatedImageOnProgressCallback = (event: AnimatedImageOnProgressEvent) => void;
```

### Layout Enums

#### LayoutAccessibilityCategory

```typescript
type LayoutAccessibilityCategory =
  | 'auto'
  | 'view'
  | 'text'
  | 'button'
  | 'image'
  | 'image-button'
  | 'input'
  | 'header'
  | 'link'
  | 'checkbox'
  | 'radio'
  | 'keyboard-key';
```

#### LayoutAccessibilityNavigation

```typescript
type LayoutAccessibilityNavigation = 
  | 'auto' 
  | 'passthrough' 
  | 'leaf' 
  | 'cover' 
  | 'group' 
  | 'ignored';
```

#### Label Enums

```typescript
type LabelTextDecoration = 'none' | 'strikethrough' | 'underline' | 'dashed-underline' | 'dotted-underline';
type LabelTextAlign = 'left' | 'right' | 'center' | 'justified';
type LabelFontWeight = 'light' | 'normal' | 'medium' | 'demi-bold' | 'bold' | 'black';
type LabelFontStyle = 'normal' | 'italic';
```

#### Image Enums

```typescript
type ImageObjectFit = 'fill' | 'contain' | 'cover' | 'none';
```

#### TextField Enums

```typescript
type TextFieldAutocapitalization = 'sentences' | 'words' | 'characters' | 'none';
type TextFieldAutocorrection = 'default' | 'none';
type TextFieldTextAlign = 'left' | 'center' | 'right';
type TextFieldReturnKeyText = 'done' | 'go' | 'join' | 'next' | 'search' | 'send' | 'continue';
type TextFieldKeyboardAppearance = 'default' | 'dark' | 'light';
type TextFieldContentType =
  | 'default'
  | 'phoneNumber'
  | 'email'
  | 'password'
  | 'url'
  | 'number'
  | 'numberDecimal'
  | 'numberDecimalSigned'
  | 'passwordNumber'
  | 'passwordVisible'
  | 'noSuggestions';
```

#### TextView Enums

```typescript
type TextViewReturnType = 'linereturn' | TextFieldReturnKeyText;
type TextViewTextGravity = 'top' | 'center' | 'bottom';
```

#### Shape Enums

```typescript
type ShapeStrokeCap = 'butt' | 'round' | 'square';
type ShapeStrokeJoin = 'bevel' | 'miter' | 'round';
```

#### BlurStyle

```typescript
type BlurStyle =
  | 'light'
  | 'dark'
  | 'extraLight'
  | 'regular'
  | 'prominent'
  | 'systemUltraThinMaterial'
  | 'systemThinMaterial'
  | 'systemMaterial'
  | 'systemThickMaterial'
  | 'systemChromeMaterial'
  | 'systemUltraThinMaterialLight'
  | 'systemThinMaterialLight'
  | 'systemMaterialLight'
  | 'systemThickMaterialLight'
  | 'systemChromeMaterialLight'
  | 'systemUltraThinMaterialDark'
  | 'systemThinMaterialDark'
  | 'systemMaterialDark'
  | 'systemThickMaterialDark'
  | 'systemChromeMaterialDark';
```

---

## Usage Examples

### Basic Layout

```tsx
<layout width="100%" height={200} padding={10} flexDirection="row">
  <view backgroundColor="red" flex={1} margin={5} />
  <view backgroundColor="blue" flex={1} margin={5} />
</layout>
```

### ScrollView with Images

```tsx
<scroll height={300} width="100%" horizontal={false}>
  <layout padding={10}>
    <image src={res.myImage} width={200} height={200} margin={10} />
    <label value="My Image" textAlign="center" />
  </layout>
</scroll>
```

### TextField with Callbacks

```tsx
<textfield
  value={this.state.text}
  placeholder="Enter your name"
  placeholderColor="#888"
  contentType="default"
  returnKeyText="done"
  onChange={(event) => {
    this.setState({ text: event.text });
  }}
  onEditEnd={(event) => {
    console.log('Editing ended:', event.text);
  }}
/>
```

### Shape Drawing

```tsx
<shape
  path={myGeometricPath}
  strokeWidth={2}
  strokeColor="blue"
  fillColor="lightblue"
  strokeCap="round"
  strokeJoin="round"
  width={100}
  height={100}
/>
```

### AnimatedImage (Lottie)

```tsx
<animatedimage
  src={res.myLottieAnimation}
  loop={true}
  advanceRate={1}
  width={200}
  height={200}
  objectFit="contain"
  onProgress={(event) => {
    console.log(`Progress: ${event.time}/${event.duration}`);
  }}
/>
```

---

## See Also

- [Core Views Guide](../docs/core-views.md) - Practical guide to using layouts and views
- [Core Images Guide](../docs/core-images.md) - Image handling and assets
- [Core Scrolls Guide](../docs/core-scrolls.md) - ScrollView usage patterns
- [Core Text Guide](../docs/core-text.md) - Working with text and inputs
- [Core Flexbox](../docs/core-flexbox.md) - Understanding flexbox layout
- [Advanced Animations](../docs/advanced-animations.md) - Animating properties
