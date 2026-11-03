import { AnimationOptions } from 'valdi_core/src/AnimationOptions';
import type { IRenderedElement } from 'valdi_core/src/IRenderedElement';
import type { IRenderer } from 'valdi_core/src/IRenderer';
import { IRendererDelegate, VisibilityObserver } from 'valdi_core/src/IRendererDelegate';
import { Style } from 'valdi_core/src/Style';
import { ElementFrame } from 'valdi_tsx/src/Geometry';
import { NativeNode } from 'valdi_tsx/src/NativeNode';
import { NativeView } from 'valdi_tsx/src/NativeView';
import { ViewFactory } from 'valdi_tsx/src/ViewFactory';
import { WebViewFactory } from './ViewFactory';
import type { AttributeUpdatedExternallyDelegate } from './core/ElementClass';
import type { ViewNode } from './core/ViewNode';
import { ViewNodeTree } from './core/ViewNodeTree';
import {
  captureComponentHierarchySnapshot,
  type ComponentPropertyEditRegistrar,
} from './debug/ComponentHierarchySnapshot';

export interface WebRendererDebugNodeSnapshot {
  id: string;
  tag: string;
  bounds?: {
    x: number;
    y: number;
    width: number;
    height: number;
  };
  children: WebRendererDebugNodeSnapshot[];
  childrenTruncated?: boolean;
  component?: {
    elementId?: string;
    key: string;
    name: string;
    properties?: Record<string, unknown>;
    propertyEdits?: Record<string, WebRendererDebugPropertyEditMetadata>;
    state?: string;
  };
  element?: {
    id: number;
    attributes: Record<string, unknown>;
    dom: {
      attributes: Record<string, string>;
      tagName: string;
    };
  };
}

export interface WebRendererDebugPropertyEditMetadata {
  componentToken: string;
  snapshotRevision: number;
}

export interface WebRendererDebugElementSnapshot extends WebRendererDebugNodeSnapshot {
  bounds: {
    x: number;
    y: number;
    width: number;
    height: number;
  };
  children: WebRendererDebugElementSnapshot[];
  element: {
    id: number;
    attributes: Record<string, unknown>;
    dom: {
      attributes: Record<string, string>;
      tagName: string;
    };
  };
}

export interface WebRendererDebugComponentSnapshot extends WebRendererDebugNodeSnapshot {
  component: {
    elementId?: string;
    key: string;
    name: string;
    properties?: Record<string, unknown>;
    propertyEdits?: Record<string, WebRendererDebugPropertyEditMetadata>;
    state?: string;
  };
}

export interface WebRendererDebugSnapshot {
  tree: WebRendererDebugNodeSnapshot | null;
  viewport: {
    width: number;
    height: number;
  };
}

interface DebugSerializationBudget {
  remainingCharacters: number;
}

interface DebugTreeTraversalBudget {
  remainingChildLinks: number;
  remainingNodes: number;
  truncated: boolean;
}

interface DebugSnapshotBudget extends DebugSerializationBudget {
  readonly renderedTree: DebugTreeTraversalBudget;
}

const MAX_DEBUG_DEPTH = 4;
const MAX_DEBUG_ENTRIES = 50;
const MAX_DEBUG_STRING_CHARACTERS = 65_536;
export const MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS = 262_144;
const MAX_DEBUG_TREE_CHILD_LINKS = 1_000;
const MAX_DEBUG_TREE_DEPTH = 64;
const MAX_DEBUG_TREE_NODES = 1_000;
const DEBUG_TRUNCATION_BUDGET_RESERVE = 192;
const DEBUG_TRUNCATION_MARKER = '... <truncated>';
const DEBUG_ACCESSOR_MARKER = '<accessor/>';
const DEBUG_CIRCULAR_MARKER = '<circular object/>';

export class ValdiWebRendererDelegate implements IRendererDelegate {
  private attributeUpdatedExternallyDelegate?: AttributeUpdatedExternallyDelegate;
  private debugTopologyRevision = 0;

