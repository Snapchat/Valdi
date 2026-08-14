import { MIN_VISIBLE_CHANGE_PIXEL } from '../attributes/AttributesBinder';
import type { ViewNode } from '../core/ViewNode';
import type { ViewNodeTree } from '../core/ViewNodeTree';
import type { LayoutAnimationSizeApplier } from '../core/ElementClass';
import type { Animator } from './Animator';
import type { AnimatorCommitPreparation } from './AnimatorCommitPreparation';
import { KeyAnimation } from './KeyAnimation';

export interface LayoutFrame {
  x: number;
  y: number;
  width: number;
  height: number;
}

interface LayoutSnapshotEntry {
  node: ViewNode;
  parent: ViewNode | undefined;
  frame: LayoutFrame;
  animationsEnabled: boolean;
}

export interface LayoutSnapshot {
  readonly entries: LayoutSnapshotEntry[];
  readonly entriesByNode: Map<ViewNode, LayoutSnapshotEntry>;
}

interface TranslationProjection {
  x: number;
  y: number;
}

interface LayoutAnimationRecord {
  node: ViewNode | undefined;
  readonly startFrame: LayoutFrame;
  readonly endFrame: LayoutFrame;
  currentFrame: LayoutFrame;
  parent: LayoutAnimationRecord | undefined;
  readonly originalTranslate: string;
  readonly sizeApplier: LayoutAnimationSizeApplier | undefined;
  projection: TranslationProjection;
}

const LAYOUT_ANIMATION_KEY = 'layout';
const MINIMUM_SCALE = 0.0001;
const FRAME_EPSILON = 0.01;
const IDENTITY_PROJECTION: TranslationProjection = { x: 0, y: 0 };
const SUPPORTS_INDEPENDENT_TRANSFORMS =
  typeof CSS === 'undefined' ||
  typeof CSS.supports !== 'function' ||
  (CSS.supports('translate', '1px 1px') && CSS.supports('scale', '1 1'));

export class LayoutAnimationPass implements AnimatorCommitPreparation {
  constructor(
    private tree: ViewNodeTree | undefined,
    private initialSnapshot: LayoutSnapshot | undefined,
  ) {}

  prepareForCommit(animator: Animator): void {
    const tree = this.tree;
    const initialSnapshot = this.initialSnapshot;
    this.tree = undefined;
    this.initialSnapshot = undefined;
    if (!tree || !initialSnapshot) {
      return;
    }
    const animation = new LayoutAnimation(initialSnapshot, tree.captureLayoutAnimationSnapshot(), tree);
    if (animation.empty) {
      animation.cancel();
      return;
    }
    tree.setActiveLayoutAnimation(animation);
    animator.addAnimation(tree, LAYOUT_ANIMATION_KEY, animation);
  }

  cancel(): void {
    this.tree = undefined;
    this.initialSnapshot = undefined;
  }
}

export class LayoutAnimation extends KeyAnimation {
  private records: LayoutAnimationRecord[];
  private readonly recordsByNode = new Map<ViewNode, LayoutAnimationRecord>();
  private destroying = false;

  constructor(
    initialSnapshot: LayoutSnapshot,
    finalSnapshot: LayoutSnapshot,
    private tree: ViewNodeTree | undefined,
  ) {
    super(MIN_VISIBLE_CHANGE_PIXEL);
    this.records = SUPPORTS_INDEPENDENT_TRANSFORMS
      ? makeAnimationRecords(initialSnapshot, finalSnapshot, this.recordsByNode)
      : [];
    for (const record of this.records) {
      record.node!.setLayoutAnimation(this);
    }
  }

  get empty(): boolean {
    return this.records.length === 0;
  }

  getFrame(node: ViewNode, current: boolean): LayoutFrame | undefined {
    const record = this.recordsByNode.get(node);
    return record ? (current ? record.currentFrame : record.endFrame) : undefined;
  }

  cancelAnimationsWithChangedFrames(): void {
    let removedAnimation = false;
    for (const record of this.records) {
      const node = record.node;
      if (!node || !framesDiffer(record.endFrame, measureLogicalFrame(node.htmlElement))) {
        continue;
      }
      restoreProjection(record);
      this.recordsByNode.delete(node);
      node.clearLayoutAnimation(this);
      record.node = undefined;
      removedAnimation = true;
    }
    if (this.recordsByNode.size === 0) {
      if (removedAnimation) {
        this.cancel();
      }
      return;
    }
    const parentChanged = this.resolveActiveParents();
    if (removedAnimation || parentChanged) {
      this.applyCurrentFrames();
    }
  }

  completeNode(node: ViewNode): void {
    this.removeNode(node, true);
  }

  retireNode(node: ViewNode): void {
    this.removeNode(node, false);
  }

  destroy(): void {
    if (this.finished) {
      return;
    }
    this.destroying = true;
    for (const record of this.records) {
      const node = record.node;
      if (node) {
        node.clearLayoutAnimation(this);
        this.recordsByNode.delete(node);
        record.node = undefined;
      }
    }
    this.records.length = 0;
    this.cancel();
  }

