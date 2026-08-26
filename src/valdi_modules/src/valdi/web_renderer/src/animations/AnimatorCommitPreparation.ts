import type { Animator } from './Animator';

export interface AnimatorCommitPreparation {
  prepareForCommit(animator: Animator): void;
  cancel(): void;
}
