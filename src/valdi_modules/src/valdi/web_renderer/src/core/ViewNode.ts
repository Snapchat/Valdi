import type { AnimationAppearanceAttributes } from 'valdi_core/src/AnimationOptions';
import type { ElementFrame } from 'valdi_tsx/src/Geometry';
import { KeyAnimation } from '../animations/KeyAnimation';
import type { LayoutAnimation, LayoutFrame } from '../animations/LayoutAnimation';
import { AttributesApplier, AttributeSetResult } from '../attributes/AttributesApplier';
import {
  getAppearanceAttributeOwner,
  getNativeOverrideAttributeOwner,
  type AttributeOwner,
} from '../attributes/AttributeOwner';
import { MIN_VISIBLE_CHANGE_ALPHA } from '../attributes/AttributesBinder';
import type { Animator } from '../animations/Animator';
import type {
  AnyElementClass,
  AttributeApplierContext,
  AttributeUpdatedExternallyDelegate,
  ElementLayoutObserver,
  LayoutAnimationSizeApplier,
} from './ElementClass';
import { ColorPaletteManager } from './Palette';
import type { ViewNodeTree } from './ViewNodeTree';

export interface ViewNodeDebugSnapshot {
  id: string;
  tag: string;
  element: {
    id: number;
    attributes: Record<string, unknown>;
    dom: {
      attributes: Record<string, string>;
      tagName: string;
      textContent?: string;
    };
  };
  bounds: {
    x: number;
    y: number;
    width: number;
    height: number;
  };
  state?: Record<string, unknown>;
  children: ViewNodeDebugSnapshot[];
}

const CHILD_COLOR_PALETTE_DIRTY = 1;
const CHILD_COLOR_PALETTE_FORCE_UPDATE = 2;
const NEEDS_ATTRIBUTE_UPDATE = 1;
const NEEDS_DESCENDANT_UPDATE = 2;
const NEEDS_COLOR_PALETTE_UPDATE = 4;
const NEEDS_FORCED_COLOR_PALETTE_UPDATE = 8;
const VIEW_CREATE_LIFECYCLE_CALLBACK_PRIORITY = 0;
const VIEW_CHANGE_LIFECYCLE_CALLBACK_PRIORITY = 1;
const VIEW_DESTROY_LIFECYCLE_CALLBACK_PRIORITY = 2;
const ENTER_APPEARANCE_CLEANUP_KEY = 'enterAppearanceCleanup';
const PENDING_REMOVAL_KEY = 'pendingRemoval';
const TRANSFORM_COMPOSITE_ATTRIBUTE = 'transformComposite';

interface AppearanceAnimationState {
  enterAnimator?: Animator;
  pendingRemovalAnimator?: Animator;
  animation?: KeyAnimation;
  pendingRemoval?: boolean;
  pendingRemovalChildCount?: number;
}

export class ViewNode implements AttributeApplierContext {
  readonly htmlElement: HTMLElement;
  private readonly children: ViewNode[] = [];
  private readonly attributesApplier: AttributesApplier;
  private parent: ViewNode | null = null;

  private state?: Record<string, unknown>;
  private attributeUpdatedExternallyDelegate?: AttributeUpdatedExternallyDelegate;
  private cleanupCallbacks?: Array<() => void>;
  private colorPaletteNameOverride: string | undefined;
  private resolvedColorPaletteName!: string;
  private needsUpdateFlag = 0;
  private attached = false;
  private viewCreateEmitted = false;
  private lastViewChangeCallback?: Function;
  private lastViewChangeAttached?: boolean;
  private destroyed = false;
  private animationsEnabled = true;
  private appearanceAnimationState?: AppearanceAnimationState;
  private layoutAnimation?: LayoutAnimation;

  constructor(
    readonly id: number,
    readonly viewClass: string,
    private readonly elementClass: AnyElementClass,
    private readonly tree: ViewNodeTree,
    private readonly colorPaletteManager: ColorPaletteManager,
    attributeUpdatedExternallyDelegate?: AttributeUpdatedExternallyDelegate,
  ) {
    this.attributeUpdatedExternallyDelegate = attributeUpdatedExternallyDelegate;
    this.appearanceAnimationState = undefined;
    this.layoutAnimation = undefined;
    this.htmlElement = elementClass.createElement(id, viewClass);
    this.attributesApplier = new AttributesApplier(id, elementClass);
  }

