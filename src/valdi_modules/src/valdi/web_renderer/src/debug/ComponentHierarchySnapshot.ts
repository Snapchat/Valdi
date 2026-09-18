import type { IComponent } from 'valdi_core/src/IComponent';
import type { IRenderedElement } from 'valdi_core/src/IRenderedElement';
import type { IRenderedVirtualNode } from 'valdi_core/src/IRenderedVirtualNode';
import type { IRenderer, RendererDebugVirtualNodeSnapshot } from 'valdi_core/src/IRenderer';
import type {
  WebRendererDebugComponentSnapshot,
  WebRendererDebugElementSnapshot,
  WebRendererDebugNodeSnapshot,
  WebRendererDebugPropertyEditMetadata,
} from '../ValdiWebRendererDelegate';
import {
  captureDebuggerPropertiesSnapshot,
  captureDebuggerPropertiesSnapshotWithReflectionLimit,
  type DebuggerValueSnapshotLimits,
} from './DebuggerValueSnapshot';

const MAX_COMPONENT_HIERARCHY_CHILD_LINKS = 1_000;
const MAX_COMPONENT_HIERARCHY_DEPTH = 64;
const MAX_COMPONENT_HIERARCHY_ID_CHARACTERS = 4_096;
const MAX_COMPONENT_HIERARCHY_NODES = 1_000;
const MAX_COMPONENT_HIERARCHY_TRAVERSAL_LINKS = 4_096;
const MAX_COMPONENT_NAME_CHARACTERS = 256;
const MAX_COMPONENT_KEY_CHARACTERS = 256;
const MAX_COMPONENT_PROTOTYPE_DEPTH = 16;
const MAX_COMPONENT_PROPERTY_BYTES = 65_536;
const MAX_COMPONENT_STATE_CAPTURE_ATTEMPTS = 1_000;
const MAX_COMPONENT_STATE_CAPTURE_REFLECTION_OPERATIONS = 16_384;
const MAX_COMPONENT_STATE_CAPTURE_WORK_BYTES = MAX_COMPONENT_PROPERTY_BYTES * 2;
const MAX_COMPONENT_STATE_CHARACTERS = 65_536;
const MAX_EDITABLE_VIEW_MODEL_OWN_KEYS = 1_000;
const EDITABLE_PROPERTY_DESCRIPTOR_FIELDS: Array<keyof PropertyDescriptor> = [
  'configurable',
  'enumerable',
  'get',
  'set',
  'value',
  'writable',
];
const COMPONENT_PROPERTY_LIMITS: DebuggerValueSnapshotLimits = {
  maximumDepth: 4,
  maximumEntries: 50,
  maximumPropertyNameCharacters: 256,
  maximumStringBytes: 65_536,
};
const COMPONENT_STATE_LIMITS: DebuggerValueSnapshotLimits = {
  // The synthetic wrapper consumes one level before the state value.
  maximumDepth: 13,
  maximumEntries: 100,
  maximumPropertyNameCharacters: 1_024,
  maximumStringBytes: 65_536,
};
const FORBIDDEN_EDITABLE_PROPERTY_NAMES = new Set(['__proto__', 'children', 'constructor', 'prototype']);

export interface ComponentPropertyEditCandidate {
  readonly component: IComponent;
  readonly componentId: string;
  readonly descriptor: PropertyDescriptor;
  readonly node: IRenderedVirtualNode;
  readonly propertyName: string;
  readonly viewModel: object;
  readonly viewModelExtensible: boolean;
}

export type ComponentPropertyEditRegistrar = (
  candidate: ComponentPropertyEditCandidate,
) => WebRendererDebugPropertyEditMetadata | undefined;

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
  runtimeStateIdentity?: object;
}

interface ComponentPropertyBudget {
  remainingBytes: number;
}

interface ComponentStateCaptureWorkBudget {
  exhausted: boolean;
  remainingBytes: number;
  remainingCaptureAttempts: number;
  remainingReflectionOperations: number;
}

