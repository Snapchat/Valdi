import type { AnimationOptions } from 'valdi_core/src/AnimationOptions';
import type { AnimatorCommitPreparation } from './AnimatorCommitPreparation';
import { KeyAnimation } from './KeyAnimation';

export interface AnimatorDelegate {
  animatorWillApplyLayoutMutation(animator: Animator): void;
}

export class Animator {
  private readonly animationsByOwner = new Map<object, Map<string, KeyAnimation>>();
  private commitPreparations?: Map<string, AnimatorCommitPreparation>;
  private hasLayoutMutation = false;
  private sealed = false;
  private preparedForCommit = false;
  private completed = false;

  constructor(
    readonly options: AnimationOptions,
    readonly token: number,
    private readonly delegate: AnimatorDelegate,
  ) {}

  willApplyLayoutMutation(): void {
    if (this.hasLayoutMutation) {
      return;
    }
    this.hasLayoutMutation = true;
    this.delegate.animatorWillApplyLayoutMutation(this);
  }

  getCommitPreparation(key: string): AnimatorCommitPreparation | undefined {
    return this.commitPreparations?.get(key);
  }

  addCommitPreparation(key: string, preparation: AnimatorCommitPreparation): void {
    if (this.sealed || this.preparedForCommit) {
      throw new Error('Cannot add a commit preparation to a committed animator');
    }
    const preparations = (this.commitPreparations ??= new Map());
    if (preparations.has(key)) {
      throw new Error(`Animator already has a commit preparation for '${key}'`);
    }
    preparations.set(key, preparation);
  }

  prepareForCommit(): void {
    if (this.preparedForCommit) {
      throw new Error('Animator was already prepared for commit');
    }
    this.preparedForCommit = true;
    const preparations = this.commitPreparations;
    this.commitPreparations = undefined;
    if (!preparations) {
      return;
    }
    preparations.forEach(preparation => {
      try {
        preparation.prepareForCommit(this);
      } catch (error) {
        preparation.cancel();
        console.error('Valdi web renderer animation commit preparation failed', error);
      }
    });
  }

  addAnimation(owner: object, key: string, animation: KeyAnimation): void {
    if (this.sealed) {
      throw new Error('Cannot add an animation to a committed animator');
    }
    let animations = this.animationsByOwner.get(owner);
    if (!animations) {
      animations = new Map<string, KeyAnimation>();
      this.animationsByOwner.set(owner, animations);
    }
    const previous = animations.get(key);
    animations.set(key, animation);
    if (previous && previous !== animation) {
      previous.cancel();
    }
  }

  get empty(): boolean {
    for (const animations of this.animationsByOwner.values()) {
      for (const animation of animations.values()) {
        if (!animation.finished) {
          return false;
        }
      }
    }
    return true;
  }

  takeAnimations(): KeyAnimation[] {
    if (this.sealed) {
      throw new Error('Animator was already committed');
    }
    this.sealed = true;
    const result: KeyAnimation[] = [];
    this.animationsByOwner.forEach(animations =>
      animations.forEach(animation => {
        if (!animation.finished) {
          result.push(animation);
        }
      }),
    );
    this.animationsByOwner.clear();
    return result;
  }

  complete(wasCancelled: boolean): void {
    if (this.completed) {
      return;
    }
    this.completed = true;
    this.cancelCommitPreparations();
    const completion = this.options.completion;
    if (!completion) {
      return;
    }
    try {
      completion(wasCancelled);
    } catch (error) {
      console.error('Valdi web renderer animation completion failed', error);
    }
  }

  private cancelCommitPreparations(): void {
    const preparations = this.commitPreparations;
    this.commitPreparations = undefined;
    preparations?.forEach(preparation => preparation.cancel());
  }
}