  makeRoot(root: HTMLElement | ShadowRoot): void {
    this.parent?.removeChild(this);
    this.parent = null;
    root.replaceChildren(this.htmlElement);
    this.propagateCurrentDirtyStateToAncestors();
    this.markColorPaletteDirty(false);
    this.setAttached(true);
  }

  move(parent: ViewNode, index: number): void {
    this.parent?.removeChild(this);
    this.parent = parent;
    const physicalIndex = parent.getPhysicalChildIndex(index);
    const referenceElement = parent.children[physicalIndex]?.htmlElement ?? null;
    parent.children.splice(physicalIndex, 0, this);
    if (this.isPendingRemoval()) {
      parent.incrementPendingRemovalChildCount();
    }
    parent.htmlElement.insertBefore(this.htmlElement, referenceElement);
    this.propagateCurrentDirtyStateToAncestors();
    this.markColorPaletteDirty(false);
    this.setAttached(true);
  }

  destroy(): void {
    if (this.destroyed) {
      return;
    }
    this.layoutAnimation?.retireNode(this);
    this.destroyed = true;
    this.appearanceAnimationState?.animation?.cancel();
    this.attributesApplier.cancelAnimations();
    this.setAttached(false);
    this.enqueueViewDestroyIfNeeded();
    const cleanupCallbacks = this.cleanupCallbacks;
    if (cleanupCallbacks) {
      for (let i = 0; i < cleanupCallbacks.length; i++) {
        cleanupCallbacks[i]();
      }
      cleanupCallbacks.length = 0;
    }
    this.parent?.removeChild(this);
    this.parent = null;
    this.elementClass.destroy(this.htmlElement);
    this.htmlElement.remove();
  }

  setAttribute(attributeName: string, value: unknown): boolean {
    const actualAttributeName = attributeName.startsWith('$') ? attributeName.substring(1) : attributeName;
    if (actualAttributeName === 'observeVisibility') {
      this.tree.setElementVisibilityObserved(this.id, this.htmlElement, !!value);
      return false;
    }
    if (actualAttributeName === 'onViewChange') {
      this.lastViewChangeCallback = undefined;
      this.lastViewChangeAttached = undefined;
    }
    const result = this.attributesApplier.setAttribute(attributeName, value);
    if (result !== AttributeSetResult.Unchanged) {
      this.markNeedsUpdate();
    }
    return result === AttributeSetResult.ChangedAndInvalidatesLayout;
  }

  setAttributeForOwner(attributeName: string, owner: AttributeOwner, value: unknown): boolean {
    const result = this.attributesApplier.setAttributeForOwner(attributeName, owner, value);
    if (result !== AttributeSetResult.Unchanged) {
      this.markNeedsUpdate();
    }
    return result === AttributeSetResult.ChangedAndInvalidatesLayout;
  }

  requestAnimatedAppearance(animator: Animator): void {
    const attributes = animator.options.appearanceBehavior?.enterAttributes;
    if (!attributes || !hasAppearanceAttributes(attributes)) {
      return;
    }
    this.getOrCreateAppearanceAnimationState().enterAnimator = animator;
    this.markNeedsUpdate();
  }

  requestAnimatedDisappearance(animator: Animator): boolean {
    const state = this.appearanceAnimationState;
    if (state?.pendingRemoval) {
      return true;
    }
    if (state?.enterAnimator === animator) {
      state.enterAnimator = undefined;
      this.releaseAppearanceAnimationStateIfEmpty();
      return false;
    }
    const attributes = animator.options.appearanceBehavior?.exitAttributes;
    if (!this.parent || !attributes || !hasAppearanceAttributes(attributes)) {
      return false;
    }
    const pendingState = this.getOrCreateAppearanceAnimationState();
    pendingState.pendingRemoval = true;
    pendingState.pendingRemovalAnimator = animator;
    this.completeLayoutAnimationsInSubtree();
    this.parent.incrementPendingRemovalChildCount();
    this.markNeedsUpdate();
    return true;
  }

  isPendingRemoval(): boolean {
    return this.appearanceAnimationState?.pendingRemoval === true;
  }

  resolveColor(value: string): string {
    return this.colorPaletteManager.resolveColor(this.resolvedColorPaletteName, value);
  }