  constructor(private htmlRoot: HTMLElement | ShadowRoot, private readonly viewNodeTree: ViewNodeTree) {}

  setRenderCompleteScheduler(schedule: (callback: () => void) => void): void {
    this.viewNodeTree.setRenderCompleteScheduler(schedule);
  }

  setAttributeUpdatedExternallyDelegate(delegate: AttributeUpdatedExternallyDelegate): void {
    this.attributeUpdatedExternallyDelegate = delegate;
  }

  onElementBecameRoot(id: number): void {
    this.viewNodeTree.makeElementRoot(id, this.htmlRoot);
    this.debugTopologyRevision++;
    this.viewNodeTree.scheduleVisibilityRefresh(false);
  }
  onElementMoved(id: number, parentId: number, parentIndex: number): void {
    this.viewNodeTree.moveElement(id, parentId, parentIndex);
    this.debugTopologyRevision++;
    this.viewNodeTree.scheduleVisibilityRefresh(false);
  }
  onElementCreated(id: number, viewClass: string): void {
    this.viewNodeTree.createElement(id, viewClass, this.attributeUpdatedExternallyDelegate);
    this.debugTopologyRevision++;
  }
  onCustomElementCreated(id: number, viewFactory: ViewFactory): void {
    if (!(viewFactory instanceof WebViewFactory)) {
      throw new Error('Expected a web view factory when creating a custom element');
    }
    this.viewNodeTree.createElementWithClass(
      id,
      'custom-view',
      viewFactory.elementClass,
      this.attributeUpdatedExternallyDelegate,
    );
    this.debugTopologyRevision++;
  }
  onElementDestroyed(id: number): void {
    this.viewNodeTree.destroyElement(id);
    this.debugTopologyRevision++;
  }
  onElementDestroyedFromParent(id: number): void {}
  onElementAttributeChangeAny(id: number, attributeName: string, attributeValue: any): void {
    this.viewNodeTree.setAttributeOnElement(id, attributeName, attributeValue);
  }
  onElementAttributeChangeNumber(id: number, attributeName: string, attributeValue: number): void {
    this.viewNodeTree.setAttributeOnElement(id, attributeName, attributeValue);
  }
  onElementAttributeChangeString(id: number, attributeName: string, attributeValue: string): void {
    this.viewNodeTree.setAttributeOnElement(id, attributeName, attributeValue);
  }
  onElementAttributeChangeTrue(id: number, attributeName: string): void {
    this.viewNodeTree.setAttributeOnElement(id, attributeName, true);
  }
  onElementAttributeChangeFalse(id: number, attributeName: string): void {
    this.viewNodeTree.setAttributeOnElement(id, attributeName, false);
  }
  onElementAttributeChangeUndefined(id: number, attributeName: string): void {
    this.viewNodeTree.setAttributeOnElement(id, attributeName, undefined);
  }
  onElementAttributeChangeStyle(id: number, attributeName: string, style: Style<any>): void {
    this.viewNodeTree.setStyleAttributeOnElement(id, attributeName, style);
  }
  onElementAttributeChangeFunction(id: number, attributeName: string, fn: () => void): void {
    this.viewNodeTree.setAttributeOnElement(id, attributeName, fn);
  }
  onNextLayoutComplete(callback: () => void): void {
    if (typeof document !== 'undefined' && document.visibilityState === 'hidden') {
      void Promise.resolve().then(() => {
        this.drainScheduledLayoutObservers(true);
        void Promise.resolve().then(() => {
          this.drainScheduledLayoutObservers(true);
          callback();
        });
      });
      return;
    }
    requestAnimationFrame(() => {
      this.drainScheduledLayoutObservers(false);
      requestAnimationFrame(() => {
        this.drainScheduledLayoutObservers(false);
        callback();
      });
    });
  }
  onNextDraw(callback: (hookTimeMs: number) => void): void {
    requestAnimationFrame(hookTimeMs => callback(hookTimeMs));
  }
  onRenderStart(): void {
    this.viewNodeTree.beginRender();
  }
  onRenderEnd(): void {
    this.viewNodeTree.endRender();
    this.viewNodeTree.scheduleVisibilityRefresh(false);
  }
  onAnimationStart(options: AnimationOptions, token: number): void {
    this.viewNodeTree.beginAnimation(options, token);
  }
  onAnimationEnd(): void {
    this.viewNodeTree.endAnimation();
  }
  onAnimationCancel(token: number): void {
    this.viewNodeTree.cancelAnimation(token);
  }
  registerVisibilityObserver(observer: VisibilityObserver): void {
    this.viewNodeTree.registerVisibilityObserver(observer);
  }
  getNativeView(id: number, callback: (instance: NativeView | undefined) => void): void {}
  getNativeNode(id: number): NativeNode | undefined {
    return this.viewNodeTree.getNode(id)?.htmlElement as unknown as NativeNode | undefined;
  }
  getCachedElementFrame(id: number): ElementFrame | undefined {
    return this.viewNodeTree.getElementFrame(id);
  }
  getElementFrame(id: number, callback: (instance: ElementFrame | undefined) => void): void {
    callback(this.viewNodeTree.getElementFrame(id));
  }
  takeElementSnapshot(id: number, callback: (snapshotBase64: string | undefined) => void): void {
    const element = this.viewNodeTree.getNode(id)?.htmlElement;
    const takeSnapshot = (
      globalThis as unknown as {
        __valdiTakeElementSnapshot?: (element: HTMLElement) => Promise<string | undefined>;
      }
    ).__valdiTakeElementSnapshot;
    if (!element || !takeSnapshot) {
      callback(undefined);
      return;
    }

    takeSnapshot(element)
      .then(snapshot => callback(snapshot))
      .catch(error => {
        console.error('Failed to capture Valdi web element snapshot', error);
        callback(undefined);
      });
  }
  onUncaughtError(message: string, error: Error): void {
    console.error(message, error);
  }
  onDestroyed(): void {
    this.viewNodeTree.destroy();
  }

