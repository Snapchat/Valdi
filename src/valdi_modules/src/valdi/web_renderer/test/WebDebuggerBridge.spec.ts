import 'jasmine/src/jasmine';
import type { IRenderer } from 'valdi_core/src/IRenderer';
import type {
  ValdiWebRendererDelegate,
  WebRendererDebugPropertyEditMetadata,
  WebRendererDebugSnapshot,
} from '../src/ValdiWebRendererDelegate';
import { MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS } from '../src/ValdiWebRendererDelegate';
import type { ComponentPropertyEditRegistrar } from '../src/debug/ComponentHierarchySnapshot';
import type { StandaloneWebDebuggerRuntime, StandaloneWebDebuggerSnapshot } from '../src/debug/WebDebuggerBridge';
import { WebDebuggerBridge } from '../src/debug/WebDebuggerBridge';

interface FakeDebuggerWindow {
  __VALDI_WEB_DEBUGGER__?: StandaloneWebDebuggerRuntime;
  addEventListener: jasmine.Spy;
  location: {
    href: string;
    search: string;
  };
  parent: FakeDebuggerWindow | { postMessage: jasmine.Spy };
  removeEventListener: jasmine.Spy;
}

interface WebDebuggerBridgeRegistryState {
  componentPropertyEditExpiryTimer?: ReturnType<typeof setTimeout>;
  componentPropertyEditPreviousTokens: Map<string, unknown>;
  componentPropertyEditTokens: Map<string, unknown>;
}

function registryState(bridge: WebDebuggerBridge): WebDebuggerBridgeRegistryState {
  return bridge as unknown as WebDebuggerBridgeRegistryState;
}

function registryTokenCount(bridge: WebDebuggerBridge): number {
  const state = registryState(bridge);
  return state.componentPropertyEditTokens.size + state.componentPropertyEditPreviousTokens.size;
}