  setColorPalette(colorPaletteName: string | undefined): void {
    const nextColorPaletteName = colorPaletteName && colorPaletteName.length > 0 ? colorPaletteName : undefined;
    if (this.colorPaletteNameOverride === nextColorPaletteName) {
      return;
    }
    this.colorPaletteNameOverride = nextColorPaletteName;
    this.markColorPaletteDirty(false);
  }

  getState<T>(key: string): T | undefined {
    return this.state?.[key] as T | undefined;
  }

  setState(key: string, value: unknown): void {
    if (value === undefined && !this.state) {
      return;
    }
    this.state ??= {};
    this.state[key] = value;
  }

  getViewAttributeElement(): HTMLElement {
    return this.elementClass.getViewAttributeElement(this.htmlElement, this);
  }

  markColorPaletteDirty(forceColorPaletteUpdate: boolean): void {
    this.markUpdateFlag(
      forceColorPaletteUpdate
        ? NEEDS_COLOR_PALETTE_UPDATE | NEEDS_FORCED_COLOR_PALETTE_UPDATE
        : NEEDS_COLOR_PALETTE_UPDATE,
    );
  }

  update(
    parentPaletteName: string,
    inheritedColorPaletteDirty: boolean,
    inheritedForceColorPaletteUpdate: boolean,
    animator: Animator | undefined,
  ): void {
    const inheritedUpdateFlag =
      (inheritedColorPaletteDirty ? NEEDS_COLOR_PALETTE_UPDATE : 0) |
      (inheritedForceColorPaletteUpdate ? NEEDS_FORCED_COLOR_PALETTE_UPDATE : 0);
    if (!(this.needsUpdateFlag | inheritedUpdateFlag)) {
      return;
    }

    let enterAppearanceAttributes: AnimationAppearanceAttributes | undefined;
    let pendingRemovalAnimator: Animator | undefined;
    let initialAnimator = animator;
    const appearanceAnimationState = this.appearanceAnimationState;
    if (appearanceAnimationState) {
      enterAppearanceAttributes = this.takeEnterAppearanceAttributes(appearanceAnimationState, animator);
      if (enterAppearanceAttributes) {
        this.setAppearanceAttributes(enterAppearanceAttributes, false);
      }
      pendingRemovalAnimator = appearanceAnimationState.pendingRemoval
        ? appearanceAnimationState.pendingRemovalAnimator
        : undefined;
      if (enterAppearanceAttributes || pendingRemovalAnimator) {
        initialAnimator = undefined;
      }
    }

    let childColorPaletteUpdateFlags = 0;
    if (
      (this.needsUpdateFlag | inheritedUpdateFlag) &
      (NEEDS_COLOR_PALETTE_UPDATE | NEEDS_FORCED_COLOR_PALETTE_UPDATE)
    ) {
      childColorPaletteUpdateFlags = this.resolveColorPaletteForUpdatePass(
        parentPaletteName,
        inheritedForceColorPaletteUpdate,
      );
    }
    childColorPaletteUpdateFlags |= this.flushAttributesForUpdate(parentPaletteName, initialAnimator);

    const resolvedAnimator = animator && this.isAnimationEnabled() ? animator : undefined;
    let suppressDescendantAnimations = false;
    if (enterAppearanceAttributes) {
      suppressDescendantAnimations = true;
      childColorPaletteUpdateFlags |= this.startAnimatedAppearance(
        parentPaletteName,
        enterAppearanceAttributes,
        resolvedAnimator,
      );
    }

    if (pendingRemovalAnimator) {
      suppressDescendantAnimations = true;
      if (!resolvedAnimator || resolvedAnimator !== pendingRemovalAnimator) {
        this.tree.finishPendingRemoval(this);
        return;
      }
      childColorPaletteUpdateFlags |= this.startAnimatedDisappearance(parentPaletteName, resolvedAnimator);
    }

    if (this.needsUpdateFlag & NEEDS_DESCENDANT_UPDATE || childColorPaletteUpdateFlags !== 0) {
      this.needsUpdateFlag &= ~NEEDS_DESCENDANT_UPDATE;
      const childColorPaletteDirty = (childColorPaletteUpdateFlags & CHILD_COLOR_PALETTE_DIRTY) !== 0;
      const childForceColorPaletteUpdate = (childColorPaletteUpdateFlags & CHILD_COLOR_PALETTE_FORCE_UPDATE) !== 0;
      const childAnimator = suppressDescendantAnimations ? undefined : resolvedAnimator;
      for (let index = 0; index < this.children.length; ) {
        const child = this.children[index];
        child.update(
          this.resolvedColorPaletteName,
          childColorPaletteDirty,
          childForceColorPaletteUpdate,
          childAnimator,
        );
        if (this.children[index] === child) {
          index++;
        }
      }
      this.needsUpdateFlag &= ~NEEDS_DESCENDANT_UPDATE;
    }
  }

