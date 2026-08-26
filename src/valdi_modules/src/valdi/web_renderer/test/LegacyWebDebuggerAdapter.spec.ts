import 'jasmine/src/jasmine';
import type { IComponent } from 'valdi_core/src/IComponent';
import type { IRenderedElement } from 'valdi_core/src/IRenderedElement';
import type { IRenderedVirtualNode } from 'valdi_core/src/IRenderedVirtualNode';
import type { IRenderer } from 'valdi_core/src/IRenderer';
import {
  MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS,
  ValdiWebRendererDelegate,
} from '../src/ValdiWebRendererDelegate';
import type { WebValdiLayout } from '../src/views/WebValdiLayout';

interface InstalledDom {
  createElement(tagName: string): HTMLElement;
  restore(): void;
}

function installDom(): InstalledDom {
  const previousGlobals = new Map<string, unknown>();
  const globalNames = [
    'Document',
    'Element',
    'HTMLElement',
    'IntersectionObserver',
    'ResizeObserver',
    'ShadowRoot',
    'customElements',
    'document',
    'window',
  ];
  for (const name of globalNames) {
    previousGlobals.set(name, (globalThis as Record<string, unknown>)[name]);
  }

  class FakeElement {
    parentElement: FakeHTMLElement | null = null;
  }

  class FakeHTMLElement extends FakeElement {
    readonly attributesByName = new Map<string, string>();
    readonly children: FakeHTMLElement[] = [];
    readonly childNodes = { item: (index: number) => this.children[index] ?? null };
    readonly classList = {
      add: (_className: string) => {},
      remove: (_className: string) => {},
    };
    readonly dataset: Record<string, string> = {};
    readonly style: Record<string, unknown> = {
      removeProperty: (_name: string) => {},
      setProperty: (_name: string, _value: string) => {},
    };
    readonly ownerDocument: FakeDocument;
    readonly tagName: string;
    scrollLeft = 0;
    scrollTop = 0;
    textContent: string | null = null;
    rect = { left: 0, top: 0, width: 0, height: 0 };

    constructor(tagName: string, ownerDocument: FakeDocument) {
      super();
      this.tagName = tagName.toUpperCase();
      this.ownerDocument = ownerDocument;
    }

    get attributes() {
      const entries = [...this.attributesByName.entries()];
      return {
        length: entries.length,
        item: (index: number) => {
          const entry = entries[index];
          return entry === undefined ? null : { name: entry[0], value: entry[1] };
        },
      };
    }

    get childElementCount(): number {
      return this.children.length;
    }

    addEventListener(): void {}
    removeEventListener(): void {}
    appendChild(child: FakeHTMLElement): FakeHTMLElement {
      child.parentElement = this;
      this.children.push(child);
      return child;
    }
    contains(): boolean {
      return false;
    }
    getAttribute(name: string): string | null {
      return this.attributesByName.get(name) ?? null;
    }
    getBoundingClientRect() {
      return { ...this.rect, x: this.rect.left, y: this.rect.top, right: 0, bottom: 0, toJSON: () => ({}) };
    }
    getRootNode(): FakeDocument {
      return this.ownerDocument;
    }
    insertBefore(child: FakeHTMLElement, reference: FakeHTMLElement | null): FakeHTMLElement {
      child.parentElement = this;
      const index = reference === null ? this.children.length : this.children.indexOf(reference);
      this.children.splice(index < 0 ? this.children.length : index, 0, child);
      return child;
    }
    remove(): void {
      const parent = this.parentElement;
      if (parent !== null) {
        const index = parent.children.indexOf(this);
        if (index >= 0) {
          parent.children.splice(index, 1);
        }
      }
      this.parentElement = null;
    }
    removeAttribute(name: string): void {
      this.attributesByName.delete(name);
    }
    replaceChildren(...children: FakeHTMLElement[]): void {
      this.children.splice(0, this.children.length, ...children);
      for (const child of children) {
        child.parentElement = this;
      }
    }
    setAttribute(name: string, value: string): void {
      this.attributesByName.set(name, value);
    }
  }

  class FakeDocument {
    readonly head: FakeHTMLElement;

    constructor() {
      this.head = new FakeHTMLElement('head', this);
    }

    createElement(tagName: string): FakeHTMLElement {
      return new FakeHTMLElement(tagName, this);
    }
  }

  class FakeShadowRoot extends FakeHTMLElement {
    querySelector(): null {
      return null;
    }
  }

  const document = new FakeDocument();
  (document.head as FakeHTMLElement & { querySelector?: () => null }).querySelector = () => null;
  (globalThis as Record<string, unknown>).Element = FakeElement;
  (globalThis as Record<string, unknown>).HTMLElement = FakeHTMLElement;
  (globalThis as Record<string, unknown>).Document = FakeDocument;
  (globalThis as Record<string, unknown>).ShadowRoot = FakeShadowRoot;
  (globalThis as Record<string, unknown>).document = document;
  (globalThis as Record<string, unknown>).window = { innerHeight: 768, innerWidth: 1024, setTimeout, clearTimeout };
  (globalThis as Record<string, unknown>).customElements = { define: () => {}, get: () => undefined };
  (globalThis as Record<string, unknown>).IntersectionObserver = function () {
    return { disconnect: () => {}, observe: () => {}, unobserve: () => {} };
  };
  (globalThis as Record<string, unknown>).ResizeObserver = function () {
    return { disconnect: () => {}, observe: () => {}, unobserve: () => {} };
  };

  return {
    createElement: tagName => document.createElement(tagName) as unknown as HTMLElement,
    restore: () => {
      for (const [name, value] of previousGlobals) {
        if (value === undefined) {
          delete (globalThis as Record<string, unknown>)[name];
        } else {
          (globalThis as Record<string, unknown>)[name] = value;
        }
      }
    },
  };
}

function makeRenderedElement(
  id: number,
  tag: string,
  attributes: Record<string, unknown>,
): IRenderedElement {
  return {
    id,
    tag,
    getAttribute: (name: string) => attributes[name],
    getAttributeNames: () => Object.keys(attributes),
  } as unknown as IRenderedElement;
}

function makeRenderer(elements: IRenderedElement[]): IRenderer {
  const elementsById = new Map(elements.map(element => [element.id, element]));
  return {
    getElementForId: id => elementsById.get(id),
  } as IRenderer;
}

interface MutableVirtualNode {
  children: MutableVirtualNode[];
  component?: IComponent;
  componentViewModel?: unknown;
  element?: IRenderedElement;
  key: string;
  parent?: MutableVirtualNode;
}

function makeVirtualNode(
  key: string,
  value: { component?: IComponent; element?: IRenderedElement },
): MutableVirtualNode {
  return { children: [], key, ...value };
}

function setVirtualChildren(parent: MutableVirtualNode, children: MutableVirtualNode[]): void {
  parent.children = children;
  for (const child of children) {
    child.parent = parent;
  }
}

function makeHierarchyRenderer(
  elements: IRenderedElement[],
  rootVirtualNode: MutableVirtualNode | undefined,
): IRenderer {
  const renderer = makeRenderer(elements);
  return Object.assign(renderer, {
    getDebugVirtualNodeSnapshot: (
      node: IRenderedVirtualNode,
      maximumChildLinks: number,
      maximumTraversalLinks: number,
    ) => {
      const mutableNode = node as unknown as MutableVirtualNode;
      const children = mutableNode.children;
      const traversedLinkCount = children.length + (mutableNode.parent === undefined ? 0 : 1);
      if (children.length > maximumChildLinks || traversedLinkCount > maximumTraversalLinks) {
        return undefined;
      }
      return {
        children: children.slice(),
        component: mutableNode.component,
        componentViewModel: mutableNode.componentViewModel,
        element: mutableNode.element,
        key: mutableNode.key,
        parent: mutableNode.parent as unknown as IRenderedVirtualNode | undefined,
        traversedLinkCount,
      };
    },
    getRootVirtualNode: () => rootVirtualNode as unknown as IRenderedVirtualNode | undefined,
  });
}

