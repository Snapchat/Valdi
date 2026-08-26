import 'jasmine/src/jasmine';
import type { IRenderer } from 'valdi_core/src/IRenderer';
import type { ValdiWebRendererDelegate } from '../src/ValdiWebRendererDelegate';
import { MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS } from '../src/ValdiWebRendererDelegate';
import type { StandaloneWebDebuggerRuntime } from '../src/debug/WebDebuggerBridge';
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

describe('WebDebuggerBridge legacy renderer adapter', () => {
  let previousWindow: unknown;
  let previousDocument: unknown;
  let previousElement: unknown;
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
  });

  it('exposes the real top-level Owl renderer only after explicit debugger opt-in', () => {
    const bridge = new WebDebuggerBridge({} as HTMLElement, delegate, renderer);

    expect(fakeWindow.__VALDI_WEB_DEBUGGER__?.getSnapshot()).toEqual({
      channel: 'valdi-web-debugger',
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
    expect(getDebugSnapshot).toHaveBeenCalledWith(renderer, jasmine.any(Number));
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
    const getDebugNode = jasmine.createSpy('getDebugNode').and.callFake((id: number) =>
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