  getDebugNode(id: number): { htmlElement: HTMLElement; type: string } | undefined {
    const node = this.viewNodeTree.getNode(id);
    return node === undefined ? undefined : { htmlElement: node.htmlElement, type: node.viewClass };
  }

  getDebugSnapshot(
    renderer: IRenderer,
    maximumSerializedCharacters: number,
    componentPropertyEditRegistrar?: ComponentPropertyEditRegistrar,
  ): WebRendererDebugSnapshot {
    const viewport = {
      width: typeof window === 'undefined' ? 0 : window.innerWidth,
      height: typeof window === 'undefined' ? 0 : window.innerHeight,
    };
    const snapshotEnvelopeCharacters = JSON.stringify({ tree: null, viewport }).length - 'null'.length;
    const snapshotCharacterLimit = Math.min(
      MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS,
      Math.max(0, maximumSerializedCharacters),
    );
    const budget: DebugSnapshotBudget = {
      remainingCharacters: Math.max(0, snapshotCharacterLimit - snapshotEnvelopeCharacters),
      renderedTree: {
        remainingChildLinks: MAX_DEBUG_TREE_CHILD_LINKS,
        remainingNodes: MAX_DEBUG_TREE_NODES,
        truncated: false,
      },
    };
    // Read-only: flushing here would run pending lifecycle/layout callbacks from a debugger read.
    const rootNode = this.viewNodeTree.getRootNode();
    const elementTree =
      rootNode === null
        ? null
        : captureDebugElementSnapshot(rootNode, null, this.viewNodeTree, renderer, budget, 0, false) ?? null;
    const elementSnapshot: WebRendererDebugSnapshot = {
      tree: elementTree,
      viewport,
    };
    // The snapshot only contains fresh data objects and primitives, so this final
    // serialization check cannot invoke getters from renderer-owned values.
    if (JSON.stringify(elementSnapshot).length > snapshotCharacterLimit) {
      return { tree: null, viewport };
    }
    if (elementTree === null) {
      return elementSnapshot;
    }

    const topologyRevision = this.debugTopologyRevision;
    const componentTree = captureComponentHierarchySnapshot(elementTree, renderer, componentPropertyEditRegistrar);
    if (componentTree === undefined || topologyRevision !== this.debugTopologyRevision) {
      return elementSnapshot;
    }
    const componentSnapshot: WebRendererDebugSnapshot = { tree: componentTree, viewport };
    if (JSON.stringify(componentSnapshot).length <= snapshotCharacterLimit) {
      return componentSnapshot;
    }
    stripComponentState(componentTree);
    if (JSON.stringify(componentSnapshot).length <= snapshotCharacterLimit) {
      return componentSnapshot;
    }
    stripComponentProperties(componentTree);
    return JSON.stringify(componentSnapshot).length <= snapshotCharacterLimit ? componentSnapshot : elementSnapshot;
  }