  getChildrenSnapshot(): ViewNode[] {
    return Array.from(this.children);
  }

  getParent(): ViewNode | null {
    return this.parent;
  }

  isDestroyed(): boolean {
    return this.destroyed;
  }

  makeLayoutAnimationSizeApplier(finalWidth: number, finalHeight: number): LayoutAnimationSizeApplier | undefined {
    return this.elementClass.makeLayoutAnimationSizeApplier(this.htmlElement, this, finalWidth, finalHeight);
  }

  setLayoutAnimation(animation: LayoutAnimation): void {
    this.layoutAnimation = animation;
  }

  clearLayoutAnimation(animation: LayoutAnimation): void {
    if (this.layoutAnimation === animation) {
      this.layoutAnimation = undefined;
    }
  }

  getLayoutAnimationFrame(current: boolean): LayoutFrame | undefined {
    return this.layoutAnimation?.getFrame(this, current);
  }

  getDebugSnapshot(): ViewNodeDebugSnapshot {
    const rect = this.htmlElement.getBoundingClientRect();
    const domAttributes: Record<string, string> = {};
    for (let i = 0; i < this.htmlElement.attributes.length; i++) {
      const attribute = this.htmlElement.attributes.item(i);
      if (attribute) {
        domAttributes[attribute.name] = attribute.value;
      }
    }

    const snapshot: ViewNodeDebugSnapshot = {
      id: String(this.id),
      tag: this.viewClass,
      element: {
        id: this.id,
        attributes: this.attributesApplier.getDebugAttributes(),
        dom: {
          attributes: domAttributes,
          tagName: this.htmlElement.tagName.toLowerCase(),
        },
      },
      bounds: {
        x: rect.left,
        y: rect.top,
        width: rect.width,
        height: rect.height,
      },
      children: this.children.filter(child => !child.isPendingRemoval()).map(child => child.getDebugSnapshot()),
    };

    const textContent = this.htmlElement.childElementCount === 0 ? this.htmlElement.textContent?.trim() : undefined;
    if (textContent) {
      snapshot.element.dom.textContent = truncateDebugText(textContent);
    }

    const state = this.getDebugDomState();
    if (Object.keys(state).length > 0) {
      snapshot.state = state;
    }
    return snapshot;
  }

  private resolveColorPaletteForUpdatePass(
    parentPaletteName: string,
    inheritedForceColorPaletteUpdate: boolean,
  ): number {
    const forceColorPaletteUpdate =
      !!(this.needsUpdateFlag & NEEDS_FORCED_COLOR_PALETTE_UPDATE) || inheritedForceColorPaletteUpdate;
    const changed = this.resolveColorPalette(parentPaletteName, forceColorPaletteUpdate);
    this.needsUpdateFlag &= ~(NEEDS_COLOR_PALETTE_UPDATE | NEEDS_FORCED_COLOR_PALETTE_UPDATE);
    let flags = 0;
    if (changed) {
      flags |= CHILD_COLOR_PALETTE_DIRTY;
    }
    if (forceColorPaletteUpdate) {
      flags |= CHILD_COLOR_PALETTE_FORCE_UPDATE;
    }
    return flags;
  }

  private resolveColorPalette(parentPaletteName: string, forceColorPaletteUpdate: boolean): boolean {
    const nextPaletteName = this.colorPaletteNameOverride ?? parentPaletteName;
    const changed = nextPaletteName !== this.resolvedColorPaletteName;
    this.resolvedColorPaletteName = nextPaletteName;
    if ((changed || forceColorPaletteUpdate) && this.attributesApplier.markColorDependentAttributesDirty()) {
      this.needsUpdateFlag |= NEEDS_ATTRIBUTE_UPDATE;
    }
    return changed;
  }

