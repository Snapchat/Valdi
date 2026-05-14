# Valdi Gestures

Touch and gesture handling: tap, long press, drag, pinch, and rotate.

## When to use

When handling user touch interactions beyond basic `onTap` — drag-to-reorder, pinch-to-zoom, long press menus, custom gesture recognition.

## Key concepts

Valdi gesture handling is **attribute-based on `<view>` elements**. Each gesture type has three attributes:
- `on<Gesture>` — the callback
- `<gesture>Enabled` — boolean to enable/disable (default: true when callback is set)
- `on<Gesture>Predicate` — function returning boolean, controls whether the gesture should fire

The topmost visible element with a matching gesture wins. Conflicting gestures on parent elements are suppressed.

## TouchEvent interface

All gesture callbacks receive a `TouchEvent` (or extension of it):

```tsx
interface TouchEvent {
  x: number;          // X relative to the element
  y: number;          // Y relative to the element
  absoluteX: number;  // X relative to root Valdi view
  absoluteY: number;  // Y relative to root Valdi view
  pointerCount: number;
  pointerLocations: Pointer[];
}
```

## Basic gestures

### onTap

```tsx
// ✅ Simple tap handler
<view onTap={this.handleTap}>
  <label value="Tap me" />
</view>

private handleTap = (event: TouchEvent) => {
  console.log(`Tapped at ${event.x}, ${event.y}`);
};
```

### onDoubleTap

```tsx
// ✅ Double tap to zoom
<view onDoubleTap={this.handleDoubleTap}>
  <image src={this.viewModel.photo} />
</view>
```

### onLongPress

```tsx
// ✅ Long press with custom duration
<view
  onLongPress={this.handleLongPress}
  longPressDuration={500}  // milliseconds before trigger
>
  <label value="Hold me" />
</view>
```

## Continuous gestures

### onDrag

```tsx
// ✅ Drag to move — fires continuously during the drag
<view onDrag={this.handleDrag}>
  <label value="Drag me" />
</view>

private handleDrag = (event: DragEvent) => {
  // event extends TouchEvent with drag-specific data
  this.cardRef.setAttribute('translateX', event.x);
  this.cardRef.setAttribute('translateY', event.y);
};
```

### onPinch

```tsx
// ✅ Pinch to zoom — requires 2+ pointers
<view onPinch={this.handlePinch}>
  <image src={this.viewModel.photo} ref={this.imageRef} />
</view>

private handlePinch = (event: PinchEvent) => {
  this.imageRef.setAttribute('scale', event.scale);
};
```

### onRotate

```tsx
// ✅ Rotate with two fingers — requires 2+ pointers
<view onRotate={this.handleRotate}>
  <image src={this.viewModel.sticker} ref={this.stickerRef} />
</view>
```

## Simultaneous gestures

`onDrag`, `onPinch`, and `onRotate` can run simultaneously. This enables multitouch interactions like pinch-zoom-rotate on a photo:

```tsx
// ✅ Combined pinch + drag + rotate on same element
<view
  onDrag={this.handleDrag}
  onPinch={this.handlePinch}
  onRotate={this.handleRotate}
>
  <image src={this.viewModel.photo} ref={this.photoRef} />
</view>
```

## Touch area extension

Expand the tappable area beyond the visible bounds:

```tsx
// ✅ Small button with larger touch target
<view
  onTap={this.handleTap}
  touchAreaExtension={10}  // extends hit area by 10pt in each direction
  width={24} height={24}
>
  <image src={SIGIcon.checkmarkFill} />
</view>
```

## Touch delay in scroll views

Inside a `<scroll>`, touches are delayed to avoid conflicts with scrolling:

```tsx
// ✅ Delay touch recognition inside scrollable content
<view
  onTouch={this.handleTouch}
  onTouchDelayDuration={150}  // ms before onTouch fires during scroll
/>
```

## Gesture predicates

Control whether a gesture should activate:

```tsx
// ✅ Only allow drag when in edit mode
<view
  onDrag={this.handleDrag}
  onDragPredicate={() => this.state.editMode}
/>
```

## onTouch (raw)

Low-level touch events that fire on all elements in the hierarchy:

```tsx
// ✅ Raw touch tracking — fires for all touch phases
<view onTouch={this.handleTouch}>
  <label value="Touch me" />
</view>

private handleTouch = (event: TouchEvent) => {
  // Fires on begin, move, end, cancel
};
```

Unlike other gestures, `onTouch` propagates to parents/siblings — it doesn't suppress other handlers.

## Common mistakes

```tsx
// ❌ WRONG — React-style event handlers
<div onClick={handler} />        // ❌ No <div>, no onClick
<button onMouseDown={handler} /> // ❌ No mouse events

// ❌ WRONG — addEventListener
element.addEventListener('touchstart', handler);  // ❌

// ❌ WRONG — Gesture on <layout> (use <view>)
<layout onTap={handler} />  // ❌ Gestures go on <view> elements

// ✅ Correct — gestures on <view>
<view onTap={handler}>
  <layout>
    <label value="Content" />
  </layout>
</view>
```
