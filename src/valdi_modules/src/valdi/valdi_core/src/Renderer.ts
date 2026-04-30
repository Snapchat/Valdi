import { arrayEquals } from 'coreutils/src/ArrayUtils';
import { StringMap } from 'coreutils/src/StringMap';
import { StringSet } from 'coreutils/src/StringSet';
import { ElementFrame } from 'valdi_tsx/src/Geometry';
import { EventTime } from 'valdi_tsx/src/GestureEvents';
import { IRenderedComponentHolder } from 'valdi_tsx/src/IRenderedComponentHolder';
import { IRenderedElementHolder } from 'valdi_tsx/src/IRenderedElementHolder';
import { NativeNode } from 'valdi_tsx/src/NativeNode';
import { NativeView } from 'valdi_tsx/src/NativeView';
import { AnimationOptions, BasicAnimationOptions, SpringAnimationOptions } from './AnimationOptions';
import { AnyRenderFunction } from './AnyRenderFunction';
import { DumpedLogs } from './BugReporter';
import { CancelToken } from './CancellableAnimation';
import { CapturedRenderedVirtualNode, IElementData } from './CapturedNode';
import { ComponentPrototype } from './ComponentPrototype';
import { ConsoleRepresentable } from './ConsoleRepresentable';
import { ComponentConstructor, IComponent } from './IComponent';
import { IRenderedElement } from './IRenderedElement';
import { IRenderedVirtualNode } from './IRenderedVirtualNode';
import { ComponentDisposable, IRenderer, RendererObserver } from './IRenderer';
import { IRendererDelegate } from './IRendererDelegate';
import { IRendererEventListener } from './IRendererEventListener';
import { NodePrototype } from './NodePrototype';
import { Style } from './Style';
import { MergeType } from './utility_types/MergeType';
import { tryReuseCallback } from './utils/CallbackInternal';
import { classNames } from './utils/ClassNames';
import { enumeratePropertyList, PropertyList, propertyListToObject } from './utils/PropertyList';
import { computeUniqueId } from './utils/RenderedVirtualNodeUtils';
import { RendererError } from './utils/RendererError';
import { trace } from './utils/Trace';

const EMPTY_OBJECT = Object.freeze({});
const EMPTY_ARRAY = Object.freeze([]) as [];

interface NodeChildren<T> {
  childByKey: { [key: string]: T };
  children: T[];
  previousChildren?: T[];
  insertionIndex: number;
}

interface Node {
  key: string;
  lastRenderId: symbol;
  children?: NodeChildren<Node>;
  parent?: Node;
  parentIndex: number;
}

/**
 * Represents a node inside the virtual node
 * tree that contains either a component or an element.
 */
interface VirtualNode extends Node {
  children: NodeChildren<VirtualNode> | undefined;
  parent: VirtualNode | undefined;

  element?: RenderedElement;
  component?: RenderedComponent;
  slot?: boolean;

  // The nearest parent element
  parentElement: RenderedElement | undefined;
  // The nearest parent component
  parentComponent: RenderedComponent | undefined;

  // Bridge instance, will be set if the virtual node was requested externally.
  bridge?: VirtualNodeBridge;
}

interface RenderedElement {
  virtualNode: VirtualNode;

  id: number;
  nodePrototype: NodePrototype | undefined;
  attributes: StringMap<any>;

  wasVisibleOnce?: boolean;
  // Bridge instance, will be set if the element was requested externally.
  bridge?: RenderedElementBridge;
  // Resolved children of this element
  childrenHead: RenderedElement | undefined;
  // Whether children are dirty and need to be resolved
  childrenDirty: boolean;
  // The last resolved frame
  frame: ElementFrame | undefined;

  // Siblings
  next: RenderedElement | undefined;
  prev: RenderedElement | undefined;

  // Ordering
  parentIndex: number | undefined;
}

interface RenderedComponent<T extends IComponent = IComponent> {
  virtualNode: VirtualNode;

  ctr: ComponentConstructor<T>;
  prototype: ComponentPrototype;

  instance: IComponent | undefined;
  viewModel: any | undefined;
  needRendering: boolean;
  viewModelChanged: boolean;
  // Whether the component is currently rendering
  rendering: boolean;

  // Attributes to inject on the root elements
  // of a component. Mostly there for backward
  // compatibility with the Vue Valdi's implementation.
  injectedAttributes: any | undefined;

  rerenderScheduled?: boolean;

  context?: any;
  disposables?: ComponentDisposable[];

  onCreateObserver?: (instance: any) => void;
  onDestroyObserver?: (instance: any) => void;

  componentRef: IRenderedComponentHolder<T, IRenderedVirtualNode> | undefined;
}

interface ComponentSlotData<F extends AnyRenderFunction = AnyRenderFunction> {
  // The node key for the slot.
  nodeKey: string;
  renderFunc: F;
  // The emitted node for the slot. Will be created when the component renders its slot
  // the first time.
  node: VirtualNode | undefined;
  lastParameters: Parameters<F> | undefined;
  ref?: IRenderedElementHolder<any>;
  // Whether the nodeKey was explicitly provided by the user
  isKeyExplicit?: boolean;
}

const SLOT_DATA_KEY = Symbol();

interface SlotFunction<F extends AnyRenderFunction> {
  (...params: Parameters<F>): ReturnType<F>;
  [SLOT_DATA_KEY]: ComponentSlotData<F>;
}

const IS_NAMED_SLOT_KEY = Symbol();

interface NamedSlotFunctions {
  [key: string]: SlotFunction<AnyRenderFunction> | undefined;
  [IS_NAMED_SLOT_KEY]: boolean;
}

let currentRenderer: Renderer | undefined;

function getRenderedElementBridge(renderer: Renderer, renderedElement: RenderedElement): RenderedElementBridge {
  if (renderedElement.bridge) {
    return renderedElement.bridge;
  }

  const instance = new RenderedElementBridge(renderer, renderedElement);
  renderedElement.bridge = instance;
  return instance;
}

function getVirtualNodeBridge(renderer: Renderer, virtualNode: VirtualNode): VirtualNodeBridge {
  if (virtualNode.bridge) {
    return virtualNode.bridge;
  }

  const instance = new VirtualNodeBridge(renderer, virtualNode);
  virtualNode.bridge = instance;
  return instance;
}

function getVirtualNodeBridgeChildren(renderer: Renderer, node: VirtualNode, children: IRenderedVirtualNode[]) {
  if (!node.children) {
    return;
  }

  for (const child of node.children.children) {
    if (child.slot) {
      getVirtualNodeBridgeChildren(renderer, child, children);
    } else {
      children.push(getVirtualNodeBridge(renderer, child));
    }
  }
}

class VirtualNodeBridge implements IRenderedVirtualNode {
  renderer: Renderer;
  private node: VirtualNode;

  private cachedUniqueId?: string;

  constructor(renderer: Renderer, node: VirtualNode) {
    this.renderer = renderer;
    this.node = node;
  }

  get key(): string {
    return this.node.key;
  }

  get parent(): IRenderedVirtualNode | undefined {
    let parent = this.node.parent;
    while (parent && parent.slot) {
      parent = parent.parent;
    }

    if (!parent || parent === this.renderer.nodeTree) {
      return undefined;
    }

    return getVirtualNodeBridge(this.renderer, parent);
  }

  get element(): IRenderedElement | undefined {
    if (!this.node.element) {
      return undefined;
    }

    return getRenderedElementBridge(this.renderer, this.node.element);
  }

  get component(): IComponent | undefined {
    if (!this.node.component) {
      return undefined;
    }

    return this.node.component.instance;
  }

  get children(): IRenderedVirtualNode[] {
    const out: IRenderedVirtualNode[] = [];

    getVirtualNodeBridgeChildren(this.renderer, this.node, out);

    return out;
  }

  get parentIndex(): number {
    return this.node.parentIndex;
  }

  get uniqueId(): string {
    if (this.cachedUniqueId) {
      return this.cachedUniqueId;
    }
    this.cachedUniqueId = computeUniqueId(this);
    return this.cachedUniqueId;
  }
}

class RenderedElementBridge implements IRenderedElement, ConsoleRepresentable {
  readonly element: RenderedElement;
  readonly renderer: Renderer;
  private _children?: IRenderedElement[];

  constructor(renderer: Renderer, element: RenderedElement) {
    this.renderer = renderer;
    this.element = element;
  }

  get tag(): string {
    return this.element.nodePrototype!.tag;
  }

  get viewClass(): string {
    return this.element.nodePrototype!.viewClass;
  }

  get id(): number {
    return this.element.id;
  }

  get key(): string {
    return this.element.virtualNode.key;
  }

  get children(): IRenderedElement[] {
    if (!this._children) {
      const element = this.element;
      const renderer = this.renderer;
      const children: IRenderedElement[] = [];
      let child = element.childrenHead;
      while (child) {
        children.push(getRenderedElementBridge(renderer, child));
        child = child.next;
      }
      this._children = children;
    }
    return this._children!;
  }

  get emittingComponent(): IComponent | undefined {
    return this.element.virtualNode.parentComponent?.instance;
  }

  get parent(): IRenderedElement | undefined {
    const parent = this.element.virtualNode.parentElement;
    if (!parent) {
      return undefined;
    }
    return getRenderedElementBridge(this.renderer, parent);
  }

  get parentIndex(): number {
    return this.element.parentIndex ?? 0;
  }

  get frame(): ElementFrame {
    return this.element.frame || { x: 0, y: 0, width: 0, height: 0 };
  }

  invalidateChildren() {
    this._children = undefined;
  }

  getAttributeNames(): string[] {
    return Object.keys(this.element.attributes);
  }

  getAttribute(name: string): any {
    return this.element.attributes[name];
  }

  setAttribute(name: string, value: any): boolean {
    if (!this.renderer.began) {
      this.renderer.doBegin();
      const changed = this.setAttribute(name, value);
      this.renderer.doEnd();
      return changed;
    }

    return this.renderer.setAttributeOnElement(this.element, name, value);
  }

