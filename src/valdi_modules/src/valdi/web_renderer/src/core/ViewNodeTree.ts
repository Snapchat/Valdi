import type { VisibilityObserver } from 'valdi_core/src/IRendererDelegate';
import type { AnimationOptions } from 'valdi_core/src/AnimationOptions';
import { Style } from 'valdi_core/src/Style';
import type { ElementFrame } from 'valdi_tsx/src/Geometry';
import { LayoutObserverController, measureElementFrame } from '../LayoutObserverController';
import { VisibilityObserverController } from '../VisibilityObserverController';
import { AnimationController } from '../animations/AnimationController';
import { Animator, type AnimatorDelegate } from '../animations/Animator';
import {
  captureLayoutSnapshot,
  LayoutAnimation,
  LayoutAnimationPass,
  type LayoutSnapshot,
} from '../animations/LayoutAnimation';
import { getElementClassForViewClass } from '../elements/ElementClassRegistry';
import type { AttributeUpdatedExternallyDelegate, ElementLayoutObserver } from './ElementClass';
import { ColorPaletteManager, COLOR_PALETTE_MANAGER } from './Palette';
import { ViewNode, type ViewNodeDebugSnapshot } from './ViewNode';

export interface ViewNodeTreeDebugSnapshot {
  tree: ViewNodeDebugSnapshot | null;
  viewport: {
    width: number;
    height: number;
  };
}

type PendingLifecycleCallback = {
  callback: () => void;
  priority: number;
  sequence: number;
};

type RenderCompleteScheduler = (callback: () => void) => void;

const LAYOUT_COMMIT_PREPARATION_KEY = 'layout';

export class ViewNodeTree implements AnimatorDelegate {
  private readonly nodesById = new Map<number, ViewNode>();
  private readonly nodeIdByHtmlElement = new WeakMap<Element, number>();
  private readonly colorPaletteManager: ColorPaletteManager;
  private readonly visibilityObserverController: VisibilityObserverController;
  private readonly layoutObserverController: LayoutObserverController;
  private rootNode: ViewNode | null = null;
  private pendingLifecycleCallbacks?: PendingLifecycleCallback[];
  private nextLifecycleCallbackSequence = 0;
  private renderBatchDepth = 0;
  private flushScheduled = false;
  private layoutRefreshNeeded = false;
  private isFlushing = false;
  private flushRequestedDuringFlush = false;
  private destroyed = false;
  private readonly animatorStack: Animator[] = [];
  private animationController?: AnimationController;
  private activeLayoutAnimation?: LayoutAnimation;
  private activeLayoutAnimationValidationNeeded = false;
  private readonly removePaletteChangeListener: () => void;

  constructor(colorPaletteManager?: ColorPaletteManager) {
    this.colorPaletteManager = colorPaletteManager ?? COLOR_PALETTE_MANAGER;
    this.visibilityObserverController = new VisibilityObserverController();
    this.layoutObserverController = new LayoutObserverController(() => this.onLayoutPassCommitted());
    this.removePaletteChangeListener = this.colorPaletteManager.addChangeListener(() => {
      this.reapplyColorPalettesOnAllNodes();
    });
  }

  setRenderCompleteScheduler(scheduler: RenderCompleteScheduler): void {
    this.layoutObserverController.setRenderCompleteScheduler(scheduler);
  }

  setPostLayoutScheduler(scheduler: ((callback: () => void) => void) | undefined): void {
    this.layoutObserverController.setPostLayoutScheduler(scheduler);
  }

  createElement(
    id: number,
    viewClass: string,
    attributeUpdatedExternallyDelegate?: AttributeUpdatedExternallyDelegate,
  ): ViewNode {
    const elementClass = getElementClassForViewClass(viewClass);
    if (!elementClass) {
      throw new Error(`Unknown viewClass: ${viewClass}`);
    }
    const node = new ViewNode(
      id,
      viewClass,
      elementClass,
      this,
      this.colorPaletteManager,
      attributeUpdatedExternallyDelegate,
    );
    this.nodesById.set(id, node);
    this.nodeIdByHtmlElement.set(node.htmlElement, id);
    const animator = this.getCurrentAnimator();
    if (animator) {
      node.requestAnimatedAppearance(animator);
    }
    return node;
  }

