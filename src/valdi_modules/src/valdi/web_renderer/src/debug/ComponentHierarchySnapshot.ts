import type { IComponent } from 'valdi_core/src/IComponent';
import type { IRenderedElement } from 'valdi_core/src/IRenderedElement';
import type { IRenderedVirtualNode } from 'valdi_core/src/IRenderedVirtualNode';
import type { IRenderer, RendererDebugVirtualNodeSnapshot } from 'valdi_core/src/IRenderer';
import type {
  WebRendererDebugComponentSnapshot,
  WebRendererDebugElementSnapshot,
  WebRendererDebugNodeSnapshot,
} from '../ValdiWebRendererDelegate';
import { captureDebuggerPropertiesSnapshot, type DebuggerValueSnapshotLimits } from './DebuggerValueSnapshot';

const MAX_COMPONENT_HIERARCHY_CHILD_LINKS = 1_000;
const MAX_COMPONENT_HIERARCHY_DEPTH = 64;
const MAX_COMPONENT_HIERARCHY_ID_CHARACTERS = 4_096;
const MAX_COMPONENT_HIERARCHY_NODES = 1_000;
const MAX_COMPONENT_HIERARCHY_TRAVERSAL_LINKS = 4_096;
const MAX_COMPONENT_NAME_CHARACTERS = 256;
const MAX_COMPONENT_KEY_CHARACTERS = 256;
const MAX_COMPONENT_PROTOTYPE_DEPTH = 16;
const MAX_COMPONENT_PROPERTY_BYTES = 65_536;
const COMPONENT_PROPERTY_LIMITS: DebuggerValueSnapshotLimits = {
  maximumDepth: 4,
  maximumEntries: 50,
  maximumPropertyNameCharacters: 256,
  maximumStringBytes: 65_536,
};

interface IndexedElementTree {
  readonly childIdsByParentId: Map<string | null, string[]>;
  readonly elementsById: Map<string, WebRendererDebugElementSnapshot>;
  readonly parentIdById: Map<string, string | null>;
}

interface CapturedHierarchyNode {
  readonly firstElementId?: string;
  readonly node: WebRendererDebugNodeSnapshot;
}

interface CapturedVirtualNode extends RendererDebugVirtualNodeSnapshot {
  readonly node: IRenderedVirtualNode;
  componentOutput?: WebRendererDebugComponentSnapshot;
}

interface ComponentPropertyBudget {
  remainingBytes: number;
}

interface VirtualTraversalFrame {
  readonly componentPath: string[];
  readonly depth: number;
  readonly expectedParent: IRenderedVirtualNode | undefined;
  readonly nearestElementId: string | null;
  readonly node: IRenderedVirtualNode;
  captured?: CapturedVirtualNode;
  capturedChildren: CapturedHierarchyNode[];
  childIndex: number;
}

type DebugVirtualNodeSnapshotReader = NonNullable<IRenderer['getDebugVirtualNodeSnapshot']>;

/**
 * Transactionally overlays the Valdi component tree on an already-captured
 * physical web-renderer tree. Returning undefined means callers must retain
 * the physical tree verbatim.
 */