describe('WebDebuggerBridge legacy renderer adapter', () => {
  let previousWindow: unknown;
  let previousDocument: unknown;
  let previousElement: unknown;
  let previousCrypto: PropertyDescriptor | undefined;
  let fakeWindow: FakeDebuggerWindow;
  let delegate: ValdiWebRendererDelegate;
  let renderer: IRenderer;
  let appendedOverlays: FakeElement[];

  class FakeElement {
    readonly children: FakeElement[] = [];
    readonly dataset: Record<string, string> = {};
    readonly style: Record<string, string> = {};
    parentElement: FakeElement | null = null;
    removed = false;
    textContent = '';

    appendChild(child: FakeElement): FakeElement {
      child.parentElement = this;
      this.children.push(child);
      return child;
    }

    getBoundingClientRect() {
      return { left: 11, top: 22, width: 120, height: 44 };
    }

    remove(): void {
      this.removed = true;
    }

    setAttribute(): void {}
  }

  beforeEach(() => {
    previousWindow = (globalThis as { window?: unknown }).window;
    previousDocument = (globalThis as { document?: unknown }).document;
    previousElement = (globalThis as { Element?: unknown }).Element;
    previousCrypto = Object.getOwnPropertyDescriptor(globalThis, 'crypto');
    appendedOverlays = [];
    fakeWindow = {
      addEventListener: jasmine.createSpy('addEventListener'),
      location: {
        href: 'http://127.0.0.1:54321/?valdiDebugger=1&valdiOwlDebugger=1',
        search: '?valdiDebugger=1&valdiOwlDebugger=1',
      },
      parent: undefined as unknown as FakeDebuggerWindow,
      removeEventListener: jasmine.createSpy('removeEventListener'),
    };
    fakeWindow.parent = fakeWindow;
    (globalThis as { Element?: unknown }).Element = FakeElement;
    let tokenCounter = 0;
    Object.defineProperty(globalThis, 'crypto', {
      configurable: true,
      value: {
        getRandomValues: (bytes: Uint8Array) => {
          tokenCounter++;
          bytes.fill(0);
          new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength).setUint32(bytes.length - 4, tokenCounter);
          return bytes;
        },
      },
    });
    (globalThis as { window?: unknown }).window = fakeWindow;
    (globalThis as { document?: unknown }).document = {
      body: {
        appendChild: (element: FakeElement) => appendedOverlays.push(element),
      },
      createElement: () => new FakeElement(),
      title: 'Valdi Owl sample',
    };
    delegate = {
      getDebugNode: () => undefined,
      getDebugSnapshot: () => ({
        tree: null,
        viewport: { width: 640, height: 480 },
      }),
    } as unknown as ValdiWebRendererDelegate;
    renderer = {
      getElementForId: () => undefined,
      getRootVirtualNode: jasmine
        .createSpy('getRootVirtualNode')
        .and.throwError('The debugger must not materialize virtual children.'),
    } as unknown as IRenderer;
  });

  afterEach(() => {
    restoreGlobal('window', previousWindow);
    restoreGlobal('document', previousDocument);
    restoreGlobal('Element', previousElement);
    restoreGlobalProperty('crypto', previousCrypto);
  });

  it('exposes the real top-level Owl renderer read only when the renderer mutation API is unavailable', () => {
    const bridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);

    expect(fakeWindow.__VALDI_WEB_DEBUGGER__?.getSnapshot()).toEqual({
      channel: 'valdi-web-debugger',
      componentPropertyEditingAvailable: false,
      source: { title: 'Valdi Owl sample', url: fakeWindow.location.href },
      snapshot: { tree: null, viewport: { width: 640, height: 480 } },
      type: 'snapshot',
    });

    bridge.destroy();
    expect(fakeWindow.__VALDI_WEB_DEBUGGER__).toBeUndefined();
  });

  it('bounds Unicode source metadata and the complete standalone envelope', () => {
    const metadataMarker = '... <truncated>';
    const fakeDocument = (globalThis as unknown as { document: { title: string } }).document;
    fakeDocument.title = '😀'.repeat(100_000);
    fakeWindow.location.href = `http://127.0.0.1:54321/?title=${'🦉'.repeat(100_000)}`;
    const getDebugSnapshot = jasmine.createSpy('getDebugSnapshot').and.returnValue({
      tree: null,
      viewport: { width: 640, height: 480 },
    });
    delegate.getDebugSnapshot = getDebugSnapshot;
    const bridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);

    const response = fakeWindow.__VALDI_WEB_DEBUGGER__!.getSnapshot();
    const titlePrefix = response.source.title.slice(0, -metadataMarker.length);
    const urlPrefix = response.source.url.slice(0, -metadataMarker.length);

    expect(JSON.stringify(response).length).toBeLessThanOrEqual(MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS);
    expect(JSON.stringify(response.source.title).length).toBeLessThanOrEqual(16_384);
    expect(JSON.stringify(response.source.url).length).toBeLessThanOrEqual(16_384);
    expect(response.source.title).toContain(metadataMarker);
    expect(response.source.url).toContain(metadataMarker);
    const lastTitleCharacter = titlePrefix.charCodeAt(titlePrefix.length - 1);
    const lastUrlCharacter = urlPrefix.charCodeAt(urlPrefix.length - 1);
    expect(lastTitleCharacter < 0xd800 || lastTitleCharacter > 0xdbff).toBeTrue();
    expect(lastUrlCharacter < 0xd800 || lastUrlCharacter > 0xdbff).toBeTrue();
    expect(getDebugSnapshot).toHaveBeenCalledWith(renderer, jasmine.any(Number), undefined);
    expect(getDebugSnapshot.calls.mostRecent().args[1]).toBeLessThan(MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS);
    expect(renderer.getRootVirtualNode as unknown as jasmine.Spy).not.toHaveBeenCalled();

    bridge.destroy();
  });

  it('enforces the final envelope ceiling when a delegate exceeds its assigned snapshot budget', () => {
    delegate.getDebugSnapshot = jasmine.createSpy('getDebugSnapshot').and.returnValue({
      tree: {
        id: '1',
        tag: 'layout',
        element: {
          id: 1,
          attributes: { payload: 'x'.repeat(300_000) },
          dom: { attributes: {}, tagName: 'div' },
        },
        bounds: { x: 0, y: 0, width: 0, height: 0 },
        children: [],
      },
      viewport: { width: 640, height: 480 },
    });
    const bridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);

    const response = fakeWindow.__VALDI_WEB_DEBUGGER__!.getSnapshot();

    expect(response.snapshot.tree).toBeNull();
    expect(JSON.stringify(response).length).toBeLessThanOrEqual(MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS);
    expect(renderer.getRootVirtualNode as unknown as jasmine.Spy).not.toHaveBeenCalled();
    bridge.destroy();
  });

  it('issues single-use exact-identity edit tokens', () => {
    const component = {} as never;
    const node = {} as never;
    const viewModel = Object.defineProperty({}, 'title', {
      configurable: false,
      enumerable: true,
      value: 'before',
      writable: true,
    });
    const editDebugComponentProperty = jasmine.createSpy('editDebugComponentProperty').and.returnValue(true);
    renderer.editDebugComponentProperty = editDebugComponentProperty;
    delegate.getDebugSnapshot = jasmine
      .createSpy('getDebugSnapshot')
      .and.callFake((_renderer: IRenderer, _maximum: number, registrar: ComponentPropertyEditRegistrar | undefined) => {
        const propertyEdit = registrar?.({
          component,
          componentId: 'component:[null,"root"]',
          descriptor: Object.getOwnPropertyDescriptor(viewModel, 'title')!,
          node,
          propertyName: 'title',
          viewModel,
          viewModelExtensible: true,
        });
        return {
          tree: {
            children: [],
            component: {
              key: 'root',
              name: 'Root',
              properties: { title: 'before' },
              ...(propertyEdit === undefined ? {} : { propertyEdits: { title: propertyEdit } }),
            },
            id: 'component:[null,"root"]',
            tag: 'Root',
          },
          viewport: { width: 640, height: 480 },
        };
      });
    const bridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);
    const runtime = fakeWindow.__VALDI_WEB_DEBUGGER__!;

    const first = runtime.getSnapshot();
    const firstEdit = first.snapshot.tree?.component?.propertyEdits?.['title'];
    expect(first.componentPropertyEditingAvailable).toBeTrue();
    expect(firstEdit?.componentToken).toMatch(/^[0-9a-f]{32}$/);
    expect(firstEdit?.snapshotRevision).toBe(1);
    if (!firstEdit) throw new Error('Expected editable property metadata.');
    for (const invalidRequest of [
      { componentId: 'other-component' },
      { propertyName: 'other-property' },
      { propertyName: '   ' },
      { snapshotRevision: firstEdit.snapshotRevision + 1 },
      { value: '😀'.repeat(20_000) },
    ]) {
      expect(() =>
        runtime.editComponentProperty?.({
          componentId: 'component:[null,"root"]',
          componentToken: firstEdit.componentToken,
          propertyName: 'title',
          snapshotRevision: firstEdit.snapshotRevision,
          value: 'after',
          ...invalidRequest,
        }),
      ).toThrowError('The component property edit is stale or invalid.');
    }
    expect(editDebugComponentProperty).not.toHaveBeenCalled();
    expect(
      runtime.editComponentProperty?.({
        componentId: 'component:[null,"root"]',
        componentToken: firstEdit.componentToken,
        propertyName: 'title',
        snapshotRevision: firstEdit.snapshotRevision,
        value: 'after',
      }),
    ).toBeTrue();
    expect(editDebugComponentProperty).toHaveBeenCalledWith(
      jasmine.objectContaining({
        component,
        expectedViewModel: viewModel,
        expectedViewModelExtensible: true,
        newValue: 'after',
        node,
        propertyName: 'title',
      }),
    );
    expect(() =>
      runtime.editComponentProperty?.({
        componentId: 'component:[null,"root"]',
        componentToken: firstEdit.componentToken,
        propertyName: 'title',
        snapshotRevision: firstEdit.snapshotRevision,
        value: 'again',
      }),
    ).toThrowError('The component property edit is stale or invalid.');

    const second = runtime.getSnapshot();
    const secondEdit = second.snapshot.tree?.component?.propertyEdits?.['title'];
    if (!secondEdit) throw new Error('Expected replacement editable property metadata.');
    expect(secondEdit?.snapshotRevision).toBe(2);
    expect(secondEdit?.componentToken).not.toBe(firstEdit.componentToken);
    expect(() =>
      runtime.editComponentProperty?.({
        componentId: 'component:[null,"root"]',
        componentToken: firstEdit.componentToken,
        propertyName: 'title',
        snapshotRevision: firstEdit.snapshotRevision,
        value: 'stale',
      }),
    ).toThrowError('The component property edit is stale or invalid.');
    bridge.destroy();
    expect(() =>
      runtime.editComponentProperty?.({
        componentId: 'component:[null,"root"]',
        componentToken: secondEdit.componentToken,
        propertyName: 'title',
        snapshotRevision: secondEdit.snapshotRevision,
        value: 'after-destroy',
      }),
    ).toThrowError('The component property edit is stale or invalid.');
    expect(editDebugComponentProperty.calls.count()).toBe(1);
  });

  it('keeps published revisions intact when a delegate calls a retained registrar too late', () => {
    const component = {} as never;
    const node = {} as never;
    const viewModel = { title: 'before' };
    const candidate = {
      component,
      componentId: 'component-id',
      descriptor: Object.getOwnPropertyDescriptor(viewModel, 'title')!,
      node,
      propertyName: 'title',
      viewModel,
      viewModelExtensible: true,
    };
    const retainedRegistrars: ComponentPropertyEditRegistrar[] = [];
    renderer.editDebugComponentProperty = jasmine.createSpy('editDebugComponentProperty').and.returnValue(true);
    delegate.getDebugSnapshot = jasmine
      .createSpy('getDebugSnapshot')
      .and.callFake((_renderer: IRenderer, _maximum: number, registrar: ComponentPropertyEditRegistrar | undefined) => {
        if (!registrar) throw new Error('Expected component property editing to be available.');
        retainedRegistrars.push(registrar);
        const metadata = registrar(candidate);
        return {
          tree: {
            children: [],
            component: {
              key: 'root',
              name: 'Root',
              properties: { title: 'before' },
              propertyEdits: { title: metadata },
            },
            id: 'component-id',
            tag: 'Root',
          },
          viewport: { height: 1, width: 1 },
        };
      });
    const bridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);
    const runtime = fakeWindow.__VALDI_WEB_DEBUGGER__!;
    const first = runtime.getSnapshot().snapshot.tree?.component?.propertyEdits?.['title'];
    const second = runtime.getSnapshot().snapshot.tree?.component?.propertyEdits?.['title'];
    if (!first || !second) throw new Error('Expected current and previous edit metadata.');

    expect(retainedRegistrars[0]?.(candidate)).toBeUndefined();
    expect(retainedRegistrars[1]?.(candidate)).toBeUndefined();
    expect(registryState(bridge).componentPropertyEditTokens.size).toBe(1);
    expect(registryState(bridge).componentPropertyEditPreviousTokens.size).toBe(1);
    for (const metadata of [first, second]) {
      expect(
        runtime.editComponentProperty?.({
          componentId: 'component-id',
          componentToken: metadata.componentToken,
          propertyName: 'title',
          snapshotRevision: metadata.snapshotRevision,
          value: 'after',
        }),
      ).toBeTrue();
    }
    expect(registryTokenCount(bridge)).toBe(0);
    bridge.destroy();
  });

  it('retains exactly one prior accepted revision across a discarded in-flight poll', () => {
    const component = {} as never;
    const node = {} as never;
    const viewModel = { title: 'before' };
    const editDebugComponentProperty = jasmine.createSpy('editDebugComponentProperty').and.returnValue(true);
    renderer.editDebugComponentProperty = editDebugComponentProperty;
    delegate.getDebugSnapshot = jasmine
      .createSpy('getDebugSnapshot')
      .and.callFake((_renderer: IRenderer, _maximum: number, registrar: ComponentPropertyEditRegistrar | undefined) => {
        const metadata = registrar?.({
          component,
          componentId: 'component-id',
          descriptor: Object.getOwnPropertyDescriptor(viewModel, 'title')!,
          node,
          propertyName: 'title',
          viewModel,
          viewModelExtensible: true,
        });
        return {
          tree: {
            children: [],
            component: {
              key: 'root',
              name: 'Root',
              properties: { title: 'before' },
              propertyEdits: { title: metadata },
            },
            id: 'component-id',
            tag: 'Root',
          },
          viewport: { height: 1, width: 1 },
        };
      });
    const bridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);
    const runtime = fakeWindow.__VALDI_WEB_DEBUGGER__!;
    const first = runtime.getSnapshot().snapshot.tree?.component?.propertyEdits?.['title'];
    const second = runtime.getSnapshot().snapshot.tree?.component?.propertyEdits?.['title'];
    if (!first || !second) throw new Error('Expected two accepted edit revisions.');

    expect(registryState(bridge).componentPropertyEditTokens.size).toBe(1);
    expect(registryState(bridge).componentPropertyEditPreviousTokens.size).toBe(1);
    expect(
      runtime.editComponentProperty?.({
        componentId: 'component-id',
        componentToken: first.componentToken,
        propertyName: 'title',
        snapshotRevision: first.snapshotRevision,
        value: 'visible-snapshot-edit',
      }),
    ).toBeTrue();

    const third = runtime.getSnapshot().snapshot.tree?.component?.propertyEdits?.['title'];
    const fourth = runtime.getSnapshot().snapshot.tree?.component?.propertyEdits?.['title'];
    if (!third || !fourth) throw new Error('Expected bounded replacement edit revisions.');
    expect(registryTokenCount(bridge)).toBe(2);
    expect(() =>
      runtime.editComponentProperty?.({
        componentId: 'component-id',
        componentToken: second.componentToken,
        propertyName: 'title',
        snapshotRevision: second.snapshotRevision,
        value: 'too-old',
      }),
    ).toThrowError('The component property edit is stale or invalid.');
    expect(
      runtime.editComponentProperty?.({
        componentId: 'component-id',
        componentToken: third.componentToken,
        propertyName: 'title',
        snapshotRevision: third.snapshotRevision,
        value: 'previous',
      }),
    ).toBeTrue();
    expect(
      runtime.editComponentProperty?.({
        componentId: 'component-id',
        componentToken: fourth.componentToken,
        propertyName: 'title',
        snapshotRevision: fourth.snapshotRevision,
        value: 'current',
      }),
    ).toBeTrue();
    expect(registryTokenCount(bridge)).toBe(0);
    bridge.destroy();
  });

  it('keeps a nested newer snapshot registry instead of adopting the superseded outer capture', () => {
    const outerComponent = {} as never;
    const innerComponent = {} as never;
    const outerNode = {} as never;
    const innerNode = {} as never;
    const outerViewModel = { title: 'outer' };
    const innerViewModel = { title: 'inner' };
    const editDebugComponentProperty = jasmine.createSpy('editDebugComponentProperty').and.returnValue(true);
    renderer.editDebugComponentProperty = editDebugComponentProperty;
    let nested = false;
    let runtime: StandaloneWebDebuggerRuntime;
    let innerSnapshot: StandaloneWebDebuggerSnapshot | undefined;
    delegate.getDebugSnapshot = jasmine
      .createSpy('getDebugSnapshot')
      .and.callFake((_renderer: IRenderer, _maximum: number, registrar: ComponentPropertyEditRegistrar | undefined) => {
        const component = nested ? innerComponent : outerComponent;
        const node = nested ? innerNode : outerNode;
        const viewModel = nested ? innerViewModel : outerViewModel;
        const title = nested ? 'inner' : 'outer';
        const propertyEdit = registrar?.({
          component,
          componentId: `component-${title}`,
          descriptor: Object.getOwnPropertyDescriptor(viewModel, 'title')!,
          node,
          propertyName: 'title',
          viewModel,
          viewModelExtensible: true,
        });
        if (!nested) {
          nested = true;
          innerSnapshot = runtime.getSnapshot();
          nested = false;
        }
        return {
          tree: {
            children: [],
            component: {
              key: title,
              name: 'Root',
              properties: { title },
              ...(propertyEdit === undefined ? {} : { propertyEdits: { title: propertyEdit } }),
            },
            id: `component-${title}`,
            tag: 'Root',
          },
          viewport: { width: 1, height: 1 },
        };
      });
    const bridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);
    runtime = fakeWindow.__VALDI_WEB_DEBUGGER__!;

    const outerSnapshot = runtime.getSnapshot();
    const innerMetadata = innerSnapshot?.snapshot.tree?.component?.propertyEdits?.['title'];

    expect(outerSnapshot.componentPropertyEditingAvailable).toBeFalse();
    expect(outerSnapshot.snapshot.tree?.component?.propertyEdits).toBeUndefined();
    expect(innerSnapshot?.componentPropertyEditingAvailable).toBeTrue();
    expect(registryState(bridge).componentPropertyEditTokens.size).toBe(1);
    if (!innerMetadata) throw new Error('Expected nested snapshot edit metadata.');
    expect(
      runtime.editComponentProperty?.({
        componentId: 'component-inner',
        componentToken: innerMetadata.componentToken,
        propertyName: 'title',
        snapshotRevision: innerMetadata.snapshotRevision,
        value: 'inner-after',
      }),
    ).toBeTrue();
    expect(editDebugComponentProperty).toHaveBeenCalledWith(
      jasmine.objectContaining({
        component: innerComponent,
        expectedViewModel: innerViewModel,
        node: innerNode,
      }),
    );
    bridge.destroy();
  });

  it('invalidates a local capture when the bridge is destroyed reentrantly during snapshotting', () => {
    const component = {} as never;
    const node = {} as never;
    const viewModel = { title: 'before' };
    renderer.editDebugComponentProperty = jasmine.createSpy('editDebugComponentProperty').and.returnValue(true);
    let bridge: WebDebuggerBridge;
    let capturedMetadata: WebRendererDebugPropertyEditMetadata | undefined;
    let retainedSnapshot: WebRendererDebugSnapshot | undefined;
    delegate.getDebugSnapshot = jasmine
      .createSpy('getDebugSnapshot')
      .and.callFake((_renderer: IRenderer, _maximum: number, registrar: ComponentPropertyEditRegistrar | undefined) => {
        capturedMetadata = registrar?.({
          component,
          componentId: 'component-id',
          descriptor: Object.getOwnPropertyDescriptor(viewModel, 'title')!,
          node,
          propertyName: 'title',
          viewModel,
          viewModelExtensible: true,
        });
        retainedSnapshot = {
          tree: {
            children: [],
            component: {
              key: 'root',
              name: 'Root',
              properties: { title: 'before' },
              ...(capturedMetadata === undefined ? {} : { propertyEdits: { title: capturedMetadata } }),
            },
            id: 'component-id',
            tag: 'Root',
          },
          viewport: { width: 1, height: 1 },
        };
        bridge.destroy();
        return retainedSnapshot;
      });
    bridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);
    const runtime = fakeWindow.__VALDI_WEB_DEBUGGER__!;

    expect(() => runtime.getSnapshot()).toThrowError('Web debugger runtime has been destroyed.');
    expect(fakeWindow.__VALDI_WEB_DEBUGGER__).toBeUndefined();
    expect(registryState(bridge).componentPropertyEditTokens.size).toBe(0);
    expect(registryState(bridge).componentPropertyEditPreviousTokens.size).toBe(0);
    expect(registryState(bridge).componentPropertyEditExpiryTimer).toBeUndefined();
    expect(retainedSnapshot?.tree?.component?.propertyEdits).toBeUndefined();
    if (!capturedMetadata) throw new Error('Expected the disposed capture to issue metadata before destruction.');
    expect(() =>
      runtime.editComponentProperty?.({
        componentId: 'component-id',
        componentToken: capturedMetadata?.componentToken,
        propertyName: 'title',
        snapshotRevision: capturedMetadata?.snapshotRevision,
        value: 'after',
      }),
    ).toThrowError('The component property edit is stale or invalid.');
  });

  it('consumes an authorized token before a renderer-side failure and expires current tokens', () => {
    jasmine.clock().install();
    jasmine.clock().mockDate(new Date(0));
    try {
      const component = {} as never;
      const node = {} as never;
      const viewModel = { enabled: true };
      const editDebugComponentProperty = jasmine.createSpy('editDebugComponentProperty').and.returnValue(false);
      renderer.editDebugComponentProperty = editDebugComponentProperty;
      delegate.getDebugSnapshot = jasmine
        .createSpy('getDebugSnapshot')
        .and.callFake(
          (_renderer: IRenderer, _maximum: number, registrar: ComponentPropertyEditRegistrar | undefined) => {
            const propertyEdit = registrar?.({
              component,
              componentId: 'component-id',
              descriptor: Object.getOwnPropertyDescriptor(viewModel, 'enabled')!,
              node,
              propertyName: 'enabled',
              viewModel,
              viewModelExtensible: true,
            });
            return {
              tree: {
                children: [],
                component: {
                  key: 'root',
                  name: 'Root',
                  properties: { enabled: true },
                  propertyEdits: { enabled: propertyEdit },
                },
                id: 'component-id',
                tag: 'Root',
              },
              viewport: { width: 1, height: 1 },
            };
          },
        );
      const bridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);
      const runtime = fakeWindow.__VALDI_WEB_DEBUGGER__!;
      const metadata = runtime.getSnapshot().snapshot.tree?.component?.propertyEdits?.['enabled'];
      if (!metadata) throw new Error('Expected property edit metadata.');
      const request = {
        componentId: 'component-id',
        componentToken: metadata.componentToken,
        propertyName: 'enabled',
        snapshotRevision: metadata.snapshotRevision,
        value: false,
      };
      expect(() => runtime.editComponentProperty?.(request)).toThrowError(
        'The component property edit is stale or invalid.',
      );
      editDebugComponentProperty.and.returnValue(true);
      expect(() => runtime.editComponentProperty?.(request)).toThrowError(
        'The component property edit is stale or invalid.',
      );
      expect(editDebugComponentProperty.calls.count()).toBe(1);

      const throwingMetadata = runtime.getSnapshot().snapshot.tree?.component?.propertyEdits?.['enabled'];
      if (!throwingMetadata) throw new Error('Expected replacement property edit metadata.');
      editDebugComponentProperty.and.throwError('sensitive renderer identity');
      const throwingRequest = {
        ...request,
        componentToken: throwingMetadata.componentToken,
        snapshotRevision: throwingMetadata.snapshotRevision,
      };
      expect(() => runtime.editComponentProperty?.(throwingRequest)).toThrowError(
        'The component property edit is stale or invalid.',
      );
      editDebugComponentProperty.and.returnValue(true);
      expect(() => runtime.editComponentProperty?.(throwingRequest)).toThrowError(
        'The component property edit is stale or invalid.',
      );
      expect(editDebugComponentProperty.calls.count()).toBe(2);

      const replacedMetadata = runtime.getSnapshot().snapshot.tree?.component?.propertyEdits?.['enabled'];
      if (!replacedMetadata) throw new Error('Expected replacement property edit metadata.');
      expect(registryState(bridge).componentPropertyEditTokens.size).toBe(1);
      const replacedTimer = registryState(bridge).componentPropertyEditExpiryTimer;
      expect(replacedTimer).toBeDefined();
      jasmine.clock().tick(1);
      const expiringMetadata = runtime.getSnapshot().snapshot.tree?.component?.propertyEdits?.['enabled'];
      if (!expiringMetadata) throw new Error('Expected expiring property edit metadata.');
      expect(expiringMetadata.componentToken).not.toBe(replacedMetadata.componentToken);
      expect(registryState(bridge).componentPropertyEditExpiryTimer).not.toBe(replacedTimer);
      jasmine.clock().tick(119_999);
      expect(registryState(bridge).componentPropertyEditTokens.size).toBe(1);
      expect(registryState(bridge).componentPropertyEditPreviousTokens.size).toBe(0);
      jasmine.clock().tick(1);
      expect(registryState(bridge).componentPropertyEditTokens.size).toBe(0);
      expect(registryState(bridge).componentPropertyEditPreviousTokens.size).toBe(0);
      expect(registryState(bridge).componentPropertyEditExpiryTimer).toBeUndefined();
      expect(() =>
        runtime.editComponentProperty?.({
          ...request,
          componentToken: expiringMetadata.componentToken,
          snapshotRevision: expiringMetadata.snapshotRevision,
        }),
      ).toThrowError('The component property edit is stale or invalid.');
      expect(runtime.getSnapshot().snapshot.tree?.component?.propertyEdits?.['enabled']).toBeDefined();
      expect(registryState(bridge).componentPropertyEditTokens.size).toBe(1);
      expect(registryState(bridge).componentPropertyEditExpiryTimer).toBeDefined();
      bridge.destroy();
      expect(registryState(bridge).componentPropertyEditTokens.size).toBe(0);
      expect(registryState(bridge).componentPropertyEditExpiryTimer).toBeUndefined();
    } finally {
      jasmine.clock().uninstall();
    }
  });

  it('limits each accepted revision to one thousand tokens and retains at most two thousand total', () => {
    const component = {} as never;
    const node = {} as never;
    const viewModel = Object.create(null) as Record<string, unknown>;
    const properties = Object.create(null) as Record<string, unknown>;
    for (let index = 0; index <= 1_000; index++) {
      const propertyName = `property${index}`;
      Object.defineProperty(viewModel, propertyName, {
        configurable: true,
        enumerable: true,
        value: index,
        writable: true,
      });
      Object.defineProperty(properties, propertyName, {
        configurable: true,
        enumerable: true,
        value: index,
        writable: true,
      });
    }
    renderer.editDebugComponentProperty = jasmine.createSpy('editDebugComponentProperty').and.returnValue(true);
    delegate.getDebugSnapshot = jasmine
      .createSpy('getDebugSnapshot')
      .and.callFake((_renderer: IRenderer, _maximum: number, registrar: ComponentPropertyEditRegistrar | undefined) => {
        const propertyEdits = Object.create(null) as Record<string, WebRendererDebugPropertyEditMetadata>;
        for (const propertyName of Object.keys(properties)) {
          const metadata = registrar?.({
            component,
            componentId: 'component-id',
            descriptor: Object.getOwnPropertyDescriptor(viewModel, propertyName)!,
            node,
            propertyName,
            viewModel,
            viewModelExtensible: true,
          });
          if (metadata !== undefined) propertyEdits[propertyName] = metadata;
        }
        return {
          tree: {
            children: [],
            component: {
              key: 'root',
              name: 'Root',
              properties,
              propertyEdits,
            },
            id: 'component-id',
            tag: 'Root',
          },
          viewport: { width: 1, height: 1 },
        };
      });
    const bridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);

    const runtime = fakeWindow.__VALDI_WEB_DEBUGGER__!;
    const snapshot = runtime.getSnapshot();
    const propertyEdits = snapshot.snapshot.tree?.component?.propertyEdits ?? {};

    expect(snapshot.componentPropertyEditingAvailable).toBeTrue();
    expect(Object.keys(propertyEdits).length).toBe(1_000);
    expect(propertyEdits['property999']?.componentToken).toMatch(/^[0-9a-f]{32}$/);
    expect(propertyEdits['property1000']).toBeUndefined();
    runtime.getSnapshot();
    expect(registryState(bridge).componentPropertyEditTokens.size).toBe(1_000);
    expect(registryState(bridge).componentPropertyEditPreviousTokens.size).toBe(1_000);
    runtime.getSnapshot();
    expect(registryTokenCount(bridge)).toBe(2_000);
    bridge.destroy();
  });

  it('downgrades to read-only and clears both retained revisions when secure randomness fails', () => {
    const component = {} as never;
    const node = {} as never;
    const viewModel = { title: 'safe' };
    renderer.editDebugComponentProperty = jasmine.createSpy('editDebugComponentProperty').and.returnValue(true);
    delegate.getDebugSnapshot = jasmine
      .createSpy('getDebugSnapshot')
      .and.callFake((_renderer: IRenderer, _maximum: number, registrar: ComponentPropertyEditRegistrar | undefined) => {
        const metadata = registrar?.({
          component,
          componentId: 'component-id',
          descriptor: Object.getOwnPropertyDescriptor(viewModel, 'title')!,
          node,
          propertyName: 'title',
          viewModel,
          viewModelExtensible: true,
        });
        return {
          tree: {
            children: [],
            component: {
              key: 'root',
              name: 'Root',
              properties: { title: 'safe' },
              ...(metadata === undefined ? {} : { propertyEdits: { title: metadata } }),
            },
            id: 'component-id',
            tag: 'Root',
          },
          viewport: { width: 1, height: 1 },
        };
      });
    const bridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);
    const runtime = fakeWindow.__VALDI_WEB_DEBUGGER__!;
    const firstMetadata = runtime.getSnapshot().snapshot.tree?.component?.propertyEdits?.['title'];
    if (!firstMetadata) throw new Error('Expected an initial secure token.');
    Object.defineProperty(globalThis, 'crypto', {
      configurable: true,
      value: {
        getRandomValues: () => {
          throw new Error('unavailable');
        },
      },
    });

    const snapshot = runtime.getSnapshot();

    expect(snapshot.componentPropertyEditingAvailable).toBeFalse();
    expect(snapshot.snapshot.tree?.component?.properties).toEqual({ title: 'safe' });
    expect(snapshot.snapshot.tree?.component?.propertyEdits).toBeUndefined();
    expect(registryTokenCount(bridge)).toBe(0);
    expect(() =>
      runtime.editComponentProperty?.({
        componentId: 'component-id',
        componentToken: firstMetadata.componentToken,
        propertyName: 'title',
        snapshotRevision: firstMetadata.snapshotRevision,
        value: 'after',
      }),
    ).toThrowError('The component property edit is stale or invalid.');
    bridge.destroy();
  });

  it('supports the exact top-level Chromium DevTools CLI flag contract', () => {
    fakeWindow.location.href = 'http://127.0.0.1:54321/?valdiDebugger=1&valdiDevTools=1';
    fakeWindow.location.search = '?valdiDebugger=1&valdiDevTools=1';
    const bridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);

    expect(fakeWindow.__VALDI_WEB_DEBUGGER__?.getSnapshot()).toEqual(
      jasmine.objectContaining({ channel: 'valdi-web-debugger' }),
    );

    bridge.destroy();
    expect(fakeWindow.__VALDI_WEB_DEBUGGER__).toBeUndefined();
  });

  it('does not expose standalone inspection without both debugger and host opt-ins', () => {
    for (const search of ['?valdiDebugger=1', '?valdiOwlDebugger=1', '?valdiDevTools=1']) {
      fakeWindow.location.search = search;
      const bridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);
      expect(fakeWindow.__VALDI_WEB_DEBUGGER__).toBeUndefined();
      bridge.destroy();
    }
    expect(fakeWindow.addEventListener).not.toHaveBeenCalled();
  });

  it('disables debugger exposure and messaging entirely in embedded frames', () => {
    const parent = { postMessage: jasmine.createSpy('postMessage') };
    fakeWindow.parent = parent;
    fakeWindow.location.search = '?valdiDebugger=1&valdiDevTools=1';

    const bridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);

    expect(fakeWindow.__VALDI_WEB_DEBUGGER__).toBeUndefined();
    expect(fakeWindow.addEventListener).not.toHaveBeenCalled();
    expect(parent.postMessage).not.toHaveBeenCalled();
    bridge.destroy();
    expect(fakeWindow.removeEventListener).not.toHaveBeenCalled();
  });

  it('highlights only safe legacy renderer node ids through the standalone runtime', () => {
    const htmlElement = new FakeElement();
    const getDebugNode = jasmine
      .createSpy('getDebugNode')
      .and.callFake((id: number) =>
        id === 9 ? ({ htmlElement: htmlElement as unknown as HTMLElement, type: 'label' } as const) : undefined,
      );
    delegate.getDebugNode = getDebugNode;
    const bridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);
    const runtime = fakeWindow.__VALDI_WEB_DEBUGGER__!;

    for (const invalidNodeId of ['', ' 9 ', '+9', '-1', '1.5', '1e1', '9007199254740992']) {
      expect(runtime.highlightNode?.(invalidNodeId)).toBeFalse();
    }
    expect(getDebugNode).not.toHaveBeenCalled();
    expect(appendedOverlays.length).toBe(0);

    expect(runtime.highlightNode?.('9')).toBeTrue();
    expect(getDebugNode).toHaveBeenCalledWith(9);
    expect(getDebugNode.calls.count()).toBe(1);
    expect(appendedOverlays.length).toBe(1);
    expect(appendedOverlays[0].dataset['valdiDebuggerOverlay']).toBe('9');
    expect(appendedOverlays[0].children[0].textContent).toBe('label · 120 × 44');
    expect(runtime.highlightNode?.('missing')).toBeFalse();
    expect(runtime.clearHighlight?.()).toBeTrue();
    expect(appendedOverlays[0].removed).toBeTrue();
    expect(runtime.clearHighlight?.()).toBeFalse();
    bridge.destroy();
  });

  it('makes retained runtime handles inert after bridge destruction', () => {
    const htmlElement = new FakeElement();
    const getDebugNode = jasmine.createSpy('getDebugNode').and.returnValue({
      htmlElement: htmlElement as unknown as HTMLElement,
      type: 'label',
    });
    const getDebugSnapshot = jasmine.createSpy('getDebugSnapshot').and.returnValue({
      tree: null,
      viewport: { width: 640, height: 480 },
    });
    delegate.getDebugNode = getDebugNode;
    delegate.getDebugSnapshot = getDebugSnapshot;
    const bridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);
    const retainedRuntime = fakeWindow.__VALDI_WEB_DEBUGGER__!;
    expect(retainedRuntime.highlightNode?.('9')).toBeTrue();

    bridge.destroy();

    expect(appendedOverlays[0].removed).toBeTrue();
    expect(() => retainedRuntime.getSnapshot()).toThrowError('Web debugger runtime has been destroyed.');
    expect(retainedRuntime.highlightNode?.('9')).toBeFalse();
    expect(retainedRuntime.clearHighlight?.()).toBeFalse();
    expect(getDebugSnapshot).not.toHaveBeenCalled();
    expect(getDebugNode.calls.count()).toBe(1);
    expect(appendedOverlays.length).toBe(1);
  });

  it('restores prior ownership and leaves newer runtimes intact', () => {
    const previousRuntime = makeRuntime('Previous renderer');
    fakeWindow.__VALDI_WEB_DEBUGGER__ = previousRuntime;
    const bridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);
    expect(fakeWindow.__VALDI_WEB_DEBUGGER__).not.toBe(previousRuntime);

    bridge.destroy();

    expect(fakeWindow.__VALDI_WEB_DEBUGGER__).toBe(previousRuntime);

    const secondBridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);
    const replacement = makeRuntime('Replacement renderer');
    fakeWindow.__VALDI_WEB_DEBUGGER__ = replacement;
    secondBridge.destroy();
    expect(fakeWindow.__VALDI_WEB_DEBUGGER__).toBe(replacement);
  });

  it('does not restore a destroyed bridge runtime when nested bridges tear down out of order', () => {
    const firstBridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);
    const firstRuntime = fakeWindow.__VALDI_WEB_DEBUGGER__;
    const secondBridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);
    const secondRuntime = fakeWindow.__VALDI_WEB_DEBUGGER__;

    expect(firstRuntime).toBeDefined();
    expect(secondRuntime).toBeDefined();
    expect(secondRuntime).not.toBe(firstRuntime);

    firstBridge.destroy();
    expect(fakeWindow.__VALDI_WEB_DEBUGGER__).toBe(secondRuntime);
    expect(() => firstRuntime!.getSnapshot()).toThrowError('Web debugger runtime has been destroyed.');

    secondBridge.destroy();
    expect(fakeWindow.__VALDI_WEB_DEBUGGER__).toBeUndefined();
  });

  function makeRuntime(title: string): StandaloneWebDebuggerRuntime {
    return {
      getSnapshot: () => ({
        channel: 'valdi-web-debugger',
        componentPropertyEditingAvailable: true,
        source: { title, url: fakeWindow.location.href },
        snapshot: { tree: null, viewport: { width: 1, height: 1 } },
        type: 'snapshot',
      }),
    };
  }
});

function restoreGlobal(name: string, value: unknown): void {
  if (value === undefined) {
    delete (globalThis as Record<string, unknown>)[name];
  } else {
    (globalThis as Record<string, unknown>)[name] = value;
  }
}

function restoreGlobalProperty(name: string, descriptor: PropertyDescriptor | undefined): void {
  delete (globalThis as Record<string, unknown>)[name];
  if (descriptor !== undefined) Object.defineProperty(globalThis, name, descriptor);
}