  destroyElement(id: number): void {
    const node = this.nodesById.get(id);
    if (!node) {
      return;
    }
    const animator = this.getCurrentAnimator();
    if (animator && node.requestAnimatedDisappearance(animator)) {
      animator.willApplyLayoutMutation();
      this.markLayoutRefreshNeeded();
      return;
    }
    animator?.willApplyLayoutMutation();
    this.markLayoutRefreshNeeded();
    this.destroyNodeSubtree(node);
    if (!animator) {
      this.requestActiveLayoutAnimationValidation();
    }
  }

  makeElementRoot(id: number, root: HTMLElement | ShadowRoot): void {
    const node = this.getNodeOrThrow(id);
    this.rootNode = node;
    node.makeRoot(root);
    this.visibilityObserverController.setRoot(root);
    this.markLayoutRefreshNeeded();
  }

  moveElement(id: number, parentId: number, parentIndex: number): void {
    const node = this.nodesById.get(id);
    const parent = this.nodesById.get(parentId);
    if (!node || !parent) {
      throw new Error(`moveElement: element or parent is missing, id: ${id}, parentId: ${parentId}`);
    }
    const animator = this.getCurrentAnimator();
    animator?.willApplyLayoutMutation();
    node.move(parent, parentIndex);
    this.markLayoutRefreshNeeded();
    if (!animator) {
      this.requestActiveLayoutAnimationValidation();
    }
  }

  setAttributeOnElement(id: number, attributeName: string, attributeValue: unknown): void {
    if (this.getNodeOrThrow(id).setAttribute(attributeName, attributeValue)) {
      if (!this.getCurrentAnimator()) {
        this.requestActiveLayoutAnimationValidation();
      }
      this.markLayoutRefreshNeeded();
    }
  }

  setStyleAttributeOnElement(id: number, _attributeName: string, style: Style<any> | undefined): void {
    this.setAttributeOnElement(id, 'style', style);
  }

  getNode(id: number): ViewNode | undefined {
    return this.nodesById.get(id);
  }

  getNodeIdForHtmlElement(element: Element): number | undefined {
    return this.nodeIdByHtmlElement.get(element);
  }

  getDebugSnapshot(): ViewNodeTreeDebugSnapshot {
    this.flush();
    return {
      tree: this.rootNode?.getDebugSnapshot() ?? null,
      viewport: {
        width: typeof window === 'undefined' ? 0 : window.innerWidth,
        height: typeof window === 'undefined' ? 0 : window.innerHeight,
      },
    };
  }

  registerVisibilityObserver(observer: VisibilityObserver): void {
    this.visibilityObserverController.registerObserver(observer);
  }

  scheduleVisibilityRefresh(force: boolean): void {
    this.visibilityObserverController.scheduleRefresh(force);
  }

  drainScheduledVisibilityRefresh(force: boolean): void {
    this.visibilityObserverController.drainScheduledRefresh(force);
  }

  drainScheduledLayoutObserverRefresh(): void {
    this.layoutObserverController.drainScheduledRefresh();
  }

  getElementFrame(id: number): ElementFrame | undefined {
    const node = this.nodesById.get(id);
    if (!node) {
      return undefined;
    }
    return measureElementFrame(node.htmlElement);
  }

  setElementOnLayoutCallback(
    id: number,
    viewClass: string,
    element: HTMLElement,
    attached: boolean,
    callback: ((frame: ElementFrame) => void) | undefined,
  ): void {
    this.layoutObserverController.setOnLayoutCallback(id, viewClass, element, attached, callback);
  }

  setElementLayoutObserver(
    id: number,
    viewClass: string,
    element: HTMLElement,
    attached: boolean,
    attributeName: string,
    observer: ElementLayoutObserver | undefined,
  ): void {
    this.layoutObserverController.setLayoutObserver(id, viewClass, element, attached, attributeName, observer);
  }