  private drainScheduledLayoutObservers(forceVisibility: boolean): void {
    this.viewNodeTree.drainScheduledLayoutObserverRefresh();
    this.viewNodeTree.drainScheduledVisibilityRefresh(forceVisibility);
  }
}

function stripComponentState(root: WebRendererDebugNodeSnapshot): void {
  const pending = [root];
  while (pending.length > 0) {
    const node = pending.pop()!;
    if (node.component !== undefined) {
      delete node.component.state;
    }
    pending.push(...node.children);
  }
}

function stripComponentProperties(root: WebRendererDebugNodeSnapshot): void {
  const pending = [root];
  while (pending.length > 0) {
    const node = pending.pop()!;
    if (node.component !== undefined) {
      delete node.component.properties;
      delete node.component.propertyEdits;
    }
    pending.push(...node.children);
  }
}

function captureRenderedElementAttributes(
  element: IRenderedElement,
  budget: DebugSerializationBudget,
): Record<string, unknown> {
  const attributes: Record<string, unknown> = {};
  let attributeNames: string[];
  try {
    attributeNames = element.getAttributeNames();
  } catch (_error) {
    addDebugTruncationProperty(attributes, '<unavailable/>', budget);
    return attributes;
  }
  const attributeCount = Math.min(attributeNames.length, MAX_DEBUG_ENTRIES);
  let attributesTruncated = attributeNames.length > attributeCount;
  for (let index = 0; index < attributeCount; index++) {
    if (budget.remainingCharacters <= DEBUG_TRUNCATION_BUDGET_RESERVE) {
      attributesTruncated = true;
      break;
    }
    const attributeName = attributeNames[index];
    const name = String(attributeName);
    if (!tryConsumeDebugPropertyPrefix(attributes, name, budget, 4)) {
      attributesTruncated = true;
      break;
    }
    try {
      setDebugProperty(
        attributes,
        name,
        toDebugValue(element.getAttribute(attributeName), 0, new Set<object>(), budget),
      );
    } catch (_error) {
      setDebugProperty(attributes, name, captureDebugString('<unavailable/>', budget, 0));
    }
  }
  if (attributesTruncated) {
    addDebugTruncationProperty(
      attributes,
      `${attributeNames.length - Object.keys(attributes).length} more attributes`,
      budget,
    );
  }
  return attributes;
}