interface EditableViewModelShape {
  readonly descriptors: ReadonlyMap<string | symbol, PropertyDescriptor>;
  readonly extensible: boolean;
  readonly keys: readonly (string | symbol)[];
  readonly prototype: object | null;
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
  componentPropertyEditRegistrar?: ComponentPropertyEditRegistrar,
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
  const componentStateCaptureWorkBudget: ComponentStateCaptureWorkBudget = {
    exhausted: false,
    remainingBytes: MAX_COMPONENT_STATE_CAPTURE_WORK_BYTES,
    remainingCaptureAttempts: MAX_COMPONENT_STATE_CAPTURE_ATTEMPTS,
    remainingReflectionOperations: MAX_COMPONENT_STATE_CAPTURE_REFLECTION_OPERATIONS,
  };
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
        componentStateCaptureWorkBudget,
        componentPropertyEditRegistrar,
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
  componentStateCaptureWorkBudget: ComponentStateCaptureWorkBudget,
  componentPropertyEditRegistrar: ComponentPropertyEditRegistrar | undefined,
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
  const propertyCapture = captureComponentProperties(
    captured.componentViewModel,
    componentPropertyBudget,
    componentPropertyEditRegistrar,
    {
      component,
      componentId,
      node: captured.node,
    },
  );
  const runtimeStateCapture = captureComponentRuntimeState(
    component,
    componentPropertyBudget,
    componentStateCaptureWorkBudget,
  );
  const node: WebRendererDebugComponentSnapshot = {
    ...(backingElement === undefined ? {} : { bounds: backingElement.bounds }),
    children: frame.capturedChildren.map(child => child.node),
    component: {
      ...(firstElementId === undefined ? {} : { elementId: firstElementId }),
      key: captured.key,
      name: componentName,
      ...(propertyCapture === undefined ? {} : { properties: propertyCapture.properties }),
      ...(propertyCapture?.propertyEdits === undefined ? {} : { propertyEdits: propertyCapture.propertyEdits }),
      ...(runtimeStateCapture === undefined ? {} : { state: runtimeStateCapture.serialized }),
    },
    id: componentId,
    tag: componentName,
  };
  captured.componentOutput = node;
  captured.runtimeStateIdentity = runtimeStateCapture?.identity;
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
    if (captured.componentOutput !== undefined) {
      if (current.componentViewModel !== captured.componentViewModel) {
        delete captured.componentOutput.component.properties;
        delete captured.componentOutput.component.propertyEdits;
      }
      if (
        captured.runtimeStateIdentity !== undefined &&
        !hasExactRuntimeStateIdentity(current.component, captured.runtimeStateIdentity)
      ) {
        delete captured.componentOutput.component.state;
      }
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

interface ComponentPropertyCaptureIdentity {
  readonly component: IComponent;
  readonly componentId: string;
  readonly node: IRenderedVirtualNode;
}

interface ComponentPropertyCapture {
  readonly properties: Record<string, unknown>;
  readonly propertyEdits?: Record<string, WebRendererDebugPropertyEditMetadata>;
}

interface ComponentRuntimeStateCapture {
  readonly identity: object;
  readonly serialized: string;
}

function captureComponentProperties(
  viewModel: unknown,
  budget: ComponentPropertyBudget,
  componentPropertyEditRegistrar: ComponentPropertyEditRegistrar | undefined,
  identity: ComponentPropertyCaptureIdentity,
): ComponentPropertyCapture | undefined {
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
  const propertyEdits = captureComponentPropertyEdits(
    viewModel,
    captured.value,
    componentPropertyEditRegistrar,
    identity,
  );
  return {
    properties: captured.value,
    ...(propertyEdits === undefined ? {} : { propertyEdits }),
  };
}

function captureComponentRuntimeState(
  component: IComponent,
  budget: ComponentPropertyBudget,
  workBudget: ComponentStateCaptureWorkBudget,
): ComponentRuntimeStateCapture | undefined {
  if (
    budget.remainingBytes <= 0 ||
    workBudget.exhausted ||
    workBudget.remainingBytes <= 0 ||
    workBudget.remainingCaptureAttempts <= 0 ||
    workBudget.remainingReflectionOperations <= 0
  ) {
    if (workBudget.remainingCaptureAttempts <= 0 || workBudget.remainingReflectionOperations <= 0) {
      workBudget.exhausted = true;
    }
    return undefined;
  }
  try {
    const stateDescriptor = Reflect.getOwnPropertyDescriptor(component, 'state');
    if (
      stateDescriptor?.enumerable !== true ||
      !Object.prototype.hasOwnProperty.call(stateDescriptor, 'value') ||
      typeof stateDescriptor.value !== 'object' ||
      stateDescriptor.value === null
    ) {
      return undefined;
    }
    const identity = stateDescriptor.value;
    const wrapper = Object.create(null) as Record<string, unknown>;
    Object.defineProperty(wrapper, 'state', {
      configurable: true,
      enumerable: true,
      value: identity,
      writable: true,
    });
    if (!consumeComponentStateCaptureAttempt(workBudget)) {
      return undefined;
    }
    const captured = captureDebuggerPropertiesSnapshotWithReflectionLimit(
      wrapper,
      Math.min(MAX_COMPONENT_PROPERTY_BYTES, workBudget.remainingBytes),
      workBudget.remainingReflectionOperations,
      COMPONENT_STATE_LIMITS,
    );
    if (captured === undefined) {
      exhaustComponentStateCaptureWork(workBudget);
      return undefined;
    }
    if (
      !consumeComponentStateCaptureWork(
        workBudget,
        captured.serializedBytes,
        captured.reflectionOperations,
        captured.unmeasurable,
      )
    ) {
      return undefined;
    }
    const capturedDescriptor = Object.getOwnPropertyDescriptor(captured?.value ?? {}, 'state');
    if (
      capturedDescriptor?.enumerable !== true ||
      !Object.prototype.hasOwnProperty.call(capturedDescriptor, 'value') ||
      typeof capturedDescriptor.value !== 'object' ||
      capturedDescriptor.value === null ||
      !hasExactRuntimeStateIdentity(component, identity)
    ) {
      return undefined;
    }
    const serialized = JSON.stringify(capturedDescriptor.value);
    if (typeof serialized !== 'string' || serialized.length > MAX_COMPONENT_STATE_CHARACTERS) {
      return undefined;
    }
    const wireWrapper = Object.create(null) as Record<string, unknown>;
    Object.defineProperty(wireWrapper, 'state', {
      configurable: true,
      enumerable: true,
      value: serialized,
      writable: true,
    });
    if (workBudget.exhausted || workBudget.remainingBytes <= 0 || workBudget.remainingReflectionOperations <= 0) {
      return undefined;
    }
    if (!consumeComponentStateCaptureAttempt(workBudget)) {
      return undefined;
    }
    const wireCapture = captureDebuggerPropertiesSnapshotWithReflectionLimit(
      wireWrapper,
      Math.min(budget.remainingBytes, workBudget.remainingBytes),
      workBudget.remainingReflectionOperations,
      COMPONENT_STATE_LIMITS,
    );
    if (wireCapture === undefined) {
      exhaustComponentStateCaptureWork(workBudget);
      return undefined;
    }
    if (
      !consumeComponentStateCaptureWork(
        workBudget,
        wireCapture.serializedBytes,
        wireCapture.reflectionOperations,
        wireCapture.unmeasurable,
      )
    ) {
      return undefined;
    }
    const wireDescriptor = Object.getOwnPropertyDescriptor(wireCapture?.value ?? {}, 'state');
    if (
      wireDescriptor?.enumerable !== true ||
      !Object.prototype.hasOwnProperty.call(wireDescriptor, 'value') ||
      wireDescriptor.value !== serialized
    ) {
      return undefined;
    }
    budget.remainingBytes -= wireCapture.serializedBytes;
    return { identity, serialized };
  } catch (_error) {
    // Throwing or revoked Proxy reflection is an expected trust-boundary
    // outcome. Exhausting the aggregate work budget prevents repeated
    // unmeasurable capture attempts while leaving properties intact.
    exhaustComponentStateCaptureWork(workBudget);
    return undefined;
  }
}

function consumeComponentStateCaptureAttempt(budget: ComponentStateCaptureWorkBudget): boolean {
  if (budget.exhausted || budget.remainingCaptureAttempts <= 0) {
    exhaustComponentStateCaptureWork(budget);
    return false;
  }
  budget.remainingCaptureAttempts--;
  return true;
}

function consumeComponentStateCaptureWork(
  budget: ComponentStateCaptureWorkBudget,
  serializedBytes: number,
  reflectionOperations: number,
  unmeasurable: boolean,
): boolean {
  if (
    budget.exhausted ||
    unmeasurable ||
    !Number.isSafeInteger(serializedBytes) ||
    serializedBytes <= 0 ||
    serializedBytes > budget.remainingBytes ||
    !Number.isSafeInteger(reflectionOperations) ||
    reflectionOperations < 0 ||
    reflectionOperations > budget.remainingReflectionOperations
  ) {
    exhaustComponentStateCaptureWork(budget);
    return false;
  }
  budget.remainingBytes -= serializedBytes;
  budget.remainingReflectionOperations -= reflectionOperations;
  budget.exhausted = budget.remainingBytes === 0 || budget.remainingReflectionOperations === 0;
  return true;
}

function exhaustComponentStateCaptureWork(budget: ComponentStateCaptureWorkBudget): void {
  budget.exhausted = true;
  budget.remainingBytes = 0;
  budget.remainingCaptureAttempts = 0;
  budget.remainingReflectionOperations = 0;
}

function hasExactRuntimeStateIdentity(component: IComponent | undefined, identity: object): boolean {
  if (component === undefined) return false;
  try {
    const descriptor = Reflect.getOwnPropertyDescriptor(component, 'state');
    return (
      descriptor?.enumerable === true &&
      Object.prototype.hasOwnProperty.call(descriptor, 'value') &&
      Object.is(descriptor.value, identity)
    );
  } catch (_error) {
    // Revalidation may encounter a throwing or revoked Proxy. Treat it as a
    // state-only identity mismatch without affecting the hierarchy snapshot.
    return false;
  }
}

function captureComponentPropertyEdits(
  viewModel: unknown,
  capturedProperties: Record<string, unknown>,
  registrar: ComponentPropertyEditRegistrar | undefined,
  identity: ComponentPropertyCaptureIdentity,
): Record<string, WebRendererDebugPropertyEditMetadata> | undefined {
  if (registrar === undefined || typeof viewModel !== 'object' || viewModel === null) {
    return undefined;
  }
  try {
    const shape = captureStableEditableViewModelShape(viewModel);
    if (shape === undefined) return undefined;
    const propertyEdits = Object.create(null) as Record<string, WebRendererDebugPropertyEditMetadata>;
    for (const propertyName of Object.keys(capturedProperties)) {
      if (propertyName.trim().length === 0 || FORBIDDEN_EDITABLE_PROPERTY_NAMES.has(propertyName)) continue;
      const descriptor = shape.descriptors.get(propertyName);
      const capturedDescriptor = Object.getOwnPropertyDescriptor(capturedProperties, propertyName);
      if (
        descriptor === undefined ||
        descriptor.enumerable !== true ||
        !Object.prototype.hasOwnProperty.call(descriptor, 'value') ||
        capturedDescriptor === undefined ||
        !Object.prototype.hasOwnProperty.call(capturedDescriptor, 'value') ||
        !isEditableScalar(descriptor.value) ||
        !Object.is(capturedDescriptor.value, descriptor.value)
      ) {
        continue;
      }
      const metadata = registrar({
        component: identity.component,
        componentId: identity.componentId,
        descriptor: { ...descriptor },
        node: identity.node,
        propertyName,
        viewModel,
        viewModelExtensible: shape.extensible,
      });
      if (metadata !== undefined) {
        Object.defineProperty(propertyEdits, propertyName, {
          configurable: true,
          enumerable: true,
          value: metadata,
          writable: true,
        });
      }
    }
    return Object.keys(propertyEdits).length === 0 ? undefined : propertyEdits;
  } catch (_error) {
    return undefined;
  }
}

function captureEditableViewModelShape(viewModel: object): EditableViewModelShape | undefined {
  const keys = Reflect.ownKeys(viewModel);
  if (keys.length > MAX_EDITABLE_VIEW_MODEL_OWN_KEYS) return undefined;
  const descriptors = new Map<string | symbol, PropertyDescriptor>();
  for (const key of keys) {
    const descriptor = Reflect.getOwnPropertyDescriptor(viewModel, key);
    if (descriptor === undefined) return undefined;
    if (!isEditableViewModelDataDescriptor(descriptor)) return undefined;
    descriptors.set(key, descriptor);
  }
  const prototype = Reflect.getPrototypeOf(viewModel);
  if (prototype !== Object.prototype && prototype !== null) return undefined;
  return {
    descriptors,
    extensible: Reflect.isExtensible(viewModel),
    keys,
    prototype,
  };
}

function captureStableEditableViewModelShape(viewModel: object): EditableViewModelShape | undefined {
  const captured = captureEditableViewModelShape(viewModel);
  const verified = captureEditableViewModelShape(viewModel);
  if (
    captured === undefined ||
    verified === undefined ||
    captured.prototype !== verified.prototype ||
    captured.extensible !== verified.extensible ||
    captured.keys.length !== verified.keys.length
  ) {
    return undefined;
  }
  for (let index = 0; index < captured.keys.length; index++) {
    const capturedKey = captured.keys[index];
    const verifiedKey = verified.keys[index];
    const capturedDescriptor = captured.descriptors.get(capturedKey);
    const verifiedDescriptor = verified.descriptors.get(verifiedKey);
    if (
      capturedKey !== verifiedKey ||
      capturedDescriptor === undefined ||
      verifiedDescriptor === undefined ||
      !sameEditablePropertyDescriptor(capturedDescriptor, verifiedDescriptor)
    ) {
      return undefined;
    }
  }
  return verified;
}

function sameEditablePropertyDescriptor(left: PropertyDescriptor, right: PropertyDescriptor): boolean {
  for (const field of EDITABLE_PROPERTY_DESCRIPTOR_FIELDS) {
    const leftField = Object.getOwnPropertyDescriptor(left, field);
    const rightField = Object.getOwnPropertyDescriptor(right, field);
    if (leftField === undefined || rightField === undefined) {
      if (leftField !== rightField) return false;
      continue;
    }
    if (
      !Object.prototype.hasOwnProperty.call(leftField, 'value') ||
      !Object.prototype.hasOwnProperty.call(rightField, 'value') ||
      !Object.is(leftField.value, rightField.value)
    ) {
      return false;
    }
  }
  return true;
}

function isEditableViewModelDataDescriptor(descriptor: PropertyDescriptor): boolean {
  const valueField = Object.getOwnPropertyDescriptor(descriptor, 'value');
  return (
    valueField !== undefined &&
    Object.prototype.hasOwnProperty.call(valueField, 'value') &&
    typeof valueField.value !== 'function'
  );
}

function isEditableScalar(value: unknown): value is boolean | number | string {
  return (
    typeof value === 'string' ||
    typeof value === 'boolean' ||
    (typeof value === 'number' && Number.isFinite(value) && !Object.is(value, -0))
  );
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
