import { AnimationCurve } from 'valdi_core/src/AnimationOptions';
import type { AnimationOptions } from 'valdi_core/src/AnimationOptions';

export type AnimationTimingFunction = (progress: number) => number;

const LINEAR_TIMING_FUNCTION: AnimationTimingFunction = progress => progress;
const EASE_IN_TIMING_FUNCTION: AnimationTimingFunction = makeCubicBezierTimingFunction(0.42, 0, 1, 1);
const EASE_OUT_TIMING_FUNCTION: AnimationTimingFunction = makeCubicBezierTimingFunction(0, 0, 0.58, 1);
const EASE_IN_OUT_TIMING_FUNCTION: AnimationTimingFunction = makeCubicBezierTimingFunction(0.42, 0, 0.58, 1);

export function makeAnimationTimingFunction(
  options: Exclude<AnimationOptions, { stiffness: number }>,
): AnimationTimingFunction {
  if (!Number.isFinite(options.duration) || options.duration < 0) {
    throw new Error('Animation duration must be a finite value greater than or equal to zero');
  }
  if ('controlPoints' in options) {
    const points = options.controlPoints;
    if (
      points.length !== 4 ||
      points.some(point => !Number.isFinite(point)) ||
      points[0] < 0 ||
      points[0] > 1 ||
      points[2] < 0 ||
      points[2] > 1
    ) {
      throw new Error('Animation controlPoints must contain four finite values with x coordinates between 0 and 1');
    }
    return makeCubicBezierTimingFunction(points[0], points[1], points[2], points[3]);
  }
  switch (options.curve ?? AnimationCurve.EaseInOut) {
    case AnimationCurve.Linear:
      return LINEAR_TIMING_FUNCTION;
    case AnimationCurve.EaseIn:
      return EASE_IN_TIMING_FUNCTION;
    case AnimationCurve.EaseOut:
      return EASE_OUT_TIMING_FUNCTION;
    case AnimationCurve.EaseInOut:
      return EASE_IN_OUT_TIMING_FUNCTION;
  }
}

function makeCubicBezierTimingFunction(
  x1: number,
  y1: number,
  x2: number,
  y2: number,
): AnimationTimingFunction {
  const sample = (t: number, a1: number, a2: number) => {
    const inverse = 1 - t;
    return 3 * inverse * inverse * t * a1 + 3 * inverse * t * t * a2 + t * t * t;
  };
  return progress => {
    let low = 0;
    let high = 1;
    let t = progress;
    for (let index = 0; index < 12; index++) {
      const x = sample(t, x1, x2);
      if (Math.abs(x - progress) < 0.000001) {
        break;
      }
      if (x < progress) {
        low = t;
      } else {
        high = t;
      }
      t = (low + high) / 2;
    }
    return sample(t, y1, y2);
  };
}
