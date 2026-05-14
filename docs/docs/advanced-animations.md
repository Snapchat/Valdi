# Advanced animations

Valdi exposes an imperative way to animate attributes. Almost all attributes can be animated in Valdi. 

## `animate` and `animatePromise`

To support animations `Valdi` class provies `animate` and `animatePromise` functions. Use `animatePromise` to observe the animation finish event.

```ts
declare class Component {
  //...
  /* 
   * Associate a set of changes with an animation
   * @param animations a block which will be executed in a render. All element mutations
   * belong to this renderer will be animated.
   * To get notified on animation complete, use `animatePromise` instead.
   */
  animate(options: AnimationOptions, animations: () => void): void;
  
  /*
   * Associate a set of changes with an animation
   * @param animations a block which will be executed in a render. All element mutations
   * belong to this renderer will be animated.
   * @return A promise which will be resolved when the animation finishes.
   */
  animatePromise(options: AnimationOptions, animations: () => void): Promise<void>;
}
```

This API is inspired by iOS's `animateWithDuration:animations:`. Your can pass an `AnimationOptions` object, which will define how long the animation should be and what curve or control points to use. Your provided `animations` function will then be called, and you will be able to make any mutation to your Component's state, or to the element's attributes directly. All mutations will be applied with an animation automatically.

## `AnimationOptions`

Both functions [accept](./generated-docs/valdi_core/AnimationOptions/valdi-generated.md) `AnimationOptions`:

```ts
export enum AnimationCurve {
  Linear = 'linear',
  EaseIn = 'easeIn',
  EaseOut = 'easeOut',
  EaseInOut = 'easeInOut',
}

export type AnimationOptions = SpringAnimationOptions | PresetCurveAnimationOptions | CustomCurveAnimationOptions;

export interface BasicAnimationOptions {
  /**
   * The duration of the animation in seconds.
   */
  duration: number;

  /**
   * Whether the animation should start from the current layer state.
   * Corresponds to CoreAnimation's beginFromCurrentState.
   */
  beginFromCurrentState?: boolean;

  /**
   * Whether the animation should crossfade with an alpha transition between the
   *  previous state of the tree to the new state.
   */
  crossfade?: boolean;

  /**
   * A completion to call when the transition has completed
   */
  completion?: (wasCancelled: boolean) => void;
}

export interface PresetCurveAnimationOptions extends BasicAnimationOptions {
  /**
   * The curve to use for the animation, defaults
   * to easeInOut. Ignored when controlPoints is set.
   */
  curve?: AnimationCurve;
}

export interface CustomCurveAnimationOptions extends BasicAnimationOptions {
  /**
   * 4 control points to control the curve.
   * If set, curve will be ignored to use those
   * control points instead.
   */
  controlPoints: Array<number>;
}

export interface SpringAnimationOptions {
  /**
   * The spring stiffness coefficient. Higher values correspond to a stiffer spring that yields a greater amount of force for moving objects.
   * Corresponds to the "spring constant" in the spring equation.
   *
   * Must be greater than 0.
   */
  stiffness: SpringStiffnessValue;

  /**
   * The damping force to apply to the spring’s motion. This is the damping coefficient in the spring equation.
   *
   * Must be greater than 0.
   */
  damping: SpringDampingValue;

  /**
   * Whether the animation should start from the current layer state.
   * Corresponds to CoreAnimation's beginFromCurrentState.
   */
  beginFromCurrentState?: boolean;

  /**
   * A completion to call when the transition has completed
   */
  completion?: (wasCanceled: boolean) => void;
}
```

## Example
```tsx
export class YourComponent extends Component {

  titleLabel = new ElementRef();
  titleContainer = new ElementRef();

  onRender() {
    <layout>
      <view ref={this.titleContainer}>
        <label value='This is a title' ref={this.titleLabel}/>
      </view>
    </layout>
  }

  somethingHappened() {
    this.animate({ duration: 0.3 }, () => {
      // This change of font will make the label crossfade to the new size
      this.titleLabel.setAttribute('font', 'title');
      // The titleContainer will be resized to that given size, any resulting
      // change of the layout will be animated.
      this.titleContainer.setAttribute('width', 200);
    });
  }
}
```

You can also animate state changes, which will render your component with the new state and automatically animate any mutations:
```tsx
interface State {
  text: string;
}
export class YourComponent extends StatefulComponent<object, State> {

  state = {text: 'This is a title'};

  onRender() {
    <layout>
      <view>
        <label value={this.state.text}/>
      </view>
    </layout>
  }

  somethingHappened() {
    // This will mutate the state, the new text will be applied to the label through a crossfade animation
    this.setStateAnimated({text: 'This is the new text'}, {duration: 0.3});
  }
}
```