  setAttributes(attributes: any): boolean {
    if (!attributes) {
      return false;
    }

    if (!this.renderer.began) {
      this.renderer.doBegin();
      const changed = this.setAttributes(attributes);
      this.renderer.doEnd();
      return changed;
    }

    let changed = false;

    enumeratePropertyList(attributes, (key, value) => {
      if (this.setAttribute(key, value)) {
        changed = true;
      }
    });

    return changed;
  }

  getNativeView(): Promise<NativeView | undefined> {
    return new Promise(resolve => {
      this.renderer.delegate.getNativeView(this.element.id, view => {
        resolve(view);
      });
    });
  }

  getNativeNode(): NativeNode | undefined {
    return this.renderer.delegate.getNativeNode(this.element.id);
  }

  takeSnapshot(): Promise<string | undefined> {
    return new Promise(resolve => {
      this.renderer.delegate.takeElementSnapshot(this.element.id, view => {
        resolve(view);
      });
    });
  }

  getVirtualNode(): IRenderedVirtualNode {
    return getVirtualNodeBridge(this.renderer, this.element.virtualNode);
  }

  toConsoleRepresentation() {
    return [this.viewClass, this.element.attributes];
  }
}

const renderedComponentKey = Symbol();
const destroyedComponentTombstone = Symbol();

function getNodeDescription(node: VirtualNode): string {
  const element = node.element;
  if (element) {
    const nodePrototype = element.nodePrototype;
    if (!nodePrototype) {
      return 'root';
    }

    return `element ${nodePrototype.viewClass} with key '${node.key}'`;
  }

  const component = node.component;
  if (component) {
    return `component ${component.ctr.name} with key '${node.key}'`;
  }

  const slot = node.slot;
  if (slot) {
    return `slot with key '${node.key}''`;
  }

  return 'unknown node';
}

interface RendererLogInfo {
  treeId: string;
  componentRerendersCount: number;
  rootRendersCount: number;
  componentsCount: number;
  elementsCount: number;
  slotsCount: number;
}

function doCaptureVirtualNode(
  node: VirtualNode,
  renderer: Renderer,
  onEmit: (node: VirtualNode, emitted: CapturedRenderedVirtualNode) => void,
): CapturedRenderedVirtualNode {
  const generatedChildren: CapturedRenderedVirtualNode[] = [];

  if (node.children) {
    const children = node.children;
    for (let i = 0; i < children.insertionIndex && i < children.children.length; i++) {
      const childNode = children.children[i];

      generatedChildren.push(doCaptureVirtualNode(childNode, renderer, onEmit));
    }
  }

  let elementData: IElementData | undefined;
  if (node.element) {
    const nodePrototype = node.element.nodePrototype!;
    elementData = {
      id: node.element.id,
      tag: nodePrototype.tag,
      viewClass: nodePrototype.viewClass,
      attributes: { ...node.element.attributes },
      frame: node.element.frame || { x: 0, y: 0, width: 0, height: 0 },
    };
  }

  const emittedNode = new CapturedRenderedVirtualNode(
    node.key,
    elementData,
    node.component?.instance,
    generatedChildren,
    renderer,
  );

  onEmit(node, emittedNode);

  return emittedNode;
}

/**
 * Build an ImmutableVirtualNode tree from the root.
 * Returns the generated node for the given node, or undefined if
 * the node was not found (should not happen unless the renderer
 * has an inccorrect state.).
 */
function captureVirtualNode(
  root: VirtualNode,
  node: VirtualNode,
  renderer: Renderer,
): IRenderedVirtualNode | undefined {
  let outNode: IRenderedVirtualNode | undefined;

  if (root.children) {
    // The root is a placeholder, it's not technically an element or component so we
    // only explicitly visit the direct children of it.
    for (const rootChild of root.children.children) {
      doCaptureVirtualNode(rootChild, renderer, (visited, emitted) => {
        if (visited === node) {
          outNode = emitted;
        }
      });
    }
  }

  return outNode;
}

function makeSlotFunction<F extends AnyRenderFunction>(
  renderer: Renderer,
  slot: ComponentSlotData<F>,
): SlotFunction<F> {
  const slotFunction = function (...args: Parameters<F>): ReturnType<F> {
    slot.lastParameters = args;

    renderer.beginSlot(slot);

    slot.renderFunc.apply(undefined, args);

    renderer.endSlot(slot);

    return undefined as any;
  } as SlotFunction<F>;

  slotFunction[SLOT_DATA_KEY] = slot;

  return slotFunction;
}

/**
 * TODO(simon): Remaining possible optimizations:
 *
 * - Interning ViewClass: we ask the c++ runtime for a unique object representing a view class.
 *   The view class already contains the bound attributes which allows to not have to do a lookup
 *    in the main thread.
 *
 */

export class Renderer implements IRenderer {
  static debug = false;

  contextId: string;

  readonly delegate: IRendererDelegate;

  // Used for resolving nested rendering.
  private parent: Renderer | undefined;

  private currentNode: VirtualNode | undefined;
  private nodeStack: VirtualNode[] = [];

  readonly nodeTree: VirtualNode;
  private readonly elementTree: RenderedElement;

  private renderId = Symbol();
  private nodeIdSequence = 0;
  private hasObservers = false;
  private elementById: { [id: number]: RenderedElement } = {};
  private pendingRenders: (() => void)[] = [];
  private completions: (() => void)[] | undefined;
  private componentRerendersCount = 0;
  private rootRendersCount = 0;
  private beginCount = 0;
  private observers: RendererObserver[] = [];
  private nextAnimationCancelToken: CancelToken = 0;
  private eventListener: IRendererEventListener | undefined = undefined;
  private allowedRootElementTypes?: StringSet;

  /** Moves buffered during render; flushed in top-down order at end to avoid bottom-up attach (ANR). */
  private pendingRootMove: { elementId: number } | undefined = undefined;
  private pendingMovesByParent = new Map<number, Array<{ elementId: number; parentIndex: number }>>();
  private pendingMoveElementIds = new Set<number>();

  /** When true (gated by VALDI_MAX_VIEW_OPERATIONS_PROCESSING_TIME), emit moves in top-down order. */
  private _useTopDownMoveOrder: boolean = false;

  enableLog: boolean = false;
  private logComponentsOnly = true;

  get began(): boolean {
    return this.beginCount > 0;
  }

  constructor(
    contextId: string,
    allowedRootElementTypes: string[] | undefined,
    delegate: IRendererDelegate,
    useTopDownMoveOrder?: boolean,
  ) {
    this.contextId = contextId;
    this.delegate = delegate;
    this._useTopDownMoveOrder = useTopDownMoveOrder ?? false;
    if (allowedRootElementTypes) {
      this.allowedRootElementTypes = {};
      for (const allowedRootElementType of allowedRootElementTypes) {
        this.allowedRootElementTypes[allowedRootElementType] = true;
      }
    }

    this.nodeTree = {
      lastRenderId: this.renderId,
      key: '',
      element: undefined,
      children: undefined,
      parent: undefined,
      parentComponent: undefined,
      parentElement: undefined,
      parentIndex: 0,
    };
    this.elementTree = {
      virtualNode: this.nodeTree,
      id: 0,
      attributes: {},
      childrenHead: undefined,
      childrenDirty: true,
      nodePrototype: undefined,
      frame: undefined,
      next: undefined,
      prev: undefined,
      parentIndex: undefined,
    };
    this.nodeTree.element = this.elementTree;
  }

  batchUpdates(block: () => void) {
    if (this.began) {
      block();
    } else {
      try {
        this.doBegin();
        block();
      } catch (err: any) {
        this.onUncaughtError('Got an error while rendering batch update', err);
        throw err;
      } finally {
        this.doEnd();
      }
    }
  }

  animate(options: AnimationOptions, block: () => void): CancelToken {
    this.validateAnimationOptions(options);
    const token = ++this.nextAnimationCancelToken;

    this.batchUpdates(() => {
      try {
        this.beginAnimation(options, token);
        block();
      } finally {
        this.endAnimation();
      }
    });
    return token;
  }

  cancelAnimation(token: CancelToken) {
    this.batchUpdates(() => {
      this.delegate.onAnimationCancel(token);
    });
  }

  private validateAnimationOptions(options: MergeType<AnimationOptions>) {
    if (options.duration === undefined) {
      this.validateSpringAnimationOptions(options);
    } else {
      this.validateBasicAnimationOptions(options as BasicAnimationOptions);
    }
  }

  private validateSpringAnimationOptions(options: Partial<SpringAnimationOptions>) {
    if (!options.stiffness || options.stiffness < 0) {
      throw new Error(`Spring animation damping must be greater than 0. Provided: ${options.stiffness}`);
    }

    if (!options.damping || options.damping < 0) {
      throw new Error(`Spring animation damping must be greater than 0. Provided: ${options.damping}`);
    }
  }

  private validateBasicAnimationOptions(options: BasicAnimationOptions) {
    if (options.duration <= 0) {
      throw new Error(`Animation duration must be greater than 0. Provided: ${options.duration}`);
    }
  }

  doBegin() {
    if (this.currentNode) {
      throw Error('Already rendering');
    }

    this.renderId = Symbol();
    this.beginCount++;

    if (this.beginCount === 1) {
      this.parent = currentRenderer;
      // eslint-disable-next-line @typescript-eslint/no-this-alias
      currentRenderer = this;

      this.delegate.onRenderStart();
      if (this.eventListener) {
        this.eventListener.onRenderBegin();
      }

      if (!this.hasObservers) {
        this.hasObservers = true;
        this.delegate.registerVisibilityObserver(this.elementsVisibilityChanged.bind(this));
        this.delegate.registerFrameObserver(this.processFrameUpdates.bind(this));
      }
    }
  }

  renderRoot(renderFunc: () => void) {
    this.begin();
    try {
      this.rootRendersCount++;
      renderFunc();
    } catch (err: any) {
      this.onUncaughtError('Got an error while rendering root component', err);
      throw err;
    } finally {
      this.end();
    }
  }