  private flushAttributesForUpdate(parentPaletteName: string, animator: Animator | undefined): number {
    let childColorPaletteUpdateFlags = 0;
    while (this.needsUpdateFlag & NEEDS_ATTRIBUTE_UPDATE) {
      this.needsUpdateFlag &= ~NEEDS_ATTRIBUTE_UPDATE;
      this.attributesApplier.flush(this.htmlElement, this, animator);
      if (this.needsUpdateFlag & (NEEDS_COLOR_PALETTE_UPDATE | NEEDS_FORCED_COLOR_PALETTE_UPDATE)) {
        childColorPaletteUpdateFlags |= this.resolveColorPaletteForUpdatePass(parentPaletteName, false);
      }
    }
    return childColorPaletteUpdateFlags;
  }

  private takeEnterAppearanceAttributes(
    state: AppearanceAnimationState,
    animator: Animator | undefined,
  ): AnimationAppearanceAttributes | undefined {
    const enterAnimator = state.enterAnimator;
    if (!enterAnimator) {
      return undefined;
    }
    if (!animator) {
      state.enterAnimator = undefined;
      this.releaseAppearanceAnimationStateIfEmpty();
      return undefined;
    }
    if (enterAnimator !== animator) {
      return undefined;
    }
    state.enterAnimator = undefined;
    this.releaseAppearanceAnimationStateIfEmpty();
    return animator.options.appearanceBehavior!.enterAttributes!;
  }

  private freezeForPendingRemoval(): void {
    const owner = getNativeOverrideAttributeOwner();
    this.setAttributeForOwner('position', owner, 'absolute');
    this.setAttributeForOwner('left', owner, this.htmlElement.offsetLeft);
    this.setAttributeForOwner('top', owner, this.htmlElement.offsetTop);
    this.setAttributeForOwner('width', owner, this.htmlElement.offsetWidth);
    this.setAttributeForOwner('height', owner, this.htmlElement.offsetHeight);
    this.setAttributeForOwner('marginLeft', owner, 0);
    this.setAttributeForOwner('marginTop', owner, 0);
  }

  private startAnimatedAppearance(
    parentPaletteName: string,
    attributes: AnimationAppearanceAttributes,
    animator: Animator | undefined,
  ): number {
    if (animator) {
      this.removeEnterAppearanceAttributes(attributes);
    } else {
      this.removeAppearanceAttributes(attributes);
    }
    const childColorPaletteUpdateFlags = this.flushAttributesForUpdate(parentPaletteName, animator);
    if (animator && hasAppearanceTransform(attributes)) {
      const animation = new EnterAppearanceCleanupAnimation(this);
      this.setAppearanceAnimation(animation);
      animator.addAnimation(this, ENTER_APPEARANCE_CLEANUP_KEY, animation);
    }
    return childColorPaletteUpdateFlags;
  }

  private startAnimatedDisappearance(parentPaletteName: string, animator: Animator): number {
    this.appearanceAnimationState!.pendingRemovalAnimator = undefined;
    this.appearanceAnimationState!.animation?.cancel();
    this.freezeForPendingRemoval();
    let childColorPaletteUpdateFlags = this.flushAttributesForUpdate(parentPaletteName, undefined);
    this.setAppearanceAttributes(animator.options.appearanceBehavior!.exitAttributes!, false);
    childColorPaletteUpdateFlags |= this.flushAttributesForUpdate(parentPaletteName, animator);
    const animation = new PendingRemovalAnimation(this);
    this.setAppearanceAnimation(animation);
    animator.addAnimation(this, PENDING_REMOVAL_KEY, animation);
    return childColorPaletteUpdateFlags;
  }

  private setAppearanceAttributes(attributes: AnimationAppearanceAttributes, identityTransform: boolean): void {
    const owner = getAppearanceAttributeOwner();
    if (attributes.opacity !== undefined) {
      this.setAttributeForOwner('opacity', owner, attributes.opacity);
    }
    if (hasAppearanceTransform(attributes)) {
      this.setAttributeForOwner(
        TRANSFORM_COMPOSITE_ATTRIBUTE,
        owner,
        makeAppearanceTransformValues(attributes, identityTransform),
      );
    }
  }

