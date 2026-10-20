import type { ComponentCtor, IRendererEventListener } from 'valdi_core/src/IRendererEventListener';
import type { ServerDOMMutationTracker } from './dom/ServerDOM';

export class RenderMutationCoordinator implements ServerDOMMutationTracker, IRendererEventListener {
  private active = false;
  private destroyed = false;
  private dirty = false;
  private renderDepth = 0;
  private scheduled = false;
  private onCommit: (() => void) | undefined;

  setCommitCallback(onCommit: () => void): void {
    this.onCommit = onCommit;
  }

  start(): void {
    this.active = true;
  }

  stop(): void {
    this.active = false;
    this.destroyed = true;
    this.dirty = false;
    this.onCommit = undefined;
  }

  markMutation(): void {
    if (!this.active || this.destroyed) {
      return;
    }
    this.dirty = true;
    if (this.renderDepth === 0) {
      this.scheduleCommit();
    }
  }

  onRenderBegin(): void {
    this.renderDepth++;
  }

  onRenderEnd(): void {
    if (this.renderDepth === 0) {
      throw new Error('Unbalanced ValdiHTMLRenderer render completion');
    }
    this.renderDepth--;
    if (this.renderDepth === 0) {
      this.commitIfNeeded();
    }
  }

  onComponentBegin(_key: string, _componentCtor: ComponentCtor): void {}
  onComponentEnd(): void {}
  onBypassComponentRender(): void {}
  onComponentViewModelPropertyChange(_viewModelPropertyName: string): void {}

  private scheduleCommit(): void {
    if (this.scheduled) {
      return;
    }
    this.scheduled = true;
    Promise.resolve().then(() => {
      this.scheduled = false;
      if (this.renderDepth === 0) {
        this.commitIfNeeded();
      }
    });
  }

  private commitIfNeeded(): void {
    if (!this.dirty || this.destroyed) {
      return;
    }
    this.dirty = false;
    this.onCommit?.();
  }
}