function captureSnapshot(
  delegate: ValdiWebRendererDelegate,
  elements: IRenderedElement[],
) {
  return delegate.getDebugSnapshot(makeRenderer(elements), MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS);
}

function getBackingNode(delegate: ValdiWebRendererDelegate, id: number): WebValdiLayout | undefined {
  return (delegate as unknown as { nodesRef: Map<number, WebValdiLayout> }).nodesRef.get(id);
}

function makeMutationMetadata(mutation: () => void): Record<string, unknown> {
  let hasMutated = false;
  return new Proxy(
    { trigger: 'value' },
    {
      ownKeys: target => {
        if (!hasMutated) {
          hasMutated = true;
          mutation();
        }
        return Reflect.ownKeys(target);
      },
    },
  );
}

function observeIndexedChildReads(
  children: WebValdiLayout[],
  onIndexedRead: () => void,
): WebValdiLayout[] {
  return new Proxy(children, {
    get: (target, property, receiver) => {
      if (typeof property === 'string' && /^(0|[1-9]\d*)$/.test(property)) {
        onIndexedRead();
      }
      return Reflect.get(target, property, receiver);
    },
  });
}

function expectUnicodeSafeTruncation(value: unknown): void {
  const marker = '... <truncated>';
  const stringValue = String(value);
  const prefix = stringValue.slice(0, -marker.length);
  const lastPrefixCharacter = prefix.charCodeAt(prefix.length - 1);
  expect(stringValue).toContain(marker);
  expect(stringValue.length).toBeLessThanOrEqual(65_536);
  expect(lastPrefixCharacter < 0xd800 || lastPrefixCharacter > 0xdbff).toBeTrue();
}

function makeZeroRect(): DOMRect {
  return {
    bottom: 0,
    height: 0,
    left: 0,
    right: 0,
    top: 0,
    width: 0,
    x: 0,
    y: 0,
    toJSON: () => ({}),
  };
}