  private removeEnterAppearanceAttributes(attributes: AnimationAppearanceAttributes): void {
    const owner = getAppearanceAttributeOwner();
    if (attributes.opacity !== undefined) {
      this.setAttributeForOwner('opacity', owner, undefined);
    }
    if (hasAppearanceTransform(attributes)) {
      this.setAttributeForOwner(TRANSFORM_COMPOSITE_ATTRIBUTE, owner, makeAppearanceTransformValues(attributes, true));
    }
  }

  private removeAppearanceAttributes(attributes: AnimationAppearanceAttributes): void {
    const owner = getAppearanceAttributeOwner();
    if (attributes.opacity !== undefined) {
      this.setAttributeForOwner('opacity', owner, undefined);
    }
    if (hasAppearanceTransform(attributes)) {
      this.setAttributeForOwner(TRANSFORM_COMPOSITE_ATTRIBUTE, owner, undefined);
    }
  }

  private setAppearanceAnimation(animation: KeyAnimation): void {
    const state = this.getOrCreateAppearanceAnimationState();
    const previousAnimation = state.animation;
    state.animation = animation;
    if (previousAnimation && previousAnimation !== animation) {
      previousAnimation.cancel();
    }
  }

  finishEnterAppearanceAnimation(animation: EnterAppearanceCleanupAnimation): void {
    if (this.appearanceAnimationState?.animation !== animation) {
      return;
    }
    this.setAttributeForOwner(TRANSFORM_COMPOSITE_ATTRIBUTE, getAppearanceAttributeOwner(), undefined);
  }

  finishPendingRemoval(animation: PendingRemovalAnimation): void {
    if (this.appearanceAnimationState?.animation === animation) {
      this.tree.finishPendingRemoval(this);
    }
  }

  unregisterAppearanceAnimation(animation: KeyAnimation): void {
    const state = this.appearanceAnimationState;
    if (state?.animation !== animation) {
      return;
    }
    state.animation = undefined;
    this.releaseAppearanceAnimationStateIfEmpty();
  }

  private getOrCreateAppearanceAnimationState(): AppearanceAnimationState {
    return (this.appearanceAnimationState ??= {});
  }

  private releaseAppearanceAnimationStateIfEmpty(): void {
    const state = this.appearanceAnimationState;
    if (
      state &&
      !state.enterAnimator &&
      !state.pendingRemovalAnimator &&
      !state.animation &&
      !state.pendingRemoval &&
      !state.pendingRemovalChildCount
    ) {
      this.appearanceAnimationState = undefined;
    }
  }

  private getPhysicalChildIndex(index: number): number {
    if (!this.appearanceAnimationState?.pendingRemovalChildCount) {
      return index;
    }
    let liveIndex = 0;
    for (let physicalIndex = 0; physicalIndex < this.children.length; physicalIndex++) {
      if (this.children[physicalIndex].isPendingRemoval()) {
        continue;
      }
      if (liveIndex === index) {
        return physicalIndex;
      }
      liveIndex++;
    }
    return this.children.length;
  }

  private incrementPendingRemovalChildCount(): void {
    const state = this.getOrCreateAppearanceAnimationState();
    state.pendingRemovalChildCount = (state.pendingRemovalChildCount ?? 0) + 1;
  }

  private decrementPendingRemovalChildCount(): void {
    const state = this.appearanceAnimationState!;
    state.pendingRemovalChildCount = state.pendingRemovalChildCount! - 1;
    this.releaseAppearanceAnimationStateIfEmpty();
  }

  addCleanup(callback: () => void): void {
    this.cleanupCallbacks ??= [];
    this.cleanupCallbacks.push(callback);
  }

  enqueuePostLayoutCallback(callback: () => void): void {
    this.tree.enqueuePostLayoutCallback(() => {
      if (!this.destroyed && this.attached) {
        callback();
      }
    });
  }

  getLayoutObserver(attributeName: string): ElementLayoutObserver | undefined {
    return this.tree.getElementLayoutObserver(this.id, attributeName);
  }

  setLayoutObserver(attributeName: string, observer: ElementLayoutObserver | undefined): void {
    this.tree.setElementLayoutObserver(
      this.id,
      this.viewClass,
      this.htmlElement,
      this.attached,
      attributeName,
      observer,
    );
  }

  requestLayoutPass(): void {
    this.tree.requestLayoutPass();
  }

  getChildHtmlElement(index: number): HTMLElement | undefined {
    return this.children[this.getPhysicalChildIndex(index)]?.htmlElement;
  }