  getElementLayoutObserver(id: number, attributeName: string): ElementLayoutObserver | undefined {
    return this.layoutObserverController.getLayoutObserver(id, attributeName);
  }

  setElementLayoutAttached(id: number, attached: boolean): void {
    this.layoutObserverController.setElementAttached(id, attached);
  }

  requestLayoutPass(): void {
    this.layoutObserverController.scheduleRefresh();
  }

  beginRender(): void {
    this.renderBatchDepth++;
  }

  endRender(): void {
    if (this.renderBatchDepth > 0) {
      this.renderBatchDepth--;
    }
    if (this.renderBatchDepth === 0) {
      this.flush();
    }
  }

  beginAnimation(options: AnimationOptions, token: number): void {
    this.flush();
    this.animatorStack.push(new Animator(options, token, this));
  }

  endAnimation(): void {
    const animator = this.getCurrentAnimator();
    if (!animator) {
      return;
    }
    this.flush();
    animator.prepareForCommit();
    this.animatorStack.pop();
    if (animator.empty) {
      animator.complete(false);
      return;
    }
    this.getOrCreateAnimationController().commit(animator);
  }

  cancelAnimation(token: number): void {
    this.animationController?.cancelTransaction(token);
  }

  animatorWillApplyLayoutMutation(animator: Animator): void {
    const snapshot = this.captureLayoutAnimationSnapshot();
    const useCurrentFrame = animator.options.beginFromCurrentState === true;
    for (const entry of snapshot.entries) {
      const animationFrame = entry.node.getLayoutAnimationFrame(useCurrentFrame);
      if (animationFrame) {
        entry.frame = animationFrame;
      }
    }
    this.activeLayoutAnimation?.cancel();
    animator.addCommitPreparation(LAYOUT_COMMIT_PREPARATION_KEY, new LayoutAnimationPass(this, snapshot));
  }

  captureLayoutAnimationSnapshot(): LayoutSnapshot {
    return captureLayoutSnapshot(this.rootNode);
  }

  setActiveLayoutAnimation(animation: LayoutAnimation): void {
    if (this.activeLayoutAnimation !== animation) {
      this.activeLayoutAnimation?.cancel();
    }
    this.activeLayoutAnimation = animation;
  }

  layoutAnimationDidFinish(animation: LayoutAnimation): void {
    if (this.activeLayoutAnimation === animation) {
      this.activeLayoutAnimation = undefined;
    }
  }

  onNodeNeedsUpdate(node: ViewNode): void {
    if (this.destroyed || node !== this.rootNode) {
      return;
    }
    if (this.isFlushing) {
      this.flushRequestedDuringFlush = true;
      return;
    }
    this.scheduleDirtyFlush();
  }

  enqueueLifecycleCallback(callback: () => void, priority: number): void {
    (this.pendingLifecycleCallbacks ??= []).push({
      callback,
      priority,
      sequence: this.nextLifecycleCallbackSequence++,
    });
    if (!this.isFlushing) {
      this.scheduleDirtyFlush();
    }
  }

  enqueuePostLayoutCallback(callback: () => void): void {
    this.layoutObserverController.enqueuePostLayoutCallback(callback);
  }

  flush(): void {
    if (this.destroyed) {
      return;
    }
    this.flushScheduled = false;
    if (this.isFlushing) {
      this.flushRequestedDuringFlush = true;
      return;
    }
    this.isFlushing = true;
    this.layoutObserverController.beginUpdate();
    try {
      do {
        this.flushRequestedDuringFlush = false;
        this.rootNode?.update(
          this.colorPaletteManager.getActiveColorPaletteName(),
          false,
          false,
          this.getCurrentAnimator(),
        );
        this.flushLifecycleCallbacks();
      } while (this.flushRequestedDuringFlush);
      this.validateActiveLayoutAnimationIfNeeded();
    } finally {
      this.isFlushing = false;
      this.schedulePendingLayoutRefresh();
      this.layoutObserverController.endUpdate();
    }
  }

  reapplyColorPalettesOnAllNodes(): void {
    this.rootNode?.markColorPaletteDirty(true);
    this.flush();
  }