function captureDebugElementSnapshot(
  node: ViewNode,
  expectedParent: ViewNode | null,
  tree: ViewNodeTree,
  renderer: IRenderer,
  budget: DebugSnapshotBudget,
  depth: number,
  hasPreviousSibling: boolean,
): WebRendererDebugElementSnapshot | undefined {
  if (!isLiveDebugNode(node, expectedParent, tree) || !tryConsumeDebugTreeNode(budget.renderedTree, depth)) {
    return undefined;
  }
  const rect = node.htmlElement.getBoundingClientRect();
  if (!isLiveDebugNode(node, expectedParent, tree)) {
    return undefined;
  }
  const renderedElement = renderer.getElementForId(node.id);
  if (!isLiveDebugNode(node, expectedParent, tree)) {
    return undefined;
  }
  const domAttributes: Record<string, string> = {};
  const attributes: Record<string, unknown> = {};
  const snapshot: WebRendererDebugElementSnapshot = {
    id: String(node.id),
    tag: node.viewClass,
    element: {
      id: node.id,
      attributes,
      dom: {
        attributes: domAttributes,
        tagName: node.htmlElement.tagName.toLowerCase(),
      },
    },
    bounds: {
      x: rect.left,
      y: rect.top,
      width: rect.width,
      height: rect.height,
    },
    children: [],
  };
  const structuralCharacters = JSON.stringify(snapshot).length + (hasPreviousSibling ? 1 : 0);
  if (!tryConsumeDebugBudget(budget, structuralCharacters)) {
    budget.renderedTree.truncated = true;
    return undefined;
  }

  if (renderedElement !== undefined) {
    snapshot.element.attributes = captureRenderedElementAttributes(renderedElement, budget);
    if (!isLiveDebugNode(node, expectedParent, tree)) {
      return undefined;
    }
  }

  const domAttributeCount = Math.min(node.htmlElement.attributes.length, MAX_DEBUG_ENTRIES);
  let domAttributesTruncated = node.htmlElement.attributes.length > domAttributeCount;
  for (let index = 0; index < domAttributeCount; index++) {
    if (budget.remainingCharacters <= DEBUG_TRUNCATION_BUDGET_RESERVE) {
      domAttributesTruncated = true;
      break;
    }
    const attribute = node.htmlElement.attributes.item(index);
    if (attribute !== null) {
      if (!tryConsumeDebugPropertyPrefix(domAttributes, attribute.name, budget, 2)) {
        domAttributesTruncated = true;
        break;
      }
      setDebugProperty(
        domAttributes,
        attribute.name,
        captureDebugString(attribute.value, budget, DEBUG_TRUNCATION_BUDGET_RESERVE),
      );
    }
  }
  if (domAttributesTruncated) {
    addDebugTruncationProperty(domAttributes, DEBUG_TRUNCATION_MARKER, budget);
  }
  if (!isLiveDebugNode(node, expectedParent, tree)) {
    return undefined;
  }

  let childrenTruncated = false;
  if (depth + 1 >= MAX_DEBUG_TREE_DEPTH) {
    const hasChildren = node.getDebugChildren().length > 0;
    if (!isLiveDebugNode(node, expectedParent, tree)) {
      return undefined;
    }
    if (hasChildren) {
      childrenTruncated = true;
      budget.renderedTree.truncated = true;
    }
  } else {
    const children = node.getDebugChildren();
    let childIndex = 0;
    for (;;) {
      if (!isLiveDebugNode(node, expectedParent, tree)) {
        return undefined;
      }
      const currentChildCount = children.length;
      if (!isLiveDebugNode(node, expectedParent, tree)) {
        return undefined;
      }
      if (childIndex >= currentChildCount) {
        break;
      }
      if (
        budget.remainingCharacters <= 0 ||
        budget.renderedTree.remainingNodes <= 0 ||
        !tryConsumeDebugTreeChildLink(budget.renderedTree)
      ) {
        childrenTruncated = true;
        budget.renderedTree.truncated = true;
        break;
      }
      const child = children[childIndex];
      childIndex++;
      if (!isLiveDebugNode(node, expectedParent, tree)) {
        return undefined;
      }
      if (child === undefined || child.isPendingRemoval() || !isLiveDebugNode(child, node, tree)) {
        continue;
      }
      const childSnapshot = captureDebugElementSnapshot(
        child,
        node,
        tree,
        renderer,
        budget,
        depth + 1,
        snapshot.children.length > 0,
      );
      if (!isLiveDebugNode(node, expectedParent, tree)) {
        return undefined;
      }
      if (!isLiveDebugNode(child, node, tree)) {
        continue;
      }
      if (childSnapshot === undefined) {
        childrenTruncated = true;
        budget.renderedTree.truncated = true;
        break;
      }
      snapshot.children.push(childSnapshot);
    }
  }
  if (childrenTruncated && tryConsumeDebugPropertyPrefix(snapshot, 'childrenTruncated', budget, 4)) {
    consumeDebugBudget(budget, 4);
    snapshot.childrenTruncated = true;
  }
  return snapshot;
}

