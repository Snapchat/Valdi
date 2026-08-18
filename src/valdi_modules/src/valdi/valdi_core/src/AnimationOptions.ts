export enum AnimationCurve {
  Linear = 'linear',
  EaseIn = 'easeIn',
  EaseOut = 'easeOut',
  EaseInOut = 'easeInOut',
}

export type AnimationOptions = SpringAnimationOptions | PresetCurveAnimationOptions | CustomCurveAnimationOptions;

export interface AnimationAppearanceAttributes {
  /**
   * The horizontal point that scale is applied from, normalized relative to the final width. Defaults to the center.
   */
  originX?: number;

  /**
   * The vertical point that scale is applied from, normalized relative to the final height. Defaults to the center.
   */
  originY?: number;

  /**
   * Initial horizontal translation as a ratio of the final width.
   */
  translationX?: number;

  /**
   * Initial vertical translation as a ratio of the final height.
   */
  translationY?: number;

  /**
   * Initial horizontal scale. Defaults to 1.
   */
  scaleX?: number;

  /**
   * Initial vertical scale. Defaults to 1.
   */
  scaleY?: number;

  /**
   * For enter animations, the opacity created elements should animate from. For exit animations, the opacity destroyed
   * elements should animate to.
   */
  opacity?: number;
}

export interface AnimationAppearanceBehavior {
  /**
   * How created elements should animate in.
   */
  enterAttributes?: AnimationAppearanceAttributes;

  /**
   * How destroyed elements should animate out.
   */
  exitAttributes?: AnimationAppearanceAttributes;
}

export namespace AnimationAppearance {
  /**
   * Slides created elements in from the left. Use this for enterAttributes.
   */
  export const SLIDE_IN: AnimationAppearanceAttributes = {
    translationX: -1,
  };

  /**
   * Slides destroyed elements out to the right. Use this for exitAttributes.
   */
  export const SLIDE_OUT: AnimationAppearanceAttributes = {
    translationX: 1,
  };

  /**
   * Fades created elements in from transparent, or destroyed elements out to transparent.
   */
  export const FADE: AnimationAppearanceAttributes = {
    opacity: 0,
  };

  /**
   * Scales created elements up from the center, or destroyed elements down into the center.
   */
  export const SCALE_FROM_CENTER: AnimationAppearanceAttributes = {
    scaleX: 0,
    scaleY: 0,
  };
}

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

  /**
   * Controls how elements that are created or destroyed inside this animation are animated.
   */
  appearanceBehavior?: AnimationAppearanceBehavior;
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
   * See: https://drive.google.com/file/d/1z5OUi8hhO4ZdgR6hkHCbwy-BRLJG8mjs/view?usp=sharing
   */
  stiffness: SpringStiffnessValue;

  /**
   * The damping force to apply to the spring’s motion. This is the damping coefficient in the spring equation.
   *
   * Must be greater than 0.
   * See: https://drive.google.com/file/d/1z5OUi8hhO4ZdgR6hkHCbwy-BRLJG8mjs/view?usp=sharing
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

  /**
   * Controls how elements that are created or destroyed inside this animation are animated.
   */
  appearanceBehavior?: AnimationAppearanceBehavior;
}

// (Taken from Principle.app)
export const SPRING_DEFAULT_STIFFNESS: SpringStiffnessValue = 381.47;
// (Taken from Principle.app)
export const SPRING_DEFAULT_DAMPING: SpringDampingValue = 20.1;

export type SpringStiffnessValue = number;
export type SpringDampingValue = number;
