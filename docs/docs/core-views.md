# The `<layout>` and `<view>`

## Understanding `<layout>` and `<view>`

Layout and Views are the basic building blocks of any Valdi feature.

`<layout>` is an invisible rectangular container that is used to configure the layout of the elements within it. It does not emit a resulting platform view (UIView/android.View) instance.

`<view>` is the main element for building a UI which emits an actual native platform view (`UIView`/`android.View`) instance that can be rendered.

![Sketch of layout and view dimensions](./img/docs/core-views-example.png)


Here is a quick comparison of their feature set.

**What are they:**
- `<layout>` is a simple invisible rectangular container
- `<view>` is a simple rendered rectangular container

**What they can do:**
- a `<layout>` can only apply the flex layout
- a `<view>` can: render a background color, render a border, receive tap events

**What do they do:**
- each `<layout>` is invisible and lives only in memory of the Valdi runtime
- each `<view>` generates a native view (depending on the platform)

**Advanced Attributes:**
- `extendViewportWithChildren`: Impacts how the runtime treats the node in a specific update pass.
- `zIndex`: Controls the rendering order of overlapping elements.

**What they feel like:**
- a `<layout>` is the most basic building block
- a `<view>` is a `<layout>` that also generate a native view in its frame

**Also useful to know:**
- a `<label>` is a `<view>` that also renders text
- a `<image>` is a `<view>` that also renders images
- a `<spinner>` is a `<view>` that also renders a loading indicator
- a `<scroll>` is a `<view>` whose children will be scrollable inside of it
- a `<glass>` is a `<view>` that renders an iOS "Liquid Glass" material behind its children (**iOS only**, see below)

You will be able to learn how to use `<scroll>`, `<image>` and `<slot>` in following dedicated pages

## The `<glass>` element (iOS only)

`<glass>` renders Apple's "Liquid Glass" material (introduced in iOS 26) behind its
children. It behaves like a `<view>` that also draws a translucent, adaptive glass
material, and it accepts children the same way a `<view>` does.

```tsx
<glass glassStyle='regular' borderRadius={24}>
  <label value='Floating panel' />
</glass>
```

### Platform behavior

`<glass>` is **iOS only**. There is no Android equivalent for `UIGlassEffect`, so:

| Platform | Result |
| --- | --- |
| iOS 26+ | Real Liquid Glass material |
| iOS < 26 | Falls back to a `UIBlurEffect` material, so the surface still reads as a translucent panel with no code changes |
| Android | Renders a plain container (children show, no material, no crash), exactly like `<blur>` |
| Web / snap_drawing surfaces | Plain container (no backdrop material support) |

If you need a specific look on Android or older iOS, branch in TypeScript and render
a translucent `<view>` fallback:

```tsx
if (Device.isIOS() && parseInt(Device.getSystemVersion(), 10) >= 26) {
  <glass glassStyle='regular' borderRadius={24}>{/* ... */}</glass>;
} else {
  <view backgroundColor='rgba(30, 30, 30, 0.85)' borderRadius={24}>{/* ... */}</view>;
}
```

### Attributes

| Attribute | Type | Notes |
| --- | --- | --- |
| `glassStyle` | `'regular' \| 'clear'` | The material style. Defaults to `regular`. |
| `glassTintColor` | `Color` | Tints the glass material. Use this instead of `backgroundColor` to tint. |
| `interactive` | `boolean` | When `true`, the glass reacts to touches with the interactive Liquid Glass animation. Defaults to `false`. |

All three attributes are iOS 26+ only and are silently ignored on Android and older iOS.

### Corner radius

Use `borderRadius` as usual. On iOS 26 `<glass>` applies it through the native
`cornerConfiguration` API rather than a clip mask, so the whole material rounds
together (backdrop, tint, and the light-bending rim), and the tinted and
background-filled cases round the same as a plain glass panel. A percentage radius
(for example `borderRadius: '50%'`) resolves to a native capsule/pill. On the pre-26
blur fallback, corners use the same shape-layer mask as `<blur>`.

You do **not** need `slowClipping` to round `<glass>`. `slowClipping` only clips
overflowing *child* content to the corners (at a performance cost), and the material
itself is already rounded by `borderRadius`.

### Interaction with `backgroundColor` and `background`

`<glass>` inherits the base `<view>` attributes, but note: setting `backgroundColor`
(or a gradient `background`) paints an opaque layer that sits *over* the sampled
backdrop and muddies or defeats the glass material. This matches how `<blur>` behaves
today. To tint the material, use `glassTintColor`, not `backgroundColor`.

## Performances considerations

### Real views

When using `<view>`/`<image>`/`<label>`/`<scroll>`, it will translate to an actual:

- `UIView` (and proper subclass) on iOS
- `View` (and proper subclass) on Android

This is necessary to render pixels on the screen.

However those are the most costly elements in the Valdi framework.

It is then important to always try to minimize the number of total "Real views" being rendered on a screen

### Faster in-memory only element: the `<layout>`

That's where the `<layout>` comes to the rescue,
sometimes when manipulating the layout of a page or feature,
it is useful to use wrapping elements and container elements to manipulate the flexbox layout.

In this case it would be preferable to use `<layout>` instead
because those are extremely cheap:

- they will be taken into account when computing the flexbox layout
- they will NOT generate any view/layer/drawable
- they will NOT even be sent to the android/iOS code

### Key Takeaways

- 1) Always try to use layout over views when possible.
- 2) For visible elements, views are necessary, and that's ok.

## Complete API Reference

For a comprehensive list of all properties and methods available on `<layout>` and `<view>` elements, see the [API Reference](../api/api-reference-elements.md).
