import 'jasmine/src/jasmine';
import { TouchEventState } from 'valdi_tsx/src/GestureEvents';
import { ColorPaletteManager } from '../src/core/Palette';
import { ViewNodeTree } from '../src/core/ViewNodeTree';

interface Rect {
  left: number;
  top: number;
  width: number;
  height: number;
}

interface Listener {
  handler: (e: any) => void;
  capture: boolean;
}

// A minimal DOM with capture and bubble dispatch, enough to drive the view element's click
// routing outside a browser. Ported from the v1 WebValdiLayoutTouch spec.
class FakeEvent {
  type: string;
  bubbles: boolean;
  composed: boolean;
  clientX: number;
  clientY: number;
  screenX: number;
  screenY: number;
  button: number;
  detail: number;
  view: unknown;
  timeStamp = 0;
  target: FakeNode | null = null;
  defaultPrevented = false;
  propagationStopped = false;
  immediatePropagationStopped = false;

  constructor(type: string, init: any = {}) {
    this.type = type;
    this.bubbles = !!init.bubbles;
    this.composed = !!init.composed;
    this.clientX = init.clientX ?? 0;
    this.clientY = init.clientY ?? 0;
    this.screenX = init.screenX ?? 0;
    this.screenY = init.screenY ?? 0;
    this.button = init.button ?? 0;
    this.detail = init.detail ?? 0;
    this.view = init.view ?? null;
  }

  preventDefault() {
    this.defaultPrevented = true;
  }

  stopPropagation() {
    this.propagationStopped = true;
  }

  stopImmediatePropagation() {
    this.propagationStopped = true;
    this.immediatePropagationStopped = true;
  }
}

class FakeNode {
  parentNode: FakeNode | null = null;
  childNodes: FakeNode[] = [];
  listeners: { [type: string]: Listener[] } = {};

  insertBefore(child: FakeNode, ref: FakeNode | null | undefined) {
    child.parentNode?.childNodes.splice(child.parentNode.childNodes.indexOf(child), 1);
    const index = ref ? this.childNodes.indexOf(ref) : -1;
    this.childNodes.splice(index >= 0 ? index : this.childNodes.length, 0, child);
    child.parentNode = this;
    return child;
  }

  appendChild(child: FakeNode) {
    return this.insertBefore(child, null);
  }

  removeChild(child: FakeNode) {
    const index = this.childNodes.indexOf(child);
    if (index >= 0) this.childNodes.splice(index, 1);
    child.parentNode = null;
    return child;
  }

  replaceChildren(...children: FakeNode[]) {
    for (const child of this.childNodes.slice()) this.removeChild(child);
    for (const child of children) this.appendChild(child);
  }

  get firstChild() {
    return this.childNodes[0] ?? null;
  }

  querySelector() {
    return null;
  }

  querySelectorAll() {
    return [];
  }

  addEventListener(type: string, handler: (e: any) => void, options?: boolean | { capture?: boolean }) {
    const capture = typeof options === 'boolean' ? options : !!options?.capture;
    (this.listeners[type] = this.listeners[type] || []).push({ handler, capture });
  }

  removeEventListener(type: string, handler: (e: any) => void) {
    this.listeners[type] = (this.listeners[type] || []).filter(l => l.handler !== handler);
  }

  getRootNode(): FakeNode {
    return this.parentNode ? this.parentNode.getRootNode() : this;
  }

  contains(other: FakeNode | null): boolean {
    for (let node = other; node; node = node.parentNode) {
      if (node === this) return true;
    }
    return false;
  }