  setOnLayoutCallback(callback: ((frame: ElementFrame) => void) | undefined): void {
    this.tree.setElementOnLayoutCallback(this.id, this.viewClass, this.htmlElement, this.attached, callback);
  }

  onAttributeUpdatedExternally(attributeName: string, attributeValue: unknown): void {
    this.attributeUpdatedExternallyDelegate?.onAttributeUpdatedExternally(this.id, attributeName, attributeValue);
  }

  emitCurrentViewCreate(callback: Function): void {
    if (this.attached) {
      this.enqueueViewCreate(callback);
    }
  }

  emitCurrentViewChange(): void {
    this.enqueueViewChangeIfNeeded(this.attached);
  }

  isAnimationEnabled(): boolean {
    return this.animationsEnabled;
  }

  setAnimationsEnabled(enabled: boolean): void {
    if (this.animationsEnabled === enabled) {
      return;
    }
    this.animationsEnabled = enabled;
    if (!enabled) {
      this.completeAnimationsInSubtree();
    }
  }

  private getAttribute(attributeName: string): unknown {
    return this.attributesApplier.getAttribute(attributeName);
  }

  private getDebugDomState(): Record<string, unknown> {
    const state: Record<string, unknown> = {};
    if (this.htmlElement.scrollLeft !== 0 || this.htmlElement.scrollTop !== 0) {
      state.scrollLeft = this.htmlElement.scrollLeft;
      state.scrollTop = this.htmlElement.scrollTop;
    }
    if ('value' in this.htmlElement && typeof this.htmlElement.value === 'string') {
      state.value = this.htmlElement.value;
    }
    if ('checked' in this.htmlElement && typeof this.htmlElement.checked === 'boolean') {
      state.checked = this.htmlElement.checked;
    }
    if ('disabled' in this.htmlElement && typeof this.htmlElement.disabled === 'boolean') {
      state.disabled = this.htmlElement.disabled;
    }
    return state;
  }

  private getAttributeFunction(attributeName: string): Function | undefined {
    const value = this.getAttribute(attributeName);
    return typeof value === 'function' ? value : undefined;
  }

  private removeChild(child: ViewNode): void {
    const index = this.children.indexOf(child);
    if (index >= 0) {
      this.children.splice(index, 1);
      if (child.isPendingRemoval()) {
        this.decrementPendingRemovalChildCount();
      }
    }
  }

  private completeAnimationsInSubtree(): void {
    this.attributesApplier.completeAnimations();
    this.appearanceAnimationState?.animation?.complete();
    this.layoutAnimation?.completeNode(this);
    if (this.destroyed) {
      return;
    }
    for (let index = 0; index < this.children.length; ) {
      const child = this.children[index];
      child.completeAnimationsInSubtree();
      if (this.children[index] === child) {
        index++;
      }
    }
  }

  private completeLayoutAnimationsInSubtree(): void {
    this.layoutAnimation?.completeNode(this);
    for (const child of this.children) {
      child.completeLayoutAnimationsInSubtree();
    }
  }

  private propagateCurrentDirtyStateToAncestors(): void {
    if (this.needsUpdateFlag) {
      this.notifyNeedsUpdate();
    }
  }

  private markNeedsUpdate(): void {
    this.markUpdateFlag(NEEDS_ATTRIBUTE_UPDATE);
  }

  private markDescendantNeedsUpdate(): void {
    this.markUpdateFlag(NEEDS_DESCENDANT_UPDATE);
  }

  private markUpdateFlag(updateFlag: number): void {
    const previousUpdateFlag = this.needsUpdateFlag;
    const nextUpdateFlag = previousUpdateFlag | updateFlag;
    if (previousUpdateFlag === nextUpdateFlag) {
      return;
    }
    this.needsUpdateFlag = nextUpdateFlag;
    if (!previousUpdateFlag) {
      this.notifyNeedsUpdate();
    }
  }

  private notifyNeedsUpdate(): void {
    if (this.parent) {
      this.parent.markDescendantNeedsUpdate();
    } else {
      this.tree.onNodeNeedsUpdate(this);
    }
  }

  private setAttached(attached: boolean): void {
    if (this.attached === attached) {
      return;
    }
    this.attached = attached;
    this.tree.setElementLayoutAttached(this.id, attached);
    if (attached) {
      this.queueViewCreateIfNeeded();
    }
    this.enqueueViewChangeIfNeeded(attached);
  }

