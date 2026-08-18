import 'jasmine/src/jasmine';
import type { ComponentPrototype } from 'valdi_core/src/ComponentPrototype';
import type { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import type { INavigatorPageConfig, INavigatorPageVisibility } from 'valdi_navigation/src/INavigator';
import { WebNavigationHost, type WebNavigationRenderer, type WebNavigator } from '../src/navigation/WebNavigator';

interface FakeNavigationEvent {
  readonly target?: FakeNavigationElement;
  readonly key?: string;
  defaultPrevented: boolean;
  preventDefault(): void;
}

class FakeNavigationElement {
  readonly attributes: Record<string, string> = {};
  readonly children: FakeNavigationElement[] = [];
  readonly style: Record<string, string> = {};
  readonly listeners = new Map<string, Array<(event: FakeNavigationEvent) => void>>();
  readonly animate = jasmine.createSpy('animate');
  className = '';
  open = false;
  parentNode: FakeNavigationElement | null = null;
  textContent = '';

  constructor(readonly tagName: string) {}

  appendChild(child: FakeNavigationElement): FakeNavigationElement {
    child.parentNode = this;
    this.children.push(child);
    return child;
  }

  remove(): void {
    const parent = this.parentNode;
    if (!parent) {
      return;
    }
    const index = parent.children.indexOf(this);
    if (index >= 0) {
      parent.children.splice(index, 1);
    }
    this.parentNode = null;
  }

  setAttribute(name: string, value: string): void {
    this.attributes[name] = value;
    if (name === 'open') {
      this.open = true;
    }
  }

  addEventListener(name: string, listener: (event: FakeNavigationEvent) => void): void {
    const listeners = this.listeners.get(name) ?? [];
    listeners.push(listener);
    this.listeners.set(name, listeners);
  }

  removeEventListener(name: string, listener: (event: FakeNavigationEvent) => void): void {
    const listeners = this.listeners.get(name);
    if (!listeners) {
      return;
    }
    const index = listeners.indexOf(listener);
    if (index >= 0) {
      listeners.splice(index, 1);
    }
  }

  dispatch(name: string, target: FakeNavigationElement | undefined): FakeNavigationEvent {
    const event: FakeNavigationEvent = {
      defaultPrevented: false,
      target,
      preventDefault() {
        this.defaultPrevented = true;
      },
    };
    for (const listener of this.listeners.get(name) ?? []) {
      listener(event);
    }
    return event;
  }

  showModal(): void {
    this.open = true;
  }

  close(): void {
    this.open = false;
  }
}

class FakeNavigationRenderer implements WebNavigationRenderer {
  readonly clear = jasmine.createSpy('clear');
  readonly destroy = jasmine.createSpy('destroy');
  component?: ComponentConstructor<IComponent>;
  context?: { navigator: WebNavigator; source?: string };
  viewModel?: unknown;

  renderRootComponent(
    constructor: ComponentConstructor<IComponent>,
    _prototype: ComponentPrototype,
    viewModel: unknown,
    context: unknown,
  ): void {
    this.component = constructor;
    this.context = context as { navigator: WebNavigator; source?: string };
    this.viewModel = viewModel;
  }

  onDestroy(): void {
    this.destroy();
  }

  renderRoot(render: () => void): void {
    render();
    this.clear();
  }
}

function makePage(path: string, partiallyHiding: boolean): INavigatorPageConfig {
  return {
    componentContext: { source: path },
    componentPath: `${path}@test/src/${path}`,
    componentViewModel: { title: path },
    isPartiallyHiding: partiallyHiding,
  };
}

function fakeConstructor(): ComponentConstructor<IComponent> {
  return function FakeComponent() {} as unknown as ComponentConstructor<IComponent>;
}

describe('WebNavigator', () => {
  let previousDocument: unknown;
  let documentListeners: Map<string, Array<(event: FakeNavigationEvent) => void>>;
  let overlayRoot: FakeNavigationElement;
  let root: FakeNavigationElement;
  let renderers: FakeNavigationRenderer[];
  let resolvedPaths: string[];
  let host: WebNavigationHost;

  beforeEach(() => {
    previousDocument = (globalThis as { document?: unknown }).document;
    documentListeners = new Map();
    (globalThis as { document?: unknown }).document = {
      addEventListener(name: string, listener: (event: FakeNavigationEvent) => void): void {
        const listeners = documentListeners.get(name) ?? [];
        listeners.push(listener);
        documentListeners.set(name, listeners);
      },
      createElement(tagName: string): FakeNavigationElement {
        return new FakeNavigationElement(tagName);
      },
      removeEventListener(name: string, listener: (event: FakeNavigationEvent) => void): void {
        const listeners = documentListeners.get(name);
        if (!listeners) {
          return;
        }
        const index = listeners.indexOf(listener);
        if (index >= 0) {
          listeners.splice(index, 1);
        }
      },
    };
    overlayRoot = new FakeNavigationElement('shadow-root');
    root = new FakeNavigationElement('root');
    overlayRoot.appendChild(root);
    renderers = [];
    resolvedPaths = [];
    host = new WebNavigationHost({
      createRenderer: () => {
        const renderer = new FakeNavigationRenderer();
        renderers.push(renderer);
        return renderer;
      },
      resolveComponent: path => {
        resolvedPaths.push(path);
        return fakeConstructor();
      },
      root: root as unknown as HTMLElement,
    });
  });

  afterEach(() => {
    host.destroy();
    if (previousDocument === undefined) {
      delete (globalThis as { document?: unknown }).document;
    } else {
      (globalThis as { document?: unknown }).document = previousDocument;
    }
  });

  it('pushes a page into its own renderer and injects a page-scoped navigator', () => {
    const page = makePage('Child', false);
    const visibility: INavigatorPageVisibility[] = [];
    host.rootNavigator.setPageVisibilityObserver(value => visibility.push(value));

    host.rootNavigator.pushComponent(page, false);

    expect(resolvedPaths).toEqual(['Child@test/src/Child']);
    expect(renderers).toHaveSize(1);
    expect(renderers[0].viewModel).toBe(page.componentViewModel);
    expect(renderers[0].context?.source).toBe('Child');
    expect(renderers[0].context?.navigator).not.toBe(host.rootNavigator);
    expect(renderers[0].context?.navigator.__shouldDisableMakeOpaque).toBeTrue();
    expect(root.style.display).toBe('none');
    expect(visibility).toEqual([1, 0]);

    renderers[0].context?.navigator.pop(false);

    expect(renderers[0].destroy).toHaveBeenCalledTimes(1);
    expect(renderers[0].clear).toHaveBeenCalledBefore(renderers[0].destroy);
    expect(root.style.display).toBe('');
    expect(visibility).toEqual([1, 0, 1]);
  });

  it('unwinds to the owning page and to the current stack root', () => {
    host.rootNavigator.pushComponent(makePage('First', false), false);
    const first = renderers[0].context?.navigator;
    first?.pushComponent(makePage('Second', false), false);
    const second = renderers[1].context?.navigator;
    second?.pushComponent(makePage('Third', false), false);

    first?.popToSelf(false);

    expect(renderers[1].destroy).toHaveBeenCalledTimes(1);
    expect(renderers[2].destroy).toHaveBeenCalledTimes(1);
    expect(renderers[0].destroy).not.toHaveBeenCalled();

    first?.pushComponent(makePage('Fourth', false), false);
    renderers[3].context?.navigator.popToRoot(false);

    expect(renderers[0].destroy).toHaveBeenCalledTimes(1);
    expect(renderers[3].destroy).toHaveBeenCalledTimes(1);
    expect(root.style.display).toBe('');
  });

  it('presents a true modal dialog while keeping the originating page in place', () => {
    const page = makePage('Dialog', false);

    host.rootNavigator.presentComponent(page, false);

    const dialog = overlayRoot.children.find(child => child.tagName === 'dialog');
    expect(dialog).toBeDefined();
    expect(dialog?.open).toBeTrue();
    expect(dialog?.attributes['aria-modal']).toBe('true');
    expect(dialog?.className).toBe('valdi-web-navigation-dialog');
    expect(root.style.display).toBe('');
    expect(root.attributes['aria-hidden']).toBe('true');
    expect(renderers[0].context?.source).toBe('Dialog');

    renderers[0].context?.navigator.pushComponent(makePage('Modal child', false), false);
    renderers[1].context?.navigator.dismiss(false);

    expect(renderers[0].destroy).toHaveBeenCalledTimes(1);
    expect(renderers[1].destroy).toHaveBeenCalledTimes(1);
    expect(renderers[0].clear).toHaveBeenCalledTimes(1);
    expect(renderers[1].clear).toHaveBeenCalledTimes(1);
    expect(overlayRoot.children.some(child => child.tagName === 'dialog')).toBeFalse();
    expect(root.attributes['aria-hidden']).toBe('false');
  });

  it('uses a bottom-sheet dialog for partially hiding presentations', () => {
    host.rootNavigator.presentComponent(makePage('Sheet', true), true);

    const dialog = overlayRoot.children.find(child => child.tagName === 'dialog');
    expect(dialog?.className).toBe('valdi-web-navigation-dialog valdi-web-navigation-sheet');
    expect(dialog?.style.height).toBe('min(292px, calc(100vh - 48px))');
    expect(dialog?.animate).toHaveBeenCalled();
    expect(overlayRoot.children.some(child => child.tagName === 'style')).toBeTrue();
  });

  it('routes Escape and backdrop dismissal through page observers and dismissal locks', () => {
    host.rootNavigator.presentComponent(makePage('Dialog', false), false);
    const navigator = renderers[0].context?.navigator;
    const dialog = overlayRoot.children.find(child => child.tagName === 'dialog');
    const observer = jasmine.createSpy('backObserver');
    navigator?.setBackButtonObserver(observer);

    dialog?.dispatch('click', dialog);

    expect(observer).toHaveBeenCalledTimes(1);
    expect(dialog?.open).toBeTrue();

    navigator?.setBackButtonObserver(undefined);
    navigator?.forceDisableDismissalGesture(true);
    const cancellation = dialog?.dispatch('cancel', dialog);

    expect(cancellation?.defaultPrevented).toBeTrue();
    expect(dialog?.open).toBeTrue();

    navigator?.forceDisableDismissalGesture(false);
    dialog?.dispatch('cancel', dialog);

    expect(dialog?.open).toBeFalse();
    expect(renderers[0].destroy).toHaveBeenCalledTimes(1);
  });

  it('dismisses the nearest presented stack and all modals above it', () => {
    host.rootNavigator.presentComponent(makePage('First dialog', false), false);
    const first = renderers[0].context?.navigator;
    first?.presentComponent(makePage('Nested dialog', false), false);

    first?.dismiss(false);

    expect(renderers[0].destroy).toHaveBeenCalledTimes(1);
    expect(renderers[1].destroy).toHaveBeenCalledTimes(1);
    expect(overlayRoot.children.filter(child => child.tagName === 'dialog')).toHaveSize(0);
  });

  it('cancels hidden-page expiry when the page becomes visible again', () => {
    jasmine.clock().install();
    try {
      host.rootNavigator.pushComponent(makePage('First', false), false);
      const first = renderers[0].context?.navigator;
      first?.setOnPausePopAfterDelay(100);
      first?.pushComponent(makePage('Second', false), false);

      jasmine.clock().tick(50);
      renderers[1].context?.navigator.pop(false);
      jasmine.clock().tick(100);

      expect(renderers[0].destroy).not.toHaveBeenCalled();
      expect(renderers[1].destroy).toHaveBeenCalledTimes(1);
    } finally {
      jasmine.clock().uninstall();
    }
  });

  it('expires a hidden page and all pages presented above it', () => {
    jasmine.clock().install();
    try {
      host.rootNavigator.pushComponent(makePage('First', false), false);
      const first = renderers[0].context?.navigator;
      first?.setOnPausePopAfterDelay(100);
      first?.pushComponent(makePage('Second', false), false);

      jasmine.clock().tick(100);

      expect(renderers[0].destroy).toHaveBeenCalledTimes(1);
      expect(renderers[1].destroy).toHaveBeenCalledTimes(1);
      expect(root.style.display).toBe('');
    } finally {
      jasmine.clock().uninstall();
    }
  });

  it('removes its keyboard listener, modal renderers, and scoped style when destroyed', () => {
    host.rootNavigator.presentComponent(makePage('Dialog', false), false);

    host.destroy();

    expect(documentListeners.get('keydown')).toHaveSize(0);
    expect(renderers[0].destroy).toHaveBeenCalledTimes(1);
    expect(overlayRoot.children.map(child => child.tagName)).toEqual(['root']);
  });
});
