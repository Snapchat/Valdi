import 'jasmine/src/jasmine';
import { TouchEventState } from 'valdi_tsx/src/GestureEvents';
import { WebValdiLayout } from '../src/views/WebValdiLayout';

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

// A minimal DOM with capture and bubble dispatch, enough to drive WebValdiLayout's click
// handling outside a browser.
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
  target: FakeNode | null = null;
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

  addEventListener(type: string, handler: (e: any) => void, options?: boolean | { capture?: boolean }) {
    const capture = typeof options === 'boolean' ? options : !!options?.capture;
    (this.listeners[type] = this.listeners[type] || []).push({ handler, capture });
  }

  removeEventListener(type: string, handler: (e: any) => void) {
    this.listeners[type] = (this.listeners[type] || []).filter(l => l.handler !== handler);
  }

  getRootNode(): FakeNode {
    let node: FakeNode = this;
    while (node.parentNode) node = node.parentNode;
    return node;
  }

  contains(other: FakeNode | null): boolean {
    for (let node = other; node; node = node.parentNode) {
      if (node === this) return true;
    }
    return false;
  }

  dispatchEvent(event: FakeEvent): boolean {
    event.target = this;
    const path: FakeNode[] = [];
    for (let node: FakeNode | null = this; node; node = node.parentNode) path.push(node);

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
    return true;
  }
}

class FakeElement extends FakeNode {
  tagName: string;
  style: any = { setProperty: () => {}, removeProperty: () => {} };
  rect: Rect = { left: 0, top: 0, width: 0, height: 0 };
  offsetWidth = 0;
  attributes: { [name: string]: string } = {};

  constructor(tagName: string) {
    super();
    this.tagName = tagName.toUpperCase();
    (this.childNodes as any).item = (index: number) => this.childNodes[index];
  }

  get children() {
    return this.childNodes;
  }

  insertBefore(child: FakeNode, ref: FakeNode | null | undefined) {
    child.parentNode?.childNodes.splice(child.parentNode.childNodes.indexOf(child), 1);
    const index = ref ? this.childNodes.indexOf(ref) : -1;
    this.childNodes.splice(index >= 0 ? index : this.childNodes.length, 0, child);
    child.parentNode = this;
  }

  appendChild(child: FakeNode) {
    this.insertBefore(child, null);
  }

  remove() {
    this.parentNode?.childNodes.splice(this.parentNode.childNodes.indexOf(this), 1);
    this.parentNode = null;
  }

  getBoundingClientRect() {
    return { ...this.rect, x: this.rect.left, y: this.rect.top };
  }

  getAttribute(name: string) {
    return this.attributes[name] ?? null;
  }

  setAttribute(name: string, value: string) {
    this.attributes[name] = value;
  }