  renderRootComponent<T extends IComponent<ViewModel, Context>, ViewModel = any, Context = any>(
    ctr: ComponentConstructor<T>,
    prototype: ComponentPrototype,
    viewModel: ViewModel,
    context: Context,
  ) {
    this.renderRoot(() => {
      this.beginComponent(ctr, prototype);
      if (context) {
        this.setComponentContext(context);
      }
      if (viewModel) {
        this.setViewModelProperties(viewModel);
      }
      this.endComponent();
    });
  }

  dumpLogMetadata(): DumpedLogs {
    const logInfo: RendererLogInfo = {
      treeId: this.contextId,
      componentRerendersCount: this.componentRerendersCount,
      rootRendersCount: this.rootRendersCount,
      componentsCount: 0,
      elementsCount: 0,
      slotsCount: 0,
    };

    if (this.nodeTree.children && this.nodeTree.children.children[0]) {
      this.collectLogInfo(this.nodeTree.children.children[0], logInfo);
    }

    return {
      verbose: logInfo,
    };
  }

  begin() {
    this.doBegin();

    const node = this.nodeTree;
    node.lastRenderId = this.renderId;
    this.pushVirtualNode(node);
  }

  doEnd() {
    if (currentRenderer !== this) {
      throw Error('Unbalanced Renderer begin()/end() calls');
    }

    this.currentNode = undefined;

    while (this.nodeStack.length) {
      this.nodeStack.pop();
    }

    if (this.beginCount === 1) {
      if (this._useTopDownMoveOrder) {
        this.flushPendingMovesInTopDownOrder();
      }
      // Flush our pending nested render callbacks
      while (this.pendingRenders.length) {
        const pendingRender = this.pendingRenders.shift();
        pendingRender!();
      }
    }

    this.beginCount -= 1;

    if (this.beginCount === 0) {
      currentRenderer = this.parent;
      this.parent = undefined;

      this.delegate.onRenderEnd();
      if (this.eventListener) {
        this.eventListener.onRenderEnd();
      }

      if (this.completions) {
        const completions = this.completions;
        this.completions = undefined;
        for (const completion of completions) {
          completion();
        }
      }
    }
  }

  end() {
    this.popVirtualNode();
    this.doEnd();
  }

  beginAnimation(options: AnimationOptions, token: CancelToken) {
    this.delegate.onAnimationStart(options, token);
  }

  endAnimation() {
    this.delegate.onAnimationEnd();
  }

  isComponentAlive(component: IComponent): boolean {
    const renderedComponent = (component as any)[renderedComponentKey];
    return renderedComponent && renderedComponent !== destroyedComponentTombstone;
  }

  private collectLogInfo(node: VirtualNode, logInfo: RendererLogInfo) {
    if (node.element) {
      logInfo.elementsCount++;
    } else if (node.component) {
      logInfo.componentsCount++;
    } else if (node.slot) {
      logInfo.slotsCount++;
    }

    if (node.children) {
      for (const child of node.children.children) {
        this.collectLogInfo(child, logInfo);
      }
    }
  }

  private resolveRenderedComponent(component: IComponent): RenderedComponent {
    const renderedComponent: RenderedComponent | undefined | typeof destroyedComponentTombstone = (component as any)[
      renderedComponentKey
    ];
    if (!renderedComponent) {
      throw Error(`Could not resolve Component '${component.constructor.name}' in current Renderer`);
    } else if (renderedComponent === destroyedComponentTombstone) {
      throw Error(`Resolved destroyed Component '${component.constructor.name}' in current Renderer`);
    }
    return renderedComponent;
  }

  renderComponent(component: IComponent, viewModel: any | undefined): void {
    const renderedComponent = this.resolveRenderedComponent(component);

    if (this.currentNode) {
      if (renderedComponent.needRendering) {
        // It will already render
        return;
      }

      // Wait until the renderer becomes available
      if (!renderedComponent.rerenderScheduled) {
        renderedComponent.rerenderScheduled = true;
        this.pendingRenders.push(() => {
          if (!this.isComponentAlive(component)) {
            return;
          }

          renderedComponent.rerenderScheduled = false;
          this.renderComponent(component, viewModel);
        });
      }

      return;
    }

    // this.log('Component ', renderedComponent.ctr.name, ' asked to explicitly rerender');
    this.componentRerendersCount++;

    this.doBegin();
    try {
      trace(`renderComponent.${renderedComponent.ctr.name}`, () => {
        // Force rerender since it was explicitly asked
        renderedComponent.needRendering = true;

        for (const observer of this.observers) {
          if (observer.onComponentWillRerender) {
            observer.onComponentWillRerender(component);
          }
        }

        this.pushVirtualNode(renderedComponent.virtualNode);

        if (viewModel) {
          this.setViewModelProperties(viewModel);
        }

        this.endComponent();
      });
    } catch (e) {
      console.log(e);
    } finally {
      this.doEnd();
    }
  }

  beginElementIfNeeded(node: NodePrototype, key?: string): boolean {
    this.beginElement(node, key);

    const renderedNode = this.getCurrentNode();
    const element = renderedNode.element!;
    if (element.wasVisibleOnce) {
      return true;
    }

    if (!this.hasAttribute('onVisibilityChanged')) {
      this.setAttribute('onVisibilityChanged', () => {
        this.rerenderLazyComponentIfNeeded(element);
      });
    }

    this.endElement();

    return false;
  }

  beginElement(nodePrototype: NodePrototype, key?: string) {
    const currentNode = this.getCurrentNode();

    const resolvedKey = key || nodePrototype.id;
    const resolvedNode = this.resolveVirtualNode(currentNode, resolvedKey, undefined, undefined);

    let justCreated = false;

    if (!resolvedNode.element) {
      const id = ++this.nodeIdSequence;

      const element: RenderedElement = {
        id,
        attributes: {},
        nodePrototype: nodePrototype,
        childrenHead: undefined,
        childrenDirty: true,
        virtualNode: resolvedNode,
        frame: undefined,
        next: undefined,
        prev: undefined,
        parentIndex: undefined,
      };

      resolvedNode.element = element;

      this.delegate.onElementCreated(id, nodePrototype.viewClass);

      justCreated = true;

      this.elementById[id] = element;
    }

    this.pushVirtualNode(resolvedNode);

    if (justCreated && nodePrototype.attributes) {
      enumeratePropertyList(nodePrototype.attributes, (name, value) => {
        this.setAttributeOnElement(resolvedNode.element!, name, value);
      });
    }
  }

  endElement() {
    const currentNode = this.currentNode;
    if (currentNode && currentNode.parentElement === this.elementTree) {
      for (const observer of this.observers) {
        if (observer.onRootElementWillEndRender) {
          observer.onRootElementWillEndRender();
        }
      }
    }

    this.popVirtualNode();
  }

  private callVisiblityChanged(elementIds: number[], visible: boolean, eventTime: EventTime) {
    for (const elementId of elementIds) {
      const element = this.elementById[elementId];
      if (!element) {
        continue;
      }

      const visibilityChanged = element.attributes['onVisibilityChanged'] as (
        visible: boolean,
        eventTime: EventTime,
      ) => void;
      if (visibilityChanged) {
        visibilityChanged(visible, eventTime);
      }
    }
  }

  private callViewportChanged(viewportChanges: number[], eventTime: EventTime) {
    const length = viewportChanges.length;

    for (let i = 0; i < length; ) {
      const elementId = viewportChanges[i++];
      const x = viewportChanges[i++];
      const y = viewportChanges[i++];
      const width = viewportChanges[i++];
      const height = viewportChanges[i++];

      const element = this.elementById[elementId];
      if (!element) {
        continue;
      }

      const viewportChanged = element.attributes['onViewportChanged'] as (
        viewport: ElementFrame,
        frame: ElementFrame,
        eventTime: EventTime,
      ) => void;
      if (viewportChanged) {
        viewportChanged({ x, y, width, height }, element.frame!, eventTime);
      }
    }
  }

  private elementsVisibilityChanged(
    appearingElements: number[],
    disappearingElements: number[],
    viewportChanges: number[],
    eventTime: EventTime,
  ) {
    this.batchUpdates(() => {
      this.callVisiblityChanged(disappearingElements, false, eventTime);
      this.callVisiblityChanged(appearingElements, true, eventTime);
      this.callViewportChanged(viewportChanges, eventTime);
    });
  }

  private rerenderLazyComponentIfNeeded(element: RenderedElement) {
    if (element.wasVisibleOnce) {
      return;
    }
    element.wasVisibleOnce = true;
    const topComponent = element.virtualNode.parentComponent;
    if (!topComponent) {
      console.warn('Unable to retrieve top component from appearing element');
      return;
    }

    this.renderComponent(topComponent.instance!, undefined);
  }

  private pushVirtualNode(node: VirtualNode) {
    // if (!this.logComponentsOnly || node.component) {
    //   this.log('Begin ', getNodeDescription(node));
    // }

    if (node.children) {
      node.children.insertionIndex = 0;
    }
    node.lastRenderId = this.renderId;

    this.currentNode = node;
    this.nodeStack.push(node);
  }

  private popVirtualNode() {
    this.nodeStack.pop();
    const newCurrentNode = this.nodeStack[this.nodeStack.length - 1];

    const node = this.currentNode!;
    this.currentNode = newCurrentNode;

    this.processChildrenUpdate(node);

    const element = node.element;
    if (element) {
      this.resolveElementChildren(node, element);
    } else {
      if (newCurrentNode !== node.parent) {
        // We don't manage elements and our new current node is not our parent.
        // This means this was a detached render. We need to see
        // if we need to explicitly ask our parent element to resolve
        // the children.
        const parentElement = node.parentElement;
        if (!parentElement) {
          return;
        }

        if (
          newCurrentNode &&
          (newCurrentNode.element === parentElement || newCurrentNode.parentElement === parentElement)
        ) {
          // Our new current node is either our parentElement, or has the same parent element.
          // It will take care of resolving the element children on its own.
          return;
        }

        // Our new node is not going to take care of our element, so we do it ourself.
        this.resolveElementChildren(parentElement.virtualNode, parentElement);
      }
    }
    // if (!this.logComponentsOnly || node.component) {
    //   this.log('Ended ', getNodeDescription(node));
    // }
  }