function isLiveDebugNode(node: ViewNode, expectedParent: ViewNode | null, tree: ViewNodeTree): boolean {
  return node.getDebugParent() === expectedParent && tree.getNode(node.id) === node;
}

function tryConsumeDebugTreeChildLink(budget: DebugTreeTraversalBudget): boolean {
  if (budget.remainingChildLinks <= 0) {
    budget.truncated = true;
    return false;
  }
  budget.remainingChildLinks--;
  return true;
}

function tryConsumeDebugTreeNode(budget: DebugTreeTraversalBudget, depth: number): boolean {
  if (depth >= MAX_DEBUG_TREE_DEPTH || budget.remainingNodes <= 0) {
    budget.truncated = true;
    return false;
  }
  budget.remainingNodes--;
  return true;
}

function toDebugValue(
  value: unknown,
  depth: number,
  activePath: Set<object>,
  budget: DebugSerializationBudget,
): unknown {
  // Depth counts edges from the renderer attribute value. Once the limit is
  // reached, replace the value itself without inspecting or serializing it.
  if (depth >= MAX_DEBUG_DEPTH) {
    return captureDebugString(DEBUG_TRUNCATION_MARKER, budget, 0);
  }
  if (value === undefined) {
    consumeDebugBudget(budget, 'null'.length);
    return value;
  }
  if (value === null || typeof value === 'number' || typeof value === 'boolean') {
    const serializedValue = JSON.stringify(value);
    consumeDebugBudget(budget, serializedValue === undefined ? 'null'.length : serializedValue.length);
    return value;
  }
  if (typeof value === 'string') {
    return captureDebugString(value, budget, DEBUG_TRUNCATION_BUDGET_RESERVE);
  }
  if (typeof value === 'function') {
    return captureDebugString('[function]', budget, DEBUG_TRUNCATION_BUDGET_RESERVE);
  }
  if (typeof value !== 'object') {
    return captureDebugString(String(value), budget, DEBUG_TRUNCATION_BUDGET_RESERVE);
  }
  if (typeof ArrayBuffer !== 'undefined' && ArrayBuffer.isView(value)) {
    return captureDebugString('<array buffer view/>', budget, DEBUG_TRUNCATION_BUDGET_RESERVE);
  }
  if (activePath.has(value)) {
    return captureDebugString(DEBUG_CIRCULAR_MARKER, budget, 0);
  }

  activePath.add(value);
  try {
    if (Array.isArray(value)) {
      const lengthDescriptor = Object.getOwnPropertyDescriptor(value, 'length');
      const length = typeof lengthDescriptor?.value === 'number' ? lengthDescriptor.value : 0;
      const itemCount = Math.min(length, MAX_DEBUG_ENTRIES);
      const debugArray: unknown[] = [];
      if (!tryConsumeDebugBudget(budget, 2)) {
        return '';
      }
      let inspectedItemCount = 0;
      for (; inspectedItemCount < itemCount; inspectedItemCount++) {
        if (
          budget.remainingCharacters <= DEBUG_TRUNCATION_BUDGET_RESERVE ||
          !tryConsumeDebugArrayItemPrefix(debugArray, budget, 4)
        ) {
          break;
        }
        const descriptor = Object.getOwnPropertyDescriptor(value, String(inspectedItemCount));
        debugArray.push(
          descriptor === undefined
            ? captureDebugString('<empty/>', budget, DEBUG_TRUNCATION_BUDGET_RESERVE)
            : descriptor.get || descriptor.set
            ? captureDebugString(DEBUG_ACCESSOR_MARKER, budget, 0)
            : toDebugValue(descriptor.value, depth + 1, activePath, budget),
        );
      }
      if (length > inspectedItemCount && tryConsumeDebugArrayItemPrefix(debugArray, budget, 2)) {
        debugArray.push(captureDebugString(`${length - inspectedItemCount} more items`, budget, 0));
      }
      return debugArray;
    }

    const debugValue: Record<string, unknown> = {};
    if (!tryConsumeDebugBudget(budget, 2)) {
      return '';
    }
    let entryCount = 0;
    let fieldsOmitted = false;
    // JavaScript has no resumable own-key iterator. A stoppable for-in loop avoids
    // materializing every key/descriptor in user space, although engines and Proxy
    // ownKeys traps may still enumerate the complete key set internally.
    for (const key in value) {
      const descriptor = Object.getOwnPropertyDescriptor(value, key);
      if (descriptor === undefined) {
        // Own enumerable keys are visited before inherited keys.
        break;
      }
      if (!descriptor.enumerable) {
        continue;
      }
      if (
        entryCount >= MAX_DEBUG_ENTRIES ||
        budget.remainingCharacters <= DEBUG_TRUNCATION_BUDGET_RESERVE ||
        !tryConsumeDebugPropertyPrefix(debugValue, key, budget, 4)
      ) {
        fieldsOmitted = true;
        break;
      }
      setDebugProperty(
        debugValue,
        key,
        descriptor.get || descriptor.set
          ? captureDebugString(DEBUG_ACCESSOR_MARKER, budget, 0)
          : toDebugValue(descriptor.value, depth + 1, activePath, budget),
      );
      entryCount++;
    }
    if (fieldsOmitted) {
      addDebugTruncationProperty(debugValue, 'more fields', budget);
    }
    return debugValue;
  } catch (_error) {
    return captureDebugString('<unavailable/>', budget, 0);
  } finally {
    activePath.delete(value);
  }
}