  override applyProgress(progress: number): boolean {
    for (const record of this.records) {
      const node = record.node;
      if (!node) {
        continue;
      }
      const start = record.startFrame;
      const end = record.endFrame;
      const desired = record.currentFrame;
      desired.x = interpolate(start.x, end.x, progress);
      desired.y = interpolate(start.y, end.y, progress);
      desired.width = record.sizeApplier ? interpolate(start.width, end.width, progress) : end.width;
      desired.height = record.sizeApplier ? interpolate(start.height, end.height, progress) : end.height;
    }
    this.applyCurrentFrames();
    return true;
  }

  override applyFinalValue(): void {
    this.clearAllProjections();
  }

  protected override didFinish(): void {
    if (!this.destroying) {
      this.clearAllProjections();
    }
    const tree = this.tree;
    this.tree = undefined;
    tree?.layoutAnimationDidFinish(this);
  }

  private applyCurrentFrames(): void {
    for (const record of this.records) {
      const node = record.node;
      if (!node) {
        continue;
      }
      const end = record.endFrame;
      const desired = record.currentFrame;
      const parentProjection = record.parent?.projection ?? IDENTITY_PROJECTION;
      const residualTranslateX = desired.x - end.x - parentProjection.x;
      const residualTranslateY = desired.y - end.y - parentProjection.y;
      const scaleX = record.sizeApplier ? Math.max(MINIMUM_SCALE, desired.width / end.width) : 1;
      const scaleY = record.sizeApplier ? Math.max(MINIMUM_SCALE, desired.height / end.height) : 1;
      const correction = record.sizeApplier?.apply(scaleX, scaleY) ?? IDENTITY_PROJECTION;

      const style = node.htmlElement.style;
      style.setProperty('translate', `${residualTranslateX + correction.x}px ${residualTranslateY + correction.y}px`);
      record.projection.x = desired.x - end.x;
      record.projection.y = desired.y - end.y;
    }
  }

  private resolveActiveParents(): boolean {
    let changed = false;
    for (const record of this.records) {
      const node = record.node;
      if (!node) {
        continue;
      }
      let parentNode = node.getParent();
      let parent: LayoutAnimationRecord | undefined;
      while (parentNode) {
        parent = this.recordsByNode.get(parentNode);
        if (parent) {
          break;
        }
        parentNode = parentNode.getParent();
      }
      if (record.parent !== parent) {
        record.parent = parent;
        changed = true;
      }
    }
    return changed;
  }

  private removeNode(node: ViewNode, clearProjection: boolean): void {
    const record = this.recordsByNode.get(node);
    if (!record) {
      return;
    }
    if (clearProjection) {
      restoreProjection(record);
    }
    this.recordsByNode.delete(node);
    node.clearLayoutAnimation(this);
    record.node = undefined;
    if (this.recordsByNode.size === 0) {
      this.cancel();
    }
  }

  private clearAllProjections(): void {
    for (const record of this.records) {
      const node = record.node;
      if (!node) {
        continue;
      }
      restoreProjection(record);
      node.clearLayoutAnimation(this);
      this.recordsByNode.delete(node);
      record.node = undefined;
    }
    this.records.length = 0;
  }
}

export function captureLayoutSnapshot(root: ViewNode | null): LayoutSnapshot {
  const entries: LayoutSnapshotEntry[] = [];
  const entriesByNode = new Map<ViewNode, LayoutSnapshotEntry>();
  if (!root || root.isDestroyed() || root.isPendingRemoval()) {
    return { entries, entriesByNode };
  }

  const rootOffset = measureLogicalOffset(root.htmlElement);
  captureLayoutSnapshotNode(root, undefined, true, rootOffset.x, rootOffset.y, entries, entriesByNode);
  return { entries, entriesByNode };
}

function captureLayoutSnapshotNode(
  node: ViewNode,
  parent: ViewNode | undefined,
  parentAnimationsEnabled: boolean,
  x: number,
  y: number,
  entries: LayoutSnapshotEntry[],
  entriesByNode: Map<ViewNode, LayoutSnapshotEntry>,
): void {
  const element = node.htmlElement;
  const frame: LayoutFrame = { x, y, width: element.offsetWidth, height: element.offsetHeight };
  const animationsEnabled = parentAnimationsEnabled && node.isAnimationEnabled();
  let capturedParent = parent;
  if (frame.width > 0 && frame.height > 0) {
    const entry: LayoutSnapshotEntry = { node, parent, frame, animationsEnabled };
    entries.push(entry);
    entriesByNode.set(node, entry);
    capturedParent = node;
  }
  const children = node.getChildrenSnapshot();
  if (children.length === 0) {
    return;
  }
  const childOriginX = x;
  const childOriginY = y;
  for (const child of children) {
    if (child.isDestroyed() || child.isPendingRemoval()) {
      continue;
    }
    const childElement = child.htmlElement;
    captureLayoutSnapshotNode(
      child,
      capturedParent,
      animationsEnabled,
      childOriginX + childElement.offsetLeft,
      childOriginY + childElement.offsetTop,
      entries,
      entriesByNode,
    );
  }
}