  private throwDisallowedRootElementType(tag: string) {
    const allowedRootElementTypes = Object.keys(this.allowedRootElementTypes!)
      .map(v => `<${v}>`)
      .join(', ');
    throw new Error(`Root element must be one of: ${allowedRootElementTypes} (resolved root element is <${tag}>)`);
  }

  private onElementMoved(parentElement: RenderedElement, element: RenderedElement, index: number) {
    const parentId = parentElement.id;
    const id = element.id;
    if (parentId === 0) {
      // Root element
      if (index !== 0) {
        throw Error('Can only have 1 element as the root.');
      }
      const tag = element.nodePrototype!.tag;
      if (this.allowedRootElementTypes && !this.allowedRootElementTypes[tag]) {
        this.throwDisallowedRootElementType(tag);
      }
      if (this._useTopDownMoveOrder) {
        this.pendingRootMove = { elementId: id };
        this.pendingMoveElementIds.add(id);
      } else {
        this.delegate.onElementBecameRoot(id);
      }
    } else {
      if (this._useTopDownMoveOrder) {
        this.pendingMoveElementIds.add(id);
        let list = this.pendingMovesByParent.get(parentId);
        if (!list) {
          list = [];
          this.pendingMovesByParent.set(parentId, list);
        }
        list.push({ elementId: id, parentIndex: index });
      } else {
        this.delegate.onElementMoved(id, parentId, index);
      }
    }
  }

  /** Emit buffered moves in top-down order (parent attached before children) to avoid ANR. */
  private flushPendingMovesInTopDownOrder() {
    if (this.pendingRootMove === undefined && this.pendingMovesByParent.size === 0) {
      return;
    }

    if (this.pendingRootMove) {
      this.delegate.onElementBecameRoot(this.pendingRootMove.elementId);
    }

    for (const list of this.pendingMovesByParent.values()) {
      list.sort((a, b) => a.parentIndex - b.parentIndex);
    }

    // Start BFS from the new root (if any) and from any stable parents (parents that are not
    // being moved this pass but have children moving into them). Without including stable
    // parents, a pass that both sets a new root and reparents nodes into non-root stable
    // parents would drop those moves because the BFS only follows move chains from the root.
    let queue: number[];
    if (this.pendingRootMove) {
      queue = [this.pendingRootMove.elementId];
    } else {
      queue = [];
    }
    for (const parentId of this.pendingMovesByParent.keys()) {
      if (!this.pendingMoveElementIds.has(parentId)) {
        queue.push(parentId);
      }
    }
    queue.sort((a, b) => a - b);

    let head = 0;
    while (head < queue.length) {
      const parentId = queue[head++];
      const children = this.pendingMovesByParent.get(parentId);
      if (!children) continue;
      for (const m of children) {
        this.delegate.onElementMoved(m.elementId, parentId, m.parentIndex);
        queue.push(m.elementId);
      }
    }

    this.pendingRootMove = undefined;
    this.pendingMovesByParent.clear();
    this.pendingMoveElementIds.clear();
  }

  private doElementIterate(
    parentElement: RenderedElement,
    vNodeChildren: NodeChildren<VirtualNode>,
    call: (child: RenderedElement) => void,
    tail?: RenderedElement,
  ): RenderedElement | undefined {
    for (const childVNode of vNodeChildren.children) {
      const childElement = childVNode.element;
      // If the element is a leaf
      if (childElement) {
        // Custom logic
        call(childElement);
        // Add the element at the tail of the new linked list
        const head = parentElement.childrenHead;
        if (!head) {
          parentElement.childrenHead = childElement;
        }
        if (tail) {
          tail.next = childElement;
        }
        childElement.next = undefined;
        childElement.prev = tail;
        tail = childElement;
      }
      // If the element is not a leaf, simply recurse
      else {
        if (childVNode.children) {
          tail = this.doElementIterate(parentElement, childVNode.children, call, tail);
        }
      }
    }
    return tail;
  }

  // Resolve pass for when an element has children for the first time.
  // This is a simplified pass where we just find all the direct elements and append
  // them to the output.
  private resolveElementChildrenInitial(parentElement: RenderedElement, vNodeChildren: NodeChildren<VirtualNode>) {
    let index = 0;
    this.doElementIterate(parentElement, vNodeChildren, childElement => {
      // Keep the rendered index
      childElement.parentIndex = index;
      // Since this is an initial pass, we simply append elements
      this.onElementMoved(parentElement, childElement, index);
      index++;
    });
  }

  private resolveElementChildrenIncremental(parentElement: RenderedElement, vNodeChildren: NodeChildren<VirtualNode>) {
    let old = parentElement.childrenHead;
    let index = 0;
    parentElement.childrenHead = undefined;
    this.doElementIterate(parentElement, vNodeChildren, childElement => {
      // Keep the rendered index
      childElement.parentIndex = index;
      // Discard deleted elements
      while (old && this.elementById[old.id] === undefined) {
        const oldNext = old.next;
        const oldPrev = old.prev;
        if (oldPrev) {
          oldPrev.next = oldNext;
        }
        if (oldNext) {
          oldNext.prev = oldPrev;
        }
        old = old.next;
      }
      // if the existing element didn't change, leave it alone
      if (childElement === old) {
        old = old.next;
      }
      // if the existing element doesnt match the old one
      else {
        this.onElementMoved(parentElement, childElement, index);
      }
      // Remove the element from its previous linked list
      const next = childElement.next;
      const prev = childElement.prev;
      if (next) {
        next.prev = prev;
      }
      if (prev) {
        prev.next = next;
      }
      // Move on
      index++;
    });
  }

  private resolveElementChildren(node: VirtualNode, element: RenderedElement) {
    if (!element.childrenDirty) {
      return;
    }
    // this.log('Resolving elements children update');
    element.childrenDirty = false;

    const vNodeChildren = node.children;
    if (!vNodeChildren) {
      // We are a leaf, we can stop here
      return;
    }

    // This is the first time we process the children, we can do a simplified pass
    if (!element.childrenHead) {
      this.resolveElementChildrenInitial(element, vNodeChildren);
    }
    // We already had children, we do the full algorithm
    else {
      this.resolveElementChildrenIncremental(element, vNodeChildren);
    }

    element.bridge?.invalidateChildren();
  }

  private processChildrenUpdate(node: VirtualNode) {
    const children = node.children;
    if (!children) {
      // We are a leaf node
      return;
    }

    if (children.children === children.previousChildren) {
      const expectedKeysLength = children.insertionIndex;
      if (expectedKeysLength === children.children.length) {
        // The keys did not change
        // this.log('Children are the same ', expectedKeysLength);
        return;
      } else {
        // Keys have been removed at the end
        children.children = children.children.slice(0, expectedKeysLength);
        // this.log('Children have been removed at the end, we now have ', expectedKeysLength, ' children');
      }
    }

    // this.log('Children have changed, processing updates');

    const childrenByKey = children.childByKey;
    const previousChildren = children.previousChildren;

    if (previousChildren) {
      // We previously had children

      // Figure out the items which have been removed

      const currentRenderId = this.renderId;

      for (const child of previousChildren) {
        if (child.lastRenderId !== currentRenderId) {
          // This item was not rendered in this pass.
          // Delete it
          delete childrenByKey[child.key];

          // this.log('Child ', getNodeDescription(child), ' has been removed');
          this.destroyVirtualNode(child, false);
        }
      }
    }
    children.previousChildren = children.children;

    if (node.element) {
      node.element.childrenDirty = true;
    } else {
      const parentElement = node.parentElement;
      if (parentElement) {
        parentElement.childrenDirty = true;
      }
    }
  }

  private insertNodeInParent(node: VirtualNode, children: NodeChildren<VirtualNode>): void {
    const parentIndex = children.insertionIndex++;
    node.parentIndex = parentIndex;

    const childrenArray = children.children;
    if (childrenArray !== children.previousChildren) {
      // Children array is already dirty
      childrenArray.push(node);
    } else {
      // Keys are not yet dirty, checking if they are
      if (childrenArray[parentIndex] !== node) {
        // A key change, we make a new set of keys
        const newChildren = childrenArray.slice(0, parentIndex);
        newChildren.push(node);
        children.children = newChildren;
        // this.log('Dirtied children at ', parentIndex, ' from ', getNodeDescription(node));
      }
    }
  }

  private resolveVirtualNode(
    parent: VirtualNode,
    resolvedKey: string,
    componentConstructor: ComponentConstructor<IComponent> | undefined,
    componentPrototype: ComponentPrototype | undefined,
  ): VirtualNode {
    let resolvedNode: VirtualNode | undefined;
    let children = parent.children;
    if (!children) {
      children = {
        childByKey: {},
        children: [],
        insertionIndex: 0,
      };
      parent.children = children;
    } else {
      resolvedNode = children.childByKey[resolvedKey];
    }

    const currentRenderId = this.renderId;

    if (!resolvedNode) {
      const parentElement = parent.element || parent.parentElement;
      const parentComponent = parent.component || parent.parentComponent;

      resolvedNode = {
        key: resolvedKey,
        lastRenderId: currentRenderId,
        parentElement,
        parentComponent,
        children: undefined,
        parent,
        parentIndex: 0,
      };
      children.childByKey[resolvedKey] = resolvedNode;
    } else {
      const component = resolvedNode.component;
      // Check if we are resolving a component and there is a key conflict
      if (componentPrototype && component && componentPrototype !== component.prototype) {
        const constructorName1 = componentConstructor?.name;
        const constructorName2 = component.ctr.name;
        throw Error(
          [
            `Duplicate Component '${constructorName1}' and '${constructorName2}' for resolved key '${resolvedKey}'.`,
            `This can happen when two different components are added in the same parent with the same key,`,
            `You can fix this issue by combining into a single <Component/> tag with conditional attributes instead.`,
          ].join(' '),
        );
      }
      if (resolvedNode.lastRenderId === currentRenderId || !!componentPrototype !== !!component) {
        // This node was rendered twice in this render
        // We need to render it with a new key

        const duplicateKeyIndex = children.insertionIndex - resolvedNode.parentIndex + 1;
        return this.resolveVirtualNode(
          parent,
          resolvedKey + duplicateKeyIndex,
          componentConstructor,
          componentPrototype,
        );
      }
    }

    this.insertNodeInParent(resolvedNode, children);

    return resolvedNode;
  }

