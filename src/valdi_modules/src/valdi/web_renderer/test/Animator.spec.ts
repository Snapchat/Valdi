import 'jasmine/src/jasmine';
import { Animator, type AnimatorDelegate } from '../src/animations/Animator';
import type { AnimatorCommitPreparation } from '../src/animations/AnimatorCommitPreparation';
import { KeyAnimation } from '../src/animations/KeyAnimation';

class TestKeyAnimation extends KeyAnimation {
  didFinishAnimation = false;

  constructor() {
    super(0.1);
  }

  override applyProgress(_progress: number): boolean {
    return true;
  }

  override applyFinalValue(): void {}

  protected override didFinish(): void {
    this.didFinishAnimation = true;
  }
}

class TestCommitPreparation implements AnimatorCommitPreparation {
  prepared = false;
  cancelled = false;

  constructor(private readonly animation: KeyAnimation) {}

  prepareForCommit(animator: Animator): void {
    this.prepared = true;
    animator.addAnimation(this, 'prepared', this.animation);
  }

  cancel(): void {
    this.cancelled = true;
  }
}

describe('Animator', () => {
  const delegate: AnimatorDelegate = {
    animatorWillApplyLayoutMutation: () => {},
  };
  it('replaces animations with the same owner and key', () => {
    const animator = new Animator({ duration: 1 }, 1, delegate);
    const owner = {};
    const first = new TestKeyAnimation();
    const second = new TestKeyAnimation();

    animator.addAnimation(owner, 'value', first);
    animator.addAnimation(owner, 'value', second);

    expect(first.didFinishAnimation).toBeTrue();
    expect(animator.takeAnimations()).toEqual([second]);
  });

  it('notifies its delegate only once about layout mutations', () => {
    let callCount = 0;
    const animator = new Animator({ duration: 1 }, 2, {
      animatorWillApplyLayoutMutation: () => callCount++,
    });

    animator.willApplyLayoutMutation();
    animator.willApplyLayoutMutation();

    expect(callCount).toBe(1);
  });

  it('runs commit preparations once and allows them to add animations', () => {
    const animator = new Animator({ duration: 1 }, 3, delegate);
    const animation = new TestKeyAnimation();
    const preparation = new TestCommitPreparation(animation);

    animator.addCommitPreparation('layout', preparation);
    expect(animator.getCommitPreparation('layout')).toBe(preparation);
    animator.prepareForCommit();

    expect(preparation.prepared).toBeTrue();
    expect(preparation.cancelled).toBeFalse();
    expect(animator.takeAnimations()).toEqual([animation]);
  });

  it('cancels commit preparations when the animator completes before preparation', () => {
    const animator = new Animator({ duration: 1 }, 4, delegate);
    const preparation = new TestCommitPreparation(new TestKeyAnimation());
    animator.addCommitPreparation('layout', preparation);

    animator.complete(true);

    expect(preparation.prepared).toBeFalse();
    expect(preparation.cancelled).toBeTrue();
  });
});
