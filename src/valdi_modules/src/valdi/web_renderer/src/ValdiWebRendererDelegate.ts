import { AnimationOptions } from 'valdi_core/src/AnimationOptions';
import type { IRenderedElement } from 'valdi_core/src/IRenderedElement';
import type { IRenderer } from 'valdi_core/src/IRenderer';
import { FrameObserver, IRendererDelegate, VisibilityObserver } from 'valdi_core/src/IRendererDelegate';
import { Style } from 'valdi_core/src/Style';
import { NativeNode } from 'valdi_tsx/src/NativeNode';
import { NativeView } from 'valdi_tsx/src/NativeView';
import {
  changeAttributeOnElement,
  createElement,
  createNodesRef,
  makeElementRoot,
  moveElement,
  NodesRef,
  registerElements,
  setAllElementsAttributeDelegate,
} from './HTMLRenderer';
import {
  captureComponentHierarchySnapshot,
  type ComponentPropertyEditRegistrar,
} from './debug/ComponentHierarchySnapshot';
import type { WebValdiLayout } from './views/WebValdiLayout';

export interface UpdateAttributeDelegate {
  updateAttribute(elementId: number, attributeName: string, attributeValue: any): void;
}

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
  private attributeDelegate?: UpdateAttributeDelegate;
  private frameObserver?: FrameObserver;
  private resizeObserver?: ResizeObserver;
  private elementIdByHtmlElement = new WeakMap<Element, number>();
  private debugTopologyRevision = 0;
  // Owned per delegate (i.e. per renderer/page) so element ids can't collide
  // with another page's (github.com/Snapchat/Valdi#115).
  private nodesRef: NodesRef = createNodesRef();
  private rootElementId?: number;

  constructor(private htmlRoot: HTMLElement | ShadowRoot) {
    registerElements();
  }
  setAttributeDelegate(delegate: UpdateAttributeDelegate) {
    this.attributeDelegate = delegate;

    setAllElementsAttributeDelegate(this.nodesRef, this.attributeDelegate);
  }

  onElementBecameRoot(id: number): void {
    makeElementRoot(this.nodesRef, id, this.htmlRoot);
    this.rootElementId = id;
    this.debugTopologyRevision++;
  }
  onElementMoved(id: number, parentId: number, parentIndex: number): void {
    moveElement(this.nodesRef, id, parentId, parentIndex);
    this.debugTopologyRevision++;
  }
  onElementCreated(id: number, viewClass: string): void {
    createElement(this.nodesRef, id, viewClass, this.attributeDelegate);
    this.debugTopologyRevision++;
    const element = this.nodesRef.get(id);
    if (element?.htmlElement) {
      this.elementIdByHtmlElement.set(element.htmlElement, id);
      this.resizeObserver?.observe(element.htmlElement);
    }
  }
  onElementDestroyed(id: number): void {
    const element = this.nodesRef.get(id);
    if (element === undefined) {
      return;
    }

    const nodesToDestroy: WebValdiLayout[] = [];
    const visitedNodes = new Set<WebValdiLayout>();
    const pendingNodes = [element];
    while (pendingNodes.length > 0) {
      const node = pendingNodes.pop()!;
      if (visitedNodes.has(node) || this.nodesRef.get(node.id) !== node) {
        continue;
      }
      visitedNodes.add(node);
      nodesToDestroy.push(node);
      for (const child of node.children) {
        // A moved child can remain in an adversarially stale child array. Only
        // purge links that still describe the live backing-node relationship.
        if (child.parent === node && this.nodesRef.get(child.id) === child) {
          pendingNodes.push(child);
        }
      }
    }

    element.parent?.removeChild(element);
    for (let index = nodesToDestroy.length - 1; index >= 0; index--) {
      const node = nodesToDestroy[index];
      this.resizeObserver?.unobserve(node.htmlElement);
      this.elementIdByHtmlElement.delete(node.htmlElement);
      node.destroy();
      this.nodesRef.delete(node.id);
      node.parent = null;
      node.children = [];
      if (this.rootElementId === node.id) {
        this.rootElementId = undefined;
      }
    }
    this.debugTopologyRevision++;
  }
  onElementAttributeChangeAny(id: number, attributeName: string, attributeValue: any): void {
    changeAttributeOnElement(this.nodesRef, id, attributeName, attributeValue);
  }
  onElementAttributeChangeNumber(id: number, attributeName: string, attributeValue: number): void {
    changeAttributeOnElement(this.nodesRef, id, attributeName, attributeValue);
  }
  onElementAttributeChangeString(id: number, attributeName: string, attributeValue: string): void {
    changeAttributeOnElement(this.nodesRef, id, attributeName, attributeValue);
  }
  onElementAttributeChangeTrue(id: number, attributeName: string): void {
    changeAttributeOnElement(this.nodesRef, id, attributeName, undefined);
  }
  onElementAttributeChangeFalse(id: number, attributeName: string): void {
    changeAttributeOnElement(this.nodesRef, id, attributeName, undefined);
  }
  onElementAttributeChangeUndefined(id: number, attributeName: string): void {
    changeAttributeOnElement(this.nodesRef, id, attributeName, undefined);
  }
  onElementAttributeChangeStyle(id: number, attributeName: string, style: Style<any>): void {
    const attributes = style.attributes ?? {};
    Object.keys(attributes).forEach(key => {
      changeAttributeOnElement(this.nodesRef, id, key, attributes[key]);
    });
  }
  onElementAttributeChangeFunction(id: number, attributeName: string, fn: () => void): void {
    changeAttributeOnElement(this.nodesRef, id, attributeName, fn);
  }
  onNextLayoutComplete(callback: () => void): void {}
  onNextDraw(callback: (hookTimeMs: number) => void): void {}
  onRenderStart(): void {
    // TODO(mgharmalkar)
    // console.log('onRenderStart');
  }
  onRenderEnd(): void {
    // TODO(mgharmalkar)
    // console.log('onRenderEnd');
  }
  onAnimationStart(options: AnimationOptions, token: number): void {
    // TODO: no animation support on web yet, so just call completion with cancelled = false.
    options.completion?.(false);
  }
  onAnimationEnd(): void {}
  onAnimationCancel(token: number): void {}
  registerVisibilityObserver(observer: VisibilityObserver): void {
    // TODO(mgharmalkar)
    // console.log('registerVisibilityObserver');
  }
  registerFrameObserver(observer: FrameObserver): void {
    this.frameObserver = observer;

    this.resizeObserver = new ResizeObserver((entries) => {
      if (!this.frameObserver) return;

      const updates: number[] = [];
      for (const entry of entries) {
        const elementId = this.elementIdByHtmlElement.get(entry.target);
        if (elementId === undefined) continue;

        const htmlElement = entry.target as HTMLElement;
        const rect = htmlElement.getBoundingClientRect();
        const offsetParent = htmlElement.offsetParent as HTMLElement | null;

        let x: number;
        let y: number;
        if (offsetParent) {
          const parentRect = offsetParent.getBoundingClientRect();
          const cs = getComputedStyle(offsetParent);
          x = rect.left - parentRect.left + offsetParent.scrollLeft - (parseFloat(cs.borderLeftWidth) || 0);
          y = rect.top - parentRect.top + offsetParent.scrollTop - (parseFloat(cs.borderTopWidth) || 0);
        } else {
          x = rect.left;
          y = rect.top;
        }

        updates.push(elementId, x, y, rect.width, rect.height);
      }

      if (updates.length > 0) {
        this.frameObserver(new Float64Array(updates));
      }
    });
  }
  getNativeView(id: number, callback: (instance: NativeView | undefined) => void): void {}
  getNativeNode(id: number): NativeNode | undefined {
    throw new Error('Method not implemented.');
  }
  getElementFrame(id: number, callback: (instance: any) => void): void {}
  takeElementSnapshot(id: number, callback: (snapshotBase64: string | undefined) => void): void {}
  onUncaughtError(message: string, error: Error): void {
    console.error(message, error);
  }
  onDestroyed(): void {
    this.frameObserver = undefined;
    this.resizeObserver?.disconnect();
  }

  getDebugNode(id: number): { htmlElement: HTMLElement; type: string } | undefined {
    const node = this.nodesRef.get(id);
    return node === undefined ? undefined : { htmlElement: node.htmlElement, type: node.type };
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
    const rootNode = this.rootElementId === undefined ? undefined : this.nodesRef.get(this.rootElementId);
    const elementTree =
      rootNode === undefined
        ? null
        : (captureDebugElementSnapshot(rootNode, null, this.nodesRef, renderer, budget, 0, false) ?? null);
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
    stripComponentProperties(componentTree);
    return JSON.stringify(componentSnapshot).length <= snapshotCharacterLimit ? componentSnapshot : elementSnapshot;
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
  node: WebValdiLayout,
  expectedParent: WebValdiLayout | null,
  nodesRef: NodesRef,
  renderer: IRenderer,
  budget: DebugSnapshotBudget,
  depth: number,
  hasPreviousSibling: boolean,
): WebRendererDebugElementSnapshot | undefined {
  if (!isLiveDebugNode(node, expectedParent, nodesRef) || !tryConsumeDebugTreeNode(budget.renderedTree, depth)) {
    return undefined;
  }
  const rect = node.htmlElement.getBoundingClientRect();
  if (!isLiveDebugNode(node, expectedParent, nodesRef)) {
    return undefined;
  }
  const renderedElement = renderer.getElementForId(node.id);
  if (!isLiveDebugNode(node, expectedParent, nodesRef)) {
    return undefined;
  }
  const domAttributes: Record<string, string> = {};
  const attributes: Record<string, unknown> = {};
  const snapshot: WebRendererDebugElementSnapshot = {
    id: String(node.id),
    tag: node.type,
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
    if (!isLiveDebugNode(node, expectedParent, nodesRef)) {
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
  if (!isLiveDebugNode(node, expectedParent, nodesRef)) {
    return undefined;
  }

  let childrenTruncated = false;
  if (depth + 1 >= MAX_DEBUG_TREE_DEPTH) {
    const hasChildren = node.children.length > 0;
    if (!isLiveDebugNode(node, expectedParent, nodesRef)) {
      return undefined;
    }
    if (hasChildren) {
      childrenTruncated = true;
      budget.renderedTree.truncated = true;
    }
  } else {
    let childIndex = 0;
    while (true) {
      if (!isLiveDebugNode(node, expectedParent, nodesRef)) {
        return undefined;
      }
      const currentChildCount = node.children.length;
      if (!isLiveDebugNode(node, expectedParent, nodesRef)) {
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
      const child = node.children[childIndex];
      childIndex++;
      if (!isLiveDebugNode(node, expectedParent, nodesRef)) {
        return undefined;
      }
      if (child === undefined || !isLiveDebugNode(child, node, nodesRef)) {
        continue;
      }
      const childSnapshot = captureDebugElementSnapshot(
        child,
        node,
        nodesRef,
        renderer,
        budget,
        depth + 1,
        snapshot.children.length > 0,
      );
      if (!isLiveDebugNode(node, expectedParent, nodesRef)) {
        return undefined;
      }
      if (!isLiveDebugNode(child, node, nodesRef)) {
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

function isLiveDebugNode(
  node: WebValdiLayout,
  expectedParent: WebValdiLayout | null,
  nodesRef: NodesRef,
): boolean {
  return node.parent === expectedParent && nodesRef.get(node.id) === node;
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

function captureDebugString(
  value: string,
  budget: DebugSerializationBudget,
  reservedCharacters: number,
): string {
  const availableCharacters = Math.min(
    budget.remainingCharacters,
    Math.max(2, budget.remainingCharacters - reservedCharacters),
  );
  const valueCharacterLimit = Math.min(value.length, MAX_DEBUG_STRING_CHARACTERS);
  if (
    value.length <= MAX_DEBUG_STRING_CHARACTERS &&
    getJsonStringCharacterLength(value) <= availableCharacters
  ) {
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