  private callComponentDisposables(component: RenderedComponent<IComponent>, disposables: ComponentDisposable[]) {
    for (const disposable of disposables) {
      try {
        if (typeof disposable === 'function') {
          disposable();
        } else {
          disposable.unsubscribe();
        }
      } catch (err: any) {
        this.onUncaughtError(`Failed to call registered disposable on component ${component.ctr.name}`, err);
      }
    }
  }

  private destroyVirtualNode(node: VirtualNode, parentElementWasDestroyed: boolean) {
    node.parent = undefined;

    if (node.element) {
      const element = node.element;
      node.element = undefined;

      delete this.elementById[element.id];

      const bridge = element.bridge;
      if (bridge) {
        this.detachElementFromHolder(element, element.attributes['ref']);
        this.detachElementFromHolder(element, element.attributes['$ref']);
        element.bridge = undefined;
      }

      // Only notify the delegate for the first element being destroyed
      // in the subtree.
      if (!parentElementWasDestroyed) {
        parentElementWasDestroyed = true;
        this.delegate.onElementDestroyed(element.id);
      }

      element.attributes = EMPTY_OBJECT;
    }

    const children = node.children;
    if (children) {
      for (const child of children.children) {
        this.destroyVirtualNode(child, parentElementWasDestroyed);
      }
      node.children = undefined;
      children.childByKey = EMPTY_OBJECT;
      children.children = EMPTY_ARRAY;
      children.previousChildren = undefined;
    }

    if (node.component) {
      const component = node.component;
      node.component = undefined;

      const componentInstance = component.instance;
      if (componentInstance) {
        if (component.componentRef) {
          if (node.bridge) {
            component.componentRef.onComponentWillDestroy(componentInstance, node.bridge);
          } else {
            // this shouldn't ever happen really
            this.onUncaughtError(
              `Failed to update component ref of ${component.ctr.name} before destroying`,
              new Error(`Component's virtual node bridge doesn't exist`),
            );
          }
          component.componentRef = undefined;
        }

        // this.log('Destroy component ', component.ctr.name);
        (componentInstance as any)[renderedComponentKey] = destroyedComponentTombstone;

        try {
          // this.log('Destroying component ', component.ctr.name);
          componentInstance.onDestroy();
        } catch (err: any) {
          this.onUncaughtError(`Failed to call onDestroy() on component ${component.ctr.name}`, err);
        }

        if (component.disposables) {
          this.callComponentDisposables(component, component.disposables);
          component.disposables = undefined;
        }

        const destroyCallback = component.onDestroyObserver;
        if (destroyCallback) {
          try {
            destroyCallback(componentInstance);
          } catch (err: any) {
            this.onUncaughtError(`Failed to call onDestroyCallback on component ${component.ctr.name}`, err);
          }
        }

        component.instance = undefined;
        component.viewModel = undefined;
        component.context = undefined;
      }

      node.bridge = undefined;
    }

    node.parentComponent = undefined;
    node.parentElement = undefined;
  }

  private getCurrentNode(): VirtualNode {
    const currentNode = this.currentNode;
    if (currentNode === undefined) {
      throw Error('Unbalanced beginElement()/endElement() or beginComponent()/endComponent() calls');
    }
    return currentNode;
  }

  private getCurrentElement(): RenderedElement {
    const currentNode = this.currentNode;
    if (!currentNode || !currentNode.element) {
      throw Error('Unbalanced beginElement()/endElement() calls');
    }

    return currentNode.element;
  }

  private getCurrentComponent(): RenderedComponent {
    const currentNode = this.currentNode;
    if (!currentNode || !currentNode.component) {
      throw Error('Unbalanced beginComponent()/endComponent() calls');
    }

    return currentNode.component;
  }

  private resolveComponentCatchingError(node: VirtualNode | undefined): IComponent | undefined {
    while (node) {
      if (node.component?.instance?.onError) {
        return node.component.instance;
      }

      node = node.parent;
    }

    return undefined;
  }

  onUncaughtError(message: string, error: Error, sourceComponent?: IComponent) {
    let lastRenderedNode: IRenderedVirtualNode | undefined;
    let componentCatchingError: IComponent | undefined;

    const currentNode = this.currentNode;
    if (currentNode) {
      lastRenderedNode = captureVirtualNode(this.nodeTree, currentNode, this);
      componentCatchingError = this.resolveComponentCatchingError(currentNode);
    } else if (sourceComponent) {
      try {
        const node = this.resolveRenderedComponent(sourceComponent).virtualNode;
        componentCatchingError = this.resolveComponentCatchingError(node);
      } catch (err: any) {
        console.log(`onUncaughtError: ${err.message}`);
      }
    } else {
      componentCatchingError = this.getRootComponent();
    }

    if (componentCatchingError?.onError) {
      componentCatchingError.onError(new RendererError(message, error, lastRenderedNode));
    } else {
      this.delegate.onUncaughtError(message, error);
    }
  }

  hasInjectedAttribute(name: string): boolean {
    const component = this.getCurrentComponent();
    const attributes = component.injectedAttributes;
    if (!attributes) {
      return false;
    }
    return !!attributes[name];
  }

  setInjectedAttribute(name: string, value: any) {
    const component = this.getCurrentComponent();
    let attributes = component.injectedAttributes;
    if (!attributes) {
      attributes = {};
      component.injectedAttributes = attributes;
    }

    attributes[name] = value;
  }

  /**
   * Set the attribute, returns whether the attribute has changed
   */
  setAttribute(name: string, value: any): boolean {
    return this.setAttributeOnElement(this.getCurrentElement(), name, value);
  }

  setAttributeBool(name: string, value: boolean | undefined): boolean {
    const element = this.getCurrentElement();
    const attributes = element.attributes;
    if (attributes[name] === value) {
      return false;
    }
    attributes[name] = value;

    if (value) {
      this.delegate.onElementAttributeChangeTrue(element.id, name);
    } else if (value === false) {
      this.delegate.onElementAttributeChangeFalse(element.id, name);
    } else {
      this.delegate.onElementAttributeChangeUndefined(element.id, name);
    }

    return true;
  }

  setAttributeNumber(name: string, value: number | undefined): boolean {
    const element = this.getCurrentElement();
    const attributes = element.attributes;
    if (attributes[name] === value) {
      return false;
    }
    attributes[name] = value;

    if (value || value === 0) {
      this.delegate.onElementAttributeChangeNumber(element.id, name, value);
    } else {
      this.delegate.onElementAttributeChangeUndefined(element.id, name);
    }

    return true;
  }

  setAttributeString(name: string, value: string | undefined): boolean {
    const element = this.getCurrentElement();
    const attributes = element.attributes;
    if (attributes[name] === value) {
      return false;
    }
    attributes[name] = value;

    if (value || value === '') {
      this.delegate.onElementAttributeChangeString(element.id, name, value);
    } else {
      this.delegate.onElementAttributeChangeUndefined(element.id, name);
    }

    return true;
  }

  setAttributeFunction(name: string, value: () => void | undefined): boolean {
    const element = this.getCurrentElement();
    const attributes = element.attributes;
    const oldValue = attributes[name];
    const newValue = tryReuseCallback(element, oldValue, value);

    if (oldValue === newValue) {
      return false;
    }
    attributes[name] = newValue;

    if (name === 'onVisibilityChanged' || name === 'onViewportChanged') {
      if (newValue) {
        this.setAttributeBool('observeVisibility', true);
      }
    } else if (name === 'onLayout') {
      if (element.frame && newValue) {
        this.onRenderComplete(() => {
          newValue(element.frame);
        });
      }
    } else {
      if (newValue) {
        this.delegate.onElementAttributeChangeFunction(element.id, name, newValue);
      } else {
        this.delegate.onElementAttributeChangeUndefined(element.id, name);
      }
    }

    return true;
  }

  setAttributeStyle(name: string, value: Style<any> | undefined): boolean {
    const element = this.getCurrentElement();
    const attributes = element.attributes;
    if (attributes[name] === value) {
      return false;
    }
    attributes[name] = value;

    if (value) {
      this.delegate.onElementAttributeChangeStyle(element.id, name, value);
    } else {
      this.delegate.onElementAttributeChangeUndefined(element.id, name);
    }
    return true;
  }

  setAttributeRef(value: IRenderedElementHolder<any> | undefined): boolean {
    const element = this.getCurrentElement();
    const attributes = element.attributes;
    const oldValue = attributes.ref;
    if (oldValue === value) {
      return false;
    }
    attributes.ref = value;

    this.detachElementFromHolder(element, oldValue);

    if (value) {
      value.onElementCreated(getRenderedElementBridge(this, element));
    }

    return true;
  }

  /**
   * Set the attributes en masse
   */
  setAttributes(attributes: PropertyList) {
    const element = this.getCurrentElement();
    enumeratePropertyList(attributes, (attributeKey, attributeValue) => {
      this.setAttributeOnElement(element, attributeKey, attributeValue);
    });
  }

  hasAttribute(name: string): boolean {
    return !!this.getCurrentElement().attributes[name];
  }

  setAttributeOnElement(element: RenderedElement, name: string, value: any): boolean {
    if (typeof value !== 'string' && (name === 'class' || name === '$class')) {
      value = classNames(value);
    }

    const currentValue = element.attributes[name];
    if (value instanceof Function) {
      value = tryReuseCallback(element, currentValue, value);
    }

    if (currentValue !== value) {
      if (Array.isArray(value) && Array.isArray(currentValue) && arrayEquals(value, currentValue)) {
        // Bypass attribute notification if the arrays are the same.
        element.attributes[name] = value;
        return false;
      }

      this.onAttributeChange(element, name, value, currentValue);
      return true;
    }
    return false;
  }