export function captureComponentHierarchySnapshot(
  elementTree: WebRendererDebugElementSnapshot,
  renderer: IRenderer,
): WebRendererDebugNodeSnapshot | undefined {
  const indexedElements = indexCompleteElementTree(elementTree);
  if (indexedElements === undefined) {
    return undefined;
  }

  let getDebugVirtualNodeSnapshot: DebugVirtualNodeSnapshotReader;
  let getRootVirtualNode: IRenderer['getRootVirtualNode'];
  let rootVirtualNode: IRenderedVirtualNode | undefined;
  try {
    const debugSnapshotReader = renderer.getDebugVirtualNodeSnapshot;
    const rootReader = renderer.getRootVirtualNode;
    if (typeof debugSnapshotReader !== 'function' || typeof rootReader !== 'function') {
      return undefined;
    }
    getDebugVirtualNodeSnapshot = debugSnapshotReader;
    getRootVirtualNode = rootReader;
    rootVirtualNode = getRootVirtualNode.call(renderer);
  } catch (_error) {
    console.warn('Valdi debugger could not read the component root; using the element hierarchy.');
    return undefined;
  }
  if (rootVirtualNode === undefined) {
    return undefined;
  }

  const capturedNodes: CapturedVirtualNode[] = [];
  const componentPropertyBudget: ComponentPropertyBudget = { remainingBytes: MAX_COMPONENT_PROPERTY_BYTES };
  const componentIds = new Set<string>();
  const consumedChildCountByParentId = new Map<string | null, number>();
  const usedElementIds = new Set<string>();
  const visitedVirtualNodes = new Set<IRenderedVirtualNode>();
  let remainingChildLinks = MAX_COMPONENT_HIERARCHY_CHILD_LINKS;
  let remainingNodes = MAX_COMPONENT_HIERARCHY_NODES;
  let remainingTraversalLinks = MAX_COMPONENT_HIERARCHY_TRAVERSAL_LINKS;
  let mergedRoot: CapturedHierarchyNode | undefined;
  const stack: VirtualTraversalFrame[] = [
    {
      capturedChildren: [],
      childIndex: 0,
      componentPath: [],
      depth: 0,
      expectedParent: undefined,
      nearestElementId: null,
      node: rootVirtualNode,
    },
  ];

  try {
    while (stack.length > 0) {
      const frame = stack[stack.length - 1];
      if (frame.captured === undefined) {
        if (frame.depth >= MAX_COMPONENT_HIERARCHY_DEPTH || remainingNodes <= 0) {
          return undefined;
        }
        if (visitedVirtualNodes.has(frame.node)) {
          return undefined;
        }
        visitedVirtualNodes.add(frame.node);
        remainingNodes--;

        const debugSnapshot = getDebugVirtualNodeSnapshot.call(
          renderer,
          frame.node,
          remainingChildLinks,
          remainingTraversalLinks,
        );
        if (
          debugSnapshot === undefined ||
          debugSnapshot.parent !== frame.expectedParent ||
          (debugSnapshot.element === undefined) === (debugSnapshot.component === undefined) ||
          typeof debugSnapshot.key !== 'string' ||
          debugSnapshot.key.length > MAX_COMPONENT_KEY_CHARACTERS ||
          !Array.isArray(debugSnapshot.children) ||
          debugSnapshot.children.length > remainingChildLinks ||
          !Number.isSafeInteger(debugSnapshot.traversedLinkCount) ||
          debugSnapshot.traversedLinkCount < 0 ||
          debugSnapshot.traversedLinkCount > remainingTraversalLinks
        ) {
          return undefined;
        }
        remainingChildLinks -= debugSnapshot.children.length;
        remainingTraversalLinks -= debugSnapshot.traversedLinkCount;
        frame.captured = {
          children: debugSnapshot.children,
          component: debugSnapshot.component,
          componentViewModel: debugSnapshot.componentViewModel,
          element: debugSnapshot.element,
          key: debugSnapshot.key,
          node: frame.node,
          parent: debugSnapshot.parent,
          traversedLinkCount: debugSnapshot.traversedLinkCount,
        };
        capturedNodes.push(frame.captured);
      }

      const captured = frame.captured;
      if (frame.childIndex < captured.children.length) {
        const child = captured.children[frame.childIndex];
        frame.childIndex++;
        if (typeof child !== 'object' || child === null) {
          return undefined;
        }
        let currentElementId = frame.nearestElementId;
        if (captured.element !== undefined) {
          const capturedElementId = readElementId(captured.element);
          if (capturedElementId === undefined) {
            return undefined;
          }
          currentElementId = capturedElementId;
        }
        stack.push({
          capturedChildren: [],
          childIndex: 0,
          componentPath: captured.component === undefined ? [] : [...frame.componentPath, captured.key],
          depth: frame.depth + 1,
          expectedParent: frame.node,
          nearestElementId: currentElementId,
          node: child,
        });
        continue;
      }

      const result = captureCompletedFrame(
        frame,
        indexedElements,
        componentIds,
        consumedChildCountByParentId,
        componentPropertyBudget,
        usedElementIds,
      );
      if (result === undefined) {
        return undefined;
      }
      stack.pop();
      const parentFrame = stack[stack.length - 1];
      if (parentFrame === undefined) {
        mergedRoot = result;
      } else {
        parentFrame.capturedChildren.push(result);
      }
    }

    if (
      mergedRoot === undefined ||
      usedElementIds.size !== indexedElements.elementsById.size ||
      !allElementChildrenConsumed(indexedElements, consumedChildCountByParentId) ||
      getRootVirtualNode.call(renderer) !== rootVirtualNode ||
      !isCapturedVirtualTopologyCurrent(capturedNodes, renderer, getDebugVirtualNodeSnapshot)
    ) {
      return undefined;
    }
    return mergedRoot.node;
  } catch (_error) {
    console.warn('Valdi debugger could not capture a stable component hierarchy; using the element hierarchy.');
    return undefined;
  }
}