  removeAttribute(name: string) {
    delete this.attributes[name];
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
  (globalThis as any).document = {
    createElement: (tag: string) => new FakeElement(tag),
    body: { contains: () => false },
  };
  (globalThis as any).window = { setTimeout, clearTimeout };
  (globalThis as any).MouseEvent = FakeEvent;
  (globalThis as any).IntersectionObserver = function () {
    return { observe: () => {}, unobserve: () => {}, disconnect: () => {} };
  };
  (globalThis as any).ResizeObserver = function () {
    return { observe: () => {}, unobserve: () => {}, disconnect: () => {} };
  };
}

function uninstallDomStubs() {
  delete (globalThis as any).document;
  delete (globalThis as any).window;
  delete (globalThis as any).MouseEvent;
  delete (globalThis as any).IntersectionObserver;
  delete (globalThis as any).ResizeObserver;
}

function element(layout: WebValdiLayout): FakeElement {
  return layout.htmlElement as unknown as FakeElement;
}

// Builds a shadow root whose host is laid out `layoutWidth` wide and drawn `drawnWidth` wide
// at (hostLeft, hostTop), as a host page does when it scales the preview to fit.
function makeRoot(hostLeft: number, hostTop: number, layoutWidth: number, drawnWidth: number) {
  const host = new FakeElement('div');
  host.offsetWidth = layoutWidth;
  host.rect = { left: hostLeft, top: hostTop, width: drawnWidth, height: drawnWidth * 2 };
  const shadowRoot = new FakeShadowRoot(host);
  const rootLayout = new WebValdiLayout(1);
  shadowRoot.childNodes.push(element(rootLayout));
  element(rootLayout).parentNode = shadowRoot;
  return { shadowRoot, rootLayout };
}

function click(target: FakeNode, clientX: number, clientY: number) {
  target.dispatchEvent(new FakeEvent('click', { bubbles: true, composed: true, clientX, clientY }));
}

describe('WebValdiLayout – hitTest', () => {
  beforeEach(() => installDomStubs());
  afterEach(() => uninstallDomStubs());

  it('passes the click to the element below when a hitTest function returns false', () => {
    const { shadowRoot, rootLayout } = makeRoot(0, 0, 100, 100);
    const button = new WebValdiLayout(2);
    const overlay = new WebValdiLayout(3);
    button.move(rootLayout, 0);
    overlay.move(rootLayout, 1);

    const onButtonTap = jasmine.createSpy('onButtonTap');
    const onOverlayTap = jasmine.createSpy('onOverlayTap');
    const hitTest = jasmine.createSpy('hitTest').and.returnValue(false);
    button.changeAttribute('onTap', onButtonTap);
    overlay.changeAttribute('onTap', onOverlayTap);
    overlay.changeAttribute('hitTest', hitTest);
    shadowRoot.hitElements = [element(overlay), element(button), element(rootLayout)];

    click(element(overlay), 40, 60);

    expect(hitTest).toHaveBeenCalledTimes(1);
    expect(hitTest.calls.mostRecent().args[0].state).toBe(TouchEventState.Started);
    expect(onOverlayTap).not.toHaveBeenCalled();
    expect(onButtonTap).toHaveBeenCalledTimes(1);
    expect(element(overlay).style.pointerEvents).toBe('auto');
  });

  it('does not loop when overlapping siblings both reject the hit test', () => {
    const { shadowRoot, rootLayout } = makeRoot(0, 0, 100, 100);
    const button = new WebValdiLayout(2);
    const overlayA = new WebValdiLayout(3);
    const overlayB = new WebValdiLayout(4);
    button.move(rootLayout, 0);
    overlayA.move(rootLayout, 1);
    overlayB.move(rootLayout, 2);

    const onButtonTap = jasmine.createSpy('onButtonTap');
    const hitTestA = jasmine.createSpy('hitTestA').and.returnValue(false);
    const hitTestB = jasmine.createSpy('hitTestB').and.returnValue(false);
    button.changeAttribute('onTap', onButtonTap);
    overlayA.changeAttribute('hitTest', hitTestA);
    overlayB.changeAttribute('hitTest', hitTestB);
    // elementsFromPoint keeps reporting both overlays above the button, so before the fix the
    // click bounces between overlayA and overlayB until the call stack overflows.
    shadowRoot.hitElements = [element(overlayA), element(overlayB), element(button), element(rootLayout)];

    click(element(overlayA), 40, 60);

    expect(hitTestA).toHaveBeenCalledTimes(1);
    expect(hitTestB).toHaveBeenCalledTimes(1);
    expect(onButtonTap).toHaveBeenCalledTimes(1);
  });

  it('delivers exactly one click past the Valdi tree when a hitTest forwards it', () => {
    const { shadowRoot, rootLayout } = makeRoot(0, 0, 100, 100);
    const button = new WebValdiLayout(2);
    const overlay = new WebValdiLayout(3);
    button.move(rootLayout, 0);
    overlay.move(rootLayout, 1);
    const onButtonTap = jasmine.createSpy('onButtonTap');
    const hostClickListener = jasmine.createSpy('hostClickListener');
    button.changeAttribute('onTap', onButtonTap);
    overlay.changeAttribute('hitTest', () => false);
    shadowRoot.addEventListener('click', hostClickListener);
    shadowRoot.hitElements = [element(overlay), element(button), element(rootLayout)];

    click(element(overlay), 40, 60);

    expect(onButtonTap).toHaveBeenCalledTimes(1);
    expect(hostClickListener).toHaveBeenCalledTimes(1);
  });

  it('does not ask the hitTest of a view inside a rejecting view', () => {
    const { shadowRoot, rootLayout } = makeRoot(0, 0, 100, 100);
    const button = new WebValdiLayout(2);
    const overlay = new WebValdiLayout(3);
    const overlayChild = new WebValdiLayout(4);
    button.move(rootLayout, 0);
    overlay.move(rootLayout, 1);
    overlayChild.move(overlay, 0);
    const onButtonTap = jasmine.createSpy('onButtonTap');
    const outerHitTest = jasmine.createSpy('outerHitTest').and.returnValue(false);
    const innerHitTest = jasmine.createSpy('innerHitTest').and.returnValue(true);
    button.changeAttribute('onTap', onButtonTap);
    overlay.changeAttribute('hitTest', outerHitTest);
    overlayChild.changeAttribute('hitTest', innerHitTest);
    shadowRoot.hitElements = [element(overlayChild), element(overlay), element(button)];

    click(element(overlayChild), 10, 10);

    expect(outerHitTest).toHaveBeenCalledTimes(1);
    expect(innerHitTest).not.toHaveBeenCalled();
    expect(onButtonTap).toHaveBeenCalledTimes(1);
  });

  it('does not forward the click when every hitTest accepts', () => {
    const { shadowRoot, rootLayout } = makeRoot(0, 0, 100, 100);
    const overlay = new WebValdiLayout(2);
    const overlayChild = new WebValdiLayout(3);
    overlay.move(rootLayout, 0);
    overlayChild.move(overlay, 0);
    const onChildTap = jasmine.createSpy('onChildTap');
    const outerHitTest = jasmine.createSpy('outerHitTest').and.returnValue(true);
    const innerHitTest = jasmine.createSpy('innerHitTest').and.returnValue(true);
    const hostClickListener = jasmine.createSpy('hostClickListener');
    overlayChild.changeAttribute('onTap', onChildTap);
    overlay.changeAttribute('hitTest', outerHitTest);
    overlayChild.changeAttribute('hitTest', innerHitTest);
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
    const { shadowRoot, rootLayout } = makeRoot(0, 0, 100, 100);
    const button = new WebValdiLayout(2);
    const overlay = new WebValdiLayout(3);
    const overlayChild = new WebValdiLayout(4);
    button.move(rootLayout, 0);
    overlay.move(rootLayout, 1);
    overlayChild.move(overlay, 0);

    const onButtonTap = jasmine.createSpy('onButtonTap');
    const onChildTap = jasmine.createSpy('onChildTap');
    button.changeAttribute('onTap', onButtonTap);
    overlayChild.changeAttribute('onTap', onChildTap);
    overlay.changeAttribute('hitTest', () => false);
    shadowRoot.hitElements = [element(overlayChild), element(overlay), element(button)];

    click(element(overlayChild), 10, 10);

    expect(onChildTap).not.toHaveBeenCalled();
    expect(onButtonTap).toHaveBeenCalledTimes(1);
  });

  it('keeps the click when a hitTest function returns true', () => {
    const { shadowRoot, rootLayout } = makeRoot(0, 0, 100, 100);
    const button = new WebValdiLayout(2);
    const overlay = new WebValdiLayout(3);
    button.move(rootLayout, 0);
    overlay.move(rootLayout, 1);

    const onButtonTap = jasmine.createSpy('onButtonTap');
    const onOverlayTap = jasmine.createSpy('onOverlayTap');
    button.changeAttribute('onTap', onButtonTap);
    overlay.changeAttribute('onTap', onOverlayTap);
    overlay.changeAttribute('hitTest', () => true);
    shadowRoot.hitElements = [element(overlay), element(button)];

    click(element(overlay), 40, 60);

    expect(onOverlayTap).toHaveBeenCalledTimes(1);
    expect(onButtonTap).not.toHaveBeenCalled();
  });

  it('keeps the boolean behavior for a non-function hitTest', () => {
    const { rootLayout } = makeRoot(0, 0, 100, 100);
    const overlay = new WebValdiLayout(2);
    overlay.move(rootLayout, 0);
    const onOverlayTap = jasmine.createSpy('onOverlayTap');
    overlay.changeAttribute('onTap', onOverlayTap);

    overlay.changeAttribute('hitTest', false);
    expect(element(overlay).style.pointerEvents).toBe('none');

    overlay.changeAttribute('hitTest', true);
    expect(element(overlay).style.pointerEvents).toBe('auto');
    click(element(overlay), 1, 1);
    expect(onOverlayTap).toHaveBeenCalledTimes(1);
  });

  it('stops calling a hitTest function after it is replaced by a boolean', () => {
    const { rootLayout } = makeRoot(0, 0, 100, 100);
    const overlay = new WebValdiLayout(2);
    overlay.move(rootLayout, 0);
    const onOverlayTap = jasmine.createSpy('onOverlayTap');
    const hitTest = jasmine.createSpy('hitTest').and.returnValue(false);
    overlay.changeAttribute('onTap', onOverlayTap);
    overlay.changeAttribute('hitTest', hitTest);

    overlay.changeAttribute('hitTest', true);
    click(element(overlay), 1, 1);

    expect(hitTest).not.toHaveBeenCalled();
    expect(onOverlayTap).toHaveBeenCalledTimes(1);
  });

  it('does not change touchEnabled', () => {
    const { rootLayout } = makeRoot(0, 0, 100, 100);
    const view = new WebValdiLayout(2);
    view.move(rootLayout, 0);

    view.changeAttribute('touchEnabled', false);
    expect(element(view).style.pointerEvents).toBe('none');
    view.changeAttribute('touchEnabled', true);
    expect(element(view).style.pointerEvents).toBe('auto');
  });
});

describe('WebValdiLayout – onTap propagation', () => {
  beforeEach(() => installDomStubs());
  afterEach(() => uninstallDomStubs());

  it('calls only the deepest view with a tap handler', () => {
    const { rootLayout } = makeRoot(0, 0, 100, 100);
    const outer = new WebValdiLayout(2);
    const inner = new WebValdiLayout(3);
    outer.move(rootLayout, 0);
    inner.move(outer, 0);
    const onOuterTap = jasmine.createSpy('onOuterTap');
    const onInnerTap = jasmine.createSpy('onInnerTap');
    outer.changeAttribute('onTap', onOuterTap);
    inner.changeAttribute('onTap', onInnerTap);

    click(element(inner), 5, 5);

    expect(onInnerTap).toHaveBeenCalledTimes(1);
    expect(onOuterTap).not.toHaveBeenCalled();
  });

  it('lets the click reach the parent when the child tap is disabled or rejected', () => {
    const { rootLayout } = makeRoot(0, 0, 100, 100);
    const outer = new WebValdiLayout(2);
    const inner = new WebValdiLayout(3);
    outer.move(rootLayout, 0);
    inner.move(outer, 0);
    const onOuterTap = jasmine.createSpy('onOuterTap');
    const onInnerTap = jasmine.createSpy('onInnerTap');
    outer.changeAttribute('onTap', onOuterTap);
    inner.changeAttribute('onTap', onInnerTap);

    inner.changeAttribute('onTapDisabled', true);
    click(element(inner), 5, 5);
    expect(onOuterTap).toHaveBeenCalledTimes(1);

    inner.changeAttribute('onTapDisabled', false);
    inner.changeAttribute('onTapPredicate', () => false);
    click(element(inner), 5, 5);
    expect(onOuterTap).toHaveBeenCalledTimes(2);
    expect(onInnerTap).not.toHaveBeenCalled();
  });

  it('keeps bubbling the click past the Valdi tree after a tap handler claims it', () => {
    const { shadowRoot, rootLayout } = makeRoot(0, 0, 100, 100);
    const outer = new WebValdiLayout(2);
    const inner = new WebValdiLayout(3);
    outer.move(rootLayout, 0);
    inner.move(outer, 0);
    const onInnerTap = jasmine.createSpy('onInnerTap');
    const onOuterAccessibilityTap = jasmine.createSpy('onOuterAccessibilityTap');
    const hostClickListener = jasmine.createSpy('hostClickListener');
    inner.changeAttribute('onTap', onInnerTap);
    outer.changeAttribute('onAccessibilityTap', onOuterAccessibilityTap);
    shadowRoot.addEventListener('click', hostClickListener);

    click(element(inner), 5, 5);

    expect(onInnerTap).toHaveBeenCalledTimes(1);
    expect(onOuterAccessibilityTap).toHaveBeenCalledTimes(1);
    expect(hostClickListener).toHaveBeenCalledTimes(1);
  });

  it('lets the click reach the parent when the child has no tap handler', () => {
    const { rootLayout } = makeRoot(0, 0, 100, 100);
    const outer = new WebValdiLayout(2);
    const inner = new WebValdiLayout(3);
    outer.move(rootLayout, 0);
    inner.move(outer, 0);
    const onOuterTap = jasmine.createSpy('onOuterTap');
    outer.changeAttribute('onTap', onOuterTap);

    click(element(inner), 5, 5);

    expect(onOuterTap).toHaveBeenCalledTimes(1);
  });
});

describe('WebValdiLayout – touch coordinates', () => {
  beforeEach(() => installDomStubs());
  afterEach(() => uninstallDomStubs());

  it('divides by the CSS scale of the root host', () => {
    // The host is laid out 375 points wide and drawn 750 pixels wide at (100, 50).
    const { rootLayout } = makeRoot(100, 50, 375, 750);
    const view = new WebValdiLayout(2);
    view.move(rootLayout, 0);
    element(view).rect = { left: 300, top: 250, width: 200, height: 100 };
    const onTap = jasmine.createSpy('onTap');
    view.changeAttribute('onTap', onTap);

    click(element(view), 340, 270);

    const touchEvent = onTap.calls.mostRecent().args[0];
    expect(touchEvent.state).toBe(TouchEventState.Ended);
    expect(touchEvent.x).toBe(20);
    expect(touchEvent.y).toBe(10);
    expect(touchEvent.absoluteX).toBe(120);
    expect(touchEvent.absoluteY).toBe(110);
  });

  it('keeps viewport coordinates when the view is not in a shadow root', () => {
    const view = new WebValdiLayout(1);
    element(view).rect = { left: 30, top: 40, width: 100, height: 100 };
    const onTap = jasmine.createSpy('onTap');
    view.changeAttribute('onTap', onTap);

    click(element(view), 35, 50);

    const touchEvent = onTap.calls.mostRecent().args[0];
    expect(touchEvent.x).toBe(5);
    expect(touchEvent.y).toBe(10);
    expect(touchEvent.absoluteX).toBe(35);
    expect(touchEvent.absoluteY).toBe(50);
  });

  it('gives the hitTest function the same Valdi points', () => {
    const { shadowRoot, rootLayout } = makeRoot(100, 50, 375, 750);
    const overlay = new WebValdiLayout(2);
    overlay.move(rootLayout, 0);
    element(overlay).rect = { left: 100, top: 50, width: 750, height: 1500 };
    const hitTest = jasmine.createSpy('hitTest').and.returnValue(true);
    overlay.changeAttribute('hitTest', hitTest);
    shadowRoot.hitElements = [element(overlay)];

    click(element(overlay), 300, 450);

    const touchEvent = hitTest.calls.mostRecent().args[0];
    expect(touchEvent.x).toBe(100);
    expect(touchEvent.y).toBe(200);
    expect(touchEvent.absoluteX).toBe(100);
    expect(touchEvent.absoluteY).toBe(200);
  });
});
