# Valdi Animations

Imperative animation API for animating element attributes and state changes.

## When to use

When animating layout changes, view attributes, or state transitions. Almost all element attributes can be animated in Valdi.

## Key concepts

Valdi uses an **imperative animation model** inspired by iOS's `animateWithDuration:animations:`. You call `this.animate()` or `this.animatePromise()` with options and a block of mutations — all changes inside the block are animated automatically.

**Not CSS transitions.** There is no `transition` property, no `@keyframes`, no `animation-duration`. Animations are triggered programmatically.

## animate and animatePromise

Both methods are available on any `Component`:

```tsx
// ✅ Fire-and-forget animation
this.animate({ duration: 0.3 }, () => {
  this.myLabel.setAttribute('font', 'title');
  this.myContainer.setAttribute('width', 200);
});

// ✅ Wait for animation to complete
await this.animatePromise({ duration: 0.5, curve: AnimationCurve.EaseOut }, () => {
  this.setState({ expanded: true });
});
// Animation finished — safe to do next step
```

## AnimationOptions

Three curve types: preset, custom bezier, and spring.

```tsx
import { AnimationCurve } from 'valdi_core/src/AnimationOptions';

// Preset curves
{ duration: 0.3, curve: AnimationCurve.Linear }
{ duration: 0.3, curve: AnimationCurve.EaseIn }
{ duration: 0.3, curve: AnimationCurve.EaseOut }
{ duration: 0.3, curve: AnimationCurve.EaseInOut }  // default

// Custom bezier control points
{ duration: 0.4, controlPoints: [0.25, 0.1, 0.25, 1.0] }

// Spring animation (no duration — spring physics determine timing)
{ stiffness: 300, damping: 30 }

// Crossfade (alpha transition between old and new state)
{ duration: 0.3, crossfade: true }

// Continue from current animation state
{ duration: 0.3, beginFromCurrentState: true }

// Completion callback
{ duration: 0.3, completion: (wasCancelled) => { /* done */ } }
```

## Animating element attributes via ElementRef

```tsx
import { ElementRef } from 'valdi_core/src/ElementRef';

export class MyComponent extends Component {
  private titleRef = new ElementRef();
  private containerRef = new ElementRef();

  onRender(): void {
    <view ref={this.containerRef} width={100}>
      <label ref={this.titleRef} value="Hello" font={systemFont(14)} />
    </view>;
  }

  private expand = () => {
    // ✅ Animate attribute changes via ElementRef
    this.animate({ duration: 0.3 }, () => {
      this.titleRef.setAttribute('font', systemBoldFont(17));
      this.containerRef.setAttribute('width', 300);
    });
  };
}
```

## Animating state changes

```tsx
interface State {
  expanded: boolean;
  text: string;
}

export class MyComponent extends StatefulComponent<{}, State> {
  state = { expanded: false, text: 'Collapsed' };

  onRender(): void {
    <view width={this.state.expanded ? 300 : 100} height={this.state.expanded ? 200 : 50}>
      <label value={this.state.text} />
    </view>;
  }

  private toggle = () => {
    // ✅ setStateAnimated — renders with new state, animates all resulting mutations
    this.setStateAnimated(
      { expanded: !this.state.expanded, text: this.state.expanded ? 'Collapsed' : 'Expanded' },
      { duration: 0.3, curve: AnimationCurve.EaseInOut },
    );
  };
}
```

## Spring animations

```tsx
// ✅ Bouncy spring — lower damping = more bounce
this.animate({ stiffness: 300, damping: 15 }, () => {
  this.cardRef.setAttribute('scale', 1.0);
});

// ✅ Stiff spring — high damping = smooth settle
this.animate({ stiffness: 500, damping: 40 }, () => {
  this.panelRef.setAttribute('translateY', 0);
});
```

## Common patterns

### Show/hide with fade

```tsx
private show = () => {
  this.animate({ duration: 0.2 }, () => {
    this.overlayRef.setAttribute('opacity', 1);
  });
};

private hide = () => {
  this.animate({ duration: 0.2 }, () => {
    this.overlayRef.setAttribute('opacity', 0);
  });
};
```

### Sequential animations

```tsx
private async runSequence() {
  await this.animatePromise({ duration: 0.2 }, () => {
    this.step1Ref.setAttribute('opacity', 1);
  });
  await this.animatePromise({ duration: 0.3 }, () => {
    this.step2Ref.setAttribute('translateY', 0);
  });
}
```

## Common mistakes

```tsx
// ❌ WRONG — CSS-style transitions don't exist
<view style={{ transition: 'all 0.3s ease' }} />  // ❌
<view className="animate-fade-in" />               // ❌

// ❌ WRONG — React-style animation libraries
import { animated, useSpring } from 'react-spring';  // ❌

// ❌ WRONG — Calling setState inside animate (use setStateAnimated instead)
this.animate({ duration: 0.3 }, () => {
  this.setState({ expanded: true });  // ❌ Won't animate the re-render
});

// ✅ Correct — use setStateAnimated for state-driven animations
this.setStateAnimated({ expanded: true }, { duration: 0.3 });

// ❌ WRONG — Duration in milliseconds (it's seconds)
this.animate({ duration: 300 }, () => { ... });  // ❌ This is 300 seconds!

// ✅ Correct — Duration in seconds
this.animate({ duration: 0.3 }, () => { ... });
```

## CoreUI animation components

The `coreui` library includes pre-built animation components:

| Component | Purpose |
|-----------|---------|
| `AnimatableVisibilityView` | Animate show/hide transitions |
| `AnimationShimmer` | Loading shimmer effect |
| `AnimationSliding` | Slide in/out animations |
| `AnimationSquishy` | Press-and-squish feedback |
| `AnimationRotate` | Continuous rotation |
| `VerticallyOpenableComponent` | Expand/collapse vertically |

Import from `coreui/src/components/animation/`.