function captureCompletedFrame(
  frame: VirtualTraversalFrame,
  indexedElements: IndexedElementTree,
  componentIds: Set<string>,
  consumedChildCountByParentId: Map<string | null, number>,
  componentPropertyBudget: ComponentPropertyBudget,
  usedElementIds: Set<string>,
): CapturedHierarchyNode | undefined {
  const captured = frame.captured;
  if (captured === undefined) {
    return undefined;
  }
  if (captured.element !== undefined) {
    const elementId = readElementId(captured.element);
    if (elementId === undefined || usedElementIds.has(elementId)) {
      return undefined;
    }
    const elementSnapshot = indexedElements.elementsById.get(elementId);
    if (
      elementSnapshot === undefined ||
      indexedElements.parentIdById.get(elementId) !== frame.nearestElementId ||
      !consumeElementInPhysicalOrder(elementId, frame.nearestElementId, indexedElements, consumedChildCountByParentId)
    ) {
      return undefined;
    }
    usedElementIds.add(elementId);
    return {
      firstElementId: elementId,
      node: {
        ...elementSnapshot,
        children: frame.capturedChildren.map(child => child.node),
      },
    };
  }

  const component = captured.component;
  if (component === undefined) {
    return undefined;
  }
  const componentName = readComponentName(component);
  const componentPath = [...frame.componentPath, captured.key];
  const componentId = createComponentId(frame.nearestElementId, componentPath);
  if (componentName === undefined || componentId === undefined || componentIds.has(componentId)) {
    return undefined;
  }
  componentIds.add(componentId);
  const firstElementId = frame.capturedChildren.find(child => child.firstElementId !== undefined)?.firstElementId;
  const backingElement = firstElementId === undefined ? undefined : indexedElements.elementsById.get(firstElementId);
  const properties = captureComponentProperties(captured.componentViewModel, componentPropertyBudget);
  const node: WebRendererDebugComponentSnapshot = {
    ...(backingElement === undefined ? {} : { bounds: backingElement.bounds }),
    children: frame.capturedChildren.map(child => child.node),
    component: {
      ...(firstElementId === undefined ? {} : { elementId: firstElementId }),
      key: captured.key,
      name: componentName,
      ...(properties === undefined ? {} : { properties }),
    },
    id: componentId,
    tag: componentName,
  };
  captured.componentOutput = node;
  return {
    ...(firstElementId === undefined ? {} : { firstElementId }),
    node,
  };
}

function indexCompleteElementTree(root: WebRendererDebugElementSnapshot): IndexedElementTree | undefined {
  const childIdsByParentId = new Map<string | null, string[]>();
  const elementsById = new Map<string, WebRendererDebugElementSnapshot>();
  const parentIdById = new Map<string, string | null>();
  const visited = new Set<WebRendererDebugElementSnapshot>();
  let childLinks = 0;
  const stack: Array<{ depth: number; node: WebRendererDebugElementSnapshot; parentId: string | null }> = [
    { depth: 0, node: root, parentId: null },
  ];
  while (stack.length > 0) {
    const frame = stack.pop()!;
    const node = frame.node;
    if (
      frame.depth >= MAX_COMPONENT_HIERARCHY_DEPTH ||
      visited.size >= MAX_COMPONENT_HIERARCHY_NODES ||
      visited.has(node) ||
      node.childrenTruncated === true ||
      !Array.isArray(node.children) ||
      !Number.isSafeInteger(node.element.id) ||
      node.element.id < 0 ||
      node.id !== String(node.element.id) ||
      elementsById.has(node.id)
    ) {
      return undefined;
    }
    childLinks += node.children.length;
    if (childLinks > MAX_COMPONENT_HIERARCHY_CHILD_LINKS) {
      return undefined;
    }
    visited.add(node);
    elementsById.set(node.id, node);
    parentIdById.set(node.id, frame.parentId);
    childIdsByParentId.set(
      node.id,
      node.children.map(child => child.id),
    );
    for (let index = node.children.length - 1; index >= 0; index--) {
      const child = node.children[index];
      if (typeof child !== 'object' || child === null) {
        return undefined;
      }
      stack.push({ depth: frame.depth + 1, node: child, parentId: node.id });
    }
  }
  childIdsByParentId.set(null, [root.id]);
  return { childIdsByParentId, elementsById, parentIdById };
}