function captureDebugString(value: string, budget: DebugSerializationBudget, reservedCharacters: number): string {
  const availableCharacters = Math.min(
    budget.remainingCharacters,
    Math.max(2, budget.remainingCharacters - reservedCharacters),
  );
  const valueCharacterLimit = Math.min(value.length, MAX_DEBUG_STRING_CHARACTERS);
  if (value.length <= MAX_DEBUG_STRING_CHARACTERS && getJsonStringCharacterLength(value) <= availableCharacters) {
    consumeDebugBudget(budget, getJsonStringCharacterLength(value));
    return value;
  }

  const markerFits = getJsonStringCharacterLength(DEBUG_TRUNCATION_MARKER) <= availableCharacters;
  const marker = markerFits ? DEBUG_TRUNCATION_MARKER : '';
  let low = 0;
  let high = Math.max(0, valueCharacterLimit - marker.length);
  let bestPrefixEnd = 0;
  while (low <= high) {
    const midpoint = Math.floor((low + high) / 2);
    const prefixEnd = getUnicodeSafePrefixEnd(value, midpoint);
    const candidate = `${value.slice(0, prefixEnd)}${marker}`;
    if (getJsonStringCharacterLength(candidate) <= availableCharacters) {
      bestPrefixEnd = prefixEnd;
      low = midpoint + 1;
    } else {
      high = midpoint - 1;
    }
  }
  const truncated = `${value.slice(0, bestPrefixEnd)}${marker}`;
  consumeDebugBudget(budget, getJsonStringCharacterLength(truncated));
  return truncated;
}