  setElementVisibilityObserved(id: number, element: HTMLElement, observed: boolean): void {
    if (observed) {
      this.visibilityObserverController.observeElement(id, element);
    } else {
      this.visibilityObserverController.unobserveElement(id);
    }
  }

  destroy(): void {
    if (this.destroyed) {
      return;
    }
    this.removePaletteChangeListener();
    this.layoutRefreshNeeded = false;
    this.activeLayoutAnimationValidationNeeded = false;
    this.activeLayoutAnimation?.destroy();
    this.activeLayoutAnimation = undefined;
    this.animationController?.destroy();
    this.animationController = undefined;
    for (const animator of this.animatorStack) {
      animator.complete(true);
    }
    this.animatorStack.length = 0;
    for (const node of Array.from(this.nodesById.values())) {
      node.destroy();
    }
    this.layoutObserverController.destroy();
    this.visibilityObserverController.destroy();
    this.flushLifecycleCallbacks();
    this.destroyed = true;
    this.nodesById.clear();
    this.rootNode = null;
  }

  finishPendingRemoval(node: ViewNode): void {
    if (this.nodesById.get(node.id) !== node || !node.isPendingRemoval()) {
      return;
    }
    this.markLayoutRefreshNeeded();
    this.destroyNodeSubtree(node);
  }

  private scheduleDirtyFlush(): void {
    if (this.renderBatchDepth > 0 || this.flushScheduled) {
      return;
    }
    this.flushScheduled = true;
    Promise.resolve().then(() => {
      this.flushScheduled = false;
      this.flush();
    });
  }

  private getOrCreateAnimationController(): AnimationController {
    this.animationController ??= new AnimationController();
    return this.animationController;
  }

  private getCurrentAnimator(): Animator | undefined {
    return this.animatorStack[this.animatorStack.length - 1];
  }

  private requestActiveLayoutAnimationValidation(): void {
    if (!this.activeLayoutAnimation) {
      return;
    }
    this.activeLayoutAnimationValidationNeeded = true;
    if (!this.isFlushing) {
      this.scheduleDirtyFlush();
    }
  }

  private validateActiveLayoutAnimationIfNeeded(): void {
    if (!this.activeLayoutAnimationValidationNeeded) {
      return;
    }
    this.activeLayoutAnimationValidationNeeded = false;
    this.activeLayoutAnimation?.cancelAnimationsWithChangedFrames();
  }

  private markLayoutRefreshNeeded(): void {
    this.layoutRefreshNeeded = true;
  }

  private schedulePendingLayoutRefresh(): void {
    if (!this.layoutRefreshNeeded) {
      return;
    }
    this.layoutRefreshNeeded = false;
    this.layoutObserverController.scheduleRefresh();
  }

  private onLayoutPassCommitted(): void {
    this.visibilityObserverController.scheduleRefresh(true);
  }

  private flushLifecycleCallbacks(): void {
    const callbacks = this.pendingLifecycleCallbacks;
    if (!callbacks) {
      return;
    }
    callbacks.sort((a, b) => a.priority - b.priority || a.sequence - b.sequence);
    for (let i = 0; i < callbacks.length; i++) {
      callbacks[i].callback();
    }
    callbacks.length = 0;
  }

  private getNodeOrThrow(id: number): ViewNode {
    const node = this.nodesById.get(id);
    if (!node) {
      throw new Error(`ViewNode is missing, id: ${id}`);
    }
    return node;
  }

  private destroyNodeSubtree(node: ViewNode): void {
    for (const child of node.getChildrenSnapshot()) {
      this.destroyNodeSubtree(child);
    }
    this.destroySingleNode(node);
  }

  private destroySingleNode(node: ViewNode): void {
    if (this.rootNode === node) {
      this.rootNode = null;
    }
    this.nodesById.delete(node.id);
    this.nodeIdByHtmlElement.delete(node.htmlElement);
    this.layoutObserverController.destroyElement(node.id);
    this.visibilityObserverController.destroyElement(node.id);
    node.destroy();
  }
}