  private detachElementFromHolder(renderedNode: RenderedElement, holder?: IRenderedElementHolder<any>) {
    if (holder?.onElementDestroyed) {
      holder.onElementDestroyed(getRenderedElementBridge(this, renderedNode));
    }
  }

  private onAttributeChange(renderedNode: RenderedElement, name: string, value: any, oldValue: any) {
    renderedNode.attributes[name] = value;

    if (name === 'ref' || name === '$ref') {
      this.detachElementFromHolder(renderedNode, oldValue);

      if (value && (value as IRenderedElementHolder<any>).onElementCreated) {
        value.onElementCreated(getRenderedElementBridge(this, renderedNode));
      }
    } else if (name === 'onVisibilityChanged' || name == 'onViewportChanged') {
      if (value) {
        this.setAttributeOnElement(renderedNode, 'observeVisibility', true);
      }
    } else if (name === 'onLayout' || name === '$onLayout') {
      if (renderedNode.frame && value) {
        this.onRenderComplete(() => {
          value(renderedNode.frame);
        });
      }
    } else {
      this.delegate.onElementAttributeChangeAny(renderedNode.id, name, value);
    }
  }

  // Component rendering

  beginComponent<T extends IComponent>(
    ctr: ComponentConstructor<T>,
    prototype: ComponentPrototype,
    key?: string,
    componentRef?: IRenderedComponentHolder<T, IRenderedVirtualNode>,
  ) {
    const resolvedKey = key || prototype.id;

    const parent = this.getCurrentNode();
    const resolvedNode = this.resolveVirtualNode(parent, resolvedKey, ctr, prototype);

    let justCreated = false;

    if (!resolvedNode.component) {
      resolvedNode.component = {
        instance: undefined,
        ctr: ctr,
        prototype: prototype,
        needRendering: true,
        viewModel: undefined,
        viewModelChanged: false,
        virtualNode: resolvedNode,
        injectedAttributes: undefined,
        rendering: false,
        componentRef: componentRef,
      };
      justCreated = true;
    }

    this.pushVirtualNode(resolvedNode);

    if (this.eventListener) {
      this.eventListener.onComponentBegin(resolvedNode.key, ctr);
    }

    if (justCreated && prototype.attributes) {
      this.setViewModelProperties(prototype.attributes);
    }
  }

  /**
   * @deprecated needed for VUE support only
   */
  beginComponentWithoutPrototype<T extends IComponent>(
    ctr: ComponentConstructor<T>,
    key?: string,
    componentRef?: IRenderedComponentHolder<T, IRenderedVirtualNode>,
  ) {
    let prototype: ComponentPrototype = (ctr as any).componentPrototype;
    if (!prototype) {
      prototype = ComponentPrototype.instanceWithNewId();
      (ctr as any).componentPrototype = prototype;
    }
    this.beginComponent(ctr, prototype, key, componentRef);
  }

  hasComponentContext(): boolean {
    return !!this.getCurrentComponent().context;
  }

  setComponentContext(context: any) {
    this.getCurrentComponent().context = context;
  }

  setComponentOnCreateObserver(observer?: (instance: any) => void) {
    this.getCurrentComponent().onCreateObserver = observer;
  }

  setComponentOnDestroyObserver(observer?: (instance: any) => void) {
    this.getCurrentComponent().onDestroyObserver = observer;
  }

  onRuntimeIssue(isError: boolean, message: string) {
    for (const observer of this.observers) {
      if (observer.onRuntimeIssue) {
        observer.onRuntimeIssue(isError, message);
      }
    }
  }

  addObserver(observer: RendererObserver) {
    this.observers.push(observer);
  }

  removeObserver(observer: RendererObserver) {
    const idx = this.observers.indexOf(observer);
    if (idx >= 0) {
      this.observers.splice(idx, 1);
    }
  }

  private createComponent(currentComponent: RenderedComponent): IComponent {
    const parentComponent = currentComponent.virtualNode.parentComponent;
    const contextExplicitlySet = currentComponent.context;

    const componentCtr = currentComponent.ctr;

    let viewModel = currentComponent.viewModel;
    if (!viewModel && componentCtr.disallowNullViewModel) {
      viewModel = {};
      currentComponent.viewModel = viewModel;
    }

    let componentInstance: IComponent;
    if (contextExplicitlySet || !parentComponent) {
      componentInstance = new componentCtr(this, viewModel, contextExplicitlySet);
    } else {
      componentInstance = new componentCtr(this, viewModel, parentComponent.context);
    }
    (componentInstance as any)[renderedComponentKey] = currentComponent;

    currentComponent.instance = componentInstance;
    currentComponent.context = componentInstance.context;

    // this.log('Creating component ', currentComponent.ctr.name);

    try {
      componentInstance.onCreate();
    } catch (err: any) {
      this.onUncaughtError(`Failed to call onCreate() on ${componentCtr.name}`, err);
    }

    const createCallback = currentComponent.onCreateObserver;
    if (createCallback) {
      createCallback(componentInstance);
    }

    if (currentComponent.viewModel) {
      try {
        componentInstance.onViewModelUpdate(undefined);
      } catch (err: any) {
        this.onUncaughtError(`Failed to call onViewModelUpdate() after onCreate() on ${componentCtr.name}`, err);
      }
      currentComponent.viewModelChanged = false;
    }

    currentComponent.componentRef?.onComponentDidCreate(
      componentInstance,
      getVirtualNodeBridge(this, currentComponent.virtualNode),
    );

    return componentInstance;
  }

  private skipVirtualNodeChildren(node: VirtualNode) {
    const children = node.children;
    if (!children) {
      return;
    }

    // Sanity check, this should not happen
    if (children.previousChildren !== children.children) {
      throw Error(`Cannot skip rendering of children in node ${node.key} if the children were already dirty.`);
    }

    children.insertionIndex = children.children.length;
  }

  private doRerenderSlotIfNeeded<F extends AnyRenderFunction>(slotData: ComponentSlotData<F>) {
    const node = slotData.node;
    if (!node) {
      return;
    }
    if (!node.parent) {
      // Node was destroyed
      slotData.node = undefined;
      return;
    }

    this.pushVirtualNode(node);

    slotData.renderFunc.apply(undefined, slotData.lastParameters!);

    this.endSlot(slotData);
  }

  private rerenderSlots(children: any) {
    if (children[IS_NAMED_SLOT_KEY]) {
      const namedSlots = children as NamedSlotFunctions;

      for (const name in namedSlots) {
        const slotFunction = namedSlots[name];
        if (slotFunction) {
          this.doRerenderSlotIfNeeded(slotFunction[SLOT_DATA_KEY]);
        }
      }
    } else {
      const slotData = (children as SlotFunction<AnyRenderFunction>)[SLOT_DATA_KEY];

      if (slotData) {
        this.doRerenderSlotIfNeeded(slotData);
      }
    }
  }

  endComponent() {
    const currentComponent = this.getCurrentComponent();

    let componentInstance = currentComponent.instance;

    let needRendering = currentComponent.needRendering;
    const viewModel = currentComponent.viewModel;

    if (!componentInstance) {
      needRendering = true;
      componentInstance = this.createComponent(currentComponent);
    } else if (currentComponent.viewModelChanged) {
      currentComponent.viewModelChanged = false;
      const oldViewModel = componentInstance.viewModel;
      componentInstance.viewModel = viewModel;

      try {
        componentInstance.onViewModelUpdate(oldViewModel);
      } catch (err: any) {
        this.onUncaughtError(`Failed to call onViewModelUpdate() on ${componentInstance.constructor.name}`, err);
      }
      // this.log('view model changed');
    }

    if (!needRendering) {
      // this.log('Dont need to rerender ', currentComponent.ctr!!.name);

      if (this.eventListener) {
        this.eventListener.onBypassComponentRender();
      }

      this.skipVirtualNodeChildren(currentComponent.virtualNode);

      const children = viewModel && viewModel.children;
      if (children) {
        // Rerender the slots
        this.rerenderSlots(children);
      }
    } else {
      // this.log('Need rendering component');
      currentComponent.needRendering = false;
      currentComponent.rendering = true;

      // this.log('Rendering component ', currentComponent.ctr.name);

      try {
        componentInstance.onRender();
      } catch (err: any) {
        this.popToVirtualNode(currentComponent.virtualNode);
        this.onUncaughtError(`Failed to render component '${currentComponent.ctr.name}'`, err);
      }

      currentComponent.rendering = false;
    }

    this.popVirtualNode();

    if (this.eventListener) {
      this.eventListener.onComponentEnd();
    }

    this.injectAttributesIfNeeded(currentComponent);
  }

  private injectAttributesIfNeeded(component: RenderedComponent) {
    const injectedAttributes = component.injectedAttributes;
    if (!injectedAttributes) {
      return;
    }

    const children = component.virtualNode.children;
    if (!children) {
      return;
    }

    for (const attributeName in injectedAttributes) {
      const attributeValue = injectedAttributes[attributeName];
      for (const child of children.children) {
        if (child.element) {
          this.setAttributeOnElement(child.element, attributeName, attributeValue);
        }
      }
    }
  }

  private popToVirtualNode(toNode: VirtualNode) {
    while (this.currentNode && this.currentNode !== toNode) {
      this.popVirtualNode();
    }
  }

  hasViewModelProperty(name: string): boolean {
    const currentComponent = this.getCurrentComponent();
    const viewModel = currentComponent.viewModel;
    if (!viewModel) {
      return false;
    }

    return !!viewModel[name];
  }