describe('ValdiWebRendererDelegate debugger adapter', () => {
  let dom: InstalledDom;

  beforeEach(() => {
    dom = installDom();
  });

  afterEach(() => {
    dom.restore();
  });

  it('captures the legacy node tree with DOM bounds and rendered element attributes', () => {
    const root = dom.createElement('main');
    const delegate = new ValdiWebRendererDelegate(root);
    delegate.onElementCreated(1, 'layout');
    delegate.onElementCreated(2, 'label');
    delegate.onElementBecameRoot(1);
    delegate.onElementMoved(2, 1, 0);

    const rootNode = delegate.getDebugNode(1)!;
    const labelNode = delegate.getDebugNode(2)!;
    expect(rootNode.htmlElement.tagName).toBe('DIV');
    expect(labelNode.htmlElement.tagName).toBe('SPAN');
    rootNode.htmlElement.setAttribute('role', 'main');
    labelNode.htmlElement.setAttribute('aria-label', 'Greeting');
    (rootNode.htmlElement as unknown as { rect: object }).rect = { left: 10, top: 20, width: 300, height: 200 };
    (labelNode.htmlElement as unknown as { rect: object }).rect = { left: 14, top: 28, width: 80, height: 24 };

    const circular: Record<string, unknown> = {};
    circular.self = circular;
    const renderedElements = [
      makeRenderedElement(1, 'layout', { backgroundColor: '#fff' }),
      makeRenderedElement(2, 'label', { onTap: () => {}, value: 'Hello', metadata: circular }),
    ];

    const snapshot = captureSnapshot(delegate, renderedElements);

    expect(snapshot.viewport).toEqual({ width: 1024, height: 768 });
    expect(snapshot.tree).toEqual(
      jasmine.objectContaining({
        id: '1',
        tag: 'layout',
        bounds: { x: 10, y: 20, width: 300, height: 200 },
        element: jasmine.objectContaining({
          attributes: { backgroundColor: '#fff' },
          dom: { attributes: { role: 'main' }, tagName: 'div' },
        }),
      }),
    );
    expect(snapshot.tree?.children[0]).toEqual(
      jasmine.objectContaining({
        id: '2',
        tag: 'label',
        bounds: { x: 14, y: 28, width: 80, height: 24 },
        element: jasmine.objectContaining({
          attributes: { onTap: '[function]', value: 'Hello', metadata: { self: '<circular object/>' } },
          dom: { attributes: { 'aria-label': 'Greeting' }, tagName: 'span' },
        }),
      }),
    );
  });

  it('transactionally interleaves component boundaries with every physical element in render order', () => {
    class RootExampleComponent {}
    class NestedExampleComponent {}

    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementCreated(2, 'label');
    delegate.onElementCreated(3, 'label');
    delegate.onElementBecameRoot(1);
    delegate.onElementMoved(2, 1, 0);
    delegate.onElementMoved(3, 1, 1);
    (delegate.getDebugNode(1)!.htmlElement as unknown as { rect: object }).rect = {
      left: 4,
      top: 8,
      width: 220,
      height: 80,
    };
    (delegate.getDebugNode(2)!.htmlElement as unknown as { rect: object }).rect = {
      left: 12,
      top: 24,
      width: 140,
      height: 20,
    };

    const rootElement = makeRenderedElement(1, 'layout', { accessibilityId: 'sample.root' });
    const nestedElement = makeRenderedElement(2, 'label', { accessibilityLabel: 'Continue' });
    const siblingElement = makeRenderedElement(3, 'label', { value: 'Later' });
    const rootComponent = makeVirtualNode('root', {
      component: new RootExampleComponent() as unknown as IComponent,
    });
    const rootElementNode = makeVirtualNode('layout', { element: rootElement });
    const nestedComponent = makeVirtualNode('nested', {
      component: new NestedExampleComponent() as unknown as IComponent,
    });
    const nestedElementNode = makeVirtualNode('continue', { element: nestedElement });
    const siblingElementNode = makeVirtualNode('later', { element: siblingElement });
    setVirtualChildren(rootComponent, [rootElementNode]);
    setVirtualChildren(rootElementNode, [nestedComponent, siblingElementNode]);
    setVirtualChildren(nestedComponent, [nestedElementNode]);

    const snapshot = delegate.getDebugSnapshot(
      makeHierarchyRenderer([rootElement, nestedElement, siblingElement], rootComponent),
      MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS,
    );

    expect(snapshot.tree).toEqual({
      bounds: { x: 4, y: 8, width: 220, height: 80 },
      children: [
        jasmine.objectContaining({
          id: '1',
          children: [
            {
              bounds: { x: 12, y: 24, width: 140, height: 20 },
              children: [jasmine.objectContaining({ id: '2', tag: 'label' })],
              component: { elementId: '2', key: 'nested', name: 'NestedExampleComponent' },
              id: 'component:["1","nested"]',
              tag: 'NestedExampleComponent',
            },
            jasmine.objectContaining({ id: '3', tag: 'label' }),
          ],
        }),
      ],
      component: { elementId: '1', key: 'root', name: 'RootExampleComponent' },
      id: 'component:[null,"root"]',
      tag: 'RootExampleComponent',
    });
    expect(JSON.stringify(snapshot).length).toBeLessThanOrEqual(MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS);
  });

  it('keeps stable component ids across fresh snapshots and captures updated physical values', () => {
    class StableComponent {}

    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'label');
    delegate.onElementBecameRoot(1);
    const attributes = { value: 'first' };
    const element = makeRenderedElement(1, 'label', attributes);
    const component = makeVirtualNode('stable-key', {
      component: new StableComponent() as unknown as IComponent,
    });
    const elementNode = makeVirtualNode('label', { element });
    setVirtualChildren(component, [elementNode]);
    const renderer = makeHierarchyRenderer([element], component);

    const first = delegate.getDebugSnapshot(renderer, MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS);
    attributes.value = 'second';
    const second = delegate.getDebugSnapshot(renderer, MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS);

    expect(first.tree?.id).toBe('component:[null,"stable-key"]');
    expect(second.tree?.id).toBe(first.tree?.id);
    expect(second.tree?.children[0].element?.attributes.value).toBe('second');
  });

  it('captures the renderer-owned ViewModel without inspecting component instance accessors', () => {
    class SafeComponent {}

    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementBecameRoot(1);
    const element = makeRenderedElement(1, 'layout', {});
    const componentInstance = new SafeComponent() as unknown as IComponent;
    let getterCalls = 0;
    Object.defineProperty(componentInstance, 'constructor', {
      configurable: true,
      get: () => {
        getterCalls++;
        throw new Error('Component fields are not debugger protocol data.');
      },
    });
    Object.defineProperty(componentInstance, 'viewModel', {
      get: () => {
        getterCalls++;
        throw new Error('Component view models belong to a later debugger layer.');
      },
    });
    const viewModel: Record<string, unknown> = { visible: 'safe' };
    Object.defineProperty(viewModel, 'secret', {
      enumerable: true,
      get: () => {
        getterCalls++;
        throw new Error('ViewModel accessors must not be invoked.');
      },
    });
    const component = makeVirtualNode('safe', { component: componentInstance });
    component.componentViewModel = viewModel;
    const elementNode = makeVirtualNode('layout', { element });
    setVirtualChildren(component, [elementNode]);

    const snapshot = delegate.getDebugSnapshot(
      makeHierarchyRenderer([element], component),
      MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS,
    );

    expect(getterCalls).toBe(0);
    expect(snapshot.tree?.component?.name).toBe('SafeComponent');
    expect(snapshot.tree?.component?.properties?.visible).toBe('safe');
    expect(Object.prototype.hasOwnProperty.call(snapshot.tree?.component?.properties ?? {}, 'secret')).toBeFalse();
  });

  it('registers only exact captured own scalar data descriptors for component edits', () => {
    class EditableComponent {}

    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementBecameRoot(1);
    const element = makeRenderedElement(1, 'layout', {});
    const componentInstance = new EditableComponent() as unknown as IComponent;
    const viewModel = Object.create(null) as Record<string, unknown>;
    const unusualPropertyName = 'line\r\n\u0000\ud800"[]';
    Object.defineProperties(viewModel, {
      '   ': { configurable: true, enumerable: true, value: 'blank name', writable: true },
      children: { enumerable: true, value: 'blocked' },
      complex: { enumerable: true, value: { nested: true } },
      count: { configurable: false, enumerable: true, value: 4, writable: false },
      enabled: { configurable: true, enumerable: true, value: true, writable: true },
      infinite: { enumerable: true, value: Number.POSITIVE_INFINITY },
      title: { configurable: true, enumerable: true, value: 'safe', writable: true },
      [unusualPropertyName]: { configurable: true, enumerable: true, value: 'unusual', writable: true },
      zero: { enumerable: true, value: -0 },
    });
    const component = makeVirtualNode('editable', { component: componentInstance });
    component.componentViewModel = viewModel;
    const elementNode = makeVirtualNode('layout', { element });
    setVirtualChildren(component, [elementNode]);
    const candidates: Array<Record<string, unknown>> = [];

    const snapshot = delegate.getDebugSnapshot(
      makeHierarchyRenderer([element], component),
      MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS,
      candidate => {
        candidates.push(candidate as unknown as Record<string, unknown>);
        return { componentToken: candidates.length.toString(16).padStart(32, '0'), snapshotRevision: 7 };
      },
    );

    expect(candidates.map(candidate => candidate['propertyName']).sort()).toEqual([
      'count',
      'enabled',
      unusualPropertyName,
      'title',
    ]);
    expect(candidates[0]?.['component'] === componentInstance).toBeTrue();
    expect(candidates[0]?.['componentId']).toBe('component:[null,"editable"]');
    expect(candidates[0]?.['node'] === (component as unknown as IRenderedVirtualNode)).toBeTrue();
    expect(candidates[0]?.['viewModel'] === viewModel).toBeTrue();
    expect(candidates.every(candidate => candidate['viewModelExtensible'] === true)).toBeTrue();
    expect(snapshot.tree?.component?.propertyEdits).toEqual({
      count: { componentToken: '0'.repeat(31) + '1', snapshotRevision: 7 },
      enabled: { componentToken: '0'.repeat(31) + '2', snapshotRevision: 7 },
      title: { componentToken: '0'.repeat(31) + '3', snapshotRevision: 7 },
      [unusualPropertyName]: { componentToken: '0'.repeat(31) + '4', snapshotRevision: 7 },
    });
  });

  it('keeps custom-prototype, accessor-, method-, and oversized ViewModels read only', () => {
    class ProxyComponent {}
    class CustomViewModel {
      title = 'custom';
    }

    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementBecameRoot(1);
    const element = makeRenderedElement(1, 'layout', {});
    const component = makeVirtualNode('restricted', {
      component: new ProxyComponent() as unknown as IComponent,
    });
    const elementNode = makeVirtualNode('layout', { element });
    setVirtualChildren(component, [elementNode]);
    const registrar = jasmine.createSpy('registrar').and.returnValue({
      componentToken: 'a'.repeat(32),
      snapshotRevision: 1,
    });

    component.componentViewModel = new CustomViewModel();
    const customPrototypeSnapshot = delegate.getDebugSnapshot(
      makeHierarchyRenderer([element], component),
      MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS,
      registrar,
    );
    expect(customPrototypeSnapshot.tree?.component?.properties).toEqual({ title: 'custom' });
    expect(customPrototypeSnapshot.tree?.component?.propertyEdits).toBeUndefined();

    const accessorGetter = jasmine.createSpy('accessorGetter').and.throwError('must not execute');
    const accessorViewModel = Object.create(null) as Record<string, unknown>;
    Object.defineProperties(accessorViewModel, {
      readSecret: { configurable: true, enumerable: false, get: accessorGetter },
      title: { configurable: true, enumerable: true, value: 'accessor-backed', writable: true },
    });
    component.componentViewModel = accessorViewModel;
    const accessorSnapshot = delegate.getDebugSnapshot(
      makeHierarchyRenderer([element], component),
      MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS,
      registrar,
    );
    expect(accessorSnapshot.tree?.component?.properties).toEqual({ title: 'accessor-backed' });
    expect(accessorSnapshot.tree?.component?.propertyEdits).toBeUndefined();
    expect(accessorGetter).not.toHaveBeenCalled();

    const methodViewModel = { title: 'method-backed' };
    Object.defineProperty(methodViewModel, 'readSecret', {
      configurable: true,
      enumerable: false,
      value() {
        return 'secret';
      },
      writable: true,
    });
    component.componentViewModel = methodViewModel;
    const methodSnapshot = delegate.getDebugSnapshot(
      makeHierarchyRenderer([element], component),
      MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS,
      registrar,
    );
    expect(methodSnapshot.tree?.component?.properties).toEqual({ title: 'method-backed' });
    expect(methodSnapshot.tree?.component?.propertyEdits).toBeUndefined();

    const oversizedViewModel = Object.fromEntries(
      Array.from({ length: 1_001 }, (_value, index) => [`property${index}`, index]),
    );
    component.componentViewModel = oversizedViewModel;
    const oversizedSnapshot = delegate.getDebugSnapshot(
      makeHierarchyRenderer([element], component),
      MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS,
      registrar,
    );
    expect(oversizedSnapshot.tree?.component?.properties?.['property0']).toBe(0);
    expect(oversizedSnapshot.tree?.component?.propertyEdits).toBeUndefined();
    expect(registrar).not.toHaveBeenCalled();
  });

  it('retains read-only properties when a Proxy blocks edit-descriptor reflection', () => {
    class ProxyComponent {}

    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementBecameRoot(1);
    const element = makeRenderedElement(1, 'layout', {});
    let ownKeyReads = 0;
    const viewModel = new Proxy(
      { title: 'safe' },
      {
        ownKeys: target => {
          ownKeyReads++;
          if (ownKeyReads > 1) throw new Error('edit reflection unavailable');
          return Reflect.ownKeys(target);
        },
      },
    );
    const component = makeVirtualNode('proxy', { component: new ProxyComponent() as unknown as IComponent });
    component.componentViewModel = viewModel;
    const elementNode = makeVirtualNode('layout', { element });
    setVirtualChildren(component, [elementNode]);
    const registrar = jasmine.createSpy('registrar');

    const snapshot = delegate.getDebugSnapshot(
      makeHierarchyRenderer([element], component),
      MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS,
      registrar,
    );

    expect(snapshot.tree?.component?.properties).toEqual({ title: 'safe' });
    expect(snapshot.tree?.component?.propertyEdits).toBeUndefined();
    expect(registrar).not.toHaveBeenCalled();
  });

  it('does not register a property deleted during Proxy reflection through a polluted descriptor-map prototype', () => {
    class ProxyComponent {}

    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementBecameRoot(1);
    const element = makeRenderedElement(1, 'layout', {});
    const target: { pollutedScalar?: string } = { pollutedScalar: 'safe' };
    let ownKeyReads = 0;
    const viewModel = new Proxy(target, {
      ownKeys: currentTarget => {
        ownKeyReads++;
        if (ownKeyReads === 2) delete currentTarget.pollutedScalar;
        return Reflect.ownKeys(currentTarget);
      },
    });
    const component = makeVirtualNode('proxy-polluted', {
      component: new ProxyComponent() as unknown as IComponent,
    });
    component.componentViewModel = viewModel;
    const elementNode = makeVirtualNode('layout', { element });
    setVirtualChildren(component, [elementNode]);
    const registrar = jasmine.createSpy('registrar').and.returnValue({
      componentToken: 'a'.repeat(32),
      snapshotRevision: 1,
    });
    Object.defineProperty(Object.prototype, 'pollutedScalar', {
      configurable: true,
      enumerable: false,
      value: { configurable: true, enumerable: true, value: 'safe', writable: true },
      writable: true,
    });
    try {
      const snapshot = delegate.getDebugSnapshot(
        makeHierarchyRenderer([element], component),
        MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS,
        registrar,
      );

      expect(snapshot.tree?.component?.properties).toEqual({ pollutedScalar: 'safe' });
      expect(snapshot.tree?.component?.propertyEdits).toBeUndefined();
      expect(registrar).not.toHaveBeenCalled();
    } finally {
      delete (Object.prototype as Record<string, unknown>)['pollutedScalar'];
    }
  });

  it('shares the component property budget without discarding over-budget component hierarchy', () => {
    class ParentBudgetComponent {}
    class ChildBudgetComponent {}

    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementBecameRoot(1);
    const element = makeRenderedElement(1, 'layout', {});
    const parentComponent = makeVirtualNode('parent-budget', {
      component: new ParentBudgetComponent() as unknown as IComponent,
    });
    parentComponent.componentViewModel = { payload: 'p'.repeat(40_000) };
    const childComponent = makeVirtualNode('child-budget', {
      component: new ChildBudgetComponent() as unknown as IComponent,
    });
    childComponent.componentViewModel = { payload: 'c'.repeat(40_000) };
    const elementNode = makeVirtualNode('layout', { element });
    setVirtualChildren(childComponent, [elementNode]);
    setVirtualChildren(parentComponent, [childComponent]);

    const snapshot = delegate.getDebugSnapshot(
      makeHierarchyRenderer([element], parentComponent),
      MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS,
    );
    const childSnapshot = snapshot.tree?.children[0];

    expect(snapshot.tree?.component?.name).toBe('ParentBudgetComponent');
    expect(snapshot.tree?.component?.properties).toBeUndefined();
    expect(childSnapshot?.component?.name).toBe('ChildBudgetComponent');
    expect(String(childSnapshot?.component?.properties?.payload).length).toBe(40_000);
    expect(JSON.stringify(childSnapshot?.component?.properties).length).toBeLessThanOrEqual(65_536);
    expect(childSnapshot?.children[0].id).toBe('1');
  });

  it('drops stale properties but keeps hierarchy when the ViewModel identity changes during capture', () => {
    class ReplacedViewModelComponent {}

    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementBecameRoot(1);
    const element = makeRenderedElement(1, 'layout', {});
    const component = makeVirtualNode('replaced-view-model', {
      component: new ReplacedViewModelComponent() as unknown as IComponent,
    });
    const elementNode = makeVirtualNode('layout', { element });
    setVirtualChildren(component, [elementNode]);
    const baseRenderer = makeHierarchyRenderer([element], component);
    const readSnapshot = baseRenderer.getDebugVirtualNodeSnapshot!;
    const initialViewModel = { value: 'stale' };
    const replacementViewModel = { value: 'fresh' };
    let componentSnapshotReads = 0;
    const renderer = Object.assign(baseRenderer, {
      getDebugVirtualNodeSnapshot: (
        node: IRenderedVirtualNode,
        maximumChildLinks: number,
        maximumTraversalLinks: number,
      ) => {
        const snapshot = readSnapshot.call(baseRenderer, node, maximumChildLinks, maximumTraversalLinks);
        if (snapshot === undefined || node !== (component as unknown as IRenderedVirtualNode)) {
          return snapshot;
        }
        componentSnapshotReads++;
        return {
          ...snapshot,
          componentViewModel: componentSnapshotReads === 1 ? initialViewModel : replacementViewModel,
        };
      },
    });

    const snapshot = delegate.getDebugSnapshot(renderer, MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS);

    expect(componentSnapshotReads).toBe(2);
    expect(snapshot.tree?.component?.name).toBe('ReplacedViewModelComponent');
    expect(snapshot.tree?.component?.properties).toBeUndefined();
    expect(snapshot.tree?.children[0].id).toBe('1');
  });

  it('strips component properties before the complete hierarchy envelope overflows', () => {
    class PropertyHeavyComponent {}

    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementBecameRoot(1);
    const element = makeRenderedElement(1, 'layout', {
      first: 'a'.repeat(50_000),
      fourth: 'd'.repeat(50_000),
      second: 'b'.repeat(50_000),
      third: 'c'.repeat(50_000),
    });
    const component = makeVirtualNode('property-heavy', {
      component: new PropertyHeavyComponent() as unknown as IComponent,
    });
    component.componentViewModel = { payload: 'x'.repeat(65_536) };
    const elementNode = makeVirtualNode('layout', { element });
    setVirtualChildren(component, [elementNode]);

    const snapshot = delegate.getDebugSnapshot(
      makeHierarchyRenderer([element], component),
      MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS,
    );

    expect(snapshot.tree?.id).toBe('component:[null,"property-heavy"]');
    expect(snapshot.tree?.component?.name).toBe('PropertyHeavyComponent');
    expect(snapshot.tree?.component?.properties).toBeUndefined();
    expect(snapshot.tree?.children[0].id).toBe('1');
    expect(JSON.stringify(snapshot).length).toBeLessThanOrEqual(MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS);
  });

  it('omits properties from a revoked Proxy without discarding the hierarchy', () => {
    class ProxyBackedComponent {}

    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementBecameRoot(1);
    const element = makeRenderedElement(1, 'layout', {});
    const component = makeVirtualNode('proxy-backed', {
      component: new ProxyBackedComponent() as unknown as IComponent,
    });
    const revocableViewModel = Proxy.revocable({ visible: 'value' }, {});
    component.componentViewModel = revocableViewModel.proxy;
    revocableViewModel.revoke();
    const elementNode = makeVirtualNode('layout', { element });
    setVirtualChildren(component, [elementNode]);

    const snapshot = delegate.getDebugSnapshot(
      makeHierarchyRenderer([element], component),
      MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS,
    );

    expect(snapshot.tree?.id).toBe('component:[null,"proxy-backed"]');
    expect(snapshot.tree?.component?.name).toBe('ProxyBackedComponent');
    expect(snapshot.tree?.component?.properties).toBeUndefined();
    expect(snapshot.tree?.children[0].id).toBe('1');
  });

  it('falls back atomically for shared, cyclic, partial, and over-deep virtual trees', () => {
    class BoundaryComponent {}

    const captureWithRoot = (rootVirtualNode: MutableVirtualNode) => {
      const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
      delegate.onElementCreated(1, 'layout');
      delegate.onElementBecameRoot(1);
      const element = makeRenderedElement(1, 'layout', {});
      const pendingNodes = [rootVirtualNode];
      const visitedNodes = new Set<MutableVirtualNode>();
      while (pendingNodes.length > 0) {
        const node = pendingNodes.pop()!;
        if (visitedNodes.has(node)) continue;
        visitedNodes.add(node);
        if (node.element !== undefined) node.element = element;
        pendingNodes.push(...node.children);
      }
      return delegate.getDebugSnapshot(
        makeHierarchyRenderer([element], rootVirtualNode),
        MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS,
      );
    };

    const sharedElement = makeVirtualNode('layout', { element: makeRenderedElement(99, 'layout', {}) });
    const sharedRoot = makeVirtualNode('shared', {
      component: new BoundaryComponent() as unknown as IComponent,
    });
    setVirtualChildren(sharedRoot, [sharedElement, sharedElement]);

    const cyclicRoot = makeVirtualNode('cycle', {
      component: new BoundaryComponent() as unknown as IComponent,
    });
    cyclicRoot.children = [cyclicRoot];
    cyclicRoot.parent = cyclicRoot;

    const partialRoot = makeVirtualNode('partial', {
      component: new BoundaryComponent() as unknown as IComponent,
    });

    const deepElement = makeVirtualNode('layout', { element: makeRenderedElement(99, 'layout', {}) });
    let deepRoot = deepElement;
    for (let depth = 0; depth < 65; depth++) {
      const parent = makeVirtualNode(`depth-${depth}`, {
        component: new BoundaryComponent() as unknown as IComponent,
      });
      setVirtualChildren(parent, [deepRoot]);
      deepRoot = parent;
    }

    for (const snapshot of [
      captureWithRoot(sharedRoot),
      captureWithRoot(cyclicRoot),
      captureWithRoot(partialRoot),
      captureWithRoot(deepRoot),
    ]) {
      expect(snapshot.tree?.id).toBe('1');
      expect(snapshot.tree?.component).toBeUndefined();
      expect(snapshot.tree?.children).toEqual([]);
    }
  });

  it('bounds virtual child arrays before indexing and falls back without partial component data', () => {
    class WideComponent {}

    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementBecameRoot(1);
    const element = makeRenderedElement(1, 'layout', {});
    const root = makeVirtualNode('wide', {
      component: new WideComponent() as unknown as IComponent,
    });
    let indexedReads = 0;
    const children = new Array<MutableVirtualNode>(1_001);
    Object.defineProperty(children, '0', {
      get: () => {
        indexedReads++;
        throw new Error('An over-cap child must not be inspected.');
      },
    });
    root.children = children;

    const snapshot = delegate.getDebugSnapshot(
      makeHierarchyRenderer([element], root),
      MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS,
    );

    expect(indexedReads).toBe(0);
    expect(snapshot.tree?.id).toBe('1');
    expect(snapshot.tree?.component).toBeUndefined();
  });

  it('falls back to the element tree when complete component metadata exceeds the envelope budget', () => {
    class LongNameComponent {}
    Object.defineProperty(LongNameComponent, 'name', { value: 'C'.repeat(256) });

    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementBecameRoot(1);
    const element = makeRenderedElement(1, 'layout', {});
    const root = makeVirtualNode('layout', { element });
    const components = Array.from({ length: 999 }, (_value, index) =>
      makeVirtualNode(`${index}:`.padEnd(256, 'k'), {
        component: new LongNameComponent() as unknown as IComponent,
      }),
    );
    setVirtualChildren(root, components);

    const snapshot = delegate.getDebugSnapshot(
      makeHierarchyRenderer([element], root),
      MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS,
    );

    expect(snapshot.tree?.id).toBe('1');
    expect(snapshot.tree?.children).toEqual([]);
    expect(snapshot.tree?.component).toBeUndefined();
    expect(JSON.stringify(snapshot).length).toBeLessThanOrEqual(MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS);
  });

  it('falls back to the captured element tree when hierarchy access reparents a backing node', () => {
    class MutatingComponent {}

    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    for (const id of [1, 2, 3]) {
      delegate.onElementCreated(id, 'layout');
    }
    delegate.onElementBecameRoot(1);
    delegate.onElementMoved(2, 1, 0);
    delegate.onElementMoved(3, 1, 1);
    const elements = [
      makeRenderedElement(1, 'layout', {}),
      makeRenderedElement(2, 'layout', {}),
      makeRenderedElement(3, 'layout', {}),
    ];
    const rootElement = makeVirtualNode('root-layout', { element: elements[0] });
    const firstChild = makeVirtualNode('first', { element: elements[1] });
    const secondChild = makeVirtualNode('second', { element: elements[2] });
    setVirtualChildren(rootElement, [firstChild, secondChild]);
    const mutatingComponent = makeVirtualNode('mutating', {
      component: new MutatingComponent() as unknown as IComponent,
    });
    setVirtualChildren(mutatingComponent, [rootElement]);
    let mutated = false;
    Object.defineProperty(mutatingComponent, 'key', {
      configurable: true,
      get: () => {
        if (!mutated) {
          mutated = true;
          delegate.onElementMoved(2, 3, 0);
        }
        return 'mutating';
      },
    });

    const snapshot = delegate.getDebugSnapshot(
      makeHierarchyRenderer(elements, mutatingComponent),
      MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS,
    );

    expect(snapshot.tree?.id).toBe('1');
    expect(snapshot.tree?.component).toBeUndefined();
    expect(snapshot.tree?.children.map(child => child.id)).toEqual(['2', '3']);
  });

  it('keeps debugger lookup scoped to this renderer and removes destroyed nodes', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(7, 'layout');

    expect(delegate.getDebugNode(7)?.type).toBe('layout');

    delegate.onElementDestroyed(7);

    expect(delegate.getDebugNode(7)).toBeUndefined();
  });

  it('unlinks a destroyed child from snapshots and the highlight lookup', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementCreated(2, 'label');
    delegate.onElementBecameRoot(1);
    delegate.onElementMoved(2, 1, 0);

    delegate.onElementDestroyed(2);

    const snapshot = captureSnapshot(delegate, [
      makeRenderedElement(1, 'layout', {}),
      makeRenderedElement(2, 'label', {}),
    ]);
    expect(delegate.getDebugNode(2)).toBeUndefined();
    expect(snapshot.tree?.children).toEqual([]);
  });

  it('purges every live descendant when one subtree destroy notification is emitted', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    for (const id of [1, 2, 3, 4]) {
      delegate.onElementCreated(id, id === 3 ? 'label' : 'layout');
    }
    delegate.onElementBecameRoot(1);
    delegate.onElementMoved(2, 1, 0);
    delegate.onElementMoved(3, 2, 0);
    delegate.onElementMoved(4, 1, 1);

    delegate.onElementDestroyed(2);

    const snapshot = captureSnapshot(delegate, [
      makeRenderedElement(1, 'layout', {}),
      makeRenderedElement(2, 'layout', {}),
      makeRenderedElement(3, 'label', {}),
      makeRenderedElement(4, 'layout', {}),
    ]);
    expect(delegate.getDebugNode(2)).toBeUndefined();
    expect(delegate.getDebugNode(3)).toBeUndefined();
    expect(snapshot.tree?.children.map(child => child.id)).toEqual(['4']);
  });

  it('preserves a child reparented out of a subsequently destroyed subtree', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    for (const id of [1, 2, 3, 4]) {
      delegate.onElementCreated(id, 'layout');
    }
    delegate.onElementBecameRoot(1);
    delegate.onElementMoved(2, 1, 0);
    delegate.onElementMoved(3, 1, 1);
    delegate.onElementMoved(4, 2, 0);
    delegate.onElementMoved(4, 3, 0);
    getBackingNode(delegate, 2)!.children.push(getBackingNode(delegate, 4)!);

    delegate.onElementDestroyed(2);

    const snapshot = captureSnapshot(delegate, [
      makeRenderedElement(1, 'layout', {}),
      makeRenderedElement(3, 'layout', {}),
      makeRenderedElement(4, 'layout', {}),
    ]);
    expect(delegate.getDebugNode(4)).toBeDefined();
    expect(snapshot.tree?.children.map(child => child.id)).toEqual(['3']);
    expect(snapshot.tree?.children[0].children.map(child => child.id)).toEqual(['4']);
  });

  it('ignores adversarial stale child links for snapshots and highlight lookup', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementCreated(2, 'label');
    delegate.onElementBecameRoot(1);
    delegate.onElementMoved(2, 1, 0);
    const rootNode = getBackingNode(delegate, 1)!;
    const staleChild = getBackingNode(delegate, 2)!;

    delegate.onElementDestroyed(2);
    rootNode.children.push(staleChild);
    staleChild.parent = rootNode;
    const getElementForId = jasmine.createSpy('getElementForId').and.callFake((id: number) =>
      makeRenderedElement(id, id === 1 ? 'layout' : 'label', {}),
    );
    const renderer = { getElementForId } as unknown as IRenderer;

    const snapshot = delegate.getDebugSnapshot(renderer, MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS);

    expect(delegate.getDebugNode(2)).toBeUndefined();
    expect(snapshot.tree?.children).toEqual([]);
    expect(getElementForId.calls.count()).toBe(1);
    expect(getElementForId).toHaveBeenCalledWith(1);
  });

  it('continues safely when metadata destroys a later sibling during capture', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    for (const id of [1, 2, 3, 4]) {
      delegate.onElementCreated(id, 'layout');
    }
    delegate.onElementBecameRoot(1);
    delegate.onElementMoved(2, 1, 0);
    delegate.onElementMoved(3, 1, 1);
    delegate.onElementMoved(4, 1, 2);
    const metadata = makeMutationMetadata(() => delegate.onElementDestroyed(3));

    const snapshot = captureSnapshot(delegate, [
      makeRenderedElement(1, 'layout', {}),
      makeRenderedElement(2, 'layout', { metadata }),
      makeRenderedElement(3, 'layout', {}),
      makeRenderedElement(4, 'layout', {}),
    ]);

    expect(delegate.getDebugNode(3)).toBeUndefined();
    expect(snapshot.tree?.children.map(child => child.id)).toEqual(['2', '4']);
  });

  it('continues safely when metadata detaches a later sibling during capture', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    for (const id of [1, 2, 3, 4]) {
      delegate.onElementCreated(id, 'layout');
    }
    delegate.onElementBecameRoot(1);
    delegate.onElementMoved(2, 1, 0);
    delegate.onElementMoved(3, 1, 1);
    delegate.onElementMoved(4, 1, 2);
    const rootNode = getBackingNode(delegate, 1)!;
    const detachedNode = getBackingNode(delegate, 3)!;
    const metadata = makeMutationMetadata(() => {
      rootNode.removeChild(detachedNode);
      detachedNode.parent = null;
    });

    const snapshot = captureSnapshot(delegate, [
      makeRenderedElement(1, 'layout', {}),
      makeRenderedElement(2, 'layout', { metadata }),
      makeRenderedElement(3, 'layout', {}),
      makeRenderedElement(4, 'layout', {}),
    ]);

    expect(delegate.getDebugNode(3)).toBeDefined();
    expect(snapshot.tree?.children.map(child => child.id)).toEqual(['2', '4']);
  });

  it('continues safely when metadata reparents a later sibling during capture', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    for (const id of [1, 2, 3, 4]) {
      delegate.onElementCreated(id, 'layout');
    }
    delegate.onElementBecameRoot(1);
    delegate.onElementMoved(2, 1, 0);
    delegate.onElementMoved(3, 1, 1);
    delegate.onElementMoved(4, 1, 2);
    const metadata = makeMutationMetadata(() => delegate.onElementMoved(3, 4, 0));

    const snapshot = captureSnapshot(delegate, [
      makeRenderedElement(1, 'layout', {}),
      makeRenderedElement(2, 'layout', { metadata }),
      makeRenderedElement(3, 'layout', {}),
      makeRenderedElement(4, 'layout', {}),
    ]);

    expect(snapshot.tree?.children.map(child => child.id)).toEqual(['2', '4']);
    expect(snapshot.tree?.children[1].children.map(child => child.id)).toEqual(['3']);
  });

  it('bounds work for a wide array containing only stale child links', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementCreated(2, 'layout');
    delegate.onElementBecameRoot(1);
    delegate.onElementMoved(2, 1, 0);
    const rootNode = getBackingNode(delegate, 1)!;
    const staleNode = getBackingNode(delegate, 2)!;
    delegate.onElementDestroyed(2);
    staleNode.parent = rootNode;
    let indexedReads = 0;
    rootNode.children = observeIndexedChildReads(
      new Array<WebValdiLayout>(20_000).fill(staleNode),
      () => indexedReads++,
    );
    const getElementForId = jasmine.createSpy('getElementForId').and.callFake((id: number) =>
      makeRenderedElement(id, 'layout', {}),
    );

    const snapshot = delegate.getDebugSnapshot(
      { getElementForId } as unknown as IRenderer,
      MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS,
    );

    expect(indexedReads).toBe(1_000);
    expect(getElementForId.calls.count()).toBe(1);
    expect(snapshot.tree?.children).toEqual([]);
    expect(snapshot.tree?.childrenTruncated).toBeTrue();
  });

  it('includes mixed live and stale links at the work cap and truncates beyond it', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    for (const id of [1, 2, 3]) {
      delegate.onElementCreated(id, 'layout');
    }
    delegate.onElementBecameRoot(1);
    delegate.onElementMoved(2, 1, 0);
    delegate.onElementMoved(3, 1, 1);
    const rootNode = getBackingNode(delegate, 1)!;
    const liveNode = getBackingNode(delegate, 2)!;
    const staleNode = getBackingNode(delegate, 3)!;
    delegate.onElementDestroyed(3);
    staleNode.parent = rootNode;
    const renderer = makeRenderer([
      makeRenderedElement(1, 'layout', {}),
      makeRenderedElement(2, 'layout', {}),
    ]);
    let atCapReads = 0;
    rootNode.children = observeIndexedChildReads(
      [...new Array<WebValdiLayout>(999).fill(staleNode), liveNode],
      () => atCapReads++,
    );

    const atCapSnapshot = delegate.getDebugSnapshot(renderer, MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS);

    expect(atCapReads).toBe(1_000);
    expect(atCapSnapshot.tree?.children.map(child => child.id)).toEqual(['2']);
    expect(atCapSnapshot.tree?.childrenTruncated).toBeUndefined();

    let overCapReads = 0;
    rootNode.children = observeIndexedChildReads(
      [...new Array<WebValdiLayout>(1_000).fill(staleNode), liveNode],
      () => overCapReads++,
    );

    const overCapSnapshot = delegate.getDebugSnapshot(renderer, MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS);

    expect(overCapReads).toBe(1_000);
    expect(overCapSnapshot.tree?.children).toEqual([]);
    expect(overCapSnapshot.tree?.childrenTruncated).toBeTrue();
  });

  it('does not invoke getters and distinguishes shared references from cycles', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementBecameRoot(1);
    let getterCalls = 0;
    const dangerous: Record<string, unknown> = {};
    Object.defineProperty(dangerous, 'secret', {
      enumerable: true,
      get: () => {
        getterCalls++;
        return 'should not be read';
      },
    });
    const shared = { value: 'shared' };
    const metadata: Record<string, unknown> = { dangerous, first: shared, second: shared };
    metadata.self = metadata;
    Object.defineProperty(metadata, '__proto__', { enumerable: true, value: 'own property' });

    const snapshot = captureSnapshot(delegate, [makeRenderedElement(1, 'layout', { metadata })]);
    const debugMetadata = snapshot.tree?.element?.attributes.metadata as Record<string, unknown>;

    expect(getterCalls).toBe(0);
    expect(debugMetadata.dangerous).toEqual({ secret: '<accessor/>' });
    expect(debugMetadata.first).toEqual({ value: 'shared' });
    expect(debugMetadata.second).toEqual({ value: 'shared' });
    expect(debugMetadata.self).toBe('<circular object/>');
    expect(Object.prototype.hasOwnProperty.call(debugMetadata, '__proto__')).toBeTrue();
    expect(debugMetadata['__proto__']).toBe('own property');
  });

  it('inspects only the capped prefix of large sparse arrays without invoking getters', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementBecameRoot(1);
    let getterCalls = 0;
    const sparseArray: unknown[] = [];
    sparseArray.length = 1_000_000;
    for (const index of [0, 49, 50]) {
      Object.defineProperty(sparseArray, String(index), {
        enumerable: true,
        get: () => {
          getterCalls++;
          return `secret-${index}`;
        },
      });
    }
    const inspectedProperties: string[] = [];
    const inspectedArray = new Proxy(sparseArray, {
      getOwnPropertyDescriptor: (target, property) => {
        inspectedProperties.push(String(property));
        return Reflect.getOwnPropertyDescriptor(target, property);
      },
    });

    const snapshot = captureSnapshot(delegate, [
      makeRenderedElement(1, 'layout', { items: inspectedArray }),
    ]);
    const debugItems = snapshot.tree?.element?.attributes.items as unknown[];

    expect(getterCalls).toBe(0);
    expect(inspectedProperties).toEqual([
      'length',
      ...Array.from({ length: 50 }, (_value, index) => String(index)),
    ]);
    expect(inspectedProperties).not.toContain('50');
    expect(debugItems[0]).toBe('<accessor/>');
    expect(debugItems[49]).toBe('<accessor/>');
    expect(debugItems[50]).toBe('999950 more items');
  });

  it('caps attribute count, individual strings, and the aggregate snapshot budget', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementBecameRoot(1);
    const attributes: Record<string, unknown> = {};
    for (let index = 0; index < 60; index++) {
      attributes[`value${index}`] = 'x'.repeat(100_000);
    }

    const snapshot = captureSnapshot(delegate, [makeRenderedElement(1, 'layout', attributes)]);
    const debugAttributes = snapshot.tree?.element?.attributes ?? {};

    expect(String(debugAttributes.value0).length).toBeLessThanOrEqual(65_536);
    expect(String(debugAttributes.value0)).toContain('<truncated>');
    expect(debugAttributes.__truncated__).toBeDefined();
    expect(JSON.stringify(debugAttributes).length).toBeLessThan(264_000);
  });

  it('truncates rendered, nested, and DOM attribute strings at Unicode boundaries', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementBecameRoot(1);
    const unicodeBoundaryValue = `${'a'.repeat(65_520)}${'😀'.repeat(10_000)}`;
    delegate.getDebugNode(1)!.htmlElement.setAttribute('data-emoji', unicodeBoundaryValue);

    const snapshot = captureSnapshot(delegate, [
      makeRenderedElement(1, 'layout', {
        metadata: { nested: unicodeBoundaryValue },
        value: unicodeBoundaryValue,
      }),
    ]);
    const attributes = snapshot.tree!.element!.attributes;
    const nested = attributes.metadata as Record<string, unknown>;

    expectUnicodeSafeTruncation(attributes.value);
    expectUnicodeSafeTruncation(nested.nested);
    expectUnicodeSafeTruncation(snapshot.tree!.element!.dom.attributes['data-emoji']);
    expect(JSON.stringify(snapshot).length).toBeLessThanOrEqual(MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS);
  });

  it('honors a smaller serialized-character budget assigned by the standalone envelope', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementBecameRoot(1);
    const renderer = makeRenderer([
      makeRenderedElement(1, 'layout', { payload: 'x'.repeat(100_000) }),
    ]);

    const snapshot = delegate.getDebugSnapshot(renderer, 8_192);

    expect(JSON.stringify(snapshot).length).toBeLessThanOrEqual(8_192);
    expect(String(snapshot.tree?.element?.attributes.payload)).toContain('<truncated>');
  });

  it('bounds the complete serialized snapshot and omits over-budget property names before insertion', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementBecameRoot(1);
    const hugeKey = `key-${'k'.repeat(2_000_000)}`;
    const hugeValue = `value-${'v'.repeat(2_000_000)}`;
    let getterCalls = 0;
    const metadata: Record<string, unknown> = {};
    Object.defineProperty(metadata, hugeKey, {
      enumerable: true,
      get: () => {
        getterCalls++;
        return 'secret';
      },
    });
    const attributeReads: string[] = [];
    const element = {
      id: 1,
      tag: 'layout',
      getAttribute: (name: string) => {
        attributeReads.push(name);
        return name === 'hugeValue' ? hugeValue : metadata;
      },
      getAttributeNames: () => ['hugeValue', 'metadata', hugeKey],
    } as unknown as IRenderedElement;
    delegate.getDebugNode(1)!.htmlElement.setAttribute(hugeKey, hugeValue);

    const snapshot = captureSnapshot(delegate, [element]);
    const serializedSnapshot = JSON.stringify(snapshot);
    const debugAttributes = snapshot.tree?.element?.attributes ?? {};

    expect(serializedSnapshot.length).toBeLessThanOrEqual(MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS);
    expect(String(debugAttributes.hugeValue).length).toBeLessThanOrEqual(65_536);
    expect(String(debugAttributes.hugeValue)).toContain('<truncated>');
    expect((debugAttributes.metadata as Record<string, unknown>).__truncated__).toBeDefined();
    expect(Object.prototype.hasOwnProperty.call(debugAttributes, hugeKey)).toBeFalse();
    expect(Object.prototype.hasOwnProperty.call(snapshot.tree?.element?.dom.attributes ?? {}, hugeKey)).toBeFalse();
    expect(attributeReads).not.toContain(hugeKey);
    expect(getterCalls).toBe(0);
  });

  it('caps nested collection entries and depth', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementBecameRoot(1);
    const properties: Record<string, number> = {};
    for (let index = 0; index < 52; index++) {
      properties[`property${index}`] = index;
    }
    let tooDeepGetterCalls = 0;
    const tooDeepObject: Record<string, unknown> = {};
    Object.defineProperty(tooDeepObject, 'secret', {
      enumerable: true,
      get: () => {
        tooDeepGetterCalls++;
        return 'should not be read';
      },
    });
    const nested = {
      level1: {
        level2: {
          boundary: 'visible',
          level3: {
            level4: 'too-deep',
            objectAtDepthLimit: tooDeepObject,
          },
        },
      },
    };

    const snapshot = captureSnapshot(delegate, [
      makeRenderedElement(1, 'layout', { nested, properties }),
    ]);
    const debugAttributes = snapshot.tree?.element?.attributes ?? {};
    const debugProperties = debugAttributes.properties as Record<string, unknown>;

    expect(debugProperties.property49).toBe(49);
    expect(debugProperties.property50).toBeUndefined();
    expect(debugProperties.__truncated__).toBe('more fields');
    expect(debugAttributes.nested).toEqual({
      level1: {
        level2: {
          boundary: 'visible',
          level3: {
            level4: '... <truncated>',
            objectAtDepthLimit: '... <truncated>',
          },
        },
      },
    });
    expect(tooDeepGetterCalls).toBe(0);
  });

  it('bounds wide backing-tree work without materializing eager renderer children', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    let renderedNodeReads = 0;
    let eagerElementChildrenReads = 0;
    let eagerVirtualChildrenReads = 0;
    const attributeReads = jasmine.createSpy('attributeReads').and.returnValue([]);
    const renderedElement = {
      id: 1,
      get children(): IRenderedElement[] {
        eagerElementChildrenReads++;
        return new Array(20_000).fill(undefined);
      },
      getAttribute: () => undefined,
      getAttributeNames: attributeReads,
    } as unknown as IRenderedElement;
    const adversarialRootVirtualNode = {} as Record<string, unknown>;
    Object.defineProperty(adversarialRootVirtualNode, 'children', {
      get: () => {
        eagerVirtualChildrenReads++;
        return new Array(20_000).fill(adversarialRootVirtualNode);
      },
    });
    const getElementForId = jasmine.createSpy('getElementForId').and.returnValue(renderedElement);
    const getRootVirtualNode = jasmine
      .createSpy('getRootVirtualNode')
      .and.returnValue(adversarialRootVirtualNode);
    const renderer = { getElementForId, getRootVirtualNode } as unknown as IRenderer;

    delegate.onElementCreated(1, 'layout');
    delegate.onElementBecameRoot(1);
    delegate.getDebugNode(1)!.htmlElement.getBoundingClientRect = () => {
      renderedNodeReads++;
      return makeZeroRect();
    };
    for (let id = 2; id <= 1_101; id++) {
      delegate.onElementCreated(id, 'label');
      delegate.onElementMoved(id, 1, id - 2);
      delegate.getDebugNode(id)!.htmlElement.getBoundingClientRect = () => {
        renderedNodeReads++;
        return makeZeroRect();
      };
    }

    const snapshot = delegate.getDebugSnapshot(renderer, MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS);

    expect(renderedNodeReads).toBe(1_000);
    expect(getElementForId.calls.count()).toBe(1_000);
    expect(attributeReads.calls.count()).toBe(1_000);
    expect(getRootVirtualNode).not.toHaveBeenCalled();
    expect(eagerElementChildrenReads).toBe(0);
    expect(eagerVirtualChildrenReads).toBe(0);
    expect(snapshot.tree?.children.length).toBe(999);
    expect(snapshot.tree?.childrenTruncated).toBeTrue();
    expect(JSON.stringify(snapshot).length).toBeLessThanOrEqual(MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS);
  });

  it('stops deeply nested backing trees without consulting recursive virtual children', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementBecameRoot(1);
    for (let id = 2; id <= 100; id++) {
      delegate.onElementCreated(id, 'layout');
      delegate.onElementMoved(id, id - 1, 0);
    }

    let eagerChildrenReads = 0;
    const renderedElement = makeRenderedElement(1, 'layout', {});
    Object.defineProperty(renderedElement, 'children', {
      get: () => {
        eagerChildrenReads++;
        return new Array(20_000).fill(renderedElement);
      },
    });
    const getElementForId = jasmine.createSpy('getElementForId').and.returnValue(renderedElement);
    const renderer = { getElementForId } as unknown as IRenderer;
    const snapshot = delegate.getDebugSnapshot(renderer, MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS);
    let capturedDepth = 0;
    let deepestNode = snapshot.tree;
    while (deepestNode !== null && deepestNode !== undefined) {
      capturedDepth++;
      if (deepestNode.children.length === 0) {
        break;
      }
      deepestNode = deepestNode.children[0];
    }

    expect(capturedDepth).toBe(64);
    expect(getElementForId.calls.count()).toBe(64);
    expect(eagerChildrenReads).toBe(0);
    expect(deepestNode?.id).toBe('64');
    expect(deepestNode?.childrenTruncated).toBeTrue();
  });

  it('does not visit descendants after the aggregate character budget is exhausted', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    delegate.onElementCreated(2, 'label');
    delegate.onElementBecameRoot(1);
    delegate.onElementMoved(2, 1, 0);
    const attributes: Record<string, unknown> = {};
    for (let index = 0; index < 5; index++) {
      attributes[`value${index}`] = 'x'.repeat(100_000);
    }
    const rootElement = makeRenderedElement(1, 'layout', attributes);
    const childElement = makeRenderedElement(2, 'label', {});
    const childAttributeReads = jasmine.createSpy('childAttributeReads').and.returnValue([]);
    childElement.getAttributeNames = childAttributeReads;

    const snapshot = captureSnapshot(delegate, [rootElement, childElement]);

    expect(childAttributeReads).not.toHaveBeenCalled();
    expect(snapshot.tree?.children).toEqual([]);
    expect(snapshot.tree?.childrenTruncated).toBeTrue();
  });

  it('returns an empty snapshot until a root is mounted and after it is destroyed', () => {
    const delegate = new ValdiWebRendererDelegate(dom.createElement('main'));
    delegate.onElementCreated(1, 'layout');
    const renderer = makeRenderer([]);

    expect(delegate.getDebugSnapshot(renderer, MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS).tree).toBeNull();

    delegate.onElementBecameRoot(1);
    expect(delegate.getDebugSnapshot(renderer, MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS).tree?.id).toBe('1');

    delegate.onElementDestroyed(1);
    expect(delegate.getDebugSnapshot(renderer, MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS).tree).toBeNull();
  });
});