function getUnicodeSafePrefixEnd(value: string, requestedEnd: number): number {
  const prefixEnd = Math.max(0, Math.min(value.length, requestedEnd));
  if (prefixEnd === 0 || prefixEnd === value.length) {
    return prefixEnd;
  }
  const previousCharacter = value.charCodeAt(prefixEnd - 1);
  const nextCharacter = value.charCodeAt(prefixEnd);
  return previousCharacter >= 0xd800 &&
    previousCharacter <= 0xdbff &&
    nextCharacter >= 0xdc00 &&
    nextCharacter <= 0xdfff
    ? prefixEnd - 1
    : prefixEnd;
}

function addDebugTruncationProperty(
  target: Record<string, unknown>,
  message: string,
  budget: DebugSerializationBudget,
): void {
  if (
    Object.prototype.hasOwnProperty.call(target, '__truncated__') ||
    !tryConsumeDebugPropertyPrefix(target, '__truncated__', budget, 2)
  ) {
    return;
  }
  setDebugProperty(target, '__truncated__', captureDebugString(message, budget, 0));
}

function setDebugProperty<T>(target: Record<string, T>, propertyName: string, value: T): void {
  Object.defineProperty(target, propertyName, {
    configurable: true,
    enumerable: true,
    value,
    writable: true,
  });
}

function tryConsumeDebugPropertyPrefix(
  target: object,
  propertyName: string,
  budget: DebugSerializationBudget,
  minimumValueCharacters: number,
): boolean {
  const separatorCharacters = Object.keys(target).length === 0 ? 0 : 1;
  const minimumPropertyPrefixCharacters = separatorCharacters + propertyName.length + 3;
  if (minimumPropertyPrefixCharacters + minimumValueCharacters > budget.remainingCharacters) {
    return false;
  }
  const propertyPrefixCharacters = separatorCharacters + getJsonStringCharacterLength(propertyName) + 1;
  if (propertyPrefixCharacters + minimumValueCharacters > budget.remainingCharacters) {
    return false;
  }
  consumeDebugBudget(budget, propertyPrefixCharacters);
  return true;
}

function tryConsumeDebugArrayItemPrefix(
  target: unknown[],
  budget: DebugSerializationBudget,
  minimumValueCharacters: number,
): boolean {
  const prefixCharacters = target.length === 0 ? 0 : 1;
  if (prefixCharacters + minimumValueCharacters > budget.remainingCharacters) {
    return false;
  }
  consumeDebugBudget(budget, prefixCharacters);
  return true;
}

function getJsonStringCharacterLength(value: string): number {
  let characterCount = 2;
  for (let index = 0; index < value.length; index++) {
    const characterCode = value.charCodeAt(index);
    if (
      characterCode === 0x22 ||
      characterCode === 0x5c ||
      characterCode === 0x08 ||
      characterCode === 0x09 ||
      characterCode === 0x0a ||
      characterCode === 0x0c ||
      characterCode === 0x0d
    ) {
      characterCount += 2;
    } else if (characterCode < 0x20) {
      characterCount += 6;
    } else if (characterCode >= 0xd800 && characterCode <= 0xdbff) {
      const nextCharacterCode = value.charCodeAt(index + 1);
      if (nextCharacterCode >= 0xdc00 && nextCharacterCode <= 0xdfff) {
        characterCount += 2;
        index++;
      } else {
        characterCount += 6;
      }
    } else if (characterCode >= 0xdc00 && characterCode <= 0xdfff) {
      characterCount += 6;
    } else {
      characterCount++;
    }
  }
  return characterCount;
}

function tryConsumeDebugBudget(budget: DebugSerializationBudget, characterCount: number): boolean {
  if (characterCount > budget.remainingCharacters) {
    return false;
  }
  consumeDebugBudget(budget, characterCount);
  return true;
}

function consumeDebugBudget(budget: DebugSerializationBudget, characterCount: number): void {
  if (characterCount > 0) {
    budget.remainingCharacters = Math.max(0, budget.remainingCharacters - characterCount);
  }
}
