import type { AnimationOptions } from 'valdi_core/src/AnimationOptions';
import { makeAnimationTimingFunction } from './AnimationTimingFunctions';
import type { AnimationTimingFunction } from './AnimationTimingFunctions';
import type { Animator } from './Animator';
import type { KeyAnimation } from './KeyAnimation';

interface SpringState {
  value: number;
  velocity: number;
  lastTimestamp: number;
}

interface RunningAnimation {
  keyAnimation: KeyAnimation;
  transaction: RunningAnimationTransaction;
  springState?: SpringState;
}

interface RunningAnimationTransaction {
  animator: Animator;
  animations: Set<RunningAnimation>;
  startTimestamp: number;
  timingFunction?: AnimationTimingFunction;
}

const VELOCITY_THRESHOLD_MULTIPLIER = 1000 / 16;

export class AnimationController {
  private readonly transactionsByToken = new Map<number, RunningAnimationTransaction>();
  private frameRequest: number | undefined;
  private destroyed = false;

  commit(animator: Animator): void {
    if (this.destroyed) {
      animator.complete(true);
      return;
    }

    const options = animator.options;
    let timing: AnimationTimingFunction | undefined;
    if (isSpringOptions(options)) {
      validateSpringOptions(options.stiffness, options.damping);
      timing = undefined;
    } else {
      timing = makeAnimationTimingFunction(options);
    }
    this.cancelTransaction(animator.token);
    const animations = animator.takeAnimations();
    const timestamp = now();
    const transaction: RunningAnimationTransaction = {
      animator,
      animations: new Set<RunningAnimation>(),
      startTimestamp: timestamp,
      timingFunction: timing,
    };
    this.transactionsByToken.set(animator.token, transaction);

    for (const keyAnimation of animations) {
      const animation: RunningAnimation = {
        keyAnimation,
        transaction,
        springState: isSpringOptions(animator.options)
          ? { value: 0, velocity: 0, lastTimestamp: timestamp }
          : undefined,
      };
      if (keyAnimation.finished) {
        continue;
      }
      transaction.animations.add(animation);
      if (!this.applyProgress(animation, 0)) {
        keyAnimation.cancel();
        transaction.animations.delete(animation);
      }
    }

    if (transaction.animations.size === 0) {
      this.finishTransaction(transaction, false);
      return;
    }
    this.scheduleFrameIfNeeded();
  }

  cancelTransaction(token: number): void {
    const transaction = this.transactionsByToken.get(token);
    if (!transaction) {
      return;
    }
    const animations = Array.from(transaction.animations);
    for (const animation of animations) {
      if (!animation.keyAnimation.finished) {
        this.completeAnimation(animation);
      }
      transaction.animations.delete(animation);
    }
    this.finishTransaction(transaction, true);
    this.cancelFrameIfIdle();
  }

  destroy(): void {
    if (this.destroyed) {
      return;
    }
    this.destroyed = true;
    if (this.frameRequest !== undefined) {
      cancelAnimationFrame(this.frameRequest);
      this.frameRequest = undefined;
    }
    const transactions = Array.from(this.transactionsByToken.values());
    this.transactionsByToken.clear();
    for (const transaction of transactions) {
      for (const animation of transaction.animations) {
        animation.keyAnimation.cancel();
      }
      transaction.animations.clear();
      transaction.animator.complete(true);
    }
  }

  private runFrame = (timestamp: number): void => {
    this.frameRequest = undefined;
    if (this.destroyed) {
      return;
    }

    for (const transaction of Array.from(this.transactionsByToken.values())) {
      for (const animation of Array.from(transaction.animations)) {
        if (animation.keyAnimation.finished) {
          transaction.animations.delete(animation);
          continue;
        }
        const sample = sampleAnimation(animation, timestamp);
        if (sample.finished) {
          this.completeAnimation(animation);
          transaction.animations.delete(animation);
        } else if (!this.applyProgress(animation, sample.progress)) {
          animation.keyAnimation.cancel();
          transaction.animations.delete(animation);
        }
      }
      if (transaction.animations.size === 0) {
        this.finishTransaction(transaction, false);
      }
    }

    this.scheduleFrameIfNeeded();
  };

  private applyProgress(animation: RunningAnimation, progress: number): boolean {
    try {
      return animation.keyAnimation.applyProgress(progress);
    } catch (error) {
      console.error('Valdi web renderer animation failed', error);
      return false;
    }
  }

  private completeAnimation(animation: RunningAnimation): void {
    try {
      animation.keyAnimation.complete();
    } catch (error) {
      console.error('Valdi web renderer final animation application failed', error);
    }
  }

  private finishTransaction(transaction: RunningAnimationTransaction, wasCancelled: boolean): void {
    if (this.transactionsByToken.get(transaction.animator.token) !== transaction) {
      return;
    }
    this.transactionsByToken.delete(transaction.animator.token);
    transaction.animator.complete(wasCancelled);
  }