function makeAnimationRecords(
  initialSnapshot: LayoutSnapshot,
  finalSnapshot: LayoutSnapshot,
  recordsByNode: Map<ViewNode, LayoutAnimationRecord>,
): LayoutAnimationRecord[] {
  const records: LayoutAnimationRecord[] = [];
  const includedParentByNode = new Map<ViewNode, LayoutAnimationRecord | undefined>();
  const hasTranslatingAncestorByNode = new Map<ViewNode, boolean>();
  const blockedByNode = new Map<ViewNode, boolean>();

  for (const finalEntry of finalSnapshot.entries) {
    if (!finalEntry.animationsEnabled) {
      blockedByNode.set(finalEntry.node, true);
      continue;
    }
    const initialEntry = initialSnapshot.entriesByNode.get(finalEntry.node);
    if (!initialEntry) {
      continue;
    }
    const parentRecord = finalEntry.parent ? includedParentByNode.get(finalEntry.parent) : undefined;
    const parentHasTranslation = finalEntry.parent
      ? hasTranslatingAncestorByNode.get(finalEntry.parent) === true
      : false;
    const parentBlocked = finalEntry.parent ? blockedByNode.get(finalEntry.parent) === true : false;
    const changesSize =
      differs(initialEntry.frame.width, finalEntry.frame.width) ||
      differs(initialEntry.frame.height, finalEntry.frame.height);
    const changesPosition =
      differs(initialEntry.frame.x, finalEntry.frame.x) || differs(initialEntry.frame.y, finalEntry.frame.y);
    const sizeApplier = changesSize
      ? finalEntry.node.makeLayoutAnimationSizeApplier(finalEntry.frame.width, finalEntry.frame.height)
      : undefined;
    const needsProjection = changesPosition || parentHasTranslation || sizeApplier !== undefined;
    const blocked = parentBlocked || (needsProjection && hasIndependentTranslation(finalEntry.node.htmlElement));
    blockedByNode.set(finalEntry.node, blocked);
    hasTranslatingAncestorByNode.set(finalEntry.node, parentHasTranslation || changesPosition);
    if (!needsProjection || blocked) {
      sizeApplier?.reset();
      includedParentByNode.set(finalEntry.node, parentRecord);
      continue;
    }

    const style = finalEntry.node.htmlElement.style;
    const record: LayoutAnimationRecord = {
      node: finalEntry.node,
      startFrame: initialEntry.frame,
      endFrame: finalEntry.frame,
      currentFrame: { ...finalEntry.frame },
      parent: parentRecord,
      originalTranslate: style.getPropertyValue('translate'),
      sizeApplier,
      projection: { ...IDENTITY_PROJECTION },
    };
    records.push(record);
    recordsByNode.set(finalEntry.node, record);
    includedParentByNode.set(finalEntry.node, record);
  }
  return records;
}

function measureLogicalFrame(element: HTMLElement): LayoutFrame {
  const offset = measureLogicalOffset(element);
  return { x: offset.x, y: offset.y, width: element.offsetWidth, height: element.offsetHeight };
}

function measureLogicalOffset(element: HTMLElement): { x: number; y: number } {
  let x = 0;
  let y = 0;
  let current: HTMLElement | null = element;
  while (current) {
    x += current.offsetLeft;
    y += current.offsetTop;
    current = current.offsetParent as HTMLElement | null;
  }
  return { x, y };
}

function hasIndependentTranslation(element: HTMLElement): boolean {
  const style = typeof getComputedStyle === 'function' ? getComputedStyle(element) : element.style;
  const translate = style.getPropertyValue('translate').trim();
  return !isIdentityTranslate(translate);
}

function isIdentityTranslate(value: string): boolean {
  return value === '' || value === 'none' || value === '0px' || value === '0px 0px';
}

function restoreProjection(record: LayoutAnimationRecord): void {
  const element = record.node?.htmlElement;
  if (!element) {
    return;
  }
  restoreStyleProperty(element.style, 'translate', record.originalTranslate);
  record.sizeApplier?.reset();
}

function restoreStyleProperty(style: CSSStyleDeclaration, name: string, value: string): void {
  if (value) {
    style.setProperty(name, value);
  } else {
    style.removeProperty(name);
  }
}

function interpolate(from: number, to: number, progress: number): number {
  return from + (to - from) * progress;
}

function framesDiffer(from: LayoutFrame, to: LayoutFrame): boolean {
  return (
    differs(from.x, to.x) || differs(from.y, to.y) || differs(from.width, to.width) || differs(from.height, to.height)
  );
}

function differs(from: number, to: number): boolean {
  return Math.abs(from - to) > FRAME_EPSILON;
}