  setViewModelProperty(name: string, value: any) {
    const currentComponent = this.getCurrentComponent();
    const viewModel = currentComponent.viewModel;
    if (!viewModel) {
      currentComponent.needRendering = true;
      currentComponent.viewModelChanged = true;
      currentComponent.viewModel = {};

      if (value instanceof Function) {
        value = tryReuseCallback(currentComponent, undefined, value);
      }

      currentComponent.viewModel[name] = value;
    } else {
      const oldValue = viewModel[name];
      if (value instanceof Function) {
        value = tryReuseCallback(currentComponent, oldValue, value);
      }

      if (oldValue !== value) {
        if (currentComponent.viewModelChanged) {
          // We have a new view model, we can store directly in it
          viewModel[name] = value;
        } else {
          currentComponent.viewModel = { ...viewModel, [name]: value };
          currentComponent.needRendering = true;
          currentComponent.viewModelChanged = true;

          if (this.eventListener) {
            this.eventListener.onComponentViewModelPropertyChange(name);
          }
          // this.log('View model property ', name, ' changed for component ', currentComponent.ctr.name);
        }
      }
    }
  }

  beginSlot<F extends AnyRenderFunction>(slotData: ComponentSlotData<F>) {
    const parentNode = this.getCurrentNode();
    const node = this.resolveVirtualNode(parentNode, slotData.nodeKey, undefined, undefined);
    slotData.node = node;
    node.slot = true;
    this.pushVirtualNode(node);
  }

  endSlot<F extends AnyRenderFunction>(slotData: ComponentSlotData<F>) {
    this.popVirtualNode();

    if (slotData.ref) {
      const elements: IRenderedElement[] = [];
      this.collectElements(slotData.node!, elements);
      slotData.ref.setElements(elements);
    }
  }

  private throwInvalidSlot(component: RenderedComponent, slotName: string, slotValue: any) {
    throw new Error(
      [
        `Cannot render slot '${slotName}' in component ${component.ctr.name}: `,
        `found an existing slot value that isn't a slot function or a named slot.`,
        `Found slot value inside the 'children' property is of type: ${typeof slotValue}`,
      ].join(' '),
    );
  }

  renderNamedSlot<T = any>(name: string, component: IComponent, ref?: IRenderedElementHolder<T>, slotKey?: string) {
    const renderedComponent = this.resolveRenderedComponent(component);

    const viewModel = renderedComponent.viewModel;
    if (!viewModel) {
      return;
    }

    const children = viewModel.children as NamedSlotFunctions | undefined;
    if (!children) {
      if (ref) {
        ref.setElements([]);
      }
      return;
    }

    if (!children[IS_NAMED_SLOT_KEY]) {
      if ((children as unknown as SlotFunction<AnyRenderFunction>)[SLOT_DATA_KEY]) {
        if (name === 'default') {
          this.renderUnnamedSlot(component, ref, slotKey);
        }
        return;
      }

      this.throwInvalidSlot(renderedComponent, name, children);
    }

    const slotFunction = children[name];

    if (slotFunction) {
      const slotData = slotFunction[SLOT_DATA_KEY];

      if (slotKey) {
        slotData.nodeKey = slotKey;
        slotData.isKeyExplicit = true;
      } else if (slotData.isKeyExplicit) {
        slotData.isKeyExplicit = false;
        slotData.nodeKey = `${renderedComponent.virtualNode.key}$${name}`;
      }

      if (ref) {
        slotData.ref = ref;
      } else if (slotData.ref) {
        slotData.ref = undefined;
      }

      slotFunction();
    } else if (ref) {
      ref.setElements([]);
    }
  }

  renderUnnamedSlot<T = any>(component: IComponent, ref?: IRenderedElementHolder<T>, slotKey?: string) {
    const renderedComponent = this.resolveRenderedComponent(component);

    const viewModel = renderedComponent.viewModel;
    if (!viewModel) {
      return;
    }

    const slotFunction = viewModel.children as SlotFunction<AnyRenderFunction> | undefined;
    if (!slotFunction) {
      if (ref) {
        ref.setElements([]);
      }
      return;
    }

    const slotData = slotFunction[SLOT_DATA_KEY];

    if (!slotData) {
      if ((slotFunction as unknown as NamedSlotFunctions)[IS_NAMED_SLOT_KEY]) {
        // Our slot is a named slot, fallback to rendering the 'default' slot value
        this.renderNamedSlot('default', component, ref, slotKey);
        return;
      }

      this.throwInvalidSlot(renderedComponent, 'unnamed', slotFunction);
    }

    if (slotKey) {
      slotData.nodeKey = slotKey;
      slotData.isKeyExplicit = true;
    } else if (slotData.isKeyExplicit) {
      slotData.isKeyExplicit = false;
      slotData.nodeKey = `${renderedComponent.virtualNode.key}$unnamed`;
    }

    if (ref) {
      slotData.ref = ref;
    } else if (slotData.ref) {
      slotData.ref = undefined;
    }

    slotFunction();
  }

  setNamedSlot<F extends AnyRenderFunction>(name: string, renderFunc: F | undefined) {
    const component = this.getCurrentComponent();

    let viewModel = component.viewModel;
    if (!viewModel) {
      viewModel = {};
      component.viewModel = viewModel;
      component.viewModelChanged = true;
      component.needRendering = true;
    }

    let namedSlots = viewModel.children as NamedSlotFunctions | undefined;

    if (renderFunc) {
      if (namedSlots && !namedSlots[IS_NAMED_SLOT_KEY]) {
        if ((namedSlots as unknown as SlotFunction<AnyRenderFunction>)[SLOT_DATA_KEY] && name !== 'default') {
          namedSlots = { [IS_NAMED_SLOT_KEY]: true, default: namedSlots as unknown as SlotFunction<AnyRenderFunction> };
          if (component.viewModelChanged) {
            viewModel.children = namedSlots;
          } else {
            component.viewModel = { ...viewModel, children: namedSlots };
            component.viewModelChanged = true;
            component.needRendering = true;
          }
        } else {
          namedSlots = undefined;
        }
      }

      if (!namedSlots) {
        namedSlots = { [IS_NAMED_SLOT_KEY]: true };
        if (component.viewModelChanged) {
          viewModel.children = namedSlots;
        } else {
          component.viewModel = { ...viewModel, children: namedSlots };
          component.viewModelChanged = true;
          component.needRendering = true;
        }
      }

      let slot = namedSlots[name];
      if (!slot) {
        // Slot does not exist, create it
        const slotData: ComponentSlotData<F> = {
          nodeKey: `${component.virtualNode.key}$${name}`,
          node: undefined,
          renderFunc,
          lastParameters: undefined,
        };
        slot = makeSlotFunction(this, slotData);

        if (component.viewModelChanged) {
          namedSlots[name] = slot;
        } else {
          namedSlots = { ...namedSlots, [name]: slot };
          component.viewModel = { ...viewModel, children: namedSlots };
          component.viewModelChanged = true;
          component.needRendering = true;
        }
      } else {
        // An existing slot exists, update it
        slot[SLOT_DATA_KEY].renderFunc = renderFunc;
      }
    } else {
      // clearing the named slot
      if (namedSlots) {
        const slot = namedSlots[name];

        if (slot) {
          // We have an existing slot, we can remove it
          if (component.viewModelChanged) {
            viewModel.children = { ...namedSlots, [name]: undefined };
          } else {
            component.viewModel = { ...viewModel, children: { ...namedSlots, [name]: undefined } };
            component.viewModelChanged = true;
            component.needRendering = true;
          }
        }
      }
    }
  }

  setUnnamedSlot<F extends AnyRenderFunction>(renderFunc: F | undefined) {
    const component = this.getCurrentComponent();

    let viewModel = component.viewModel;
    if (!viewModel) {
      viewModel = {};
      component.viewModel = viewModel;
      component.viewModelChanged = true;
      component.needRendering = true;
    }

    let slot = viewModel.children as SlotFunction<F> | undefined;

    if (renderFunc) {
      if (slot) {
        // An existing slot exists, attempt to update it if it's compatible
        const slotData = slot[SLOT_DATA_KEY];
        if (slotData) {
          slotData.renderFunc = renderFunc;
          return;
        } else {
          if ((slot as unknown as NamedSlotFunctions)[IS_NAMED_SLOT_KEY]) {
            // We have a named slot, fallback to setting the slot as 'default'
            this.setNamedSlot('default', renderFunc);
            return;
          }

          // Not a slot, will fallthrough to create it
        }
      }

      // Slot does not exist, create it
      const slotData: ComponentSlotData<F> = {
        nodeKey: `${component.virtualNode.key}$unnamed`,
        node: undefined,
        renderFunc,
        lastParameters: undefined,
      };
      slot = makeSlotFunction(this, slotData);

      if (component.viewModelChanged) {
        viewModel.children = slot;
      } else {
        component.viewModel = { ...viewModel, children: slot };
        component.viewModelChanged = true;
        component.needRendering = true;
      }
    } else {
      // clearing the unnamed slot
      if (slot) {
        // We have an existing slot, we can remove it
        if (component.viewModelChanged) {
          viewModel.children = undefined;
        } else {
          component.viewModel = { ...viewModel, children: undefined };
          component.viewModelChanged = true;
          component.needRendering = true;
        }
      }
    }
  }

  setNamedSlots(namedSlots: StringMap<AnyRenderFunction | undefined>) {
    for (const name in namedSlots) {
      this.setNamedSlot(name, namedSlots[name]);
    }
  }

  setViewModelProperties(viewModel?: PropertyList) {
    if (!viewModel) {
      return;
    }

    enumeratePropertyList(viewModel, (key, value) => {
      if (key !== 'children') {
        this.setViewModelProperty(key, value);
      }
    });
  }

  // Will replace the component's existing view model with the
  // new value if it is not identical.
  setViewModelFull(viewModel?: PropertyList) {
    const currentComponent = this.getCurrentComponent();
    const existingViewModel = currentComponent.viewModel;

    if (existingViewModel === viewModel) {
      return;
    }

    currentComponent.needRendering = true;
    currentComponent.viewModelChanged = true;

    if (viewModel) {
      currentComponent.viewModel = propertyListToObject(viewModel);
    } else {
      if (currentComponent.ctr.disallowNullViewModel) {
        currentComponent.viewModel = {};
      } else {
        currentComponent.viewModel = undefined;
      }
    }
  }