  dispatchEvent(event: FakeEvent): boolean {
    event.target = this;
    const path: FakeNode[] = [this];
    while (path[path.length - 1].parentNode) path.push(path[path.length - 1].parentNode!);

    const invoke = (node: FakeNode, capture: boolean) => {
      for (const listener of (node.listeners[event.type] || []).slice()) {
        if (event.immediatePropagationStopped) return;
        if (listener.capture === capture) listener.handler(event);
      }
    };

    for (let i = path.length - 1; i > 0 && !event.propagationStopped; i--) invoke(path[i], true);
    if (!event.propagationStopped) invoke(this, true);
    if (!event.propagationStopped) invoke(this, false);
    if (event.bubbles) {
      for (let i = 1; i < path.length && !event.propagationStopped; i++) invoke(path[i], false);
    }
    return !event.defaultPrevented;
  }
}

class FakeStyle {
  [key: string]: any;
  setProperty(name: string, value: string) {
    this[name] = value;
  }
  removeProperty(name: string) {
    delete this[name];
  }
  getPropertyValue(name: string) {
    return this[name] ?? '';
  }
}

class FakeElement extends FakeNode {
  tagName: string;
  style: any = new FakeStyle();
  rect: Rect = { left: 0, top: 0, width: 0, height: 0 };
  offsetWidth = 0;
  offsetHeight = 0;
  attributes: { [name: string]: string } = {};
  classList = { add: () => {}, remove: () => {}, contains: () => false };
  ownerDocument: any;

  constructor(tagName: string) {
    super();
    this.tagName = tagName.toUpperCase();
    (this.childNodes as any).item = (index: number) => this.childNodes[index] ?? null;
    this.ownerDocument = (globalThis as any).document;
  }

  get children() {
    return this.childNodes;
  }

  get parentElement() {
    return this.parentNode;
  }

  cloneNode(_deep?: boolean) {
    const clone = new FakeElement(this.tagName);
    Object.assign(clone.style, this.style);
    clone.attributes = { ...this.attributes };
    return clone;
  }

  remove() {
    this.parentNode?.childNodes.splice(this.parentNode.childNodes.indexOf(this), 1);
    this.parentNode = null;
  }

  getBoundingClientRect() {
    return {
      ...this.rect,
      x: this.rect.left,
      y: this.rect.top,
      right: this.rect.left + this.rect.width,
      bottom: this.rect.top + this.rect.height,
    };
  }

  getAttribute(name: string) {
    return this.attributes[name] ?? null;
  }

  setAttribute(name: string, value: string) {
    this.attributes[name] = String(value);
  }

  removeAttribute(name: string) {
    delete this.attributes[name];
  }

  hasAttribute(name: string) {
    return name in this.attributes;
  }
}

class FakeShadowRoot extends FakeNode {
  host: FakeElement;
  hitElements: FakeNode[] = [];

  constructor(host: FakeElement) {
    super();
    this.host = host;
  }

  elementsFromPoint(_x: number, _y: number): FakeNode[] {
    return this.hitElements;
  }
}

function installDomStubs() {
  const g = globalThis as any;
  g.document = {
    createElement: (tag: string) => new FakeElement(tag),
    createElementNS: (_ns: string, tag: string) => new FakeElement(tag),
    body: { contains: () => false },
    head: new FakeElement('head'),
    addEventListener: () => {},
    removeEventListener: () => {},
    documentElement: new FakeElement('html'),
  };
  g.window = {
    setTimeout,
    clearTimeout,
    devicePixelRatio: 1,
    getComputedStyle: () => ({ getPropertyValue: () => '' }),
    addEventListener: () => {},
    removeEventListener: () => {},
    matchMedia: () => ({ matches: false, addEventListener: () => {}, removeEventListener: () => {} }),
  };
  g.MouseEvent = FakeEvent;
  g.IntersectionObserver = function () {
    return { observe: () => {}, unobserve: () => {}, disconnect: () => {} };
  };
  g.ResizeObserver = function () {
    return { observe: () => {}, unobserve: () => {}, disconnect: () => {} };
  };
  g.requestAnimationFrame = (callback: (time: number) => void) => setTimeout(() => callback(0), 0);
  g.cancelAnimationFrame = (handle: number) => clearTimeout(handle);
}