  private scheduleFrameIfNeeded(): void {
    if (this.frameRequest !== undefined || this.transactionsByToken.size === 0 || this.destroyed) {
      return;
    }
    this.frameRequest = requestAnimationFrame(this.runFrame);
  }

  private cancelFrameIfIdle(): void {
    if (this.transactionsByToken.size !== 0 || this.frameRequest === undefined) {
      return;
    }
    cancelAnimationFrame(this.frameRequest);
    this.frameRequest = undefined;
  }
}

interface AnimationSample {
  progress: number;
  finished: boolean;
}

function sampleAnimation(animation: RunningAnimation, timestamp: number): AnimationSample {
  const options = animation.transaction.animator.options;
  if (isSpringOptions(options)) {
    return sampleSpring(animation, timestamp, options.stiffness, options.damping);
  }
  const linearProgress =
    options.duration <= 0
      ? 1
      : Math.max(0, Math.min(1, (timestamp - animation.transaction.startTimestamp) / (options.duration * 1000)));
  return {
    progress: animation.transaction.timingFunction!(linearProgress),
    finished: linearProgress >= 1,
  };
}

function sampleSpring(
  animation: RunningAnimation,
  timestamp: number,
  stiffness: number,
  damping: number,
): AnimationSample {
  const state = animation.springState!;
  const deltaSeconds = Math.max(0, timestamp - state.lastTimestamp) / 1000;
  state.lastTimestamp = timestamp;
  if (deltaSeconds > 0) {
    updateSpringState(state, deltaSeconds, stiffness, damping);
  }
  const threshold = Math.max(Math.abs(animation.keyAnimation.minimumVisibleChange), Number.EPSILON);
  const finished =
    Math.abs(state.value - 1) < threshold && Math.abs(state.velocity) < threshold * VELOCITY_THRESHOLD_MULTIPLIER;
  if (finished) {
    state.value = 1;
    state.velocity = 0;
  }
  return { progress: state.value, finished };
}

function updateSpringState(state: SpringState, deltaSeconds: number, stiffness: number, damping: number): void {
  const frequency = Math.sqrt(stiffness);
  const dampingRatio = damping / (2 * frequency);
  const displacement = state.value - 1;
  const velocity = state.velocity;
  let nextDisplacement: number;
  let nextVelocity: number;

  if (dampingRatio > 1) {
    const root = frequency * Math.sqrt(dampingRatio * dampingRatio - 1);
    const gammaPlus = -dampingRatio * frequency + root;
    const gammaMinus = -dampingRatio * frequency - root;
    const coefficientA = displacement - (gammaMinus * displacement - velocity) / (gammaMinus - gammaPlus);
    const coefficientB = (gammaMinus * displacement - velocity) / (gammaMinus - gammaPlus);
    const minusTerm = Math.exp(gammaMinus * deltaSeconds);
    const plusTerm = Math.exp(gammaPlus * deltaSeconds);
    nextDisplacement = coefficientA * minusTerm + coefficientB * plusTerm;
    nextVelocity = coefficientA * gammaMinus * minusTerm + coefficientB * gammaPlus * plusTerm;
  } else if (Math.abs(dampingRatio - 1) < Number.EPSILON) {
    const coefficientA = displacement;
    const coefficientB = velocity + frequency * displacement;
    const exponential = Math.exp(-frequency * deltaSeconds);
    nextDisplacement = (coefficientA + coefficientB * deltaSeconds) * exponential;
    nextVelocity = nextDisplacement * -frequency + coefficientB * exponential;
  } else {
    const dampedFrequency = frequency * Math.sqrt(1 - dampingRatio * dampingRatio);
    const cosCoefficient = displacement;
    const sinCoefficient = (dampingRatio * frequency * displacement + velocity) / dampedFrequency;
    const exponential = Math.exp(-dampingRatio * frequency * deltaSeconds);
    const cosine = Math.cos(dampedFrequency * deltaSeconds);
    const sine = Math.sin(dampedFrequency * deltaSeconds);
    nextDisplacement = exponential * (cosCoefficient * cosine + sinCoefficient * sine);
    nextVelocity =
      nextDisplacement * -frequency * dampingRatio +
      exponential * (-dampedFrequency * cosCoefficient * sine + dampedFrequency * sinCoefficient * cosine);
  }

  state.value = nextDisplacement + 1;
  state.velocity = nextVelocity;
}

function isSpringOptions(options: AnimationOptions): options is Extract<AnimationOptions, { stiffness: number }> {
  return 'stiffness' in options;
}

function validateSpringOptions(stiffness: number, damping: number): void {
  if (!Number.isFinite(stiffness) || stiffness <= 0) {
    throw new Error('Animation spring stiffness must be a finite value greater than zero');
  }
  if (!Number.isFinite(damping) || damping <= 0) {
    throw new Error('Animation spring damping must be a finite value greater than zero');
  }
}

function now(): number {
  return typeof performance === 'undefined' ? Date.now() : performance.now();
}