  private queueViewCreateIfNeeded(): void {
    const onViewCreate = this.getAttributeFunction('onViewCreate');
    if (onViewCreate) {
      this.enqueueViewCreate(onViewCreate);
    }
  }

  private enqueueViewCreate(callback: Function): void {
    if (this.viewCreateEmitted) {
      return;
    }
    this.viewCreateEmitted = true;
    this.tree.enqueueLifecycleCallback(() => {
      this.invokeLifecycleCallback('onViewCreate', callback);
    }, VIEW_CREATE_LIFECYCLE_CALLBACK_PRIORITY);
  }

  private enqueueViewChangeIfNeeded(attached: boolean): void {
    const onViewChange = this.getAttributeFunction('onViewChange');
    if (onViewChange) {
      if (this.lastViewChangeCallback === onViewChange && this.lastViewChangeAttached === attached) {
        return;
      }
      this.lastViewChangeCallback = onViewChange;
      this.lastViewChangeAttached = attached;
      this.tree.enqueueLifecycleCallback(() => {
        this.invokeLifecycleCallback('onViewChange', onViewChange, { type: attached ? 'Attached' : 'Detached' });
      }, VIEW_CHANGE_LIFECYCLE_CALLBACK_PRIORITY);
    }
  }

  private enqueueViewDestroyIfNeeded(): void {
    const onViewDestroy = this.getAttributeFunction('onViewDestroy');
    if (onViewDestroy) {
      this.tree.enqueueLifecycleCallback(() => {
        this.invokeLifecycleCallback('onViewDestroy', onViewDestroy);
      }, VIEW_DESTROY_LIFECYCLE_CALLBACK_PRIORITY);
    }
  }

  private invokeLifecycleCallback(attributeName: string, callback: Function, ...args: unknown[]): void {
    try {
      callback(...args);
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      console.error(
        `Valdi web renderer failed to call '${attributeName}' on node ${this.id} (${this.elementClass.className}): ${message}`,
      );
    }
  }
}

function truncateDebugText(value: string): string {
  const maxLength = 240;
  return value.length > maxLength ? `${value.slice(0, maxLength - 1)}...` : value;
}

class EnterAppearanceCleanupAnimation extends KeyAnimation {
  constructor(private readonly node: ViewNode) {
    super(MIN_VISIBLE_CHANGE_ALPHA);
  }

  override applyProgress(_progress: number): boolean {
    return true;
  }

  override applyFinalValue(): void {
    this.node.finishEnterAppearanceAnimation(this);
  }

  protected override didFinish(): void {
    this.node.unregisterAppearanceAnimation(this);
  }
}

class PendingRemovalAnimation extends KeyAnimation {
  constructor(private readonly node: ViewNode) {
    super(MIN_VISIBLE_CHANGE_ALPHA);
  }

  override applyProgress(_progress: number): boolean {
    return true;
  }

  override applyFinalValue(): void {
    this.node.finishPendingRemoval(this);
  }

  protected override didFinish(): void {
    this.node.unregisterAppearanceAnimation(this);
  }
}

function hasAppearanceAttributes(attributes: AnimationAppearanceAttributes): boolean {
  return attributes.opacity !== undefined || hasAppearanceTransform(attributes);
}

function hasAppearanceTransform(attributes: AnimationAppearanceAttributes): boolean {
  return (
    attributes.translationX !== undefined ||
    attributes.translationY !== undefined ||
    attributes.scaleX !== undefined ||
    attributes.scaleY !== undefined
  );
}

function makeAppearanceTransformValues(
  attributes: AnimationAppearanceAttributes,
  identity: boolean,
): ReadonlyArray<unknown> {
  const originX = (attributes.originX ?? 0.5) * 100;
  const originY = (attributes.originY ?? 0.5) * 100;
  const translationX = (identity ? 0 : (attributes.translationX ?? 0)) * 100;
  const translationY = (identity ? 0 : (attributes.translationY ?? 0)) * 100;
  return [
    `${originX}% ${originY}%`,
    undefined,
    `${translationX}%`,
    `${translationY}%`,
    identity ? 1 : (attributes.scaleX ?? 1),
    identity ? 1 : (attributes.scaleY ?? 1),
    0,
  ];
}