function uninstallDomStubs() {
  const g = globalThis as any;
  delete g.document;
  delete g.window;
  delete g.MouseEvent;
  delete g.IntersectionObserver;
  delete g.ResizeObserver;
  delete g.requestAnimationFrame;
  delete g.cancelAnimationFrame;
}

function click(target: FakeNode, clientX: number, clientY: number) {
  target.dispatchEvent(new FakeEvent('click', { bubbles: true, composed: true, clientX, clientY }));
}

describe('view element touch routing', () => {
  let tree: ViewNodeTree;
  let nextId = 1;

  beforeEach(() => {
    installDomStubs();
    tree = new ViewNodeTree(new ColorPaletteManager());
    tree.setPostLayoutScheduler(callback => callback());
    nextId = 1;
  });

  afterEach(() => {
    tree.destroy();
    uninstallDomStubs();
  });

  function element(id: number): FakeElement {
    const node = tree.getNode(id);
    if (!node) throw new Error(`Missing test node ${id}`);
    return node.htmlElement as unknown as FakeElement;
  }

  function createView(parentId?: number, index = 0): number {
    const id = nextId++;
    tree.createElement(id, 'view');
    if (parentId !== undefined) {
      tree.moveElement(id, parentId, index);
    }
    return id;
  }

  function set(id: number, name: string, value: unknown) {
    tree.setAttributeOnElement(id, name, value);
    tree.flush();
  }

  // Builds a shadow root whose host is laid out `layoutWidth` wide and drawn `drawnWidth` wide
  // at (hostLeft, hostTop), as a host page does when it scales the preview to fit.
  function makeRoot(hostLeft: number, hostTop: number, layoutWidth: number, drawnWidth: number) {
    const host = new FakeElement('div');
    host.offsetWidth = layoutWidth;
    host.rect = { left: hostLeft, top: hostTop, width: drawnWidth, height: drawnWidth * 2 };
    const shadowRoot = new FakeShadowRoot(host);
    const root = createView();
    tree.makeElementRoot(root, shadowRoot as unknown as ShadowRoot);
    tree.flush();
    return { shadowRoot, root };
  }

  describe('hitTest', () => {
    it('passes the click to the element below when a hitTest function returns false', () => {
      const { shadowRoot, root } = makeRoot(0, 0, 100, 100);
      const button = createView(root, 0);
      const overlay = createView(root, 1);
      const onButtonTap = jasmine.createSpy('onButtonTap');
      const onOverlayTap = jasmine.createSpy('onOverlayTap');
      const hitTest = jasmine.createSpy('hitTest').and.returnValue(false);
      set(button, 'onTap', onButtonTap);
      set(overlay, 'onTap', onOverlayTap);
      set(overlay, 'hitTest', hitTest);
      shadowRoot.hitElements = [element(overlay), element(button), element(root)];

      click(element(overlay), 40, 60);

      expect(hitTest).toHaveBeenCalledTimes(1);
      expect(hitTest.calls.mostRecent().args[0].state).toBe(TouchEventState.Started);
      expect(onOverlayTap).not.toHaveBeenCalled();
      expect(onButtonTap).toHaveBeenCalledTimes(1);
      expect(element(overlay).style.pointerEvents).toBe('auto');
    });

    it('does not loop when overlapping siblings both reject the hit test', () => {
      const { shadowRoot, root } = makeRoot(0, 0, 100, 100);
      const button = createView(root, 0);
      const overlayA = createView(root, 1);
      const overlayB = createView(root, 2);
      const onButtonTap = jasmine.createSpy('onButtonTap');
      const hitTestA = jasmine.createSpy('hitTestA').and.returnValue(false);
      const hitTestB = jasmine.createSpy('hitTestB').and.returnValue(false);
      set(button, 'onTap', onButtonTap);
      set(overlayA, 'hitTest', hitTestA);
      set(overlayB, 'hitTest', hitTestB);
      shadowRoot.hitElements = [element(overlayA), element(overlayB), element(button), element(root)];

      click(element(overlayA), 40, 60);

      expect(hitTestA).toHaveBeenCalledTimes(1);
      expect(hitTestB).toHaveBeenCalledTimes(1);
      expect(onButtonTap).toHaveBeenCalledTimes(1);
    });

    it('delivers exactly one click past the Valdi tree when a hitTest forwards it', () => {
      const { shadowRoot, root } = makeRoot(0, 0, 100, 100);
      const button = createView(root, 0);
      const overlay = createView(root, 1);
      const onButtonTap = jasmine.createSpy('onButtonTap');
      const hostClickListener = jasmine.createSpy('hostClickListener');
      set(button, 'onTap', onButtonTap);
      set(overlay, 'hitTest', () => false);
      shadowRoot.addEventListener('click', hostClickListener);
      shadowRoot.hitElements = [element(overlay), element(button), element(root)];

      click(element(overlay), 40, 60);

      expect(onButtonTap).toHaveBeenCalledTimes(1);
      expect(hostClickListener).toHaveBeenCalledTimes(1);
    });

    it('does not ask the hitTest of a view inside a rejecting view', () => {
      const { shadowRoot, root } = makeRoot(0, 0, 100, 100);
      const button = createView(root, 0);
      const overlay = createView(root, 1);
      const overlayChild = createView(overlay, 0);
      const onButtonTap = jasmine.createSpy('onButtonTap');
      const outerHitTest = jasmine.createSpy('outerHitTest').and.returnValue(false);
      const innerHitTest = jasmine.createSpy('innerHitTest').and.returnValue(true);
      set(button, 'onTap', onButtonTap);
      set(overlay, 'hitTest', outerHitTest);
      set(overlayChild, 'hitTest', innerHitTest);
      shadowRoot.hitElements = [element(overlayChild), element(overlay), element(button)];

      click(element(overlayChild), 10, 10);

      expect(outerHitTest).toHaveBeenCalledTimes(1);
      expect(innerHitTest).not.toHaveBeenCalled();
      expect(onButtonTap).toHaveBeenCalledTimes(1);
    });

    it('does not forward the click when every hitTest accepts', () => {
      const { shadowRoot, root } = makeRoot(0, 0, 100, 100);
      const overlay = createView(root, 0);
      const overlayChild = createView(overlay, 0);
      const onChildTap = jasmine.createSpy('onChildTap');
      const outerHitTest = jasmine.createSpy('outerHitTest').and.returnValue(true);
      const innerHitTest = jasmine.createSpy('innerHitTest').and.returnValue(true);
      const hostClickListener = jasmine.createSpy('hostClickListener');
      set(overlayChild, 'onTap', onChildTap);
      set(overlay, 'hitTest', outerHitTest);
      set(overlayChild, 'hitTest', innerHitTest);
      shadowRoot.addEventListener('click', hostClickListener);
      shadowRoot.hitElements = [element(overlayChild), element(overlay)];

      const event = new FakeEvent('click', { bubbles: true, composed: true, clientX: 10, clientY: 10 });
      element(overlayChild).dispatchEvent(event);

      expect(outerHitTest).toHaveBeenCalledTimes(1);
      expect(innerHitTest).toHaveBeenCalledTimes(1);
      expect(onChildTap).toHaveBeenCalledTimes(1);
      expect(hostClickListener).toHaveBeenCalledTimes(1);
      expect(hostClickListener.calls.mostRecent().args[0]).toBe(event);
    });

    it('skips the children of a view whose hitTest function returns false', () => {
      const { shadowRoot, root } = makeRoot(0, 0, 100, 100);
      const button = createView(root, 0);
      const overlay = createView(root, 1);
      const overlayChild = createView(overlay, 0);
      const onButtonTap = jasmine.createSpy('onButtonTap');
      const onChildTap = jasmine.createSpy('onChildTap');
      set(button, 'onTap', onButtonTap);
      set(overlayChild, 'onTap', onChildTap);
      set(overlay, 'hitTest', () => false);
      shadowRoot.hitElements = [element(overlayChild), element(overlay), element(button), element(root)];

      click(element(overlayChild), 10, 10);

      expect(onChildTap).not.toHaveBeenCalled();
      expect(onButtonTap).toHaveBeenCalledTimes(1);
    });

    it('keeps the click when a hitTest function returns true', () => {
      const { shadowRoot, root } = makeRoot(0, 0, 100, 100);
      const button = createView(root, 0);
      const overlay = createView(root, 1);
      const onButtonTap = jasmine.createSpy('onButtonTap');
      const onOverlayTap = jasmine.createSpy('onOverlayTap');
      set(button, 'onTap', onButtonTap);
      set(overlay, 'onTap', onOverlayTap);
      set(overlay, 'hitTest', () => true);
      shadowRoot.hitElements = [element(overlay), element(button), element(root)];

      click(element(overlay), 40, 60);

      expect(onOverlayTap).toHaveBeenCalledTimes(1);
      expect(onButtonTap).not.toHaveBeenCalled();
    });

    it('keeps the boolean behavior for a non-function hitTest', () => {
      const { root } = makeRoot(0, 0, 100, 100);
      const overlay = createView(root, 0);
      const onOverlayTap = jasmine.createSpy('onOverlayTap');
      set(overlay, 'onTap', onOverlayTap);

      set(overlay, 'hitTest', false);
      expect(element(overlay).style.pointerEvents).toBe('none');

      set(overlay, 'hitTest', true);
      expect(element(overlay).style.pointerEvents).toBe('auto');
      click(element(overlay), 5, 5);
      expect(onOverlayTap).toHaveBeenCalledTimes(1);
    });

    it('stops calling a hitTest function after it is replaced by a boolean', () => {
      const { shadowRoot, root } = makeRoot(0, 0, 100, 100);
      const overlay = createView(root, 0);
      const onOverlayTap = jasmine.createSpy('onOverlayTap');
      const hitTest = jasmine.createSpy('hitTest').and.returnValue(false);
      set(overlay, 'onTap', onOverlayTap);
      set(overlay, 'hitTest', hitTest);
      set(overlay, 'hitTest', true);
      shadowRoot.hitElements = [element(overlay), element(root)];

      click(element(overlay), 5, 5);

      expect(hitTest).not.toHaveBeenCalled();
      expect(onOverlayTap).toHaveBeenCalledTimes(1);
    });

    it('does not change touchEnabled', () => {
      const { root } = makeRoot(0, 0, 100, 100);
      const view = createView(root, 0);
      set(view, 'touchEnabled', false);
      set(view, 'hitTest', () => true);
      expect(element(view).style.pointerEvents).toBe('none');
      set(view, 'touchEnabled', true);
      expect(element(view).style.pointerEvents).toBe('auto');
    });
  });

  describe('onTap propagation', () => {
    it('calls only the deepest view with a tap handler', () => {
      const { root } = makeRoot(0, 0, 100, 100);
      const outer = createView(root, 0);
      const inner = createView(outer, 0);
      const onOuterTap = jasmine.createSpy('onOuterTap');
      const onInnerTap = jasmine.createSpy('onInnerTap');
      set(outer, 'onTap', onOuterTap);
      set(inner, 'onTap', onInnerTap);

      click(element(inner), 5, 5);

      expect(onInnerTap).toHaveBeenCalledTimes(1);
      expect(onOuterTap).not.toHaveBeenCalled();
    });

    it('lets the click reach the parent when the child tap is disabled or rejected', () => {
      const { root } = makeRoot(0, 0, 100, 100);
      const outer = createView(root, 0);
      const inner = createView(outer, 0);
      const onOuterTap = jasmine.createSpy('onOuterTap');
      const onInnerTap = jasmine.createSpy('onInnerTap');
      set(outer, 'onTap', onOuterTap);
      set(inner, 'onTap', onInnerTap);

      set(inner, 'onTapDisabled', true);
      click(element(inner), 5, 5);
      expect(onOuterTap).toHaveBeenCalledTimes(1);

      set(inner, 'onTapDisabled', false);
      set(inner, 'onTapPredicate', () => false);
      click(element(inner), 5, 5);
      expect(onOuterTap).toHaveBeenCalledTimes(2);
      expect(onInnerTap).not.toHaveBeenCalled();
    });

    it('keeps bubbling the click past the Valdi tree after a tap handler claims it', () => {
      const { shadowRoot, root } = makeRoot(0, 0, 100, 100);
      const outer = createView(root, 0);
      const inner = createView(outer, 0);
      const onInnerTap = jasmine.createSpy('onInnerTap');
      const hostClickListener = jasmine.createSpy('hostClickListener');
      set(inner, 'onTap', onInnerTap);
      shadowRoot.addEventListener('click', hostClickListener);

      click(element(inner), 5, 5);

      expect(onInnerTap).toHaveBeenCalledTimes(1);
      expect(hostClickListener).toHaveBeenCalledTimes(1);
    });

    it('lets the click reach the parent when the child has no tap handler', () => {
      const { root } = makeRoot(0, 0, 100, 100);
      const outer = createView(root, 0);
      const inner = createView(outer, 0);
      const onOuterTap = jasmine.createSpy('onOuterTap');
      set(outer, 'onTap', onOuterTap);

      click(element(inner), 5, 5);

      expect(onOuterTap).toHaveBeenCalledTimes(1);
    });
  });

  describe('touch coordinates', () => {
    it('divides by the CSS scale of the root host', () => {
      // The host is laid out 375 points wide and drawn 750 pixels wide at (100, 50).
      const { root } = makeRoot(100, 50, 375, 750);
      const view = createView(root, 0);
      element(view).rect = { left: 300, top: 250, width: 200, height: 100 };
      const onTap = jasmine.createSpy('onTap');
      set(view, 'onTap', onTap);

      click(element(view), 340, 270);

      const touchEvent = onTap.calls.mostRecent().args[0];
      expect(touchEvent.state).toBe(TouchEventState.Ended);
      expect(touchEvent.x).toBe(20);
      expect(touchEvent.y).toBe(10);
      expect(touchEvent.absoluteX).toBe(120);
      expect(touchEvent.absoluteY).toBe(110);
    });

    it('keeps viewport coordinates when the view is not in a shadow root', () => {
      const view = createView();
      const container = new FakeElement('div');
      tree.makeElementRoot(view, container as unknown as HTMLElement);
      tree.flush();
      element(view).rect = { left: 30, top: 40, width: 100, height: 100 };
      const onTap = jasmine.createSpy('onTap');
      set(view, 'onTap', onTap);

      click(element(view), 35, 50);

      const touchEvent = onTap.calls.mostRecent().args[0];
      expect(touchEvent.x).toBe(5);
      expect(touchEvent.y).toBe(10);
      expect(touchEvent.absoluteX).toBe(35);
      expect(touchEvent.absoluteY).toBe(50);
    });

    it('gives the hitTest function the same Valdi points', () => {
      const { shadowRoot, root } = makeRoot(100, 50, 375, 750);
      const overlay = createView(root, 0);
      element(overlay).rect = { left: 100, top: 50, width: 750, height: 1500 };
      const hitTest = jasmine.createSpy('hitTest').and.returnValue(true);
      set(overlay, 'hitTest', hitTest);
      shadowRoot.hitElements = [element(overlay)];

      click(element(overlay), 300, 450);

      const touchEvent = hitTest.calls.mostRecent().args[0];
      expect(touchEvent.x).toBe(100);
      expect(touchEvent.y).toBe(200);
      expect(touchEvent.absoluteX).toBe(100);
      expect(touchEvent.absoluteY).toBe(200);
    });
  });
});