function consumeElementInPhysicalOrder(
  elementId: string,
  parentId: string | null,
  indexedElements: IndexedElementTree,
  consumedChildCountByParentId: Map<string | null, number>,
): boolean {
  const childIds = indexedElements.childIdsByParentId.get(parentId);
  const childIndex = consumedChildCountByParentId.get(parentId) ?? 0;
  if (childIds === undefined || childIds[childIndex] !== elementId) {
    return false;
  }
  consumedChildCountByParentId.set(parentId, childIndex + 1);
  return true;
}

function allElementChildrenConsumed(
  indexedElements: IndexedElementTree,
  consumedChildCountByParentId: Map<string | null, number>,
): boolean {
  for (const [parentId, childIds] of indexedElements.childIdsByParentId) {
    if ((consumedChildCountByParentId.get(parentId) ?? 0) !== childIds.length) {
      return false;
    }
  }
  return true;
}

function isCapturedVirtualTopologyCurrent(
  capturedNodes: CapturedVirtualNode[],
  renderer: IRenderer,
  getDebugVirtualNodeSnapshot: DebugVirtualNodeSnapshotReader,
): boolean {
  let remainingChildLinks = MAX_COMPONENT_HIERARCHY_CHILD_LINKS;
  let remainingTraversalLinks = MAX_COMPONENT_HIERARCHY_TRAVERSAL_LINKS;
  for (const captured of capturedNodes) {
    const current = getDebugVirtualNodeSnapshot.call(
      renderer,
      captured.node,
      remainingChildLinks,
      remainingTraversalLinks,
    );
    const children = current?.children;
    if (
      current === undefined ||
      !Array.isArray(children) ||
      children.length > remainingChildLinks ||
      current.parent !== captured.parent ||
      current.element !== captured.element ||
      current.component !== captured.component ||
      current.key !== captured.key ||
      !Number.isSafeInteger(current.traversedLinkCount) ||
      current.traversedLinkCount < 0 ||
      current.traversedLinkCount > remainingTraversalLinks
    ) {
      return false;
    }
    if (current.componentViewModel !== captured.componentViewModel && captured.componentOutput !== undefined) {
      delete captured.componentOutput.component.properties;
    }
    remainingChildLinks -= children.length;
    remainingTraversalLinks -= current.traversedLinkCount;
    if (children.length !== captured.children.length) {
      return false;
    }
    for (let index = 0; index < children.length; index++) {
      if (children[index] !== captured.children[index]) {
        return false;
      }
    }
  }
  return true;
}

function captureComponentProperties(
  viewModel: unknown,
  budget: ComponentPropertyBudget,
): Record<string, unknown> | undefined {
  if (budget.remainingBytes <= 0) {
    return undefined;
  }
  const captured = captureDebuggerPropertiesSnapshot(
    viewModel,
    MAX_COMPONENT_PROPERTY_BYTES,
    COMPONENT_PROPERTY_LIMITS,
  );
  if (captured === undefined) {
    return undefined;
  }
  if (captured.serializedBytes > budget.remainingBytes) {
    budget.remainingBytes = 0;
    return undefined;
  }
  budget.remainingBytes -= captured.serializedBytes;
  return captured.value;
}

function readElementId(element: IRenderedElement): string | undefined {
  const id = element.id;
  return Number.isSafeInteger(id) && id >= 0 ? String(id) : undefined;
}

function readComponentName(component: IComponent): string | undefined {
  let prototype: object | null = Object.getPrototypeOf(component);
  let depth = 0;
  while (prototype !== null && depth < MAX_COMPONENT_PROTOTYPE_DEPTH) {
    const constructorDescriptor = Object.getOwnPropertyDescriptor(prototype, 'constructor');
    const constructorValue = constructorDescriptor?.value;
    if (typeof constructorValue === 'function') {
      const nameDescriptor = Object.getOwnPropertyDescriptor(constructorValue, 'name');
      const name = nameDescriptor?.value;
      if (typeof name === 'string' && name.length > 0 && name.length <= MAX_COMPONENT_NAME_CHARACTERS) {
        return name;
      }
    }
    prototype = Object.getPrototypeOf(prototype);
    depth++;
  }
  return undefined;
}

function createComponentId(nearestElementId: string | null, componentPath: string[]): string | undefined {
  const serializedPath = JSON.stringify([nearestElementId, ...componentPath]);
  const id = `component:${serializedPath}`;
  return id.length <= MAX_COMPONENT_HIERARCHY_ID_CHARACTERS ? id : undefined;
}