  render(renderFunc: () => void): IRenderedElement[] {
    const currentNode = this.getCurrentNode();
    const beforeIndex = currentNode.children?.insertionIndex ?? 0;

    renderFunc();

    const children = currentNode.children;

    const out: IRenderedElement[] = [];

    if (children) {
      const afterIndex = children.insertionIndex;
      const childrenArray = children.children;

      for (let i = beforeIndex; i < afterIndex; i++) {
        this.collectElements(childrenArray[i], out);
      }
    }

    return out;
  }

  getRootComponent(): IComponent | undefined {
    return this.getRootComponents()[0];
  }

  getRootComponents(): IComponent[] {
    const nodes: IComponent[] = [];

    this.collectComponentsChildren(this.nodeTree, nodes);

    return nodes;
  }

  getRootElement(): IRenderedElement | undefined {
    const out: IRenderedElement[] = [];
    if (this.nodeTree.children) {
      for (const node of this.nodeTree.children.children) {
        this.collectElements(node, out);
      }
    }
    return out[0];
  }

  getComponentKey(component: IComponent): string {
    return this.resolveRenderedComponent(component).virtualNode.key || '';
  }

  getComponentChildren(component: IComponent): IComponent[] {
    const components: IComponent[] = [];

    const renderedComponent = this.resolveRenderedComponent(component);
    this.collectComponentsChildren(renderedComponent.virtualNode, components);

    return components;
  }

  getComponentParent(component: IComponent): IComponent | undefined {
    const renderedComponent = this.resolveRenderedComponent(component);

    return renderedComponent.virtualNode?.parentComponent?.instance;
  }

  public getCurrentComponentInstance(): IComponent | undefined {
    return this.getCurrentComponent().instance;
  }

  getComponentRootElements(component: IComponent, collectElementsOfChildComponents: boolean): IRenderedElement[] {
    const renderedComponent = this.resolveRenderedComponent(component);

    const out: IRenderedElement[] = [];
    if (collectElementsOfChildComponents) {
      this.collectElements(renderedComponent.virtualNode, out);
    } else {
      const children = renderedComponent.virtualNode.children;
      if (children) {
        for (const child of children.children) {
          if (child.element) {
            out.push(getRenderedElementBridge(this, child.element));
          }
        }
      }
    }

    return out;
  }

  getRootVirtualNode(): IRenderedVirtualNode | undefined {
    if (this.nodeTree.children) {
      for (const node of this.nodeTree.children.children) {
        return getVirtualNodeBridge(this, node);
      }
    }

    return undefined;
  }

  getComponentVirtualNode(component: IComponent): IRenderedVirtualNode {
    const renderedComponent = this.resolveRenderedComponent(component);
    const virtualNodeBridge = getVirtualNodeBridge(this, renderedComponent.virtualNode);
    return virtualNodeBridge;
  }

  getElementForId(elementId: number): IRenderedElement | undefined {
    const element = this.elementById[elementId];
    if (!element) {
      return undefined;
    }

    return getRenderedElementBridge(this, element);
  }

  onRenderComplete(callback: () => void): void {
    if (this.began) {
      if (!this.completions) {
        this.completions = [];
      }
      this.completions.push(callback);
    } else {
      callback();
    }
  }

  onLayoutComplete(callback: () => void): void {
    if (this.began) {
      this.delegate.onNextLayoutComplete(callback);
    } else {
      this.doBegin();
      this.delegate.onNextLayoutComplete(callback);
      this.doEnd();
    }
  }

  onNextDraw(callback: (hookTimeMs: number) => void): void {
    if (this.began) {
      this.delegate.onNextDraw(callback);
    } else {
      this.doBegin();
      this.delegate.onNextDraw(callback);
      this.doEnd();
    }
  }

  onNextRenderComplete(callback: () => void): void {
    if (!this.completions) {
      this.completions = [];
    }
    this.completions.push(callback);
  }

  registerComponentDisposable(component: IComponent, disposable: ComponentDisposable) {
    const renderedComponent = this.resolveRenderedComponent(component);

    let disposables = renderedComponent.disposables;
    if (!disposables) {
      disposables = [];
      renderedComponent.disposables = disposables;
    }

    disposables.push(disposable);
  }

  attributeUpdatedExternally(elementId: number, attributeName: string, attributeValue: any) {
    const element = this.getElementById(elementId);
    if (element) {
      element.attributes[attributeName] = attributeValue;
    }
  }

  setEventListener(eventListener: IRendererEventListener): void {
    this.eventListener = eventListener;
  }

  getEventListener(): IRendererEventListener | undefined {
    return this.eventListener;
  }

  private collectElements(node: VirtualNode, output: IRenderedElement[]) {
    if (node.element) {
      output.push(getRenderedElementBridge(this, node.element));
    } else {
      const children = node.children;
      if (children) {
        for (const childNode of children.children) {
          this.collectElements(childNode, output);
        }
      }
    }
  }

  private collectComponentsChildren(node: VirtualNode, output: IComponent[]) {
    const children = node.children;
    if (children) {
      for (const child of children.children) {
        if (child.component) {
          const instance = child.component.instance;
          if (instance) {
            output.push(instance);
          }
        } else {
          this.collectComponentsChildren(child, output);
        }
      }
    }
  }

  private getElementById(elementId: number): RenderedElement | undefined {
    return this.elementById[elementId];
  }

  private processFrameUpdates(updates: Float64Array) {
    const elementById = this.elementById;
    let elementsWithCallback: RenderedElement[] | undefined;

    trace('processFrameUpdates', () => {
      let i = 0;
      while (i < updates.length) {
        const elementId = updates[i++];
        const x = updates[i++];
        const y = updates[i++];
        const width = updates[i++];
        const height = updates[i++];

        const element = elementById[elementId];
        if (element) {
          element.frame = { x, y, width, height };

          if (element.attributes.onLayout || element.attributes.$onLayout) {
            if (!elementsWithCallback) {
              elementsWithCallback = [];
            }
            elementsWithCallback.push(element);
          }
        }
      }
    });

    if (elementsWithCallback) {
      for (const element of elementsWithCallback) {
        let onLayout = element.attributes.onLayout;
        if (onLayout) {
          try {
            onLayout(element.frame);
          } catch (err: any) {
            this.onUncaughtError('Failed to call onLayout', err);
          }
        }
        onLayout = element.attributes.$onLayout;
        if (onLayout) {
          try {
            onLayout(element.frame);
          } catch (err: any) {
            this.onUncaughtError('Failed to call onLayout', err);
          }
        }
      }
    }
  }

  /**
   * Verifies the integrity of the internal data structure of the Renderer.
   */
  verifyIntegrity() {
    if (this.nodeStack.length) {
      throw Error('Node stack is not empty');
    }
    if (this.currentNode) {
      throw Error('We have a current node');
    }
    this.doVerifyIntegrity(this.nodeTree, undefined, undefined);
  }

  private doVerifyIntegrity(
    node: VirtualNode,
    lastParentComponent: RenderedComponent | undefined,
    lastParentElement: RenderedElement | undefined,
  ) {
    if (node.component && node.element) {
      throw Error(`Node with key ${node.key} has both component and element!`);
    }

    if (node.parentComponent !== lastParentComponent) {
      throw Error(`Node ${node.key} has incorrect parentComponent`);
    }
    if (node.parentElement !== lastParentElement) {
      throw Error(`Node ${node.key} has incorrect parentElement`);
    }

    if (node.component) {
      const component = node.component;

      if (component.needRendering) {
        throw Error(`needRendering is true in Component ${node.key}`);
      }
      if (component.viewModelChanged) {
        throw Error(`viewModelChanged is true in Component ${node.key}`);
      }
      if (component.rerenderScheduled) {
        throw Error(`rerenderScheduled is true in Component ${node.key}`);
      }

      lastParentComponent = node.component;
    }

    if (node.element) {
      const element = node.element;
      lastParentElement = element;
      if (element.childrenDirty) {
        throw Error(`Children elements are dirty for ${getNodeDescription(node)}`);
      }
    }

    const children = node.children;

    if (children) {
      if (!children.previousChildren) {
        throw Error('Missing previousChildKeys');
      }

      if (children.previousChildren !== children.children) {
        throw Error('Out of sync previous child keys with childKeys');
      }

      const allKeys: StringSet = {};

      let index = 0;
      for (const child of children.children) {
        const key = child.key;

        if (child.parentIndex !== index) {
          throw Error(`Child ${key} should have index ${index} but has ${child.parentIndex}`);
        }

        if (child.parent !== node) {
          throw Error(`Child ${key} should have parent ${node.key} but has ${child.parent?.key}`);
        }

        if (allKeys[key]) {
          throw Error(`Duplicate child key ${key}`);
        }

        allKeys[key] = true;

        this.doVerifyIntegrity(child, lastParentComponent, lastParentElement);

        index++;
      }

      for (const childKey in children.childByKey) {
        const child = children.childByKey[childKey];
        if (child && !allKeys[childKey]) {
          throw Error(`Orphan child for key ${childKey}, present in childByKey but not in childKeys list`);
        }
      }
    }
  }

  // private log(...messages: any[]) {
  //   if (!this.enableLog) {
  //     return;
  //   }

  //   let logMessage = '';
  //   for (let i = 0; i < this.nodeStack.length; i++) {
  //     if (!this.logComponentsOnly || this.nodeStack[i].component) {
  //       logMessage += '  ';
  //     }
  //   }

  //   for (const message of messages) {
  //     if (typeof message === 'function') {
  //       logMessage += '<function>';
  //     } else if (message === undefined) {
  //       logMessage += '<undefined>';
  //     } else {
  //       logMessage += message.toString();
  //     }
  //   }

  //   console.log(logMessage);
  // }
}

export function getRenderer(): Renderer {
  const renderer = currentRenderer;
  if (!renderer) {
    throw Error('Cannot call this outside of a onRender callback');
  }

  return renderer;
}
