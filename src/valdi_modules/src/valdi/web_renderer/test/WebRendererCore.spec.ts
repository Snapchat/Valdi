import 'jasmine/src/jasmine';
import { AnimationCurve, type AnimationOptions } from 'valdi_core/src/AnimationOptions';
import { GeometricPathBuilder, GeometricPathScaleType } from 'valdi_core/src/GeometricPath';
import { Style } from 'valdi_core/src/Style';
import { ViewNodeAssetTracker } from 'valdi_core/src/ViewNodeAssetTracker';
import { AttributedTextBuilder } from 'valdi_core/src/utils/AttributedTextBuilder';
import { AttributesBinder } from '../src/attributes/AttributesBinder';
import { AttributeApplier, ElementClass } from '../src/core/ElementClass';
import { registerElementClassAlias } from '../src/elements/ElementClassRegistry';
import { ColorPaletteManager } from '../src/core/Palette';
import { ViewNode } from '../src/core/ViewNode';
import { ViewNodeTree } from '../src/core/ViewNodeTree';
import { ViewElementClass } from '../src/elements/ViewElementClass';
import { ValdiWebRendererDelegate } from '../src/ValdiWebRendererDelegate';
import { WebViewFactory } from '../src/ViewFactory';
import { registerWebViewClass, type WebViewClassFactory } from '../src/WebViewClassRegistry';
import {
  dispatchAttributedTextLayouts,
  ParsedAttributedText,
  renderAttributedText,
} from '../src/utils/parseAttributedText';
import { geometricPathToSvgPath } from '../src/utils/geometricPath';

type FakeStyle = Record<string, string> & {
  getPropertyValue(name: string): string;
  setProperty(name: string, value: string): void;
  removeProperty(name: string): void;
};

type FakeCanvasContext = {
  drawImage: jasmine.Spy;
  clearRect: jasmine.Spy;
  save: jasmine.Spy;
  restore: jasmine.Spy;
  scale: jasmine.Spy;
  translate: jasmine.Spy;
  rotate: jasmine.Spy;
  setTransform: jasmine.Spy;
  fillStyle: string;
  strokeStyle: string;
  lineWidth: number;
  getImageData(_x: number, _y: number, _width: number, _height: number): { data: Uint8ClampedArray };
  putImageData(_imageData: { data: Uint8ClampedArray }, _x: number, _y: number): void;
};

type FakeDomEvent = {
  type: string;
  defaultPrevented?: boolean;
  inputType?: string;
  key?: string;
  preventDefault(): void;
};

interface FakeMouseDragEvent extends FakeDomEvent {
  readonly buttons: number;
  readonly clientX: number;
  readonly clientY: number;
  readonly timeStamp: number;
}

interface FakePointerDragEvent extends FakeMouseDragEvent {
  readonly pointerId: number;
}

type FakeEventListener = ((event: FakeDomEvent) => void) | { handleEvent(event: FakeDomEvent): void };

type FakeElement = {
  id: string;
  tagName: string;
  style: FakeStyle;
  attributes: Record<string, string>;
  childNodes: { readonly length: number; item(index: number): FakeElement | null };
  parentElement: FakeElement | null;
  ownerDocument: Record<string, unknown>;
  textContent: string;
  contentEditable?: string;
  value: string;
  className: string;
  classList: { add(...names: string[]): void; contains(name: string): boolean; remove(...names: string[]): void };
  rectWidth: number;
  rectHeight: number;
  rectLeft: number;
  rectTop: number;
  rectReadCount: number;
  layoutLeft: number;
  layoutTop: number;
  layoutWidth: number;
  layoutHeight: number;
  layoutReadCount: number;
  offsetParent: FakeElement | null;
  readonly offsetLeft: number;
  readonly offsetTop: number;
  readonly offsetWidth: number;
  readonly offsetHeight: number;
  width: number;
  height: number;
  canvasContext: FakeCanvasContext;
  scrollLeft: number;
  scrollTop: number;
  scrollWidth: number;
  scrollHeight: number;
  clientWidth: number;
  clientHeight: number;
  cloneNode(deep?: boolean): FakeElement;
  setAttribute(name: string, value: string): void;
  getAttribute(name: string): string | null;
  removeAttribute(name: string): void;
  appendChild(child: FakeElement): void;
  removeChild(child: FakeElement): void;
  insertBefore(child: FakeElement, before: FakeElement | null): void;
  querySelector(selector: string): FakeElement | null;
  querySelectorAll(selector: string): FakeElement[];
  replaceChildren(...newChildren: FakeElement[]): void;
  remove(): void;
  addEventListener(name: string, listener: unknown): void;
  removeEventListener(name: string, listener: unknown): void;
  dispatchEvent(event: FakeDomEvent): boolean;
  focus(): void;
  blur(): void;
  setSelectionRange(start: number, end: number): void;
  getBoundingClientRect(): { left: number; top: number; width: number; height: number };
  getContext(contextId: string): FakeCanvasContext | null;
  getRootNode(): Record<string, unknown>;
  getTotalLength(): number;
  play(): Promise<void>;
  pause(): void;
};

type FakeImage = FakeElement & {
  crossOrigin: string | null;
  naturalWidth: number;
  naturalHeight: number;
  src: string;
  onload: (() => void) | null;
  onabort: (() => void) | null;
  onerror: (() => void) | null;
  removeAttribute(name: string): void;
};

function makeStyle(): FakeStyle {
  const style = {} as FakeStyle;
  style.getPropertyValue = (name: string) => style[name] ?? '';
  style.setProperty = (name: string, value: string) => {
    style[name] = value;
  };
  style.removeProperty = (name: string) => {
    delete style[name];
  };
  return style;
}

function makeFakeElement(tagName: string): FakeElement {
  const children: FakeElement[] = [];
  const classNames = new Set<string>();
  const listeners = new Map<string, FakeEventListener[]>();
  const canvasContext: FakeCanvasContext = {
    drawImage: jasmine.createSpy('drawImage'),
    clearRect: jasmine.createSpy('clearRect'),
    save: jasmine.createSpy('save'),
    restore: jasmine.createSpy('restore'),
    scale: jasmine.createSpy('scale'),
    translate: jasmine.createSpy('translate'),
    rotate: jasmine.createSpy('rotate'),
    setTransform: jasmine.createSpy('setTransform'),
    fillStyle: '',
    strokeStyle: '',
    lineWidth: 0,
    getImageData(_x: number, _y: number, _width: number, _height: number): { data: Uint8ClampedArray } {
      return { data: new Uint8ClampedArray(0) };
    },
    putImageData(_imageData: { data: Uint8ClampedArray }, _x: number, _y: number): void {},
  };
  const element: FakeElement = {
    id: '',
    tagName: tagName.toUpperCase(),
    style: makeStyle(),
    attributes: {},
    childNodes: {
      get length(): number {
        return children.length;
      },
      item(index: number): FakeElement | null {
        return children[index] ?? null;
      },
    },
    parentElement: null,
    ownerDocument: (globalThis as unknown as { document?: Record<string, unknown> }).document ?? {},
    textContent: '',
    value: '',
    className: '',
    classList: {
      add(...names: string[]): void {
        for (let i = 0; i < names.length; i++) {
          classNames.add(names[i]);
        }
      },
      contains(name: string): boolean {
        return classNames.has(name);
      },
      remove(...names: string[]): void {
        for (let i = 0; i < names.length; i++) {
          classNames.delete(names[i]);
        }
      },
    },
    rectWidth: 0,
    rectHeight: 0,
    rectLeft: 0,
    rectTop: 0,
    rectReadCount: 0,
    layoutLeft: 0,
    layoutTop: 0,
    layoutWidth: 0,
    layoutHeight: 0,
    layoutReadCount: 0,
    offsetParent: null,
    get offsetLeft(): number {
      this.layoutReadCount++;
      return this.layoutLeft;
    },
    get offsetTop(): number {
      this.layoutReadCount++;
      return this.layoutTop;
    },
    get offsetWidth(): number {
      this.layoutReadCount++;
      return this.layoutWidth;
    },
    get offsetHeight(): number {
      this.layoutReadCount++;
      return this.layoutHeight;
    },
    width: 0,
    height: 0,
    canvasContext,
    scrollLeft: 0,
    scrollTop: 0,
    scrollWidth: 0,
    scrollHeight: 0,
    clientWidth: 0,
    clientHeight: 0,
    cloneNode(deep?: boolean): FakeElement {
      const clone = makeFakeElement(tagName);
      const styleKeys = Object.keys(this.style);
      for (let i = 0; i < styleKeys.length; i++) {
        const key = styleKeys[i];
        const value = this.style[key];
        if (typeof value !== 'function') {
          clone.style[key] = value;
        }
      }
      const attributeNames = Object.keys(this.attributes);
      for (let i = 0; i < attributeNames.length; i++) {
        const name = attributeNames[i];
        clone.attributes[name] = this.attributes[name];
      }
      clone.textContent = this.textContent;
      clone.value = this.value;
      clone.className = this.className;
      classNames.forEach(name => clone.classList.add(name));
      clone.rectWidth = this.rectWidth;
      clone.rectHeight = this.rectHeight;
      clone.rectLeft = this.rectLeft;
      clone.rectTop = this.rectTop;
      clone.rectReadCount = this.rectReadCount;
      clone.layoutLeft = this.layoutLeft;
      clone.layoutTop = this.layoutTop;
      clone.layoutWidth = this.layoutWidth;
      clone.layoutHeight = this.layoutHeight;
      clone.layoutReadCount = this.layoutReadCount;
      clone.width = this.width;
      clone.height = this.height;
      clone.scrollLeft = this.scrollLeft;
      clone.scrollTop = this.scrollTop;
      clone.scrollWidth = this.scrollWidth;
      clone.scrollHeight = this.scrollHeight;
      clone.clientWidth = this.clientWidth;
      clone.clientHeight = this.clientHeight;
      if (deep) {
        for (let i = 0; i < children.length; i++) {
          clone.appendChild(children[i].cloneNode(true));
        }
      }
      return clone;
    },
    setAttribute(name: string, value: string): void {
      this.attributes[name] = String(value);
    },
    getAttribute(name: string): string | null {
      return this.attributes[name] ?? null;
    },
    removeAttribute(name: string): void {
      delete this.attributes[name];
    },
    appendChild(child: FakeElement): void {
      child.parentElement = this;
      children.push(child);
    },
    removeChild(child: FakeElement): void {
      const index = children.indexOf(child);
      if (index >= 0) {
        children.splice(index, 1);
        child.parentElement = null;
      }
    },
    insertBefore(child: FakeElement, before: FakeElement | null): void {
      const existingIndex = children.indexOf(child);
      if (existingIndex >= 0) {
        children.splice(existingIndex, 1);
      }
      child.parentElement = this;
      const index = before ? children.indexOf(before) : -1;
      if (index >= 0) {
        children.splice(index, 0, child);
      } else {
        children.push(child);
      }
    },
    querySelector(selector: string): FakeElement | null {
      const isIdSelector = selector.charAt(0) === '#';
      const id = isIdSelector ? selector.slice(1) : '';
      const tag = isIdSelector ? '' : selector.toUpperCase();
      for (let i = 0; i < children.length; i++) {
        const child = children[i];
        if ((isIdSelector && child.id === id) || (!isIdSelector && child.tagName === tag)) {
          return child;
        }
        const nestedChild = child.querySelector(selector);
        if (nestedChild) {
          return nestedChild;
        }
      }
      return null;
    },
    querySelectorAll(selector: string): FakeElement[] {
      const matches: FakeElement[] = [];
      const isIdSelector = selector.charAt(0) === '#';
      const id = isIdSelector ? selector.slice(1) : '';
      const tag = isIdSelector ? '' : selector.toUpperCase();
      for (let i = 0; i < children.length; i++) {
        const child = children[i];
        if ((isIdSelector && child.id === id) || (!isIdSelector && child.tagName === tag)) {
          matches.push(child);
        }
        matches.push(...child.querySelectorAll(selector));
      }
      return matches;
    },
    replaceChildren(...newChildren: FakeElement[]): void {
      children.length = 0;
      for (let i = 0; i < newChildren.length; i++) {
        newChildren[i].parentElement = this;
      }
      children.push(...newChildren);
    },
    remove(): void {
      this.parentElement?.removeChild(this);
    },
    addEventListener(name: string, listener: unknown): void {
      const listenerObject = listener as { handleEvent?: unknown };
      if (typeof listener !== 'function' && (!listener || typeof listenerObject.handleEvent !== 'function')) {
        return;
      }
      const namedListeners = listeners.get(name) ?? [];
      namedListeners.push(listener as FakeEventListener);
      listeners.set(name, namedListeners);
    },
    removeEventListener(name: string, listener: unknown): void {
      const namedListeners = listeners.get(name);
      if (!namedListeners) {
        return;
      }
      const index = namedListeners.indexOf(listener as FakeEventListener);
      if (index >= 0) {
        namedListeners.splice(index, 1);
      }
    },
    dispatchEvent(event: FakeDomEvent): boolean {
      const namedListeners = listeners.get(event.type) ?? [];
      for (let i = 0; i < namedListeners.length; i++) {
        const listener = namedListeners[i];
        if (typeof listener === 'function') {
          listener(event);
        } else {
          listener.handleEvent(event);
        }
      }
      return event.defaultPrevented !== true;
    },
    focus(): void {},
    blur(): void {},
    setSelectionRange(_start: number, _end: number): void {},
    getBoundingClientRect(): { left: number; top: number; width: number; height: number } {
      this.rectReadCount++;
      const width = this.rectWidth || (this.style.width === '100%' ? (this.parentElement?.rectWidth ?? 0) : 0);
      const height = this.rectHeight || (this.style.height === '100%' ? (this.parentElement?.rectHeight ?? 0) : 0);
      return { left: this.rectLeft, top: this.rectTop, width, height };
    },
    getContext(contextId: string): FakeCanvasContext | null {
      return contextId === '2d' ? this.canvasContext : null;
    },
    getRootNode(): Record<string, unknown> {
      return (globalThis as unknown as { document?: Record<string, unknown> }).document ?? this.ownerDocument;
    },
    getTotalLength(): number {
      return 100;
    },
    play(): Promise<void> {
      return Promise.resolve();
    },
    pause(): void {},
  };
  return element;
}

let lastImage: FakeImage | undefined;
let imageConstructionCount = 0;

function installDomStubs(): () => void {
  const previousDocument = (globalThis as { document?: unknown }).document;
  const previousImage = (globalThis as { Image?: unknown }).Image;
  const previousRequestAnimationFrame = (globalThis as { requestAnimationFrame?: unknown }).requestAnimationFrame;
  const previousCancelAnimationFrame = (globalThis as { cancelAnimationFrame?: unknown }).cancelAnimationFrame;
  const previousPerformance = (globalThis as { performance?: unknown }).performance;
  const previousWindow = (globalThis as { window?: unknown }).window;
  const previousResizeObserver = (globalThis as { ResizeObserver?: unknown }).ResizeObserver;
  const animationFrameCallbacks = new Map<number, (time: number) => void>();
  const windowListeners = new Map<string, Array<() => void>>();
  const documentListeners = new Map<string, FakeEventListener[]>();
  let animationFrameTime = 0;
  let animationFrameRequestCount = 0;
  let nextAnimationFrameHandle = 1;
  lastImage = undefined;
  imageConstructionCount = 0;
  const head = makeFakeElement('head');
  (globalThis as { document?: unknown }).document = {
    dir: 'ltr',
    activeElement: null,
    head,
    createElement(tagName: string): FakeElement {
      return makeFakeElement(tagName);
    },
    createElementNS(_namespaceURI: string, qualifiedName: string): FakeElement {
      return makeFakeElement(qualifiedName);
    },
    querySelector(selector: string): FakeElement | null {
      return head.querySelector(selector);
    },
    appendChild(child: FakeElement): void {
      head.appendChild(child);
    },
    addEventListener(name: string, listener: unknown): void {
      const listeners = documentListeners.get(name) ?? [];
      listeners.push(listener as FakeEventListener);
      documentListeners.set(name, listeners);
    },
    removeEventListener(name: string, listener: unknown): void {
      const listeners = documentListeners.get(name);
      if (listeners === undefined) {
        return;
      }
      const index = listeners.indexOf(listener as FakeEventListener);
      if (index !== -1) {
        listeners.splice(index, 1);
      }
    },
    dispatchEvent(event: FakeDomEvent): boolean {
      const listeners = [...(documentListeners.get(event.type) ?? [])];
      listeners.forEach(listener => {
        if (typeof listener === 'function') {
          listener(event);
        } else {
          listener.handleEvent(event);
        }
      });
      return event.defaultPrevented !== true;
    },
  };
  (globalThis as { Image?: unknown }).Image = function () {
    imageConstructionCount++;
    const image = makeFakeElement('img') as FakeImage;
    const removeElementAttribute = image.removeAttribute.bind(image);
    image.crossOrigin = null;
    image.naturalWidth = 0;
    image.naturalHeight = 0;
    image.src = '';
    image.onload = null;
    image.onabort = null;
    image.onerror = null;
    image.removeAttribute = (name: string): void => {
      removeElementAttribute(name);
      if (name === 'src') {
        image.src = '';
      } else if (name === 'crossorigin') {
        image.crossOrigin = null;
      }
    };
    lastImage = image;
    return image;
  };
  (globalThis as { window?: unknown }).window = {
    devicePixelRatio: 1,
    addEventListener(name: string, listener: () => void): void {
      const listeners = windowListeners.get(name) ?? [];
      listeners.push(listener);
      windowListeners.set(name, listeners);
    },
    removeEventListener(name: string, listener: () => void): void {
      const listeners = windowListeners.get(name);
      if (!listeners) {
        return;
      }
      const index = listeners.indexOf(listener);
      if (index >= 0) {
        listeners.splice(index, 1);
      }
    },
    dispatchEvent(event: { type: string }): void {
      const listeners = windowListeners.get(event.type) ?? [];
      for (const listener of Array.from(listeners)) {
        listener();
      }
    },
    listenerCount(name: string): number {
      return windowListeners.get(name)?.length ?? 0;
    },
  };
  (globalThis as { requestAnimationFrame?: (callback: (time: number) => void) => number }).requestAnimationFrame =
    callback => {
      animationFrameRequestCount++;
      const handle = nextAnimationFrameHandle++;
      animationFrameCallbacks.set(handle, callback);
      return handle;
    };
  (globalThis as { cancelAnimationFrame?: (handle: number) => void }).cancelAnimationFrame = handle => {
    animationFrameCallbacks.delete(handle);
  };
  (globalThis as { performance?: unknown }).performance = { now: () => animationFrameTime };
  (globalThis as { __flushTextAnimationFrame?: (time: number) => void }).__flushTextAnimationFrame = time => {
    animationFrameTime = time;
    const callbacks = Array.from(animationFrameCallbacks.values());
    animationFrameCallbacks.clear();
    for (let i = 0; i < callbacks.length; i++) {
      callbacks[i](time);
    }
  };
  (globalThis as { __getAnimationFrameRequestCount?: () => number }).__getAnimationFrameRequestCount = () =>
    animationFrameRequestCount;
  delete (globalThis as { ResizeObserver?: unknown }).ResizeObserver;
  return () => {
    (globalThis as { document?: unknown }).document = previousDocument;
    if (previousImage === undefined) {
      delete (globalThis as { Image?: unknown }).Image;
    } else {
      (globalThis as { Image?: unknown }).Image = previousImage;
    }
    if (previousRequestAnimationFrame === undefined) {
      delete (globalThis as { requestAnimationFrame?: unknown }).requestAnimationFrame;
    } else {
      (globalThis as { requestAnimationFrame?: unknown }).requestAnimationFrame = previousRequestAnimationFrame;
    }
    if (previousCancelAnimationFrame === undefined) {
      delete (globalThis as { cancelAnimationFrame?: unknown }).cancelAnimationFrame;
    } else {
      (globalThis as { cancelAnimationFrame?: unknown }).cancelAnimationFrame = previousCancelAnimationFrame;
    }
    if (previousPerformance === undefined) {
      delete (globalThis as { performance?: unknown }).performance;
    } else {
      (globalThis as { performance?: unknown }).performance = previousPerformance;
    }
    if (previousWindow === undefined) {
      delete (globalThis as { window?: unknown }).window;
    } else {
      (globalThis as { window?: unknown }).window = previousWindow;
    }
    if (previousResizeObserver === undefined) {
      delete (globalThis as { ResizeObserver?: unknown }).ResizeObserver;
    } else {
      (globalThis as { ResizeObserver?: unknown }).ResizeObserver = previousResizeObserver;
    }
    delete (globalThis as { __flushTextAnimationFrame?: unknown }).__flushTextAnimationFrame;
    delete (globalThis as { __getAnimationFrameRequestCount?: unknown }).__getAnimationFrameRequestCount;
  };
}

describe('web renderer core', () => {
  let uninstallDomStubs: () => void;
  let nextId = 1;
  let paletteManager: ColorPaletteManager;
  let tree: ViewNodeTree;

  beforeEach(() => {
    uninstallDomStubs = installDomStubs();
    paletteManager = new ColorPaletteManager();
    tree = new ViewNodeTree(paletteManager);
    tree.setPostLayoutScheduler(callback => callback());
  });

  afterEach(() => {
    tree.destroy();
    uninstallDomStubs();
  });

  function getNode(id: number): ViewNode {
    const node = tree.getNode(id);
    if (!node) {
      throw new Error(`Missing test node ${id}`);
    }
    return node;
  }

  function getViewPaintElement(id: number): FakeElement {
    const element = getNode(id).htmlElement as unknown as FakeElement;
    const paintElement = element.childNodes.item(0);
    if (!paintElement) {
      throw new Error(`Missing paint element for test node ${id}`);
    }
    return paintElement;
  }

  function reflectPixelStyleSizeInLayout(element: FakeElement): void {
    let width = element.style.width;
    let height = element.style.height;
    Object.defineProperty(element.style, 'width', {
      configurable: true,
      get: () => width,
      set: value => {
        width = value;
        if (typeof value === 'string' && value.endsWith('px')) {
          element.layoutWidth = Number.parseFloat(value);
        }
      },
    });
    Object.defineProperty(element.style, 'height', {
      configurable: true,
      get: () => height,
      set: value => {
        height = value;
        if (typeof value === 'string' && value.endsWith('px')) {
          element.layoutHeight = Number.parseFloat(value);
        }
      },
    });
  }

  function reflectPixelStylePositionInLayout(element: FakeElement): void {
    let left = element.style.left;
    let top = element.style.top;
    Object.defineProperty(element.style, 'left', {
      configurable: true,
      get: () => left,
      set: value => {
        left = value;
        if (typeof value === 'string' && value.endsWith('px')) {
          element.layoutLeft = Number.parseFloat(value);
        }
      },
    });
    Object.defineProperty(element.style, 'top', {
      configurable: true,
      get: () => top,
      set: value => {
        top = value;
        if (typeof value === 'string' && value.endsWith('px')) {
          element.layoutTop = Number.parseFloat(value);
        }
      },
    });
  }

  function createTestElement(viewClass: string): number {
    const id = nextId++;
    tree.createElement(id, viewClass);
    return id;
  }

  function createRootTestElement(viewClass: string): number {
    const id = createTestElement(viewClass);
    tree.makeElementRoot(id, makeFakeElement('root') as unknown as HTMLElement);
    return id;
  }

  function getLastImage(): FakeImage {
    if (!lastImage) {
      throw new Error('Expected an image to be constructed');
    }
    return lastImage;
  }

  function triggerImageLoad(naturalWidth: number, naturalHeight: number): void {
    const image = getLastImage();
    image.naturalWidth = naturalWidth;
    image.naturalHeight = naturalHeight;
    image.onload?.();
  }

  function triggerImageError(): void {
    getLastImage().onerror?.();
  }

  function flushTextAnimationFrame(time: number): void {
    (globalThis as unknown as { __flushTextAnimationFrame: (time: number) => void }).__flushTextAnimationFrame(time);
  }

  function getAnimationFrameRequestCount(): number {
    return (
      globalThis as unknown as { __getAnimationFrameRequestCount: () => number }
    ).__getAnimationFrameRequestCount();
  }

  function makeFakeEvent(type: string, key?: string): FakeDomEvent {
    const event: FakeDomEvent = {
      type,
      key,
      defaultPrevented: false,
      preventDefault(): void {
        event.defaultPrevented = true;
      },
    };
    return event;
  }

  function animatedText(text: string) {
    return new AttributedTextBuilder()
      .append(text, {
        animationTransform: {
          duration: 1,
          opacity: 0,
          timeOffsetBetweenParts: 0.1,
        },
      })
      .build();
  }

  function attributedPartSpan(id: number): FakeElement {
    const element = getNode(id).htmlElement as unknown as FakeElement;
    const container = element.childNodes.item(0)!;
    return container.childNodes.item(0)!;
  }

  let nextTestElementClassId = 1;

  function registerTestElementClass(
    attributeAppliers: Readonly<Record<string, AttributeApplier>>,
    destroy?: (element: HTMLElement) => void,
  ): string {
    const viewClass = `test-view-node-${nextTestElementClassId++}`;
    class TestElementClass extends ElementClass {
      constructor() {
        super(viewClass, attributeAppliers);
      }

      protected onCreateElement(): HTMLElement {
        return document.createElement('div');
      }

      destroy(element: HTMLElement): void {
        destroy?.(element);
      }
    }
    registerElementClassAlias(viewClass, new TestElementClass());
    return viewClass;
  }

  function registerTestWebViewClass(factory: WebViewClassFactory): string {
    const className = `test-web-view-${nextTestElementClassId++}`;
    registerWebViewClass(className, factory);
    return className;
  }

  async function waitForScheduledFlush(): Promise<void> {
    await Promise.resolve();
    await Promise.resolve();
  }

  function dispatchWindowResize(): void {
    (window as unknown as { dispatchEvent(event: { type: string }): void }).dispatchEvent({ type: 'resize' });
  }

  it('registers and unregisters view-node trees by context ID', () => {
    const contextId = 'web-renderer-core-test';

    expect(ViewNodeTree.getForContextId(contextId)).toBeUndefined();

    ViewNodeTree.register(contextId, tree);
    expect(ViewNodeTree.getForContextId(contextId)).toBe(tree);

    ViewNodeTree.unregister(contextId);
    expect(ViewNodeTree.getForContextId(contextId)).toBeUndefined();
  });

  it('resolves style attributes below direct attributes and falls back after direct removal', () => {
    const id = createRootTestElement('view');
    const style = new Style({ width: '100%', backgroundColor: 'red' });

    tree.setStyleAttributeOnElement(id, 'style', style);
    tree.flush();
    expect(getNode(id).htmlElement.style.width).toBe('100%');

    tree.setAttributeOnElement(id, 'width', 42);
    tree.flush();
    expect(getNode(id).htmlElement.style.width).toBe('42px');

    tree.setAttributeOnElement(id, 'width', undefined);
    tree.flush();
    expect(getNode(id).htmlElement.style.width).toBe('100%');
  });

  it('animates opacity through an animation transaction and applies the exact final value', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement;
    let completion: boolean | undefined;
    tree.setAttributeOnElement(id, 'opacity', 0);
    tree.flush();

    tree.beginAnimation(
      {
        curve: AnimationCurve.Linear,
        duration: 1,
        completion: cancelled => {
          completion = cancelled;
        },
      },
      1,
    );
    tree.setAttributeOnElement(id, 'opacity', 1);
    tree.endAnimation();

    expect(element.style.opacity).toBe('0');
    flushTextAnimationFrame(500);
    expect(Number(element.style.opacity)).toBeCloseTo(0.5, 5);
    expect(completion).toBeUndefined();
    flushTextAnimationFrame(1000);
    expect(element.style.opacity).toBe('1');
    expect(completion).toBeFalse();
  });

  it('keeps layout elements separate from view appearance bindings', () => {
    const id = createRootTestElement('layout');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    const warnSpy = spyOn(console, 'warn');
    let created = false;

    tree.setAttributeOnElement(id, 'width', 120);
    tree.setAttributeOnElement(id, 'accessibilityLabel', 'Layout container');
    tree.setAttributeOnElement(id, 'onViewCreate', () => {
      created = true;
    });
    tree.setAttributeOnElement(id, 'backgroundColor', 'red');
    tree.setAttributeOnElement(id, 'opacity', 0.5);
    tree.setAttributeOnElement(id, 'translationX', 20);
    tree.setAttributeOnElement(id, 'onTap', () => {});
    tree.flush();

    expect(element.style.width).toBe('120px');
    expect(element.getAttribute('aria-label')).toBe('Layout container');
    expect(created).toBeTrue();
    expect(element.childNodes.length).toBe(0);
    expect(element.style.backgroundColor).toBeUndefined();
    expect(element.style.opacity).toBeUndefined();
    expect(element.style.transform).toBeUndefined();
    expect(
      warnSpy.calls
        .allArgs()
        .map(args => String(args[0]))
        .join('\n'),
    ).toContain("'backgroundColor'");
    expect(
      warnSpy.calls
        .allArgs()
        .map(args => String(args[0]))
        .join('\n'),
    ).toContain("'onTap'");
  });

  it('does not create a paint element for layout, subtree, or transform-only behavior', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement as unknown as FakeElement;

    tree.setAttributeOnElement(id, 'width', 100);
    tree.setAttributeOnElement(id, 'color', 'purple');
    tree.setAttributeOnElement(id, 'opacity', 0.6);
    tree.setAttributeOnElement(id, 'translationX', 12);
    tree.setAttributeOnElement(id, 'maskPath', 'M 0 0 L 1 1');
    tree.setAttributeOnElement(id, 'slowClipping', true);
    tree.setAttributeOnElement(id, 'touchEnabled', false);
    tree.flush();

    expect(element.childNodes.length).toBe(0);
    expect(element.style.color).toBe('purple');
    expect(element.style.opacity).toBe('0.6');
    expect(element.style.transform).toContain('translate(12px, 0px)');
    expect(element.style.getPropertyValue('mask-image')).toContain('data:image/svg+xml');
    expect(element.style.overflow).toBe('hidden');
    expect(element.style.pointerEvents).toBe('none');
  });

  it('creates one retained paint element for all generic view decorations', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement as unknown as FakeElement;

    tree.setAttributeOnElement(id, 'backgroundColor', 'red');
    tree.setAttributeOnElement(id, 'borderWidth', 3);
    tree.setAttributeOnElement(id, 'borderColor', 'blue');
    tree.setAttributeOnElement(id, 'borderStyle', 'solid');
    tree.setAttributeOnElement(id, 'borderRadius', 14);
    tree.setAttributeOnElement(id, 'boxShadow', '0 2 6 black');
    tree.flush();

    const paintElement = getViewPaintElement(id);
    expect(element.childNodes.length).toBe(1);
    expect(paintElement.style.position).toBe('absolute');
    expect(paintElement.style.inset).toBe('0');
    expect(paintElement.style.pointerEvents).toBe('none');
    expect(paintElement.style.transformOrigin).toBe('0 0');
    expect(paintElement.style.zIndex).toBe('-1');
    expect(paintElement.getAttribute('aria-hidden')).toBe('true');
    expect(element.style.isolation).toBe('isolate');
    expect(paintElement.style.backgroundColor).toBe('red');
    expect(paintElement.style.borderWidth).toBe('3px');
    expect(paintElement.style.borderColor).toBe('blue');
    expect(paintElement.style.borderStyle).toBe('solid');
    expect(paintElement.style.borderRadius).toBe('14px');
    expect(paintElement.style.boxShadow).toBe('0px 2px 6px black');
    expect(element.style.backgroundColor).toBeUndefined();

    tree.setAttributeOnElement(id, 'backgroundColor', undefined);
    tree.setAttributeOnElement(id, 'borderWidth', undefined);
    tree.setAttributeOnElement(id, 'borderColor', undefined);
    tree.setAttributeOnElement(id, 'borderStyle', undefined);
    tree.setAttributeOnElement(id, 'borderRadius', undefined);
    tree.setAttributeOnElement(id, 'boxShadow', undefined);
    tree.flush();

    expect(element.childNodes.length).toBe(1);
    expect(element.childNodes.item(0)).toBe(paintElement);
  });

  it('renders border width and color without relying on an ambient border style', () => {
    const id = createRootTestElement('view');

    tree.setAttributeOnElement(id, 'borderWidth', 2);
    tree.setAttributeOnElement(id, 'borderColor', 'black');
    tree.flush();

    const paintElement = getViewPaintElement(id);
    expect(paintElement.style.borderWidth).toBe('2px');
    expect(paintElement.style.borderColor).toBe('black');
    expect(paintElement.style.borderStyle).toBe('solid');

    tree.setAttributeOnElement(id, 'borderWidth', undefined);
    tree.flush();

    expect(paintElement.style.borderWidth).toBe('0px');
  });

  it('scales label decorations without scaling or replacing its text content', () => {
    const id = createRootTestElement('label');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.layoutWidth = 100;
    element.layoutHeight = 20;
    reflectPixelStyleSizeInLayout(element);

    tree.setAttributeOnElement(id, 'value', 'Initial label');
    tree.setAttributeOnElement(id, 'backgroundColor', 'red');
    tree.setAttributeOnElement(id, 'borderWidth', 2);
    tree.setAttributeOnElement(id, 'borderColor', 'blue');
    tree.setAttributeOnElement(id, 'borderStyle', 'solid');
    tree.flush();

    const paintElement = getViewPaintElement(id);
    const textContentElement = element.childNodes.item(1)!;
    expect(element.childNodes.length).toBe(2);
    expect(paintElement.style.backgroundColor).toBe('red');
    expect(paintElement.style.borderWidth).toBe('2px');
    expect(paintElement.style.borderColor).toBe('blue');
    expect(paintElement.style.borderStyle).toBe('solid');
    expect(textContentElement.style.display).toBe('contents');
    expect(textContentElement.textContent).toBe('Initial label');
    expect(element.style.backgroundColor).toBeUndefined();

    tree.setAttributeOnElement(id, 'value', 'Updated label');
    tree.flush();
    expect(element.childNodes.item(0)).toBe(paintElement);
    expect(element.childNodes.item(1)).toBe(textContentElement);
    expect(textContentElement.textContent).toBe('Updated label');

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 898);
    tree.setAttributeOnElement(id, 'width', 200);
    tree.setAttributeOnElement(id, 'height', 40);
    tree.endAnimation();

    expect(element.style.scale).toBeUndefined();
    expect(textContentElement.style.scale).toBeUndefined();
    expect(paintElement.style.scale).toBe('0.5 0.5');
    flushTextAnimationFrame(1000);
    expect(paintElement.style.scale).toBeUndefined();
  });

  it('shrinks labels within flex rows while preserving native numberOfLines defaults', () => {
    const id = createRootTestElement('label');
    const element = getNode(id).htmlElement as unknown as FakeElement;

    expect(String(element.style.flexShrink)).toBe('1');
    expect(element.style.display).toBe('-webkit-box');
    expect(element.style.overflow).toBe('hidden');
    expect(element.style.getPropertyValue('-webkit-line-clamp')).toBe('1');
    expect(element.style.getPropertyValue('-webkit-box-orient')).toBe('vertical');

    tree.setAttributeOnElement(id, 'numberOfLines', 0);
    tree.flush();
    expect(element.style.display).toBe('inline');
    expect(element.style.overflow).toBe('');
    expect(element.style.getPropertyValue('-webkit-line-clamp')).toBe('');
    expect(element.style.getPropertyValue('-webkit-box-orient')).toBe('');

    tree.setAttributeOnElement(id, 'numberOfLines', 2);
    tree.flush();
    expect(element.style.display).toBe('-webkit-box');
    expect(element.style.overflow).toBe('hidden');
    expect(element.style.getPropertyValue('-webkit-line-clamp')).toBe('2');
    expect(element.style.getPropertyValue('-webkit-box-orient')).toBe('vertical');

    tree.setAttributeOnElement(id, 'numberOfLines', undefined);
    tree.flush();
    expect(element.style.display).toBe('-webkit-box');
    expect(element.style.overflow).toBe('hidden');
    expect(element.style.getPropertyValue('-webkit-line-clamp')).toBe('1');
    expect(element.style.getPropertyValue('-webkit-box-orient')).toBe('vertical');
  });

  it('keeps the paint element outside logical child ordering', () => {
    const root = createRootTestElement('view');
    tree.setAttributeOnElement(root, 'backgroundColor', 'red');
    tree.flush();
    const paintElement = getViewPaintElement(root);
    const first = createTestElement('view');
    const second = createTestElement('view');
    const firstElement = getNode(first).htmlElement as unknown as FakeElement;
    const secondElement = getNode(second).htmlElement as unknown as FakeElement;

    tree.moveElement(first, root, 0);
    tree.moveElement(second, root, 1);
    const rootElement = getNode(root).htmlElement as unknown as FakeElement;
    expect(rootElement.childNodes.item(0)).toBe(paintElement);
    expect(rootElement.childNodes.item(1)).toBe(firstElement);
    expect(rootElement.childNodes.item(2)).toBe(secondElement);

    tree.moveElement(second, root, 0);
    expect(rootElement.childNodes.item(0)).toBe(paintElement);
    expect(rootElement.childNodes.item(1)).toBe(secondElement);
    expect(rootElement.childNodes.item(2)).toBe(firstElement);

    tree.destroyElement(second);
    expect(rootElement.childNodes.item(0)).toBe(paintElement);
    expect(rootElement.childNodes.item(1)).toBe(firstElement);
  });

  it('does not capture layout when a decoration changes', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.layoutWidth = 100;
    element.layoutHeight = 100;
    element.layoutReadCount = 0;

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 899);
    tree.setAttributeOnElement(id, 'backgroundColor', 'red');
    tree.endAnimation();

    expect(element.layoutReadCount).toBe(0);
    expect(getViewPaintElement(id).style.backgroundColor).toBe('transparent');
    expect(getAnimationFrameRequestCount()).toBe(1);
  });

  it('captures one layout pass for multiple layout attributes', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.layoutWidth = 100;
    element.layoutHeight = 100;
    reflectPixelStyleSizeInLayout(element);
    tree.setAttributeOnElement(id, 'backgroundColor', 'red');
    tree.flush();
    const paintElement = getViewPaintElement(id);
    element.layoutReadCount = 0;

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 900);
    tree.setAttributeOnElement(id, 'width', 200);
    tree.setAttributeOnElement(id, 'height', 150);
    tree.endAnimation();

    expect(element.layoutReadCount).toBe(8);
    expect(element.style.scale).toBeUndefined();
    expect(paintElement.style.scale).toBe('0.5 0.6666666666666666');
    expect(getAnimationFrameRequestCount()).toBe(1);

    flushTextAnimationFrame(500);
    expect(paintElement.style.scale).toBe('0.75 0.8333333333333334');
    flushTextAnimationFrame(1000);
    expect(paintElement.style.scale).toBeUndefined();
  });

  it('reads each nested element layout once per snapshot', () => {
    const viewClass = registerTestElementClass({
      testWidth: {
        layoutDependent: true,
        apply(element, value) {
          (element as unknown as FakeElement).layoutWidth = Number(value);
        },
        reset(element) {
          (element as unknown as FakeElement).layoutWidth = 0;
        },
      },
    });
    const root = createRootTestElement(viewClass);
    const child = createTestElement('view');
    const grandchild = createTestElement('view');
    tree.moveElement(child, root, 0);
    tree.moveElement(grandchild, child, 0);
    const rootElement = getNode(root).htmlElement as unknown as FakeElement;
    const childElement = getNode(child).htmlElement as unknown as FakeElement;
    const grandchildElement = getNode(grandchild).htmlElement as unknown as FakeElement;
    rootElement.layoutWidth = 100;
    rootElement.layoutHeight = 100;
    childElement.layoutWidth = 50;
    childElement.layoutHeight = 50;
    grandchildElement.layoutWidth = 25;
    grandchildElement.layoutHeight = 25;
    childElement.offsetParent = rootElement;
    grandchildElement.offsetParent = childElement;
    rootElement.layoutReadCount = 0;
    childElement.layoutReadCount = 0;
    grandchildElement.layoutReadCount = 0;

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 914);
    tree.setAttributeOnElement(root, 'testWidth', 200);
    tree.endAnimation();

    expect(rootElement.layoutReadCount).toBe(8);
    expect(childElement.layoutReadCount).toBe(8);
    expect(grandchildElement.layoutReadCount).toBe(8);
  });

  it('captures layout before inserting a child', () => {
    const root = createRootTestElement('view');
    const existing = createTestElement('view');
    tree.moveElement(existing, root, 0);
    const rootElement = getNode(root).htmlElement as unknown as FakeElement;
    const existingElement = getNode(existing).htmlElement as unknown as FakeElement;
    rootElement.layoutWidth = 300;
    rootElement.layoutHeight = 100;
    existingElement.layoutWidth = 100;
    existingElement.layoutHeight = 50;

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 907);
    const inserted = createTestElement('view');
    tree.moveElement(inserted, root, 0);
    const insertedElement = getNode(inserted).htmlElement as unknown as FakeElement;
    insertedElement.layoutWidth = 80;
    insertedElement.layoutHeight = 50;
    existingElement.layoutLeft = 80;
    tree.endAnimation();

    expect(existingElement.style.translate).toBe('-80px 0px');
    expect(insertedElement.style.translate).toBeUndefined();
    flushTextAnimationFrame(500);
    expect(existingElement.style.translate).toBe('-40px 0px');
    flushTextAnimationFrame(1000);
    expect(existingElement.style.translate).toBeUndefined();
  });

  it('captures layout before reordering children', () => {
    const root = createRootTestElement('view');
    const first = createTestElement('view');
    const second = createTestElement('view');
    tree.moveElement(first, root, 0);
    tree.moveElement(second, root, 1);
    const rootElement = getNode(root).htmlElement as unknown as FakeElement;
    const firstElement = getNode(first).htmlElement as unknown as FakeElement;
    const secondElement = getNode(second).htmlElement as unknown as FakeElement;
    rootElement.layoutWidth = 300;
    rootElement.layoutHeight = 100;
    firstElement.layoutWidth = 100;
    firstElement.layoutHeight = 50;
    secondElement.layoutLeft = 100;
    secondElement.layoutWidth = 100;
    secondElement.layoutHeight = 50;

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 910);
    tree.moveElement(second, root, 0);
    firstElement.layoutLeft = 100;
    secondElement.layoutLeft = 0;
    tree.endAnimation();

    expect(firstElement.style.translate).toBe('-100px 0px');
    expect(secondElement.style.translate).toBe('100px 0px');
    flushTextAnimationFrame(1000);
    expect(firstElement.style.translate).toBeUndefined();
    expect(secondElement.style.translate).toBeUndefined();
  });

  it('captures layout before removing a child', () => {
    const root = createRootTestElement('view');
    const removed = createTestElement('view');
    const remaining = createTestElement('view');
    tree.moveElement(removed, root, 0);
    tree.moveElement(remaining, root, 1);
    const rootElement = getNode(root).htmlElement as unknown as FakeElement;
    const removedElement = getNode(removed).htmlElement as unknown as FakeElement;
    const remainingElement = getNode(remaining).htmlElement as unknown as FakeElement;
    rootElement.layoutWidth = 300;
    rootElement.layoutHeight = 100;
    removedElement.layoutWidth = 100;
    removedElement.layoutHeight = 50;
    remainingElement.layoutLeft = 100;
    remainingElement.layoutWidth = 100;
    remainingElement.layoutHeight = 50;

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 908);
    tree.destroyElement(removed);
    remainingElement.layoutLeft = 0;
    tree.endAnimation();

    expect(remainingElement.style.translate).toBe('100px 0px');
    flushTextAnimationFrame(500);
    expect(remainingElement.style.translate).toBe('50px 0px');
    flushTextAnimationFrame(1000);
    expect(remainingElement.style.translate).toBeUndefined();
  });

  it('animates sibling layout while retaining a child for exit appearance', () => {
    const root = createRootTestElement('view');
    const exiting = createTestElement('view');
    const remaining = createTestElement('view');
    tree.moveElement(exiting, root, 0);
    tree.moveElement(remaining, root, 1);
    const rootElement = getNode(root).htmlElement as unknown as FakeElement;
    const exitingElement = getNode(exiting).htmlElement as unknown as FakeElement;
    const remainingElement = getNode(remaining).htmlElement as unknown as FakeElement;
    rootElement.layoutWidth = 300;
    rootElement.layoutHeight = 100;
    exitingElement.layoutWidth = 100;
    exitingElement.layoutHeight = 50;
    remainingElement.layoutLeft = 100;
    remainingElement.layoutWidth = 100;
    remainingElement.layoutHeight = 50;

    tree.beginAnimation(
      {
        curve: AnimationCurve.Linear,
        duration: 1,
        appearanceBehavior: { exitAttributes: { opacity: 0 } },
      },
      911,
    );
    tree.destroyElement(exiting);
    remainingElement.layoutLeft = 0;
    tree.endAnimation();

    expect(tree.getNode(exiting)).toBeDefined();
    expect(remainingElement.style.translate).toBe('100px 0px');
    flushTextAnimationFrame(1000);
    expect(tree.getNode(exiting)).toBeUndefined();
    expect(remainingElement.style.translate).toBeUndefined();
  });

  it('keeps active layout animations whose frames survive an immediate tree mutation', () => {
    const root = createRootTestElement('view');
    const rootElement = getNode(root).htmlElement as unknown as FakeElement;
    rootElement.layoutWidth = 100;
    rootElement.layoutHeight = 100;
    reflectPixelStyleSizeInLayout(rootElement);
    tree.setAttributeOnElement(root, 'backgroundColor', 'red');
    tree.flush();
    const paintElement = getViewPaintElement(root);

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 909);
    tree.setAttributeOnElement(root, 'width', 200);
    tree.endAnimation();
    expect(paintElement.style.scale).toBe('0.5 1');

    const child = createTestElement('view');
    tree.moveElement(child, root, 0);
    tree.flush();

    expect(paintElement.style.scale).toBe('0.5 1');
    flushTextAnimationFrame(1000);
    expect(paintElement.style.scale).toBeUndefined();
  });

  it('cancels only layout animations whose scheduled frames changed', () => {
    const viewClass = registerTestElementClass({
      testLeft: {
        layoutDependent: true,
        apply(element, value) {
          (element as unknown as FakeElement).layoutLeft = Number(value);
        },
        reset(element) {
          (element as unknown as FakeElement).layoutLeft = 0;
        },
      },
    });
    const root = createRootTestElement('view');
    const first = createTestElement(viewClass);
    const second = createTestElement(viewClass);
    tree.moveElement(first, root, 0);
    tree.moveElement(second, root, 1);
    const rootElement = getNode(root).htmlElement as unknown as FakeElement;
    const firstElement = getNode(first).htmlElement as unknown as FakeElement;
    const secondElement = getNode(second).htmlElement as unknown as FakeElement;
    rootElement.layoutWidth = 400;
    rootElement.layoutHeight = 100;
    firstElement.layoutWidth = 50;
    firstElement.layoutHeight = 50;
    secondElement.layoutLeft = 100;
    secondElement.layoutWidth = 50;
    secondElement.layoutHeight = 50;

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 912);
    tree.setAttributeOnElement(first, 'testLeft', 100);
    tree.setAttributeOnElement(second, 'testLeft', 200);
    tree.endAnimation();
    expect(firstElement.style.translate).toBe('-100px 0px');
    expect(secondElement.style.translate).toBe('-100px 0px');

    const inserted = createTestElement('view');
    tree.moveElement(inserted, root, 0);
    firstElement.layoutLeft = 150;
    tree.flush();

    expect(firstElement.style.translate).toBeUndefined();
    expect(secondElement.style.translate).toBe('-100px 0px');
    flushTextAnimationFrame(500);
    expect(secondElement.style.translate).toBe('-50px 0px');
    flushTextAnimationFrame(1000);
    expect(secondElement.style.translate).toBeUndefined();
  });

  it('reprojects a surviving counter-translation after cancelling its animated parent', () => {
    const viewClass = registerTestElementClass({
      testLeft: {
        layoutDependent: true,
        apply(element, value) {
          (element as unknown as FakeElement).layoutLeft = Number(value);
        },
        reset(element) {
          (element as unknown as FakeElement).layoutLeft = 0;
        },
      },
    });
    const parent = createRootTestElement(viewClass);
    const child = createTestElement(viewClass);
    tree.moveElement(child, parent, 0);
    const parentElement = getNode(parent).htmlElement as unknown as FakeElement;
    const childElement = getNode(child).htmlElement as unknown as FakeElement;
    parentElement.layoutWidth = 100;
    parentElement.layoutHeight = 100;
    childElement.layoutWidth = 50;
    childElement.layoutHeight = 20;
    childElement.offsetParent = parentElement;

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 913);
    tree.setAttributeOnElement(parent, 'testLeft', 100);
    tree.setAttributeOnElement(child, 'testLeft', -100);
    tree.endAnimation();
    expect(parentElement.style.translate).toBe('-100px 0px');
    expect(childElement.style.translate).toBe('100px 0px');

    tree.beginRender();
    tree.setAttributeOnElement(parent, 'testLeft', 150);
    tree.setAttributeOnElement(child, 'testLeft', -150);
    tree.endRender();
    tree.flush();

    expect(parentElement.style.translate).toBeUndefined();
    expect(childElement.style.translate).toBe('0px 0px');
    flushTextAnimationFrame(1000);
    expect(childElement.style.translate).toBeUndefined();
  });

  it('scales generic view paint without scaling its text descendants', () => {
    const parentId = createRootTestElement('view');
    const textId = createTestElement('label');
    tree.moveElement(textId, parentId, 0);
    const parent = getNode(parentId).htmlElement as unknown as FakeElement;
    const text = getNode(textId).htmlElement as unknown as FakeElement;
    parent.layoutWidth = 100;
    parent.layoutHeight = 100;
    reflectPixelStyleSizeInLayout(parent);
    text.layoutWidth = 50;
    text.layoutHeight = 20;
    tree.setAttributeOnElement(parentId, 'backgroundColor', 'red');
    tree.flush();
    const paintElement = getViewPaintElement(parentId);

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 901);
    tree.setAttributeOnElement(parentId, 'width', 200);
    tree.endAnimation();

    expect(parent.style.scale).toBeUndefined();
    expect(paintElement.style.scale).toBe('0.5 1');
    expect(text.style.scale).toBeUndefined();
    flushTextAnimationFrame(1000);
    expect(paintElement.style.scale).toBeUndefined();
    expect(text.style.scale).toBeUndefined();
  });

  it('scales image nodes and compensates their transform origins', () => {
    const root = createRootTestElement('view');
    const cases = [
      { origin: 'center', scale: '0.5 0.5', translate: '-50px -40px' },
      { origin: 'left top', scale: '0.5 0.5', translate: '0px 0px' },
      { origin: '25% 75%', scale: '0.5 0.5', translate: '-25px -60px' },
      { origin: '12px 18px', scale: '0.5 0.5', translate: '-6px -9px' },
      { origin: 'center', originalScale: '2 3', scale: '1 1.5', translate: '-100px -120px' },
    ];
    const ids: number[] = [];
    for (const testCase of cases) {
      const id = createTestElement('image');
      ids.push(id);
      tree.moveElement(id, root, ids.length - 1);
      const element = getNode(id).htmlElement as unknown as FakeElement;
      element.layoutWidth = 100;
      element.layoutHeight = 80;
      reflectPixelStyleSizeInLayout(element);
      if (testCase.originalScale) {
        element.style.setProperty('scale', testCase.originalScale);
      }
      tree.setAttributeOnElement(id, 'transformOrigin', testCase.origin);
    }
    const rootElement = getNode(root).htmlElement as unknown as FakeElement;
    rootElement.layoutWidth = 500;
    rootElement.layoutHeight = 200;
    tree.flush();

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 915);
    for (const id of ids) {
      tree.setAttributeOnElement(id, 'width', 200);
      tree.setAttributeOnElement(id, 'height', 160);
    }
    tree.endAnimation();

    for (let index = 0; index < ids.length; index++) {
      const element = getNode(ids[index]).htmlElement;
      expect(element.style.scale).toBe(cases[index].scale);
      expect(element.style.translate).toBe(cases[index].translate);
    }

    flushTextAnimationFrame(1000);
    for (let index = 0; index < ids.length; index++) {
      const element = getNode(ids[index]).htmlElement;
      if (cases[index].originalScale) {
        expect(element.style.scale).toBe(cases[index].originalScale!);
      } else {
        expect(element.style.scale).toBeUndefined();
      }
      expect(element.style.translate).toBeUndefined();
    }
  });

  it('renders image content at the final layout size before scaling it in either direction', () => {
    const id = createRootTestElement('image');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.layoutWidth = 100;
    element.layoutHeight = 50;
    element.rectWidth = 100;
    element.rectHeight = 50;
    element.style.transformOrigin = 'center';
    reflectPixelStyleSizeInLayout(element);
    tree.setAttributeOnElement(id, 'src', 'test.png');
    tree.setAttributeOnElement(id, 'objectFit', 'cover');
    tree.setAttributeOnElement(id, 'width', 100);
    tree.setAttributeOnElement(id, 'height', 50);
    tree.flush();
    triggerImageLoad(300, 150);
    tree.drainScheduledLayoutObserverRefresh();
    const image = getLastImage();
    expect(image.style.width).toBe('100px');
    expect(image.style.height).toBe('50px');

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 916);
    tree.setAttributeOnElement(id, 'width', 200);
    tree.setAttributeOnElement(id, 'height', 100);
    tree.endAnimation();

    expect(element.style.scale).toBe('0.5 0.5');
    expect(image.style.width).toBe('200px');
    expect(image.style.height).toBe('100px');

    element.rectWidth = 100;
    element.rectHeight = 50;
    tree.drainScheduledLayoutObserverRefresh();
    expect(image.style.width).toBe('200px');
    expect(image.style.height).toBe('100px');

    flushTextAnimationFrame(1000);
    element.rectWidth = 200;
    element.rectHeight = 100;

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 917);
    tree.setAttributeOnElement(id, 'width', 100);
    tree.setAttributeOnElement(id, 'height', 50);
    tree.endAnimation();

    expect(element.style.scale).toBe('2 2');
    expect(image.style.width).toBe('100px');
    expect(image.style.height).toBe('50px');

    element.rectWidth = 200;
    element.rectHeight = 100;
    tree.drainScheduledLayoutObserverRefresh();
    expect(image.style.width).toBe('100px');
    expect(image.style.height).toBe('50px');

    flushTextAnimationFrame(2000);
    expect(element.style.scale).toBeUndefined();
  });

  it('snaps label size to final layout while animating its position', () => {
    const id = createRootTestElement('label');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.layoutWidth = 100;
    element.layoutHeight = 40;
    reflectPixelStyleSizeInLayout(element);
    reflectPixelStylePositionInLayout(element);

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 916);
    tree.setAttributeOnElement(id, 'left', 100);
    tree.setAttributeOnElement(id, 'width', 200);
    tree.endAnimation();

    expect(element.layoutWidth).toBe(200);
    expect(element.style.translate).toBe('-100px 0px');
    expect(element.style.scale).toBeUndefined();
    flushTextAnimationFrame(500);
    expect(element.style.translate).toBe('-50px 0px');
    expect(element.style.scale).toBeUndefined();
  });

  it('does not create size animation work for undecorated generic views', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.layoutWidth = 100;
    element.layoutHeight = 40;
    reflectPixelStyleSizeInLayout(element);
    const animationFrameCount = getAnimationFrameRequestCount();

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 917);
    tree.setAttributeOnElement(id, 'width', 200);
    tree.endAnimation();

    expect(element.childNodes.length).toBe(0);
    expect(element.layoutWidth).toBe(200);
    expect(element.style.scale).toBeUndefined();
    expect(getAnimationFrameRequestCount()).toBe(animationFrameCount);
  });

  it('does not capture layout for an animations-disabled subtree', () => {
    const viewClass = registerTestElementClass({
      animationsEnabled: {
        apply(_element, value, _attributeName, context) {
          context.setAnimationsEnabled(Boolean(value));
        },
        reset(_element, _attributeName, context) {
          context.setAnimationsEnabled(true);
        },
      },
      testWidth: {
        layoutDependent: true,
        apply(element, value) {
          (element as unknown as FakeElement).layoutWidth = Number(value);
        },
        reset(element) {
          (element as unknown as FakeElement).layoutWidth = 0;
        },
      },
    });
    const id = createRootTestElement(viewClass);
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.layoutWidth = 100;
    element.layoutHeight = 100;
    tree.setAttributeOnElement(id, 'animationsEnabled', false);
    tree.flush();
    element.layoutReadCount = 0;

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 902);
    tree.setAttributeOnElement(id, 'testWidth', 200);
    tree.endAnimation();

    expect(element.layoutReadCount).toBe(0);
    expect(element.layoutWidth).toBe(200);
    expect(element.style.scale).toBeUndefined();
    expect(getAnimationFrameRequestCount()).toBe(0);
  });

  it('removes layout projection when its transaction is cancelled', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.layoutWidth = 100;
    element.layoutHeight = 100;
    reflectPixelStyleSizeInLayout(element);
    tree.setAttributeOnElement(id, 'backgroundColor', 'red');
    tree.flush();
    const paintElement = getViewPaintElement(id);
    let completion: boolean | undefined;

    tree.beginAnimation(
      {
        curve: AnimationCurve.Linear,
        duration: 1,
        completion: cancelled => {
          completion = cancelled;
        },
      },
      903,
    );
    tree.setAttributeOnElement(id, 'width', 200);
    tree.endAnimation();
    expect(paintElement.style.scale).toBe('0.5 1');

    tree.cancelAnimation(903);

    expect(element.layoutWidth).toBe(200);
    expect(paintElement.style.scale).toBeUndefined();
    expect(completion).toBeTrue();
  });

  it('rebases interrupted layout animations from current or logical frames', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.layoutWidth = 100;
    element.layoutHeight = 100;
    reflectPixelStyleSizeInLayout(element);
    tree.setAttributeOnElement(id, 'backgroundColor', 'red');
    tree.flush();
    const paintElement = getViewPaintElement(id);

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 904);
    tree.setAttributeOnElement(id, 'width', 200);
    tree.endAnimation();
    flushTextAnimationFrame(500);
    expect(paintElement.style.scale).toBe('0.75 1');

    tree.beginAnimation({ beginFromCurrentState: true, curve: AnimationCurve.Linear, duration: 1 }, 905);
    tree.setAttributeOnElement(id, 'width', 300);
    tree.endAnimation();
    expect(paintElement.style.scale).toBe('0.5 1');

    tree.beginAnimation({ beginFromCurrentState: false, curve: AnimationCurve.Linear, duration: 1 }, 906);
    tree.setAttributeOnElement(id, 'width', 400);
    tree.endAnimation();
    expect(paintElement.style.scale).toBe('0.75 1');
  });

  it('keeps an interrupted layout animation stable after an external ancestor scrolls', () => {
    const viewClass = registerTestElementClass({
      testTop: {
        layoutDependent: true,
        apply(element, value) {
          (element as unknown as FakeElement).layoutTop = Number(value);
        },
        reset(element) {
          (element as unknown as FakeElement).layoutTop = 0;
        },
      },
    });
    const scrollContainer = makeFakeElement('div');
    const id = createTestElement(viewClass);
    tree.makeElementRoot(id, scrollContainer as unknown as HTMLElement);
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.layoutTop = 100;
    element.layoutWidth = 100;
    element.layoutHeight = 100;

    tree.beginAnimation({ beginFromCurrentState: true, curve: AnimationCurve.Linear, duration: 1 }, 920);
    tree.setAttributeOnElement(id, 'testTop', 200);
    tree.endAnimation();
    flushTextAnimationFrame(500);

    scrollContainer.scrollTop = 40;
    const visualYBeforeInterruption =
      element.layoutTop - scrollContainer.scrollTop + Number.parseFloat(element.style.translate.split(' ')[1]);

    tree.beginAnimation({ beginFromCurrentState: true, curve: AnimationCurve.Linear, duration: 1 }, 921);
    tree.setAttributeOnElement(id, 'testTop', 300);
    tree.endAnimation();

    const visualYAfterInterruption =
      element.layoutTop - scrollContainer.scrollTop + Number.parseFloat(element.style.translate.split(' ')[1]);
    expect(visualYBeforeInterruption).toBe(110);
    expect(visualYAfterInterruption).toBe(visualYBeforeInterruption);
  });

  it('keeps an interrupted layout animation stable after a Valdi scroll ancestor scrolls', () => {
    const viewClass = registerTestElementClass({
      testTop: {
        layoutDependent: true,
        apply(element, value) {
          (element as unknown as FakeElement).layoutTop = Number(value);
        },
        reset(element) {
          (element as unknown as FakeElement).layoutTop = 0;
        },
      },
    });
    const root = createRootTestElement('view');
    const scroller = createTestElement('scroll');
    const id = createTestElement(viewClass);
    tree.moveElement(scroller, root, 0);
    tree.moveElement(id, scroller, 0);
    const rootElement = getNode(root).htmlElement as unknown as FakeElement;
    const scrollElement = getNode(scroller).htmlElement as unknown as FakeElement;
    const element = getNode(id).htmlElement as unknown as FakeElement;
    rootElement.layoutWidth = 500;
    rootElement.layoutHeight = 500;
    scrollElement.layoutWidth = 300;
    scrollElement.layoutHeight = 300;
    scrollElement.offsetParent = rootElement;
    element.layoutTop = 100;
    element.layoutWidth = 100;
    element.layoutHeight = 100;
    element.offsetParent = scrollElement;

    tree.beginAnimation({ beginFromCurrentState: true, curve: AnimationCurve.Linear, duration: 1 }, 922);
    tree.setAttributeOnElement(id, 'testTop', 200);
    tree.endAnimation();
    flushTextAnimationFrame(500);

    scrollElement.scrollTop = 40;
    const visualYBeforeInterruption =
      element.layoutTop - scrollElement.scrollTop + Number.parseFloat(element.style.translate.split(' ')[1]);

    tree.beginAnimation({ beginFromCurrentState: true, curve: AnimationCurve.Linear, duration: 1 }, 923);
    tree.setAttributeOnElement(id, 'testTop', 300);
    tree.endAnimation();

    const visualYAfterInterruption =
      element.layoutTop - scrollElement.scrollTop + Number.parseFloat(element.style.translate.split(' ')[1]);
    expect(visualYBeforeInterruption).toBe(110);
    expect(visualYAfterInterruption).toBe(visualYBeforeInterruption);
  });

  it('samples preset and custom duration curves with shared frame timing', () => {
    const root = createRootTestElement('view');
    const curves: Array<{ options: AnimationOptions; id: number }> = [
      { id: createTestElement('view'), options: { curve: AnimationCurve.Linear, duration: 1 } },
      { id: createTestElement('view'), options: { curve: AnimationCurve.EaseIn, duration: 1 } },
      { id: createTestElement('view'), options: { curve: AnimationCurve.EaseOut, duration: 1 } },
      { id: createTestElement('view'), options: { curve: AnimationCurve.EaseInOut, duration: 1 } },
      { id: createTestElement('view'), options: { controlPoints: [0.2, 0.8, 0.2, 1], duration: 1 } },
    ];
    for (const curve of curves) {
      tree.moveElement(curve.id, root, 0);
      tree.setAttributeOnElement(curve.id, 'opacity', 0);
      tree.flush();
      tree.beginAnimation(curve.options, 100 + curve.id);
      tree.setAttributeOnElement(curve.id, 'opacity', 1);
      tree.endAnimation();
    }

    flushTextAnimationFrame(500);
    const values = curves.map(curve => Number(getNode(curve.id).htmlElement.style.opacity));
    expect(values[0]).toBeCloseTo(0.5, 4);
    expect(values[1]).toBeLessThan(values[0]);
    expect(values[2]).toBeGreaterThan(values[0]);
    expect(values[3]).toBeCloseTo(0.5, 4);
    expect(values[4]).toBeGreaterThan(values[2]);
  });

  it('finishes underdamped, critically damped, and overdamped springs', () => {
    const root = createRootTestElement('view');
    const dampingValues = [10, 20, 30];
    const completions: boolean[] = [];
    for (let index = 0; index < dampingValues.length; index++) {
      const id = createTestElement('view');
      tree.moveElement(id, root, 0);
      tree.setAttributeOnElement(id, 'opacity', 0);
      tree.flush();
      tree.beginAnimation(
        {
          stiffness: 100,
          damping: dampingValues[index],
          completion: cancelled => completions.push(cancelled),
        },
        200 + index,
      );
      tree.setAttributeOnElement(id, 'opacity', 1);
      tree.endAnimation();
    }

    for (let time = 16; time <= 10000 && completions.length < dampingValues.length; time += 16) {
      flushTextAnimationFrame(time);
    }
    expect(completions).toEqual([false, false, false]);
  });

  it('animates palette-ready colors and preserves the exact final color', () => {
    const id = createRootTestElement('view');
    tree.setAttributeOnElement(id, 'backgroundColor', '#000000');
    tree.flush();
    const paintElement = getViewPaintElement(id);

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 300);
    tree.setAttributeOnElement(id, 'backgroundColor', '#ffffff');
    tree.endAnimation();
    flushTextAnimationFrame(500);
    expect(paintElement.style.backgroundColor).toContain('color-mix(in srgb');
    flushTextAnimationFrame(1000);
    expect(paintElement.style.backgroundColor).toBe('#ffffff');
  });

  it('animates to the lower-priority style value when a direct value is removed', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement;
    tree.setStyleAttributeOnElement(id, 'style', new Style({ opacity: 0.8 }));
    tree.setAttributeOnElement(id, 'opacity', 0.2);
    tree.flush();

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 301);
    tree.setAttributeOnElement(id, 'opacity', undefined);
    tree.endAnimation();
    flushTextAnimationFrame(500);
    expect(Number(element.style.opacity)).toBeCloseTo(0.5, 5);
    flushTextAnimationFrame(1000);
    expect(element.style.opacity).toBe('0.8');
  });

  it('begins from the current opacity when interrupting and requested', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement;
    tree.setAttributeOnElement(id, 'opacity', 0);
    tree.flush();

    tree.beginAnimation({ beginFromCurrentState: true, curve: AnimationCurve.Linear, duration: 1 }, 2);
    tree.setAttributeOnElement(id, 'opacity', 1);
    tree.endAnimation();
    flushTextAnimationFrame(400);
    expect(Number(element.style.opacity)).toBeCloseTo(0.4, 5);

    tree.beginAnimation({ beginFromCurrentState: true, curve: AnimationCurve.Linear, duration: 1 }, 3);
    tree.setAttributeOnElement(id, 'opacity', 0);
    tree.endAnimation();
    expect(Number(element.style.opacity)).toBeCloseTo(0.4, 5);
    flushTextAnimationFrame(900);
    expect(Number(element.style.opacity)).toBeCloseTo(0.2, 5);
  });

  it('finishes an explicitly cancelled transaction at its exact target', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement;
    let completion: boolean | undefined;
    tree.setAttributeOnElement(id, 'opacity', 0);
    tree.flush();
    tree.beginAnimation({ duration: 1, completion: cancelled => (completion = cancelled) }, 4);
    tree.setAttributeOnElement(id, 'opacity', 1);
    tree.endAnimation();
    flushTextAnimationFrame(250);

    tree.cancelAnimation(4);
    expect(element.style.opacity).toBe('1');
    expect(completion).toBeTrue();
  });

  it('animates the top newly created node from its enter appearance attributes', () => {
    const root = createRootTestElement('view');
    const token = 401;

    tree.beginAnimation(
      {
        curve: AnimationCurve.Linear,
        duration: 1,
        appearanceBehavior: {
          enterAttributes: {
            originX: 0,
            originY: 1,
            translationX: -1,
            translationY: 0.5,
            scaleX: 0.5,
            scaleY: 0.25,
            opacity: 0,
          },
        },
      },
      token,
    );
    const parent = createTestElement('view');
    const child = createTestElement('view');
    tree.moveElement(parent, root, 0);
    tree.moveElement(child, parent, 0);
    tree.endAnimation();

    const parentElement = getNode(parent).htmlElement;
    const childElement = getNode(child).htmlElement;
    expect(parentElement.style.opacity).toBe('0');
    expect(parentElement.style.transformOrigin).toBe('0% 100%');
    expect(parentElement.style.transform).toContain('translate(-100%, 50%)');
    expect(parentElement.style.transform).toContain('scale(0.5, 0.25)');
    expect(childElement.style.opacity).toBeUndefined();
    expect(childElement.style.transform).toBeUndefined();

    flushTextAnimationFrame(500);
    expect(Number(parentElement.style.opacity)).toBeCloseTo(0.5, 5);
    expect(parentElement.style.transform).toContain('translate(-50%, 25%)');
    expect(parentElement.style.transform).toContain('scale(0.75, 0.625)');

    flushTextAnimationFrame(1000);
    tree.flush();
    expect(parentElement.style.opacity).toBe('');
    expect(parentElement.style.transform).toBe('');
    expect(parentElement.style.transformOrigin).toBe('');
  });

  it('keeps authored attributes above enter appearance attributes', () => {
    const root = createRootTestElement('view');
    tree.beginAnimation(
      {
        curve: AnimationCurve.Linear,
        duration: 1,
        appearanceBehavior: {
          enterAttributes: { opacity: 0, translationX: -1, scaleX: 0.5 },
        },
      },
      402,
    );
    const child = createTestElement('view');
    tree.setAttributeOnElement(child, 'opacity', 0.7);
    tree.setAttributeOnElement(child, 'translationX', 20);
    tree.moveElement(child, root, 0);
    tree.endAnimation();

    const element = getNode(child).htmlElement;
    expect(element.style.opacity).toBe('0.7');
    expect(element.style.transform).toContain('translate(20px, 0px)');
    flushTextAnimationFrame(1000);
    expect(element.style.opacity).toBe('0.7');
    expect(element.style.transform).toContain('translate(20px, 0px)');
  });

  it('freezes and retains an exiting subtree until its appearance animation completes', () => {
    const root = createRootTestElement('view');
    const child = createTestElement('view');
    const grandchild = createTestElement('view');
    tree.moveElement(child, root, 0);
    tree.moveElement(grandchild, child, 0);
    let destroyCount = 0;
    tree.setAttributeOnElement(child, 'onViewDestroy', () => destroyCount++);
    tree.setAttributeOnElement(child, 'position', 'relative');
    tree.setAttributeOnElement(child, 'left', 2);
    tree.setAttributeOnElement(child, 'top', 3);
    tree.setAttributeOnElement(child, 'width', 20);
    tree.setAttributeOnElement(child, 'height', 10);
    tree.setAttributeOnElement(child, 'marginLeft', 20);
    tree.setAttributeOnElement(child, 'marginTop', 14);
    tree.flush();

    const element = getNode(child).htmlElement as unknown as FakeElement;
    const grandchildElement = getNode(grandchild).htmlElement;
    element.layoutLeft = 12;
    element.layoutTop = 18;
    element.layoutWidth = 80;
    element.layoutHeight = 40;

    tree.beginAnimation(
      {
        curve: AnimationCurve.Linear,
        duration: 1,
        appearanceBehavior: {
          exitAttributes: {
            originX: 1,
            originY: 0,
            translationX: 1,
            translationY: -0.5,
            scaleX: 0.5,
            scaleY: 0.25,
            opacity: 0,
          },
        },
      },
      403,
    );
    tree.destroyElement(child);
    tree.endAnimation();

    expect(tree.getNode(child)).toBeDefined();
    expect(tree.getNode(grandchild)).toBeDefined();
    expect(element.parentElement).not.toBeNull();
    expect(element.style.position).toBe('absolute');
    expect(element.style.left).toBe('12px');
    expect(element.style.top).toBe('18px');
    expect(element.style.width).toBe('80px');
    expect(element.style.height).toBe('40px');
    expect(element.style.marginLeft).toBe('0px');
    expect(element.style.marginTop).toBe('0px');
    expect(element.layoutReadCount).toBe(4);
    expect(grandchildElement.style.opacity).toBeUndefined();
    expect(destroyCount).toBe(0);

    flushTextAnimationFrame(500);
    expect(Number(element.style.opacity)).toBeCloseTo(0.5, 5);
    expect(element.style.transformOrigin).toBe('100% 0%');
    expect(element.style.transform).toContain('translate(50%, -25%)');
    expect(element.style.transform).toContain('scale(0.75, 0.625)');

    flushTextAnimationFrame(1000);
    expect(tree.getNode(child)).toBeUndefined();
    expect(tree.getNode(grandchild)).toBeUndefined();
    expect(element.parentElement).toBeNull();
    tree.flush();
    expect(destroyCount).toBe(1);
  });

  it('inserts live siblings independently of a retained exiting element', () => {
    const root = createRootTestElement('view');
    const first = createTestElement('view');
    const second = createTestElement('view');
    tree.moveElement(first, root, 0);
    tree.moveElement(second, root, 1);
    tree.flush();
    const rootElement = getNode(root).htmlElement as unknown as FakeElement;
    const firstElement = getNode(first).htmlElement as unknown as FakeElement;
    const secondElement = getNode(second).htmlElement as unknown as FakeElement;

    tree.beginAnimation(
      {
        curve: AnimationCurve.Linear,
        duration: 1,
        appearanceBehavior: { exitAttributes: { opacity: 0 } },
      },
      404,
    );
    tree.destroyElement(first);
    const replacement = createTestElement('view');
    const replacementElement = getNode(replacement).htmlElement as unknown as FakeElement;
    tree.moveElement(replacement, root, 0);
    tree.endAnimation();

    expect(rootElement.childNodes.item(0)).toBe(firstElement);
    expect(rootElement.childNodes.item(1)).toBe(replacementElement);
    expect(rootElement.childNodes.item(2)).toBe(secondElement);

    flushTextAnimationFrame(1000);
    expect(rootElement.childNodes.item(0)).toBe(replacementElement);
    expect(rootElement.childNodes.item(1)).toBe(secondElement);
    expect(rootElement.childNodes.item(2)).toBeNull();
  });

  it('destroys a node created and removed in the same appearance transaction immediately', () => {
    const root = createRootTestElement('view');
    const initialFrameRequests = getAnimationFrameRequestCount();
    let completion: boolean | undefined;
    tree.beginAnimation(
      {
        curve: AnimationCurve.Linear,
        duration: 1,
        appearanceBehavior: {
          enterAttributes: { opacity: 0 },
          exitAttributes: { opacity: 0 },
        },
        completion: cancelled => (completion = cancelled),
      },
      405,
    );
    const child = createTestElement('view');
    tree.moveElement(child, root, 0);
    tree.destroyElement(child);
    tree.endAnimation();

    expect(tree.getNode(child)).toBeUndefined();
    expect(completion).toBeFalse();
    expect(getAnimationFrameRequestCount()).toBe(initialFrameRequests);
  });

  it('destroys a cancelled exit at its final target and reports cancellation once', () => {
    const root = createRootTestElement('view');
    const child = createTestElement('view');
    tree.moveElement(child, root, 0);
    tree.flush();
    let completionCount = 0;
    let wasCancelled: boolean | undefined;

    tree.beginAnimation(
      {
        duration: 1,
        appearanceBehavior: { exitAttributes: { opacity: 0 } },
        completion: cancelled => {
          completionCount++;
          wasCancelled = cancelled;
        },
      },
      406,
    );
    tree.destroyElement(child);
    tree.endAnimation();
    flushTextAnimationFrame(250);
    expect(tree.getNode(child)).toBeDefined();

    tree.cancelAnimation(406);
    tree.cancelAnimation(406);
    expect(tree.getNode(child)).toBeUndefined();
    expect(completionCount).toBe(1);
    expect(wasCancelled).toBeTrue();
  });

  it('begins an interrupted exit from the current enter presentation when requested', () => {
    const root = createRootTestElement('view');
    tree.beginAnimation(
      {
        curve: AnimationCurve.Linear,
        duration: 1,
        appearanceBehavior: { enterAttributes: { opacity: 0 } },
      },
      411,
    );
    const child = createTestElement('view');
    tree.moveElement(child, root, 0);
    tree.endAnimation();
    const element = getNode(child).htmlElement;
    flushTextAnimationFrame(400);
    expect(Number(element.style.opacity)).toBeCloseTo(0.4, 5);

    tree.beginAnimation(
      {
        beginFromCurrentState: true,
        curve: AnimationCurve.Linear,
        duration: 1,
        appearanceBehavior: { exitAttributes: { opacity: 0 } },
      },
      412,
    );
    tree.destroyElement(child);
    tree.endAnimation();
    expect(Number(element.style.opacity)).toBeCloseTo(0.4, 5);
    flushTextAnimationFrame(900);
    expect(Number(element.style.opacity)).toBeCloseTo(0.2, 5);
  });

  it('begins an interrupted exit from the enter destination by default', () => {
    const root = createRootTestElement('view');
    tree.beginAnimation(
      {
        curve: AnimationCurve.Linear,
        duration: 1,
        appearanceBehavior: { enterAttributes: { opacity: 0 } },
      },
      413,
    );
    const child = createTestElement('view');
    tree.moveElement(child, root, 0);
    tree.endAnimation();
    const element = getNode(child).htmlElement;
    flushTextAnimationFrame(400);
    expect(Number(element.style.opacity)).toBeCloseTo(0.4, 5);

    tree.beginAnimation(
      {
        curve: AnimationCurve.Linear,
        duration: 1,
        appearanceBehavior: { exitAttributes: { opacity: 0 } },
      },
      414,
    );
    tree.destroyElement(child);
    tree.endAnimation();
    expect(element.style.opacity).toBe('1');
  });

  it('keeps authored values above exit appearance attributes until removal', () => {
    const root = createRootTestElement('view');
    const child = createTestElement('view');
    tree.moveElement(child, root, 0);
    tree.setAttributeOnElement(child, 'opacity', 0.7);
    tree.setAttributeOnElement(child, 'translationX', 20);
    tree.flush();
    const element = getNode(child).htmlElement;

    tree.beginAnimation(
      {
        curve: AnimationCurve.Linear,
        duration: 1,
        appearanceBehavior: { exitAttributes: { opacity: 0, translationX: 1 } },
      },
      415,
    );
    tree.destroyElement(child);
    tree.endAnimation();
    flushTextAnimationFrame(500);

    expect(element.style.opacity).toBe('0.7');
    expect(element.style.transform).toContain('translate(20px, 0px)');
    expect(tree.getNode(child)).toBeDefined();
    flushTextAnimationFrame(1000);
    expect(tree.getNode(child)).toBeUndefined();
  });

  it('bypasses appearance work in disabled subtrees', () => {
    const root = createRootTestElement('view');
    const parent = createTestElement('view');
    tree.moveElement(parent, root, 0);
    tree.setAttributeOnElement(parent, 'animationsEnabled', false);
    tree.flush();
    const initialFrameRequests = getAnimationFrameRequestCount();

    tree.beginAnimation(
      {
        duration: 1,
        appearanceBehavior: {
          enterAttributes: { opacity: 0 },
          exitAttributes: { opacity: 0 },
        },
      },
      407,
    );
    const child = createTestElement('view');
    tree.moveElement(child, parent, 0);
    tree.endAnimation();
    expect(getNode(child).htmlElement.style.opacity).toBeUndefined();
    expect(getAnimationFrameRequestCount()).toBe(initialFrameRequests);

    tree.beginAnimation(
      {
        duration: 1,
        appearanceBehavior: { exitAttributes: { opacity: 0 } },
      },
      408,
    );
    tree.destroyElement(child);
    tree.endAnimation();
    expect(tree.getNode(child)).toBeUndefined();
    expect(getAnimationFrameRequestCount()).toBe(initialFrameRequests);
  });

  it('bypasses exit appearance when animations are disabled on the node in the same update', () => {
    const root = createRootTestElement('view');
    const child = createTestElement('view');
    tree.moveElement(child, root, 0);
    tree.flush();
    const initialFrameRequests = getAnimationFrameRequestCount();

    tree.beginAnimation(
      {
        duration: 1,
        appearanceBehavior: { exitAttributes: { opacity: 0 } },
      },
      416,
    );
    tree.setAttributeOnElement(child, 'animationsEnabled', false);
    tree.destroyElement(child);
    tree.endAnimation();

    expect(tree.getNode(child)).toBeUndefined();
    expect(getAnimationFrameRequestCount()).toBe(initialFrameRequests);
  });

  it('bypasses enter appearance when animations are disabled on the node in the same update', () => {
    const root = createRootTestElement('view');
    const initialFrameRequests = getAnimationFrameRequestCount();

    tree.beginAnimation(
      {
        duration: 1,
        appearanceBehavior: { enterAttributes: { opacity: 0, scaleX: 0.5 } },
      },
      417,
    );
    const child = createTestElement('view');
    tree.setAttributeOnElement(child, 'animationsEnabled', false);
    tree.moveElement(child, root, 0);
    tree.endAnimation();

    const element = getNode(child).htmlElement;
    expect(element.style.opacity).toBe('');
    expect(element.style.transform).toBe('');
    expect(getAnimationFrameRequestCount()).toBe(initialFrameRequests);
  });

  it('completes a pending exit when animations are disabled on its ancestor', () => {
    const root = createRootTestElement('view');
    const parent = createTestElement('view');
    const child = createTestElement('view');
    tree.moveElement(parent, root, 0);
    tree.moveElement(child, parent, 0);
    tree.flush();
    const element = getNode(child).htmlElement;

    tree.beginAnimation(
      {
        curve: AnimationCurve.Linear,
        duration: 1,
        appearanceBehavior: { exitAttributes: { opacity: 0 } },
      },
      409,
    );
    tree.destroyElement(child);
    tree.endAnimation();
    flushTextAnimationFrame(250);
    expect(Number(element.style.opacity)).toBeCloseTo(0.75, 5);

    tree.setAttributeOnElement(parent, 'animationsEnabled', false);
    tree.flush();
    expect(element.style.opacity).toBe('0');
    expect(tree.getNode(child)).toBeUndefined();
  });

  it('cancels appearance lifecycles without final writes when the tree is destroyed', () => {
    const root = createRootTestElement('view');
    const child = createTestElement('view');
    tree.moveElement(child, root, 0);
    tree.flush();
    const element = getNode(child).htmlElement as unknown as FakeElement;
    let completion: boolean | undefined;

    tree.beginAnimation(
      {
        duration: 1,
        appearanceBehavior: { exitAttributes: { opacity: 0 } },
        completion: cancelled => (completion = cancelled),
      },
      410,
    );
    tree.destroyElement(child);
    tree.endAnimation();
    flushTextAnimationFrame(250);
    const opacityBeforeDestroy = element.style.opacity;

    tree.destroy();
    expect(element.style.opacity).toBe(opacityBeforeDestroy);
    expect(element.parentElement).toBeNull();
    expect(completion).toBeTrue();
  });

  it('does not read layout or schedule animation frames for ordinary destruction', () => {
    const root = createRootTestElement('view');
    const child = createTestElement('view');
    tree.moveElement(child, root, 0);
    tree.flush();
    const element = getNode(child).htmlElement as unknown as FakeElement;
    const initialFrameRequests = getAnimationFrameRequestCount();

    tree.destroyElement(child);

    expect(element.layoutReadCount).toBe(0);
    expect(getAnimationFrameRequestCount()).toBe(initialFrameRequests);
  });

  it('does not schedule animation work for appearance origins without transforms', () => {
    const root = createRootTestElement('view');
    const initialFrameRequests = getAnimationFrameRequestCount();
    let completion: boolean | undefined;

    tree.beginAnimation(
      {
        duration: 1,
        appearanceBehavior: { enterAttributes: { originX: 0, originY: 1 } },
        completion: cancelled => (completion = cancelled),
      },
      419,
    );
    const child = createTestElement('view');
    tree.moveElement(child, root, 0);
    tree.endAnimation();

    expect(completion).toBeFalse();
    expect(getAnimationFrameRequestCount()).toBe(initialFrameRequests);
  });

  it('animates component transforms atomically', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement;
    tree.setAttributeOnElement(id, 'translationX', 0);
    tree.setAttributeOnElement(id, 'translationY', 0);
    tree.setAttributeOnElement(id, 'scaleX', 1);
    tree.setAttributeOnElement(id, 'scaleY', 1);
    tree.setAttributeOnElement(id, 'rotation', 0);
    tree.flush();

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 5);
    tree.setAttributeOnElement(id, 'translationX', 100);
    tree.setAttributeOnElement(id, 'translationY', 40);
    tree.setAttributeOnElement(id, 'scaleX', 2);
    tree.setAttributeOnElement(id, 'scaleY', 0.5);
    tree.setAttributeOnElement(id, 'rotation', 1);
    tree.endAnimation();
    flushTextAnimationFrame(500);

    expect(element.style.transform).toContain('translate(50px, 20px)');
    expect(element.style.transform).toContain('scale(1.5, 0.75)');
    expect(element.style.transform).toContain('rotate(0.5rad)');
  });

  it('keeps transform origin changes immediate inside animation transactions', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement;
    tree.setAttributeOnElement(id, 'transformOrigin', '0% 0%');
    tree.flush();
    const initialFrameRequests = getAnimationFrameRequestCount();
    let completion: boolean | undefined;

    tree.beginAnimation(
      {
        curve: AnimationCurve.Linear,
        duration: 1,
        completion: cancelled => (completion = cancelled),
      },
      418,
    );
    tree.setAttributeOnElement(id, 'transformOrigin', '100% 100%');
    tree.endAnimation();

    expect(element.style.transformOrigin).toBe('100% 100%');
    expect(completion).toBeFalse();
    expect(getAnimationFrameRequestCount()).toBe(initialFrameRequests);
  });

  it('animates a component transform from its unset defaults', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement;

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 51);
    tree.setAttributeOnElement(id, 'translationX', 100);
    tree.setAttributeOnElement(id, 'scaleX', 2);
    tree.endAnimation();
    flushTextAnimationFrame(500);

    expect(element.style.transform).toContain('translate(50px, 0px)');
    expect(element.style.transform).toContain('scale(1.5, 1)');
  });

  it('animates a component transform to its unset defaults before resetting it', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement;
    tree.setAttributeOnElement(id, 'translationX', 100);
    tree.setAttributeOnElement(id, 'scaleX', 2);
    tree.flush();

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 52);
    tree.setAttributeOnElement(id, 'translationX', undefined);
    tree.setAttributeOnElement(id, 'scaleX', undefined);
    tree.endAnimation();
    flushTextAnimationFrame(500);

    expect(element.style.transform).toContain('translate(50px, 0px)');
    expect(element.style.transform).toContain('scale(1.5, 1)');
    flushTextAnimationFrame(1000);
    expect(element.style.transform).toBe('');
  });

  it('applies an unsupported mixed-unit translation immediately', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement;
    let completion: boolean | undefined;
    tree.setAttributeOnElement(id, 'translationX', '10%');
    tree.flush();

    tree.beginAnimation({ duration: 1, completion: cancelled => (completion = cancelled) }, 6);
    tree.setAttributeOnElement(id, 'translationX', '20px');
    tree.endAnimation();

    expect(element.style.transform).toContain('translate(20px, 0px)');
    expect(completion).toBeFalse();
  });

  it('accepts the translation dimensions supported by ValueConverter', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement;

    tree.setAttributeOnElement(id, 'translationX', '12');
    tree.setAttributeOnElement(id, 'translationY', '25%');
    tree.flush();
    expect(element.style.transform).toContain('translate(12px, 25%)');

    tree.setAttributeOnElement(id, 'translationX', '7pt');
    tree.setAttributeOnElement(id, 'translationY', '9px');
    tree.flush();
    expect(element.style.transform).toContain('translate(7px, 9px)');

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 61);
    tree.setAttributeOnElement(id, 'translationX', '17px');
    tree.endAnimation();
    flushTextAnimationFrame(500);
    expect(element.style.transform).toContain('translate(12px, 9px)');
  });

  it('rejects translation units that ValueConverter does not support', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement;
    const errorSpy = spyOn(console, 'error');
    tree.setAttributeOnElement(id, 'translationX', 5);
    tree.flush();

    tree.setAttributeOnElement(id, 'translationX', '1em');
    tree.flush();

    expect(element.style.transform).toContain('translate(5px, 0px)');
    expect(errorSpy).toHaveBeenCalledWith(jasmine.stringContaining('unitless, px, pt, or percent'));
  });

  it('resets an unset animated opacity only after applying intermediate frames', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement;
    tree.setAttributeOnElement(id, 'opacity', 0.25);
    tree.flush();

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 7);
    tree.setAttributeOnElement(id, 'opacity', undefined);
    tree.endAnimation();
    flushTextAnimationFrame(500);
    expect(Number(element.style.opacity)).toBeCloseTo(0.625, 5);
    flushTextAnimationFrame(1000);
    expect(element.style.opacity).toBe('');
  });

  it('animates border radius shorthand values and applies the exact final value', () => {
    const id = createRootTestElement('view');
    tree.setAttributeOnElement(id, 'borderRadius', 4);
    tree.flush();
    const paintElement = getViewPaintElement(id);

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 71);
    tree.setAttributeOnElement(id, 'borderRadius', '20 30 40 50');
    tree.endAnimation();
    flushTextAnimationFrame(500);

    expect(paintElement.style.borderRadius).toBe('12px 17px 22px 27px');
    flushTextAnimationFrame(1000);
    expect(paintElement.style.borderRadius).toBe('20px 30px 40px 50px');
  });

  it('animates mixed point and percent border radii using the last observed size', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.rectWidth = 200;
    element.rectHeight = 80;
    tree.setAttributeOnElement(id, 'borderRadius', 8);
    tree.flush();
    const paintElement = getViewPaintElement(id);

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 72);
    tree.setAttributeOnElement(id, 'borderRadius', '50%');
    tree.endAnimation();
    const readsAfterCommit = element.rectReadCount;
    flushTextAnimationFrame(500);

    expect(paintElement.style.borderRadius).toBe('24px');
    expect(element.rectReadCount).toBe(readsAfterCommit);

    element.rectHeight = 40;
    dispatchWindowResize();
    expect(paintElement.style.borderRadius).toBe('14px');

    flushTextAnimationFrame(1000);
    expect(paintElement.style.borderRadius).toBe('20px');
  });

  it('begins an interrupted border radius animation from its current presentation', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.rectWidth = 100;
    element.rectHeight = 100;
    tree.setAttributeOnElement(id, 'borderRadius', 0);
    tree.flush();
    const paintElement = getViewPaintElement(id);

    tree.beginAnimation({ beginFromCurrentState: true, curve: AnimationCurve.Linear, duration: 1 }, 73);
    tree.setAttributeOnElement(id, 'borderRadius', '100%');
    tree.endAnimation();
    flushTextAnimationFrame(400);
    expect(paintElement.style.borderRadius).toBe('40px');

    tree.beginAnimation({ beginFromCurrentState: true, curve: AnimationCurve.Linear, duration: 1 }, 74);
    tree.setAttributeOnElement(id, 'borderRadius', 0);
    tree.endAnimation();
    expect(paintElement.style.borderRadius).toBe('40px');
    flushTextAnimationFrame(900);
    expect(paintElement.style.borderRadius).toBe('20px');
  });

  it('animates border radius to its unset default before resetting it', () => {
    const id = createRootTestElement('view');
    tree.setAttributeOnElement(id, 'borderRadius', 20);
    tree.flush();
    const paintElement = getViewPaintElement(id);

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 75);
    tree.setAttributeOnElement(id, 'borderRadius', undefined);
    tree.endAnimation();
    flushTextAnimationFrame(500);
    expect(paintElement.style.borderRadius).toBe('10px');
    flushTextAnimationFrame(1000);
    expect(paintElement.style.borderRadius).toBe('');
  });

  it('animates blur border radius and clip path through the shared applier', () => {
    const id = createRootTestElement('blur');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.rectWidth = 80;
    element.rectHeight = 40;
    tree.setAttributeOnElement(id, 'borderRadius', 0);
    tree.flush();

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 76);
    tree.setAttributeOnElement(id, 'borderRadius', '50%');
    tree.endAnimation();
    flushTextAnimationFrame(500);

    expect(element.style.borderRadius).toBe('10px');
    expect(element.style.clipPath).toBe('inset(0 round 10px)');
    flushTextAnimationFrame(1000);
    expect(element.style.borderRadius).toBe('20px');
    expect(element.style.clipPath).toBe('inset(0 round 20px)');
  });

  it('applies pixel units and clipping to both glass corner-radius attributes', () => {
    const id = createRootTestElement('glass');
    const element = getNode(id).htmlElement;

    tree.setAttributeOnElement(id, 'borderRadius', 22);
    tree.flush();
    expect(element.style.borderRadius).toBe('22px');
    expect(element.style.clipPath).toBe('inset(0 round 22px)');

    tree.setAttributeOnElement(id, 'glassCornerRadius', 14);
    tree.flush();
    expect(element.style.borderRadius).toBe('14px');
    expect(element.style.clipPath).toBe('inset(0 round 14px)');
  });

  it('applies unsupported border radius expressions immediately', () => {
    const id = createRootTestElement('view');
    let completion: boolean | undefined;
    tree.setAttributeOnElement(id, 'borderRadius', 8);
    tree.flush();
    const paintElement = getViewPaintElement(id);

    tree.beginAnimation({ duration: 1, completion: cancelled => (completion = cancelled) }, 77);
    tree.setAttributeOnElement(id, 'borderRadius', 'calc(10px + 2%)');
    tree.endAnimation();

    expect(paintElement.style.borderRadius).toBe('calc(10px + 2%)');
    expect(completion).toBeFalse();
  });

  it('finishes active descendant properties when animations are disabled on an ancestor', () => {
    const parent = createRootTestElement('view');
    const child = createTestElement('view');
    tree.moveElement(child, parent, 0);
    const childElement = getNode(child).htmlElement;
    tree.setAttributeOnElement(child, 'opacity', 0);
    tree.flush();

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 8);
    tree.setAttributeOnElement(child, 'opacity', 1);
    tree.endAnimation();
    flushTextAnimationFrame(250);
    expect(Number(childElement.style.opacity)).toBeCloseTo(0.25, 5);

    tree.setAttributeOnElement(parent, 'animationsEnabled', false);
    tree.flush();
    expect(childElement.style.opacity).toBe('1');

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 82);
    tree.setAttributeOnElement(child, 'opacity', 0);
    tree.endAnimation();
    expect(childElement.style.opacity).toBe('0');

    tree.setAttributeOnElement(parent, 'animationsEnabled', true);
    tree.flush();
    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 83);
    tree.setAttributeOnElement(child, 'opacity', 1);
    tree.endAnimation();
    expect(childElement.style.opacity).toBe('0');
  });

  it('finishes or bypasses animations when animations are disabled in the same flush', () => {
    const parent = createRootTestElement('view');
    const animationFirst = createTestElement('view');
    const disableFirst = createTestElement('view');
    tree.moveElement(animationFirst, parent, 0);
    tree.moveElement(disableFirst, parent, 1);
    tree.setAttributeOnElement(animationFirst, 'opacity', 0);
    tree.setAttributeOnElement(disableFirst, 'opacity', 0);
    tree.flush();

    tree.beginAnimation({ curve: AnimationCurve.Linear, duration: 1 }, 81);
    tree.setAttributeOnElement(animationFirst, 'animationsEnabled', false);
    tree.setAttributeOnElement(animationFirst, 'opacity', 1);
    tree.setAttributeOnElement(disableFirst, 'opacity', 1);
    tree.setAttributeOnElement(disableFirst, 'animationsEnabled', false);
    tree.endAnimation();

    expect(getNode(animationFirst).htmlElement.style.opacity).toBe('1');
    expect(getNode(disableFirst).htmlElement.style.opacity).toBe('1');
  });

  it('resolves percent border radii against the shorter element side', () => {
    tree.setPostLayoutScheduler(callback => callback());
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.rectWidth = 84;
    element.rectHeight = 48;

    tree.setAttributeOnElement(id, 'borderRadius', '100%');
    tree.flush();
    const paintElement = getViewPaintElement(id);
    expect(paintElement.style.borderRadius).toBe('48px');

    tree.setAttributeOnElement(id, 'borderRadius', '8 50% 100% 0');
    tree.flush();
    expect(paintElement.style.borderRadius).toBe('8px 24px 48px 0px');
  });

  it('does not read geometry while layout-dependent attributes are being flushed', () => {
    let runLayoutPass: (() => void) | undefined;
    tree.setPostLayoutScheduler(callback => {
      runLayoutPass = callback;
    });
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    const parent = element.parentElement!;
    parent.rectWidth = 320;
    parent.rectHeight = 200;

    tree.setAttributeOnElement(id, 'borderRadius', '100%');
    tree.setAttributeOnElement(id, 'onMeasure', () => [180, 76]);
    tree.flush();

    expect(element.rectReadCount).toBe(0);
    expect(parent.rectReadCount).toBe(0);
    expect(runLayoutPass).toBeDefined();

    runLayoutPass!();
    expect(element.rectReadCount).toBe(2);
    expect(parent.rectReadCount).toBe(1);
  });

  it('runs layout observers for changed layout attributes and structural moves only', () => {
    const observerViewClass = registerTestElementClass({
      onMeasure: {
        apply(_element, value, attributeName, context) {
          context.setLayoutObserver(attributeName, {
            onMeasure() {
              (value as Function)();
            },
          });
        },
        reset(_element, attributeName, context) {
          context.setLayoutObserver(attributeName, undefined);
        },
      },
      layoutValue: {
        layoutDependent: true,
        apply() {},
        reset() {},
      },
      paintValue: {
        apply() {},
        reset() {},
      },
    });
    const rootId = createRootTestElement(observerViewClass);
    tree.flush();
    let rootLayoutCount = 0;
    tree.setAttributeOnElement(rootId, 'onMeasure', () => rootLayoutCount++);
    tree.flush();
    expect(rootLayoutCount).toBe(1);

    tree.setAttributeOnElement(rootId, 'paintValue', 'red');
    tree.flush();
    expect(rootLayoutCount).toBe(1);

    tree.setAttributeOnElement(rootId, 'layoutValue', 100);
    tree.flush();
    expect(rootLayoutCount).toBe(2);
    tree.setAttributeOnElement(rootId, 'layoutValue', 100);
    tree.flush();
    expect(rootLayoutCount).toBe(2);

    const childId = createTestElement(observerViewClass);
    let childLayoutCount = 0;
    tree.setAttributeOnElement(childId, 'onMeasure', () => childLayoutCount++);
    tree.flush();
    expect(childLayoutCount).toBe(0);

    tree.moveElement(childId, rootId, 0);
    tree.flush();
    expect(childLayoutCount).toBe(1);
  });

  it('updates percent border radii through the centralized browser resize pass', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.rectWidth = 80;
    element.rectHeight = 40;

    tree.setAttributeOnElement(id, 'borderRadius', '100%');
    tree.flush();
    const paintElement = getViewPaintElement(id);
    expect(paintElement.style.borderRadius).toBe('40px');

    element.rectWidth = 60;
    element.rectHeight = 32;
    dispatchWindowResize();
    expect(paintElement.style.borderRadius).toBe('32px');
  });

  it('removes attribute layout observers on reset and node destruction', () => {
    const resetId = createRootTestElement('view');
    const resetElement = getNode(resetId).htmlElement as unknown as FakeElement;
    resetElement.rectWidth = 80;
    resetElement.rectHeight = 40;
    tree.setAttributeOnElement(resetId, 'borderRadius', '100%');
    tree.flush();

    tree.setAttributeOnElement(resetId, 'borderRadius', undefined);
    tree.flush();
    resetElement.rectReadCount = 0;
    dispatchWindowResize();
    expect(resetElement.rectReadCount).toBe(0);

    const destroyedId = createRootTestElement('view');
    const destroyedElement = getNode(destroyedId).htmlElement as unknown as FakeElement;
    destroyedElement.rectWidth = 80;
    destroyedElement.rectHeight = 40;
    tree.setAttributeOnElement(destroyedId, 'borderRadius', '100%');
    tree.flush();
    tree.destroyElement(destroyedId);
    destroyedElement.rectReadCount = 0;
    dispatchWindowResize();
    expect(destroyedElement.rectReadCount).toBe(0);
  });

  it('keeps boolean false as a real attribute value', () => {
    const id = createRootTestElement('view');

    tree.setAttributeOnElement(id, 'touchEnabled', false);
    tree.flush();
    expect(getNode(id).htmlElement.style.pointerEvents).toBe('none');

    tree.setAttributeOnElement(id, 'touchEnabled', true);
    tree.flush();
    expect(getNode(id).htmlElement.style.pointerEvents).toBe('auto');
  });

  it('keeps layout views pointer-transparent while browser controls remain interactive', () => {
    const layoutId = createTestElement('layout');
    const viewId = createTestElement('view');
    const scrollId = createTestElement('scroll');
    const textFieldId = createTestElement('textfield');
    const textViewId = createTestElement('textview');

    expect(getNode(layoutId).htmlElement.style.pointerEvents).toBe('none');
    expect(getNode(viewId).htmlElement.style.pointerEvents).toBe('none');
    expect(getNode(scrollId).htmlElement.style.pointerEvents).toBe('auto');
    expect(getNode(textFieldId).htmlElement.style.pointerEvents).toBe('auto');
    expect(getNode(textViewId).htmlElement.style.pointerEvents).toBe('auto');
  });

  it('exports a serializable debug snapshot for the rooted tree', () => {
    const root = createRootTestElement('view');
    const child = createTestElement('label');
    tree.moveElement(child, root, 0);
    tree.setAttributeOnElement(root, 'width', 42);
    tree.setAttributeOnElement(child, 'value', 'Hello debugger');

    const rootElement = getNode(root).htmlElement as unknown as FakeElement;
    rootElement.rectLeft = 4;
    rootElement.rectTop = 8;
    rootElement.rectWidth = 200;
    rootElement.rectHeight = 120;
    const childElement = getNode(child).htmlElement as unknown as FakeElement;
    childElement.rectLeft = 12;
    childElement.rectTop = 24;
    childElement.rectWidth = 140;
    childElement.rectHeight = 20;

    const snapshot = tree.getDebugSnapshot();

    expect(snapshot.tree?.id).toBe(String(root));
    expect(snapshot.tree?.tag).toBe('view');
    expect(snapshot.tree?.element.attributes.width).toBe(42);
    expect(snapshot.tree?.bounds).toEqual({ x: 4, y: 8, width: 200, height: 120 });
    expect(snapshot.tree?.children[0].id).toBe(String(child));
    expect(snapshot.tree?.children[0].tag).toBe('label');
    expect(snapshot.tree?.children[0].element.attributes.value).toBe('Hello debugger');
    expect(snapshot.tree?.children[0].bounds).toEqual({ x: 12, y: 24, width: 140, height: 20 });
  });

  it('destroys descendant nodes when a subtree root is destroyed', () => {
    const root = createTestElement('view');
    const child = createTestElement('view');
    const grandchild = createTestElement('view');

    tree.makeElementRoot(root, makeFakeElement('root') as unknown as HTMLElement);
    tree.moveElement(child, root, 0);
    tree.moveElement(grandchild, child, 0);
    tree.destroyElement(child);

    expect(tree.getNode(root)).toBeDefined();
    expect(tree.getNode(child)).toBeUndefined();
    expect(tree.getNode(grandchild)).toBeUndefined();
  });

  it('replays buffered custom-view attributes in order and forwards live updates', () => {
    const changes: Array<[string, unknown]> = [];
    const webClass = registerTestWebViewClass(() => ({
      changeAttribute(name: string, value: unknown): void {
        changes.push([name, value]);
      },
      destroy(): void {},
    }));
    const id = createRootTestElement('custom-view');

    tree.setAttributeOnElement(id, 'latex', 'x');
    tree.flush();
    tree.setAttributeOnElement(id, 'block', false);
    tree.flush();
    tree.setAttributeOnElement(id, 'webClass', webClass);
    tree.flush();

    expect(changes).toEqual([
      ['latex', 'x'],
      ['block', false],
    ]);

    tree.setAttributeOnElement(id, 'latex', 'y');
    tree.flush();
    tree.setAttributeOnElement(id, 'block', undefined);
    tree.flush();

    expect(changes).toEqual([
      ['latex', 'x'],
      ['block', false],
      ['latex', 'y'],
      ['block', undefined],
    ]);
  });

  it('preserves an explicit minimum height when attaching a custom web view', () => {
    const webClass = registerTestWebViewClass(() => ({
      changeAttribute(): void {},
    }));
    const id = createRootTestElement('custom-view');

    tree.setAttributeOnElement(id, 'minHeight', 32);
    tree.setAttributeOnElement(id, 'webClass', webClass);
    tree.flush();

    expect(getNode(id).htmlElement.style.minHeight).toBe('32px');
  });

  it('creates custom views from an element-class view factory and applies normal attributes', () => {
    const changes: Array<[string, unknown]> = [];
    const destroy = jasmine.createSpy('destroy');
    const binder = new AttributesBinder<HTMLElement>();
    binder.bindNumberAttribute(
      'contentReferenceIndex',
      (_element, value) => changes.push(['contentReferenceIndex', value]),
      () => changes.push(['contentReferenceIndex', undefined]),
    );
    binder.bindStringAttribute(
      'name',
      (_element, value) => changes.push(['name', value]),
      () => changes.push(['name', undefined]),
    );
    class TestFactoryElementClass extends ViewElementClass {
      constructor() {
        super('test-factory-element', binder.attributeAppliers, {});
      }

      protected onCreateElement(): HTMLElement {
        return document.createElement('span');
      }

      override destroy(element: HTMLElement): void {
        destroy(element);
      }
    }
    const viewFactory = new WebViewFactory(new TestFactoryElementClass());
    const root = makeFakeElement('root') as unknown as HTMLElement;
    const delegate = new ValdiWebRendererDelegate(root, tree);
    const id = nextId++;
    delegate.onCustomElementCreated(id, viewFactory);
    tree.makeElementRoot(id, root);

    tree.setAttributeOnElement(id, 'width', 120);
    tree.setAttributeOnElement(id, 'contentReferenceIndex', 1);
    tree.setAttributeOnElement(id, 'name', 'code_block');
    tree.flush();

    const customElement = getNode(id).htmlElement as unknown as FakeElement;
    expect(customElement.tagName).toBe('SPAN');
    expect(customElement.style.width).toBe('120px');
    expect(customElement.childNodes.length).toBe(0);
    expect(changes).toEqual([
      ['name', 'code_block'],
      ['contentReferenceIndex', 1],
    ]);

    tree.setAttributeOnElement(id, 'contentReferenceIndex', 2);
    tree.flush();
    expect(changes).toEqual([
      ['name', 'code_block'],
      ['contentReferenceIndex', 1],
      ['contentReferenceIndex', 2],
    ]);

    tree.destroyElement(id);
    tree.destroy();
    expect(destroy).toHaveBeenCalledTimes(1);
    expect(destroy).toHaveBeenCalledWith(customElement);
  });

  it('destroys a custom-view attribute handler exactly once when its node is removed', () => {
    const destroy = jasmine.createSpy('destroy');
    const webClass = registerTestWebViewClass(() => ({
      changeAttribute(): void {},
      destroy,
    }));
    const id = createRootTestElement('custom-view');
    tree.setAttributeOnElement(id, 'webClass', webClass);
    tree.flush();

    tree.destroyElement(id);
    tree.destroy();

    expect(destroy).toHaveBeenCalledTimes(1);
  });

  it('destroys custom-view attribute handlers during full tree teardown', () => {
    const destroy = jasmine.createSpy('destroy');
    const webClass = registerTestWebViewClass(() => ({
      changeAttribute(): void {},
      destroy,
    }));
    const id = createRootTestElement('custom-view');
    tree.setAttributeOnElement(id, 'webClass', webClass);
    tree.flush();

    tree.destroy();

    expect(destroy).toHaveBeenCalledTimes(1);
  });

  it('allows custom-view factories without an attribute handler', () => {
    const factory = jasmine.createSpy('factory');
    const webClass = registerTestWebViewClass(factory);
    const id = createRootTestElement('custom-view');
    tree.setAttributeOnElement(id, 'webClass', webClass);
    tree.flush();

    expect(factory).toHaveBeenCalledTimes(1);
    expect(() => tree.destroyElement(id)).not.toThrow();
  });

  it('allows custom-view attribute handlers without a destroy method', () => {
    const changes: Array<[string, unknown]> = [];
    const webClass = registerTestWebViewClass(() => ({
      changeAttribute(name: string, value: unknown): void {
        changes.push([name, value]);
      },
    }));
    const id = createRootTestElement('custom-view');
    tree.setAttributeOnElement(id, 'webClass', webClass);
    tree.flush();
    tree.setAttributeOnElement(id, 'latex', 'x');
    tree.flush();

    expect(changes).toEqual([['latex', 'x']]);
    expect(() => tree.destroyElement(id)).not.toThrow();
  });

  it('invokes lifecycle callbacks from resolved attributes without scratch state indirection', () => {
    const id = createTestElement('view');
    const records: string[] = [];

    tree.setAttributeOnElement(id, 'onViewCreate', () => records.push('create'));
    tree.setAttributeOnElement(id, 'onViewChange', (event: { type: string }) => records.push(`change:${event.type}`));
    tree.setAttributeOnElement(id, 'onViewDestroy', () => records.push('destroy'));

    tree.makeElementRoot(id, makeFakeElement('root') as unknown as HTMLElement);
    expect(records).toEqual([]);

    tree.flush();
    expect(records).toEqual(['create', 'change:Attached']);

    tree.destroyElement(id);
    expect(records).toEqual(['create', 'change:Attached']);

    tree.flush();
    expect(records).toEqual(['create', 'change:Attached', 'change:Detached', 'destroy']);
  });

  it('invokes create and change callbacks when lifecycle attributes are attached after the node', () => {
    const id = createTestElement('view');
    const records: string[] = [];

    tree.makeElementRoot(id, makeFakeElement('root') as unknown as HTMLElement);
    tree.flush();

    tree.setAttributeOnElement(id, 'onViewCreate', () => records.push('create'));
    tree.setAttributeOnElement(id, 'onViewChange', (event: { type: string }) => records.push(`change:${event.type}`));
    tree.setAttributeOnElement(id, 'onViewDestroy', () => records.push('destroy'));
    tree.flush();

    expect(records).toEqual(['create', 'change:Attached']);

    tree.destroyElement(id);
    tree.flush();

    expect(records).toEqual(['create', 'change:Attached', 'change:Detached', 'destroy']);
  });

  it('flushes lifecycle callbacks when the full tree is destroyed', () => {
    const id = createRootTestElement('view');
    const records: string[] = [];

    tree.setAttributeOnElement(id, 'onViewCreate', () => records.push('create'));
    tree.setAttributeOnElement(id, 'onViewChange', (event: { type: string }) => records.push(`change:${event.type}`));
    tree.setAttributeOnElement(id, 'onViewDestroy', () => records.push('destroy'));
    tree.flush();

    expect(records).toEqual(['create', 'change:Attached']);

    tree.destroy();

    expect(records).toEqual(['create', 'change:Attached', 'change:Detached', 'destroy']);
  });

  it('updates the html element lookup when nodes are destroyed', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement;

    expect(tree.getNodeIdForHtmlElement(element)).toBe(id);

    tree.destroyElement(id);

    expect(tree.getNode(id)).toBeUndefined();
    expect(tree.getNodeIdForHtmlElement(element)).toBeUndefined();
  });

  it('creates elements by cloning one template per element class', () => {
    let templateCreateCount = 0;
    const viewClass = `test-template-clone-${nextTestElementClassId++}`;
    class TestTemplateElementClass extends ElementClass {
      constructor() {
        super(viewClass, {});
      }

      protected onCreateElement(): HTMLElement {
        templateCreateCount++;
        const element = document.createElement('div');
        element.style.width = '10px';
        const child = document.createElement('span');
        child.textContent = 'template child';
        element.appendChild(child);
        return element;
      }
    }
    registerElementClassAlias(viewClass, new TestTemplateElementClass());

    const first = createTestElement(viewClass);
    const second = createTestElement(viewClass);
    const firstElement = getNode(first).htmlElement;
    const secondElement = getNode(second).htmlElement;

    expect(templateCreateCount).toBe(1);
    expect(firstElement).not.toBe(secondElement);
    expect(firstElement.style.width).toBe('10px');
    expect(secondElement.style.width).toBe('10px');
    expect(firstElement.childNodes.item(0)).not.toBe(secondElement.childNodes.item(0));

    firstElement.style.width = '20px';
    expect(secondElement.style.width).toBe('10px');
  });

  it('throws for unknown view classes and missing move endpoints', () => {
    const root = createTestElement('view');
    tree.makeElementRoot(root, makeFakeElement('root') as unknown as HTMLElement);

    expect(() => tree.createElement(nextId++, 'not-a-view-class')).toThrowError(/Unknown viewClass/);
    expect(() => tree.moveElement(nextId++, root, 0)).toThrowError(/moveElement/);
    expect(() => tree.moveElement(root, nextId++, 0)).toThrowError(/moveElement/);
  });

  it('renders text animation groups as plain views', () => {
    const id = createTestElement('SCValdiTextAnimationGroup');

    expect(getNode(id).htmlElement.tagName).toBe('DIV');
  });

  it('isolates embedded WebViews and cleans up their browser controller bindings', () => {
    const id = createRootTestElement('webview');
    const host = getNode(id).htmlElement as unknown as FakeElement;
    const frame = host.querySelector('iframe');
    const firstController = {
      attachWebView: jasmine.createSpy('attachFirstWebView'),
      detachWebView: jasmine.createSpy('detachFirstWebView'),
    };
    const secondController = {
      attachWebView: jasmine.createSpy('attachSecondWebView'),
      detachWebView: jasmine.createSpy('detachSecondWebView'),
    };

    expect(frame?.getAttribute('sandbox')).toBe('allow-scripts allow-forms');
    expect(frame?.getAttribute('sandbox')).not.toContain('allow-same-origin');

    tree.setAttributeOnElement(id, 'controller', firstController);
    tree.flush();
    expect(firstController.attachWebView).toHaveBeenCalledOnceWith(frame);

    tree.setAttributeOnElement(id, 'controller', secondController);
    tree.flush();
    expect(firstController.detachWebView).toHaveBeenCalledOnceWith(frame);
    expect(secondController.attachWebView).toHaveBeenCalledOnceWith(frame);

    tree.destroyElement(id);
    expect(secondController.detachWebView).toHaveBeenCalledOnceWith(frame);
  });

  it('maps native label selectability to browser text-selection behavior', () => {
    const id = createRootTestElement('label');
    const label = getNode(id).htmlElement;

    expect(label.style.userSelect).toBe('none');

    tree.setAttributeOnElement(id, 'selectable', false);
    tree.flush();
    expect(label.style.userSelect).toBe('none');

    tree.setAttributeOnElement(id, 'selectable', true);
    tree.flush();
    expect(label.style.userSelect).toBe('text');

    tree.setAttributeOnElement(id, 'selectable', undefined);
    tree.flush();
    expect(label.style.userSelect).toBe('none');
  });

  it('replaces the root container children when making an element root', () => {
    const host = makeFakeElement('host');
    const previousChild = makeFakeElement('previous');
    host.replaceChildren(previousChild);

    const id = createTestElement('view');
    tree.makeElementRoot(id, host as unknown as HTMLElement);

    expect(host.childNodes.item(0)).toBe(getNode(id).htmlElement as unknown as FakeElement);
    expect(host.childNodes.item(1)).toBeNull();
  });

  it('inserts and reorders moved elements at the requested parent index', () => {
    const root = createRootTestElement('view');
    const first = createTestElement('view');
    const second = createTestElement('view');
    const third = createTestElement('view');
    const rootElement = getNode(root).htmlElement;
    const firstElement = getNode(first).htmlElement;
    const secondElement = getNode(second).htmlElement;
    const thirdElement = getNode(third).htmlElement;

    tree.moveElement(first, root, 0);
    tree.moveElement(second, root, 0);
    tree.moveElement(third, root, 1);

    expect(rootElement.childNodes.item(0)).toBe(secondElement);
    expect(rootElement.childNodes.item(1)).toBe(thirdElement);
    expect(rootElement.childNodes.item(2)).toBe(firstElement);

    tree.moveElement(first, root, 0);

    expect(rootElement.childNodes.item(0)).toBe(firstElement);
    expect(rootElement.childNodes.item(1)).toBe(secondElement);
    expect(rootElement.childNodes.item(2)).toBe(thirdElement);
  });

  it('schedules one microtask flush for dirty root updates outside a render batch', async () => {
    const appliedValues: unknown[] = [];
    const viewClass = registerTestElementClass({
      score: {
        apply(_element, value) {
          appliedValues.push(value);
        },
        reset() {},
      },
    });
    const id = createRootTestElement(viewClass);
    tree.flush();
    await waitForScheduledFlush();
    const flushSpy = spyOn(tree, 'flush').and.callThrough();

    tree.setAttributeOnElement(id, 'score', 1);
    tree.setAttributeOnElement(id, 'score', 2);
    await waitForScheduledFlush();

    expect(flushSpy.calls.count()).toBe(1);
    expect(appliedValues).toEqual([2]);
  });

  it('waits for the outermost render batch before flushing dirty nodes', () => {
    let applyCount = 0;
    const viewClass = registerTestElementClass({
      score: {
        apply() {
          applyCount++;
        },
        reset() {},
      },
    });
    const id = createRootTestElement(viewClass);

    tree.beginRender();
    tree.beginRender();
    tree.setAttributeOnElement(id, 'score', 1);
    tree.endRender();

    expect(applyCount).toBe(0);

    tree.endRender();

    expect(applyCount).toBe(1);
  });

  it('coalesces dirty attributes during a render batch', () => {
    let applyCount = 0;
    let appliedValue: unknown;
    const applier: AttributeApplier = {
      apply(_element, value) {
        applyCount++;
        appliedValue = value;
      },
      reset() {},
    };
    class TestCounterElementClass extends ElementClass {
      constructor() {
        super('test-counter', { score: applier });
      }

      protected onCreateElement(): HTMLElement {
        return document.createElement('div');
      }
    }
    registerElementClassAlias('test-counter', new TestCounterElementClass());
    const id = createRootTestElement('test-counter');

    tree.beginRender();
    tree.setAttributeOnElement(id, 'score', 1);
    tree.setAttributeOnElement(id, 'score', 2);
    tree.setAttributeOnElement(id, 'score', 3);
    expect(applyCount).toBe(0);
    tree.endRender();

    expect(applyCount).toBe(1);
    expect(appliedValue).toBe(3);
  });

  it('does not notify the tree again when a node already needs update', () => {
    const id = createRootTestElement('view');
    tree.flush();
    const needsUpdateSpy = spyOn(tree, 'onNodeNeedsUpdate').and.callThrough();

    tree.beginRender();
    tree.setAttributeOnElement(id, 'width', 1);
    tree.setAttributeOnElement(id, 'height', 2);

    expect(needsUpdateSpy.calls.count()).toBe(1);
    tree.endRender();
  });

  it('does not schedule a dirty flush for detached nodes', async () => {
    const id = createTestElement('view');
    const flushSpy = spyOn(tree, 'flush').and.callThrough();

    tree.setAttributeOnElement(id, 'width', 1);
    await Promise.resolve();

    expect(flushSpy).not.toHaveBeenCalled();
  });

  it('flushes from the root node instead of scanning detached nodes', () => {
    const appliedValues: unknown[] = [];
    const applier: AttributeApplier = {
      apply(_element, value) {
        appliedValues.push(value);
      },
      reset() {},
    };
    class TestRootFlushElementClass extends ElementClass {
      constructor() {
        super('test-root-flush', { score: applier });
      }

      protected onCreateElement(): HTMLElement {
        return document.createElement('div');
      }
    }
    registerElementClassAlias('test-root-flush', new TestRootFlushElementClass());

    const root = createRootTestElement('test-root-flush');
    const detached = createTestElement('test-root-flush');

    tree.setAttributeOnElement(root, 'score', 1);
    tree.setAttributeOnElement(detached, 'score', 2);
    tree.flush();
    expect(appliedValues).toEqual([1]);

    tree.moveElement(detached, root, 0);
    tree.flush();
    expect(appliedValues).toEqual([1, 2]);
  });

  it('propagates an already dirty detached subtree when it is moved under the root', async () => {
    const appliedValues: unknown[] = [];
    const viewClass = registerTestElementClass({
      score: {
        apply(_element, value) {
          appliedValues.push(value);
        },
        reset() {},
      },
    });
    const root = createRootTestElement(viewClass);
    const detachedParent = createTestElement(viewClass);
    const detachedChild = createTestElement(viewClass);
    tree.flush();
    await waitForScheduledFlush();

    tree.moveElement(detachedChild, detachedParent, 0);
    tree.setAttributeOnElement(detachedChild, 'score', 'child');
    await waitForScheduledFlush();
    expect(appliedValues).toEqual([]);

    tree.moveElement(detachedParent, root, 0);
    await waitForScheduledFlush();

    expect(appliedValues).toEqual(['child']);
  });

  it('schedules a dirty detached root when it becomes the tree root', async () => {
    const appliedValues: unknown[] = [];
    const viewClass = registerTestElementClass({
      score: {
        apply(_element, value) {
          appliedValues.push(value);
        },
        reset() {},
      },
    });
    const id = createTestElement(viewClass);

    tree.setAttributeOnElement(id, 'score', 'root');
    await waitForScheduledFlush();
    expect(appliedValues).toEqual([]);

    tree.makeElementRoot(id, makeFakeElement('root') as unknown as HTMLElement);
    await waitForScheduledFlush();

    expect(appliedValues).toEqual(['root']);
  });

  it('continues the update pass when an attribute applier dirties another attribute during flush', () => {
    const appliedValues: string[] = [];
    let id = 0;
    const viewClass = registerTestElementClass({
      first: {
        apply() {
          appliedValues.push('first');
          tree.setAttributeOnElement(id, 'second', 'from-first');
        },
        reset() {},
      },
      second: {
        apply(_element, value) {
          appliedValues.push(`second:${String(value)}`);
        },
        reset() {},
      },
    });
    id = createRootTestElement(viewClass);

    tree.setAttributeOnElement(id, 'first', true);
    tree.flush();

    expect(appliedValues).toEqual(['first', 'second:from-first']);
  });

  it('resolves palette updates triggered by an attribute during the same update pass', () => {
    paletteManager.configureColorPalette('light', { tone: 'blue' });
    paletteManager.configureColorPalette('dark', { tone: 'red' });
    paletteManager.setActiveColorPalette('light');
    const appliedColors: string[] = [];
    const viewClass = registerTestElementClass({
      tone: {
        colorDependent: true,
        apply(_element, value, _attributeName, context) {
          appliedColors.push(context.resolveColor(String(value)));
        },
        reset() {},
      },
      useDarkPalette: {
        apply(_element, _value, _attributeName, context) {
          context.setColorPalette('dark');
        },
        reset(_element, _attributeName, context) {
          context.setColorPalette(undefined);
        },
      },
    });
    const id = createRootTestElement(viewClass);

    tree.beginRender();
    tree.setAttributeOnElement(id, 'tone', 'tone');
    tree.setAttributeOnElement(id, 'useDarkPalette', true);
    tree.endRender();

    expect(appliedColors.length).toBeGreaterThan(0);
    expect(appliedColors[appliedColors.length - 1]).toBe('red');
  });

  it('signals externally updated attributes through the node creation delegate', () => {
    const records: Array<{ id: number; attributeName: string; attributeValue: unknown }> = [];
    const applier: AttributeApplier = {
      apply(_element, _value, _attributeName, context) {
        context.onAttributeUpdatedExternally('value', 'dom-value');
      },
      reset() {},
    };
    class TestExternalUpdateElementClass extends ElementClass {
      constructor() {
        super('test-external-update', { trigger: applier });
      }

      protected onCreateElement(): HTMLElement {
        return document.createElement('div');
      }
    }
    registerElementClassAlias('test-external-update', new TestExternalUpdateElementClass());

    const id = nextId++;
    tree.createElement(id, 'test-external-update', {
      onAttributeUpdatedExternally(elementId, attributeName, attributeValue) {
        records.push({ id: elementId, attributeName, attributeValue });
      },
    });
    tree.makeElementRoot(id, makeFakeElement('root') as unknown as HTMLElement);

    tree.setAttributeOnElement(id, 'trigger', true);
    tree.flush();

    expect(records).toEqual([{ id, attributeName: 'value', attributeValue: 'dom-value' }]);
  });

  it('logs typed applier failures with node context without escaping', () => {
    const id = createRootTestElement('view');
    const errorSpy = spyOn(console, 'error');

    expect(() => {
      tree.setAttributeOnElement(id, 'width', {});
      tree.flush();
    }).not.toThrow();

    expect(errorSpy).toHaveBeenCalled();
    const message = String(errorSpy.calls.mostRecent().args[0]);
    expect(message).toContain(`node ${id}`);
    expect(message).toContain('(view)');
    expect(message).toContain("'width'");
    expect(message).toContain('Expected');
  });

  it('warns with node context when an attribute has no applier', () => {
    const id = createRootTestElement('view');
    const warnSpy = spyOn(console, 'warn');

    tree.setAttributeOnElement(id, 'unknownAttribute', 42);
    tree.flush();

    expect(warnSpy).toHaveBeenCalled();
    const message = String(warnSpy.calls.mostRecent().args[0]);
    expect(message).toContain(`node ${id}`);
    expect(message).toContain('(view)');
    expect(message).toContain("'unknownAttribute'");
    expect(message).toContain('42');
  });

  it('reapplies color-dependent attributes across palette changes and subtree overrides', () => {
    paletteManager.configureColorPalette('light', { background: 'blue', foreground: 'green' });
    paletteManager.configureColorPalette('dark', { background: 'red', foreground: 'yellow' });
    paletteManager.setActiveColorPalette('light');

    const root = createTestElement('view');
    const child = createTestElement('view');
    const grandchild = createTestElement('view');
    tree.makeElementRoot(root, makeFakeElement('root') as unknown as HTMLElement);
    tree.moveElement(child, root, 0);
    tree.moveElement(grandchild, child, 0);

    tree.setAttributeOnElement(root, 'backgroundColor', 'background');
    tree.setAttributeOnElement(child, 'colorPaletteName', 'dark');
    tree.setAttributeOnElement(child, 'backgroundColor', 'background');
    tree.setAttributeOnElement(grandchild, 'backgroundColor', 'foreground');
    tree.flush();

    expect(getViewPaintElement(root).style.backgroundColor).toBe('blue');
    expect(getViewPaintElement(child).style.backgroundColor).toBe('red');
    expect(getViewPaintElement(grandchild).style.backgroundColor).toBe('yellow');

    paletteManager.configureColorPalette('dark', { background: 'black', foreground: 'white' });
    expect(getViewPaintElement(child).style.backgroundColor).toBe('black');
    expect(getViewPaintElement(grandchild).style.backgroundColor).toBe('white');

    paletteManager.setActiveColorPalette('dark');
    expect(getViewPaintElement(root).style.backgroundColor).toBe('black');
  });

  it('resolves attributed text colors through the node palette override', () => {
    paletteManager.configureColorPalette('light', { foreground: 'black' });
    paletteManager.configureColorPalette('dark', { foreground: 'white' });
    paletteManager.setActiveColorPalette('light');
    const attributedText = new AttributedTextBuilder().pushColor('foreground').append('themed').pop().build();
    const id = createRootTestElement('label');

    tree.setAttributeOnElement(id, 'colorPaletteName', 'dark');
    tree.setAttributeOnElement(id, 'value', attributedText);
    tree.flush();

    const label = getNode(id).htmlElement as unknown as FakeElement;
    let styledSpan = label.childNodes.item(0)!.childNodes.item(0)!;
    expect(styledSpan.style.color).toBe('white');

    paletteManager.configureColorPalette('dark', { foreground: 'yellow' });

    styledSpan = label.childNodes.item(0)!.childNodes.item(0)!;
    expect(styledSpan.style.color).toBe('yellow');
  });

  it('uses the active palette from the root update pass rather than node creation time', () => {
    paletteManager.configureColorPalette('light', { background: 'blue' });
    paletteManager.configureColorPalette('dark', { background: 'red' });
    paletteManager.setActiveColorPalette('light');

    const id = createTestElement('view');
    paletteManager.setActiveColorPalette('dark');
    tree.makeElementRoot(id, makeFakeElement('root') as unknown as HTMLElement);
    tree.setAttributeOnElement(id, 'backgroundColor', 'background');
    tree.flush();

    expect(getViewPaintElement(id).style.backgroundColor).toBe('red');
  });

  it('recomputes inherited palette when a node moves to a different palette scope', () => {
    paletteManager.configureColorPalette('light', { background: 'blue' });
    paletteManager.configureColorPalette('dark', { background: 'red' });
    paletteManager.setActiveColorPalette('light');

    const root = createRootTestElement('view');
    const darkParent = createTestElement('view');
    const child = createTestElement('view');

    tree.moveElement(darkParent, root, 0);
    tree.moveElement(child, darkParent, 0);
    tree.setAttributeOnElement(darkParent, 'colorPaletteName', 'dark');
    tree.setAttributeOnElement(child, 'backgroundColor', 'background');
    tree.flush();
    expect(getViewPaintElement(child).style.backgroundColor).toBe('red');

    tree.moveElement(child, root, 1);
    tree.flush();

    expect(getViewPaintElement(child).style.backgroundColor).toBe('blue');
  });

  it('does not keep palette change listeners after the tree is destroyed', () => {
    const id = createRootTestElement('view');
    tree.setAttributeOnElement(id, 'backgroundColor', 'background');
    tree.flush();
    const reapplySpy = spyOn(tree, 'reapplyColorPalettesOnAllNodes').and.callThrough();

    tree.destroy();
    paletteManager.configureColorPalette('default', { background: 'black' });

    expect(reapplySpy).not.toHaveBeenCalled();
  });

  it('coalesces transform composite parts into one final transform value', () => {
    const id = createRootTestElement('view');

    tree.beginRender();
    tree.setAttributeOnElement(id, 'translationX', 10);
    tree.setAttributeOnElement(id, 'scaleX', 2);
    tree.setAttributeOnElement(id, 'rotation', 1);
    tree.endRender();

    expect(getNode(id).htmlElement.style.transform).toBe('translate(10px, 0px) scale(2, 1) rotate(1rad)');
  });

  it('does not expose the synthetic transform composite as a rendered attribute', () => {
    const id = createRootTestElement('view');
    tree.setAttributeOnElement(id, 'translationX', 10);
    tree.flush();

    const attributes = getNode(id).getDebugSnapshot().element.attributes;
    expect(attributes.translationX).toBe(10);
    expect(attributes.transformComposite).toBeUndefined();
  });

  it('maps Yoga overflow scroll to visible CSS overflow for plain views', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement;

    tree.setAttributeOnElement(id, 'overflow', 'scroll');
    tree.flush();

    expect(element.style.overflow).toBe('visible');
  });

  it('applies onMeasure tuple results as measured lazy layout dimensions', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    const records: string[] = [];
    if (!element.parentElement) {
      throw new Error('Expected root parent element');
    }
    element.parentElement.rectWidth = 320;
    element.parentElement.rectHeight = 200;

    tree.setAttributeOnElement(
      id,
      'onMeasure',
      (width: number, widthMode: number, height: number, heightMode: number) => {
        records.push(`${width}:${widthMode}:${height}:${heightMode}`);
        return [180, 76];
      },
    );
    tree.flush();

    expect(records).toEqual(['320:1:200:2', '320:1:200:2']);
    expect(element.style.width).toBeUndefined();
    expect(element.style.height).toBe('76px');
  });

  it('uses estimated dimensions as lazy layout placeholder dimensions', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement;

    tree.setAttributeOnElement(id, 'estimatedWidth', 150);
    tree.setAttributeOnElement(id, 'estimatedHeight', 64);
    tree.flush();

    expect(element.style.containIntrinsicWidth).toBe('150px');
    expect(element.style.containIntrinsicHeight).toBe('64px');
    expect(element.style.width).toBeUndefined();
    expect(element.style.height).toBe('64px');

    tree.setAttributeOnElement(id, 'estimatedWidth', undefined);
    tree.setAttributeOnElement(id, 'estimatedHeight', undefined);
    tree.flush();

    expect(element.style.width).toBeUndefined();
    expect(element.style.height).toBe('');
  });

  it('suppresses boxShadow while slowClipping clips child content', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement;

    tree.setAttributeOnElement(id, 'boxShadow', 'complex 0 12 28 rgba(17, 24, 39, 0.35)');
    tree.flush();
    const paintElement = getViewPaintElement(id);

    expect(paintElement.style.boxShadow).toBe('0px 12px 28px rgba(17, 24, 39, 0.35)');

    tree.setAttributeOnElement(id, 'slowClipping', true);
    tree.flush();

    expect(paintElement.style.boxShadow).toBe('');
    expect(element.style.overflow).toBe('hidden');

    tree.setAttributeOnElement(id, 'slowClipping', false);
    tree.flush();
    expect(paintElement.style.boxShadow).toBe('0px 12px 28px rgba(17, 24, 39, 0.35)');
  });

  it('mirrors the resolved paint radius onto the host only while slowClipping is enabled', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.rectWidth = 100;
    element.rectHeight = 60;
    tree.setAttributeOnElement(id, 'borderRadius', '50%');
    tree.flush();
    const paintElement = getViewPaintElement(id);

    expect(paintElement.style.borderRadius).toBe('30px');
    expect(element.style.borderRadius).toBe('');

    tree.setAttributeOnElement(id, 'slowClipping', true);
    tree.flush();
    expect(element.style.borderRadius).toBe('30px');

    tree.setAttributeOnElement(id, 'slowClipping', false);
    tree.flush();
    expect(element.style.borderRadius).toBe('');
    expect(paintElement.style.borderRadius).toBe('30px');
  });

  it('keeps slowClipping active when overflow is applied afterward', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement;

    tree.setAttributeOnElement(id, 'slowClipping', true);
    tree.flush();
    expect(element.style.overflow).toBe('hidden');

    tree.setAttributeOnElement(id, 'overflow', 'visible');
    tree.flush();
    expect(element.style.overflow).toBe('hidden');

    tree.setAttributeOnElement(id, 'slowClipping', false);
    tree.flush();
    expect(element.style.overflow).toBe('visible');
  });

  it('adds CSS units to Valdi layout shorthand numbers', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement;

    tree.setAttributeOnElement(id, 'padding', '8 18 14 28');
    tree.setAttributeOnElement(id, 'gap', '10 14');
    tree.flush();

    expect(element.style.padding).toBe('8px 18px 14px 28px');
    expect(element.style.gap).toBe('10px 14px');
  });

  it('applies maskPath using CSS mask styles', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement as unknown as FakeElement;

    tree.setAttributeOnElement(id, 'maskOpacity', 0.5);
    tree.setAttributeOnElement(id, 'maskPath', 'M 0 0 L 1 0 L 1 1 Z');
    tree.flush();

    expect(element.style['mask-image']).toContain('data:image/svg+xml');
    expect(element.style['mask-mode']).toBe('luminance');

    tree.setAttributeOnElement(id, 'maskPath', undefined);
    tree.flush();
    expect(element.style['mask-image']).toBeUndefined();
  });

  it('converts serialized geometric paths to SVG paths', () => {
    const path = new GeometricPathBuilder(10, 20, GeometricPathScaleType.Contain)
      .moveTo(1, 2)
      .lineTo(3, 4)
      .quadTo(5, 6, 7, 8)
      .cubicTo(9, 10, 11, 12, 13, 14)
      .roundRectTo(1, 1, 4, 6, 2, 3)
      .arcTo(5, 5, 2, 0, Math.PI / 2)
      .close()
      .build();

    const svgPath = geometricPathToSvgPath(path);
    expect(svgPath.viewBox).toBe('0 0 10 20');
    expect(svgPath.preserveAspectRatio).toBe('xMidYMid meet');
    expect(svgPath.d).toContain('M 1 2 L 3 4 Q 5 6 7 8 C 9 10 11 12 13 14');
    expect(svgPath.d).toContain('A 2 2 0 0 1');
  });

  it('respects explicit scroll indicator booleans', () => {
    const id = createRootTestElement('scroll');
    const element = getNode(id).htmlElement as unknown as FakeElement;

    expect(element.classList.contains('hide-v-scrollbar')).toBeTrue();
    expect(element.classList.contains('hide-h-scrollbar')).toBeTrue();

    tree.setAttributeOnElement(id, 'showsVerticalScrollIndicator', true);
    tree.setAttributeOnElement(id, 'showsHorizontalScrollIndicator', true);
    tree.flush();
    expect(element.classList.contains('hide-v-scrollbar')).toBeFalse();
    expect(element.classList.contains('hide-h-scrollbar')).toBeFalse();

    tree.setAttributeOnElement(id, 'showsVerticalScrollIndicator', false);
    tree.setAttributeOnElement(id, 'showsHorizontalScrollIndicator', false);
    tree.flush();
    expect(element.classList.contains('hide-v-scrollbar')).toBeTrue();
    expect(element.classList.contains('hide-h-scrollbar')).toBeTrue();
  });

  it('keeps the scrollbar visible when an axis is always scrollable', () => {
    const id = createRootTestElement('scroll');
    const element = getNode(id).htmlElement as unknown as FakeElement;

    tree.setAttributeOnElement(id, 'showsHorizontalScrollIndicator', false);
    tree.setAttributeOnElement(id, 'showsVerticalScrollIndicator', false);
    tree.setAttributeOnElement(id, 'canAlwaysScrollHorizontal', true);
    tree.flush();

    expect(element.style.overflowX).toBe('scroll');
    expect(element.style['scrollbar-width']).toBe('auto');
    expect(element.classList.contains('hide-h-scrollbar')).toBeFalse();
    expect(element.classList.contains('hide-v-scrollbar')).toBeTrue();

    tree.setAttributeOnElement(id, 'canAlwaysScrollHorizontal', false);
    tree.flush();

    expect(element.style['scrollbar-width']).toBe('none');
    expect(element.classList.contains('hide-h-scrollbar')).toBeTrue();
  });

  it('reports native-shaped drag lifecycle, movement, and velocity', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    const events: Array<{ deltaX: number; deltaY: number; state: number; velocityY: number }> = [];
    const mouseEvent = (
      type: string,
      clientX: number,
      clientY: number,
      buttons: number,
      timeStamp: number,
    ): FakeMouseDragEvent => ({
      buttons,
      clientX,
      clientY,
      preventDefault(): void {},
      timeStamp,
      type,
    });

    tree.setAttributeOnElement(
      id,
      'onDrag',
      (event: { deltaX: number; deltaY: number; state: number; velocityY: number }) => {
        events.push(event);
      },
    );
    tree.flush();

    element.dispatchEvent(mouseEvent('mousedown', 40, 100, 1, 0));
    element.dispatchEvent(mouseEvent('mousemove', 48, 65, 1, 50));
    element.dispatchEvent(mouseEvent('mouseup', 52, 20, 0, 100));

    expect(events.map(event => event.state)).toEqual([0, 1, 2]);
    expect(events.map(event => [event.deltaX, event.deltaY])).toEqual([
      [0, 0],
      [8, -35],
      [12, -80],
    ]);
    expect(events[1].velocityY).toBe(-700);
    expect(events[2].velocityY).toBe(-900);
  });

  it('reports pointer-driven onTouch events in element-relative coordinates', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.rectLeft = 100;
    element.rectTop = 40;
    const events: Array<{
      absoluteX: number;
      absoluteY: number;
      pointerCount: number;
      state: number;
      x: number;
      y: number;
    }> = [];
    const pointerEvent = (type: string, clientX: number, clientY: number, buttons: number): FakePointerDragEvent => ({
      buttons,
      clientX,
      clientY,
      pointerId: 7,
      preventDefault(): void {},
      timeStamp: 0,
      type,
    });

    tree.setAttributeOnElement(id, 'onTouch', (event: (typeof events)[number]) => events.push(event));
    tree.flush();

    expect(element.style.pointerEvents).toBe('auto');
    element.dispatchEvent(pointerEvent('pointermove', 110, 50, 0));
    element.dispatchEvent(pointerEvent('pointerdown', 125, 65, 1));
    element.dispatchEvent(pointerEvent('pointermove', 150, 85, 1));
    element.dispatchEvent(pointerEvent('pointerup', 160, 95, 0));

    expect(events.map(event => event.state)).toEqual([0, 1, 2]);
    expect(events.map(event => [event.x, event.y])).toEqual([
      [25, 25],
      [50, 45],
      [60, 55],
    ]);
    expect(events.map(event => [event.absoluteX, event.absoluteY])).toEqual([
      [125, 65],
      [150, 85],
      [160, 95],
    ]);
    expect(events.map(event => event.pointerCount)).toEqual([1, 1, 0]);

    tree.setAttributeOnElement(id, 'onTouch', undefined);
    tree.flush();
    expect(element.style.pointerEvents).toBe('none');
    element.dispatchEvent(pointerEvent('pointerdown', 125, 65, 1));
    expect(events.length).toBe(3);
  });

  it('does not recognize a disabled drag interaction', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    const onDrag = jasmine.createSpy('onDrag');
    tree.setAttributeOnElement(id, 'onDrag', onDrag);
    tree.setAttributeOnElement(id, 'onDragDisabled', true);
    tree.flush();

    const event: FakeMouseDragEvent = {
      buttons: 1,
      clientX: 20,
      clientY: 50,
      preventDefault(): void {},
      timeStamp: 0,
      type: 'mousedown',
    };
    const moved: FakeMouseDragEvent = { ...event, clientY: 10, timeStamp: 50, type: 'mousemove' };
    const ended: FakeMouseDragEvent = { ...event, buttons: 0, clientY: 10, timeStamp: 100, type: 'mouseup' };
    element.dispatchEvent(event);
    element.dispatchEvent(moved);
    element.dispatchEvent(ended);

    expect(onDrag).not.toHaveBeenCalled();
  });

  it('continues an active drag on the owner document after the pointer leaves its moving view', () => {
    const id = createRootTestElement('view');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    const events: Array<{ deltaX: number; deltaY: number; state: number }> = [];
    const document = element.ownerDocument as unknown as FakeElement;
    const createEvent = (type: string, clientX: number, clientY: number, buttons: number): FakeMouseDragEvent => ({
      buttons,
      clientX,
      clientY,
      preventDefault: jasmine.createSpy('preventDefault'),
      timeStamp: 1,
      type,
    });

    tree.setAttributeOnElement(id, 'onDrag', (event: { deltaX: number; deltaY: number; state: number }) => {
      events.push(event);
    });
    tree.flush();

    element.dispatchEvent(createEvent('mousedown', 20, 30, 1));
    document.dispatchEvent(createEvent('mousemove', 145, 90, 1));
    document.dispatchEvent(createEvent('mouseup', 145, 90, 0));
    document.dispatchEvent(createEvent('mousemove', 200, 140, 1));

    expect(events.map(event => event.state)).toEqual([0, 1, 2]);
    expect(events[1].deltaX).toBe(125);
    expect(events[1].deltaY).toBe(60);
  });

  it('scrolls nonselectable content when dragged with the primary mouse button', () => {
    const id = createRootTestElement('scroll');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    const label = makeFakeElement('span');
    element.clientHeight = 100;
    element.scrollHeight = 500;
    element.appendChild(label);

    tree.setAttributeOnElement(id, 'cancelsTouchesOnScroll', true);
    tree.flush();

    const mouseEvent = (type: string, clientY: number, buttons: number): FakeDomEvent => {
      return {
        type,
        button: 0,
        buttons,
        clientX: 20,
        clientY,
        target: label,
        preventDefault(): void {
          this.defaultPrevented = true;
        },
        stopImmediatePropagation(): void {},
      } as FakeDomEvent;
    };

    element.dispatchEvent(mouseEvent('mousedown', 90, 1));
    element.dispatchEvent(mouseEvent('mousemove', 40, 1));
    expect(element.scrollTop).toBe(0);

    label.style.userSelect = 'none';
    element.dispatchEvent(mouseEvent('mousedown', 90, 1));
    const drag = mouseEvent('mousemove', 40, 1);
    element.dispatchEvent(drag);
    expect(element.scrollTop).toBe(50);
    expect(drag.defaultPrevented).toBeTrue();

    element.dispatchEvent(mouseEvent('mouseup', 40, 0));
    const click = mouseEvent('click', 40, 0);
    element.dispatchEvent(click);
    expect(click.defaultPrevented).toBeTrue();

    tree.setAttributeOnElement(id, 'cancelsTouchesOnScroll', false);
    tree.flush();
    element.scrollTop = 0;
    element.dispatchEvent(mouseEvent('mousedown', 90, 1));
    element.dispatchEvent(mouseEvent('mousemove', 40, 1));
    expect(element.scrollTop).toBe(0);
  });

  it('reports changed scroll content size when the observed element size changes', () => {
    const sizes: Array<{ width: number; height: number }> = [];
    const id = createRootTestElement('scroll');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.rectWidth = 200;
    element.rectHeight = 300;
    element.scrollWidth = 240;
    element.scrollHeight = 360;

    tree.setAttributeOnElement(id, 'onContentSizeChange', (size: { width: number; height: number }) => {
      sizes.push(size);
    });
    tree.flush();
    expect(sizes).toEqual([{ width: 240, height: 360 }]);
    tree.setAttributeOnElement(id, 'width', 200);
    tree.flush();
    expect(sizes.length).toBe(1);

    element.scrollHeight = 420;
    element.rectHeight = 320;
    dispatchWindowResize();
    expect(sizes).toEqual([
      { width: 240, height: 360 },
      { width: 240, height: 420 },
    ]);
  });

  it('reapplies scroll contentOffset after renderer post-layout callbacks', () => {
    let runPostLayoutCallbacks: (() => void) | undefined;
    tree.setPostLayoutScheduler(callback => {
      runPostLayoutCallbacks = callback;
    });
    const id = createRootTestElement('scroll');
    const element = getNode(id).htmlElement as unknown as FakeElement;

    tree.setAttributeOnElement(id, 'contentOffsetY', 48);
    tree.flush();

    expect(element.scrollTop).toBe(48);
    expect(runPostLayoutCallbacks).toBeDefined();

    element.scrollTop = 0;
    runPostLayoutCallbacks!();

    expect(element.scrollTop).toBe(48);
  });

  it('ignores stale scheduled scroll contentOffset callbacks', () => {
    let runPostLayoutCallbacks: (() => void) | undefined;
    tree.setPostLayoutScheduler(callback => {
      runPostLayoutCallbacks = callback;
    });
    const id = createRootTestElement('scroll');
    const element = getNode(id).htmlElement as unknown as FakeElement;

    tree.setAttributeOnElement(id, 'contentOffsetY', 24);
    tree.flush();
    tree.setAttributeOnElement(id, 'contentOffsetY', 96);
    tree.flush();

    expect(element.scrollTop).toBe(96);
    expect(runPostLayoutCallbacks).toBeDefined();

    element.scrollTop = 0;
    runPostLayoutCallbacks!();

    expect(element.scrollTop).toBe(96);
  });

  it('omits the start fading edge when scroll offset is at the start', () => {
    const id = createRootTestElement('scroll');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.clientHeight = 260;
    element.scrollHeight = 460;
    element.scrollTop = 0;

    tree.setAttributeOnElement(id, 'fadingEdgeLength', 32);
    tree.setAttributeOnElement(id, 'fadingEdgeStart', true);
    tree.setAttributeOnElement(id, 'fadingEdgeEnd', true);
    tree.flush();

    expect(element.style.maskImage).toBe('linear-gradient(to bottom, black, black calc(100% - 32px), transparent)');

    element.scrollTop = 20;
    element.dispatchEvent(makeFakeEvent('scroll'));

    expect(element.style.maskImage).toBe(
      'linear-gradient(to bottom, transparent, black 32px, black calc(100% - 32px), transparent)',
    );
  });

  it('updates fading edge after renderer post-layout callbacks resolve scroll metrics', () => {
    let runPostLayoutCallbacks: (() => void) | undefined;
    tree.setPostLayoutScheduler(callback => {
      runPostLayoutCallbacks = callback;
    });
    const id = createRootTestElement('scroll');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.clientHeight = 260;
    element.scrollHeight = 460;
    element.scrollTop = 0;

    tree.setAttributeOnElement(id, 'fadingEdgeLength', 32);
    tree.flush();

    expect(element.style.maskImage).toBeUndefined();
    expect(runPostLayoutCallbacks).toBeDefined();

    element.scrollTop = 20;
    runPostLayoutCallbacks!();

    expect(element.style.maskImage).toBe(
      'linear-gradient(to bottom, transparent, black 32px, black calc(100% - 32px), transparent)',
    );
  });

  it('prevents browser focus outlines on single-line and multiline text inputs', () => {
    const textViewId = createRootTestElement('textview');
    const textView = getNode(textViewId).htmlElement as unknown as FakeElement;

    tree.setAttributeOnElement(textViewId, 'enabled', true);
    tree.setAttributeOnElement(textViewId, 'placeholder', 'Write a message');
    tree.setAttributeOnElement(textViewId, 'accessibilityLabel', 'Write a message');
    tree.flush();

    expect(textView.style.outline).toBe('none');
    expect(textView.getAttribute('aria-placeholder')).toBe('Write a message');
    expect(textView.getAttribute('aria-disabled')).toBe('false');
    expect(textView.contentEditable).toBe('plaintext-only');

    const textFieldId = createRootTestElement('textfield');
    const textField = getNode(textFieldId).htmlElement as unknown as FakeElement;
    expect(textField.style.outline).toBe('none');
  });

  it('keeps multiline text editable unless it is explicitly disabled', () => {
    const id = createRootTestElement('textview');
    const textView = getNode(id).htmlElement as unknown as FakeElement;

    expect(textView.contentEditable).toBe('plaintext-only');

    tree.setAttributeOnElement(id, 'enabled', false);
    tree.flush();
    expect(textView.contentEditable).toBe('false');
    expect(textView.getAttribute('aria-disabled')).toBe('true');

    tree.setAttributeOnElement(id, 'enabled', undefined);
    tree.flush();
    expect(textView.contentEditable).toBe('plaintext-only');
    expect(textView.getAttribute('aria-disabled')).toBeNull();
  });

  it('renders accessible, independently styled textview placeholders', () => {
    const id = createRootTestElement('textview');
    const element = getNode(id).htmlElement as unknown as FakeElement;

    tree.setAttributeOnElement(id, 'placeholder', 'Write a message');
    tree.setAttributeOnElement(id, 'placeholderColor', '#7f7f7f');
    tree.flush();

    expect(element.getAttribute('aria-placeholder')).toBe('Write a message');
    expect(element.getAttribute('placeholder')).toBe('Write a message');
    expect(element.getAttribute('data-valdi-empty')).toBe('true');
    expect(element.style.getPropertyValue('--valdi-textview-placeholder-color')).toBe('#7f7f7f');
  });

  it('hides and restores textview placeholders as their controlled value changes', () => {
    const id = createRootTestElement('textview');
    const element = getNode(id).htmlElement as unknown as FakeElement;

    tree.setAttributeOnElement(id, 'placeholder', 'Describe your work');
    tree.flush();
    expect(element.getAttribute('data-valdi-empty')).toBe('true');

    tree.setAttributeOnElement(id, 'value', 'Start a launch plan');
    tree.flush();
    expect(element.getAttribute('data-valdi-empty')).toBeNull();

    tree.setAttributeOnElement(id, 'value', '');
    tree.flush();
    expect(element.getAttribute('data-valdi-empty')).toBe('true');
  });

  it('updates and removes textview placeholder attributes', () => {
    const id = createRootTestElement('textview');
    const element = getNode(id).htmlElement as unknown as FakeElement;

    tree.setAttributeOnElement(id, 'placeholder', 'Initial placeholder');
    tree.flush();

    tree.setAttributeOnElement(id, 'placeholder', 'Updated placeholder');
    tree.flush();
    expect(element.getAttribute('aria-placeholder')).toBe('Updated placeholder');
    expect(element.getAttribute('placeholder')).toBe('Updated placeholder');

    tree.setAttributeOnElement(id, 'placeholder', undefined);
    tree.setAttributeOnElement(id, 'placeholderColor', undefined);
    tree.flush();
    expect(element.getAttribute('aria-placeholder')).toBeNull();
    expect(element.getAttribute('data-valdi-empty')).toBeNull();
    expect(element.getAttribute('placeholder')).toBeNull();
    expect(element.style.getPropertyValue('--valdi-textview-placeholder-color')).toBe('');
  });

  it('submits textview send actions without inserting a browser line break', () => {
    const id = createRootTestElement('textview');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    const submissions: string[] = [];

    tree.setAttributeOnElement(id, 'value', 'Ready to send');
    tree.setAttributeOnElement(id, 'returnType', 'send');
    tree.setAttributeOnElement(id, 'onReturn', (event: { text: string }) => {
      submissions.push(event.text);
    });
    tree.flush();

    const event = makeFakeEvent('keydown', 'Enter');
    element.dispatchEvent(event);

    expect(element.getAttribute('enterkeyhint')).toBe('send');
    expect(event.defaultPrevented).toBeTrue();
    expect(submissions).toEqual(['Ready to send']);
  });

  it('preserves normal multiline Return behavior for textview line returns', () => {
    const id = createRootTestElement('textview');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    const submissions: string[] = [];

    tree.setAttributeOnElement(id, 'value', 'Keep editing');
    tree.setAttributeOnElement(id, 'returnType', 'linereturn');
    tree.setAttributeOnElement(id, 'onReturn', (event: { text: string }) => {
      submissions.push(event.text);
    });
    tree.flush();

    const event = makeFakeEvent('keydown', 'Enter');
    element.dispatchEvent(event);

    expect(element.getAttribute('enterkeyhint')).toBeNull();
    expect(event.defaultPrevented).toBeFalse();
    expect(submissions).toEqual(['Keep editing']);
  });

  it('updates and resets browser return-key hints for editable text views', () => {
    const id = createRootTestElement('textview');
    const element = getNode(id).htmlElement as unknown as FakeElement;

    tree.setAttributeOnElement(id, 'returnType', 'search');
    tree.flush();
    expect(element.getAttribute('enterkeyhint')).toBe('search');

    tree.setAttributeOnElement(id, 'returnType', 'continue');
    tree.flush();
    expect(element.getAttribute('enterkeyhint')).toBe('enter');

    tree.setAttributeOnElement(id, 'returnType', undefined);
    tree.flush();
    expect(element.getAttribute('enterkeyhint')).toBeNull();
  });

  it('clears controlled textview values after a real contenteditable edit', () => {
    const id = createRootTestElement('textview');
    const element = getNode(id).htmlElement as unknown as FakeElement;

    tree.setAttributeOnElement(id, 'placeholder', 'Write a message');
    tree.setAttributeOnElement(id, 'value', '');
    tree.setAttributeOnElement(id, 'onChange', () => {});
    tree.flush();

    element.textContent = 'A real controlled draft';
    element.dispatchEvent(makeFakeEvent('input'));
    expect(element.getAttribute('data-valdi-empty')).toBeNull();

    tree.setAttributeOnElement(id, 'value', '');
    tree.flush();

    expect(element.textContent).toBe('');
    expect(element.value).toBe('');
    expect(element.getAttribute('data-valdi-empty')).toBe('true');
  });

  it('clears controlled textfield values after a real browser input edit', () => {
    const id = createRootTestElement('textfield');
    const element = getNode(id).htmlElement as unknown as FakeElement;

    tree.setAttributeOnElement(id, 'value', '');
    tree.setAttributeOnElement(id, 'onChange', () => {});
    tree.flush();

    element.value = 'A real controlled draft';
    element.dispatchEvent(makeFakeEvent('input'));

    tree.setAttributeOnElement(id, 'value', '');
    tree.flush();

    expect(element.value).toBe('');
  });

  it('reports textview contenteditable changes from DOM text content', () => {
    const externalUpdates: Array<{ id: number; attributeName: string; attributeValue: unknown }> = [];
    const changeEvents: Array<{ text: string; selectionStart: number; selectionEnd: number }> = [];
    const id = nextId++;

    tree.createElement(id, 'textview', {
      onAttributeUpdatedExternally(elementId, attributeName, attributeValue) {
        externalUpdates.push({ id: elementId, attributeName, attributeValue });
      },
    });
    tree.makeElementRoot(id, makeFakeElement('root') as unknown as HTMLElement);

    tree.setAttributeOnElement(id, 'value', 'initial');
    tree.setAttributeOnElement(id, 'placeholder', 'Start typing');
    tree.setAttributeOnElement(
      id,
      'onChange',
      (event: { text: string; selectionStart: number; selectionEnd: number }) => {
        changeEvents.push(event);
      },
    );
    tree.flush();

    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.textContent = 'edited';
    element.dispatchEvent(makeFakeEvent('input'));

    expect(changeEvents).toEqual([{ text: 'edited', selectionStart: 0, selectionEnd: 0 }]);
    expect(externalUpdates).toEqual([{ id, attributeName: 'value', attributeValue: 'edited' }]);
    expect(element.getAttribute('data-valdi-empty')).toBeNull();

    element.textContent = '';
    element.dispatchEvent(makeFakeEvent('input'));

    expect(element.getAttribute('data-valdi-empty')).toBe('true');
  });

  it('collapses Chromium line-break padding without consuming intentional consecutive newlines', () => {
    const externalUpdates: string[] = [];
    const changeEvents: string[] = [];
    const id = nextId++;
    tree.createElement(id, 'textview', {
      onAttributeUpdatedExternally(_elementId, attributeName, attributeValue) {
        if (attributeName === 'value') {
          externalUpdates.push(String(attributeValue));
        }
      },
    });
    tree.makeElementRoot(id, makeFakeElement('root') as unknown as HTMLElement);
    const original = 'First line\nSecond line\nThird line\nFourth line';
    tree.setAttributeOnElement(id, 'value', original);
    tree.setAttributeOnElement(id, 'returnType', 'linereturn');
    tree.setAttributeOnElement(id, 'onChange', (event: { text: string }) => {
      changeEvents.push(event.text);
    });
    tree.flush();
    const element = getNode(id).htmlElement as unknown as FakeElement;

    element.textContent = `${original}\n\n`;
    const firstReturn = makeFakeEvent('input');
    firstReturn.inputType = 'insertLineBreak';
    element.dispatchEvent(firstReturn);

    expect(changeEvents).toEqual([`${original}\n`]);
    expect(externalUpdates).toEqual([`${original}\n`]);
    expect(element.textContent).toBe(`${original}\n`);
    expect(element.value).toBe(`${original}\n`);

    element.textContent = `${original}\n\n\n`;
    const secondReturn = makeFakeEvent('input');
    secondReturn.inputType = 'insertLineBreak';
    element.dispatchEvent(secondReturn);

    expect(changeEvents).toEqual([`${original}\n`, `${original}\n\n`]);
    expect(externalUpdates).toEqual([`${original}\n`, `${original}\n\n`]);
    expect(element.textContent).toBe(`${original}\n\n`);
    expect(element.value).toBe(`${original}\n\n`);
  });

  it('preserves real multiline paste, ordinary input, and middle-of-text line breaks unchanged', () => {
    const changes: string[] = [];
    const id = createRootTestElement('textview');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    tree.setAttributeOnElement(id, 'value', 'First line');
    tree.setAttributeOnElement(id, 'returnType', 'linereturn');
    tree.setAttributeOnElement(id, 'onChange', (event: { text: string }) => {
      changes.push(event.text);
    });
    tree.flush();

    element.textContent = 'First line\n\n';
    const pasted = makeFakeEvent('input');
    pasted.inputType = 'insertFromPaste';
    element.dispatchEvent(pasted);
    expect(changes).toEqual(['First line\n\n']);
    expect(element.textContent).toBe('First line\n\n');

    tree.setAttributeOnElement(id, 'value', 'First line');
    tree.flush();
    element.textContent = 'First line\n\n';
    const typed = makeFakeEvent('input');
    typed.inputType = 'insertText';
    element.dispatchEvent(typed);
    expect(changes).toEqual(['First line\n\n', 'First line\n\n']);
    expect(element.textContent).toBe('First line\n\n');

    tree.setAttributeOnElement(id, 'value', 'First line');
    tree.flush();
    element.textContent = 'First\n line';
    const middleReturn = makeFakeEvent('input');
    middleReturn.inputType = 'insertLineBreak';
    element.dispatchEvent(middleReturn);
    expect(changes).toEqual(['First line\n\n', 'First line\n\n', 'First\n line']);
    expect(element.textContent).toBe('First\n line');
    expect(element.value).toBe('First\n line');
  });

  it('renders textview background effects while preserving font attributes', () => {
    const id = createRootTestElement('textview');
    const element = getNode(id).htmlElement as unknown as FakeElement;

    tree.setAttributeOnElement(id, 'font', 'system-bold 18');
    tree.setAttributeOnElement(id, 'value', 'highlighted text');
    tree.setAttributeOnElement(id, 'backgroundEffectColor', 'rgba(251, 191, 36, 0.45)');
    tree.setAttributeOnElement(id, 'backgroundEffectBorderRadius', 12);
    tree.setAttributeOnElement(id, 'backgroundEffectPadding', 8);
    tree.flush();

    const wrapper = element.childNodes.item(0)!;
    const span = wrapper.childNodes.item(0)!;
    expect(element.style.fontFamily).toContain('-apple-system');
    expect(element.style.fontWeight).toBe('600');
    expect(wrapper.style.padding).toBe('4px 8px');
    expect(span.style.backgroundColor).toBe('rgba(251, 191, 36, 0.45)');
    expect(span.style['box-decoration-break']).toBe('clone');
    expect(span.style.borderRadius).toBe('12px');
    expect(span.style.padding).toBe('4px 8px');
    expect(span.style.marginLeft).toBe('-8px');
    expect(span.style.marginRight).toBe('-8px');
  });

  it('uses the system font stack for default text controls', () => {
    const labelId = createRootTestElement('label');
    const textFieldId = createRootTestElement('textfield');
    const textViewId = createRootTestElement('textview');

    expect(getNode(labelId).htmlElement.style.fontFamily).toContain('-apple-system');
    expect(getNode(textFieldId).htmlElement.style.fontFamily).toContain('-apple-system');
    expect(getNode(textViewId).htmlElement.style.fontFamily).toContain('-apple-system');
  });

  it('prefers the host application font stack for system text controls', () => {
    for (const viewClass of ['label', 'textfield', 'textview']) {
      const id = createRootTestElement(viewClass);
      const element = getNode(id).htmlElement;

      expect(element.style.fontFamily).withContext(viewClass).toContain('var(--font-sans,');

      tree.setAttributeOnElement(id, 'font', 'system-semibold 18');
      tree.flush();

      expect(element.style.fontFamily).withContext(viewClass).toContain('var(--font-sans,');
    }
  });

  it('maps system font weights to their visually equivalent browser weights', () => {
    const id = createRootTestElement('label');
    const element = getNode(id).htmlElement;

    for (const [font, weight] of [
      ['system 18', ''],
      ['system-medium 18', '500'],
      ['system-medium-italic 18', '500'],
      ['system-semibold 18', '600'],
      ['system-demi-bold 18', '600'],
      ['system-demi-bold-italic 18', '600'],
      ['system-bold 18', '600'],
      ['system-bold-italic 18', '600'],
      ['bold 18', '600'],
      ['title 18', '600'],
    ]) {
      tree.setAttributeOnElement(id, 'font', font);
      tree.flush();
      expect(element.style.fontWeight).withContext(font).toBe(weight);
    }
  });

  it('maps text input autocorrection modes to the browser attribute', () => {
    const id = createRootTestElement('textfield');
    const element = getNode(id).htmlElement;

    tree.setAttributeOnElement(id, 'autocorrection', 'none');
    tree.flush();
    expect(element.getAttribute('autocorrect')).toBe('off');

    tree.setAttributeOnElement(id, 'autocorrection', 'default');
    tree.flush();
    expect(element.getAttribute('autocorrect')).toBeNull();

    tree.setAttributeOnElement(id, 'autocorrection', 'none');
    tree.flush();
    tree.setAttributeOnElement(id, 'autocorrection', undefined);
    tree.flush();
    expect(element.getAttribute('autocorrect')).toBeNull();
  });

  it('maps label overflow modes to CSS and resets them', () => {
    const id = createRootTestElement('label');
    const element = getNode(id).htmlElement;

    tree.setAttributeOnElement(id, 'textOverflow', 'ellipsis');
    tree.flush();
    expect(element.style.textOverflow).toBe('ellipsis');

    tree.setAttributeOnElement(id, 'textOverflow', 'clip');
    tree.flush();
    expect(element.style.textOverflow).toBe('clip');

    tree.setAttributeOnElement(id, 'textOverflow', undefined);
    tree.flush();
    expect(element.style.textOverflow).toBe('');
  });

  it('maps relative and absolute line heights using the current text attribute contract', () => {
    const id = createRootTestElement('label');
    const element = getNode(id).htmlElement;

    tree.setAttributeOnElement(id, 'lineHeight', 1.5);
    tree.flush();
    expect(element.style.lineHeight).toBe('1.5');

    tree.setAttributeOnElement(id, 'lineHeightAbsolute', 24);
    tree.flush();
    expect(element.style.lineHeight).toBe('24px');

    tree.setAttributeOnElement(id, 'lineHeightAbsolute', undefined);
    tree.flush();
    expect(element.style.lineHeight).toBe('1.5');

    tree.setAttributeOnElement(id, 'lineHeight', undefined);
    tree.flush();
    expect(element.style.lineHeight).toBe('');
  });

  it('parses Valdi textShadow values with colors that contain spaces', () => {
    const id = createRootTestElement('label');
    const element = getNode(id).htmlElement;

    tree.setAttributeOnElement(id, 'textShadow', 'rgba(10, 20, 30, 0.8) 4 0.5 6 8');
    tree.flush();

    expect(element.style.textShadow).toBe('6px 8px 4px rgba(10, 20, 30, 0.4)');
  });

  it('reports attributed text layout without outline stroke inflation', () => {
    let reported: { x: number; y: number; width: number; height: number } | undefined;
    const attributedText = new AttributedTextBuilder()
      .append('outline', {
        font: 'system-bold 18',
        onLayout: (x, y, width, height) => {
          reported = { x, y, width, height };
        },
        outlineColor: '#FBBF24',
        outlineWidth: 1,
      })
      .build();
    const parsedAttributedText = ParsedAttributedText.parse(attributedText);
    const container = renderAttributedText(parsedAttributedText) as unknown as FakeElement;
    const span = container.childNodes.item(0)!;
    container.rectLeft = 10;
    container.rectTop = 20;
    span.rectLeft = 10;
    span.rectTop = 20;
    span.rectWidth = 59;
    span.rectHeight = 21;

    dispatchAttributedTextLayouts(parsedAttributedText, container as unknown as HTMLElement);

    expect(reported).toEqual({ x: 0, y: 0, width: 57, height: 21 });
  });

  it('keeps tappable attributed text interactive inside pointer-transparent labels', () => {
    const attributedText = new AttributedTextBuilder().append('tap', { onTap: () => {} }).build();
    const container = renderAttributedText(ParsedAttributedText.parse(attributedText)) as unknown as FakeElement;

    expect(container.childNodes.item(0)!.style.pointerEvents).toBe('auto');
  });

  it('reports attributed text layout after the post-layout scheduler when measurement is drained early', () => {
    let runPostLayoutCallbacks: (() => void) | undefined;
    tree.setPostLayoutScheduler(callback => {
      runPostLayoutCallbacks = callback;
    });

    let reported: { x: number; y: number; width: number; height: number } | undefined;
    const attributedText = new AttributedTextBuilder()
      .append('scheduled', {
        onLayout: (x, y, width, height) => {
          reported = { x, y, width, height };
        },
      })
      .build();
    const id = createRootTestElement('label');
    const label = getNode(id).htmlElement as unknown as FakeElement;
    label.rectWidth = 100;
    label.rectHeight = 20;

    tree.setAttributeOnElement(id, 'value', attributedText);
    tree.flush();

    expect(reported).toBeUndefined();
    expect(runPostLayoutCallbacks).toBeDefined();

    const container = label.childNodes.item(0)!;
    const span = container.childNodes.item(0)!;
    container.rectLeft = 20;
    container.rectTop = 30;
    span.rectLeft = 25;
    span.rectTop = 37;
    span.rectWidth = 44;
    span.rectHeight = 12;

    tree.drainScheduledLayoutObserverRefresh();
    expect(reported).toBeUndefined();
    runPostLayoutCallbacks!();

    expect(reported).toEqual({ x: 5, y: 7, width: 44, height: 12 });

    reported = undefined;
    container.rectLeft = 30;
    span.rectLeft = 38;
    span.rectWidth = 52;
    label.rectWidth = 110;
    dispatchWindowResize();
    expect(runPostLayoutCallbacks).toBeDefined();
    tree.drainScheduledLayoutObserverRefresh();
    expect(reported).toBeUndefined();
    runPostLayoutCallbacks!();
    expect(reported as { x: number; y: number; width: number; height: number } | undefined).toEqual({
      x: 8,
      y: 7,
      width: 52,
      height: 12,
    });
  });

  it('does not report a queued attributed text layout after the value is replaced', () => {
    let runPostLayoutCallbacks: (() => void) | undefined;
    tree.setPostLayoutScheduler(callback => {
      runPostLayoutCallbacks = callback;
    });

    let reportCount = 0;
    const attributedText = new AttributedTextBuilder()
      .append('scheduled', {
        onLayout: () => {
          reportCount++;
        },
      })
      .build();
    const id = createRootTestElement('label');
    const label = getNode(id).htmlElement as unknown as FakeElement;
    label.rectWidth = 100;
    label.rectHeight = 20;

    tree.setAttributeOnElement(id, 'value', attributedText);
    tree.flush();
    tree.drainScheduledLayoutObserverRefresh();

    tree.setAttributeOnElement(id, 'value', 'replacement');
    tree.flush();
    runPostLayoutCallbacks!();

    expect(reportCount).toBe(0);
  });

  it('renders label inline view attachments using child elements', () => {
    const labelId = createRootTestElement('label');
    const childId = createTestElement('view');
    const label = getNode(labelId).htmlElement;
    const child = getNode(childId).htmlElement as unknown as FakeElement;
    tree.moveElement(childId, labelId, 0);
    const attributedText = new AttributedTextBuilder().append('before ').appendInlineView(0).append(' after').build();

    tree.setAttributeOnElement(labelId, 'value', attributedText);
    tree.flush();

    const container = label.childNodes.item(0)!;
    const inlineSpan = container.childNodes.item(1)! as unknown as FakeElement;
    expect(inlineSpan.childNodes.item(0)).toBe(child);
    expect(inlineSpan.style.display).toBe('inline-flex');
    expect(inlineSpan.style.verticalAlign).toBe('middle');
  });

  it('animates attributed label parts and restores final styles', () => {
    const id = createRootTestElement('label');
    const attributedText = new AttributedTextBuilder()
      .append('fade', {
        animationTransform: {
          duration: 1,
          opacity: 0,
          scale: 0.5,
          translationY: 10,
        },
      })
      .build();

    tree.setAttributeOnElement(id, 'value', attributedText);
    tree.flush();

    const label = getNode(id).htmlElement as unknown as FakeElement;
    const container = label.childNodes.item(0)!;
    const span = container.childNodes.item(0)!;
    expect(span.style.opacity).toBe('0');
    expect(span.style.transform).toContain('translateY(10px)');
    expect(span.style.transform).toContain('scale(0.5)');

    flushTextAnimationFrame(1000);

    expect(span.style.opacity).toBe('');
    expect(span.style.transform).toBe('');
  });

  it('splits text animation partPattern matches and leaves unmatched text unanimated', () => {
    const id = createRootTestElement('label');
    const attributedText = new AttributedTextBuilder()
      .append('hi there', {
        animationTransform: {
          duration: 1,
          opacity: 0,
          partPattern: '\\S+',
        },
      })
      .build();

    tree.setAttributeOnElement(id, 'value', attributedText);
    tree.flush();

    const label = getNode(id).htmlElement as unknown as FakeElement;
    const container = label.childNodes.item(0)!;
    const partSpan = container.childNodes.item(0)!;
    const firstWord = partSpan.childNodes.item(0)!;
    const space = partSpan.childNodes.item(1)!;
    const secondWord = partSpan.childNodes.item(2)!;

    expect(partSpan.childNodes.length).toBe(3);
    expect(firstWord.textContent).toBe('hi');
    expect(space.textContent).toBe(' ');
    expect(secondWord.textContent).toBe('there');
    expect(firstWord.style.opacity).toBe('0');
    expect(space.style.opacity).toBeUndefined();
    expect(secondWord.style.opacity).toBe('0');
  });

  it('logs invalid text animation partPattern values and renders the text unanimated', () => {
    const errorSpy = spyOn(console, 'error');
    const id = createRootTestElement('label');
    const attributedText = new AttributedTextBuilder()
      .append('invalid', {
        animationTransform: {
          opacity: 0,
          partPattern: '[',
        },
      })
      .build();

    tree.setAttributeOnElement(id, 'value', attributedText);
    tree.flush();

    const label = getNode(id).htmlElement as unknown as FakeElement;
    const container = label.childNodes.item(0)!;
    const partSpan = container.childNodes.item(0)!;

    expect(errorSpy).toHaveBeenCalled();
    expect(String(errorSpy.calls.mostRecent().args[0])).toContain('Invalid text animation partPattern');
    expect(partSpan.childNodes.length).toBe(0);
    expect(partSpan.style.opacity).toBeUndefined();
  });

  it('continues textview animations across content rerenders with the same key', () => {
    const id = createRootTestElement('textview');
    const attributedText = new AttributedTextBuilder()
      .append('rerender', {
        animationTransform: {
          duration: 1,
          key: 'stable',
          opacity: 0,
        },
      })
      .build();

    tree.setAttributeOnElement(id, 'value', attributedText);
    tree.flush();

    const textView = getNode(id).htmlElement as unknown as FakeElement;
    let container = textView.childNodes.item(0)!;
    let span = container.childNodes.item(0)!;
    expect(span.style.opacity).toBe('0');

    flushTextAnimationFrame(500);
    expect(Number(span.style.opacity)).toBeCloseTo(0.875, 3);

    tree.setAttributeOnElement(id, 'backgroundEffectColor', '#DDEEFF');
    tree.flush();

    const wrapper = textView.childNodes.item(0)!;
    container = wrapper.childNodes.item(0)!;
    span = container.childNodes.item(0)!;
    expect(Number(span.style.opacity)).toBeCloseTo(0.875, 3);
  });

  it('coordinates text animations across textanimationgroup descendants', () => {
    const root = createRootTestElement('view');
    const group = createTestElement('SCValdiTextAnimationGroup');
    const first = createTestElement('label');
    const nestedView = createTestElement('view');
    const second = createTestElement('label');

    tree.moveElement(group, root, 0);
    tree.moveElement(first, group, 0);
    tree.moveElement(nestedView, group, 1);
    tree.moveElement(second, nestedView, 0);

    tree.setAttributeOnElement(first, 'value', animatedText('first'));
    tree.setAttributeOnElement(second, 'value', animatedText('second'));
    tree.flush();

    const firstSpan = attributedPartSpan(first);
    const secondSpan = attributedPartSpan(second);
    expect(firstSpan.style.opacity).toBe('0');
    expect(secondSpan.style.opacity).toBe('0');

    flushTextAnimationFrame(0);
    flushTextAnimationFrame(50);

    expect(Number(firstSpan.style.opacity)).toBeGreaterThan(0);
    expect(secondSpan.style.opacity).toBe('0');

    flushTextAnimationFrame(150);

    expect(Number(secondSpan.style.opacity)).toBeGreaterThan(0);
  });

  it('keeps nested textanimationgroup timelines isolated from ancestor groups', () => {
    const root = createRootTestElement('view');
    const outerGroup = createTestElement('SCValdiTextAnimationGroup');
    const outerFirst = createTestElement('label');
    const innerGroup = createTestElement('SCValdiTextAnimationGroup');
    const innerLabel = createTestElement('label');
    const outerSecond = createTestElement('label');

    tree.moveElement(outerGroup, root, 0);
    tree.moveElement(outerFirst, outerGroup, 0);
    tree.moveElement(innerGroup, outerGroup, 1);
    tree.moveElement(innerLabel, innerGroup, 0);
    tree.moveElement(outerSecond, outerGroup, 2);

    tree.setAttributeOnElement(outerFirst, 'value', animatedText('outer first'));
    tree.setAttributeOnElement(innerLabel, 'value', animatedText('inner'));
    tree.setAttributeOnElement(outerSecond, 'value', animatedText('outer second'));
    tree.flush();

    const innerSpan = attributedPartSpan(innerLabel);
    const outerSecondSpan = attributedPartSpan(outerSecond);

    flushTextAnimationFrame(0);
    flushTextAnimationFrame(50);

    expect(Number(innerSpan.style.opacity)).toBeGreaterThan(0);
    expect(outerSecondSpan.style.opacity).toBe('0');
  });

  it('unregisters text animation participants when attributed text becomes plain text', () => {
    const id = createRootTestElement('label');
    const attributedText = new AttributedTextBuilder()
      .append('animated', {
        animationTransform: {
          duration: 1,
          opacity: 0,
        },
      })
      .build();

    tree.setAttributeOnElement(id, 'value', attributedText);
    tree.flush();

    const animatedSpan = attributedPartSpan(id);
    expect(animatedSpan.style.opacity).toBe('0');

    tree.setAttributeOnElement(id, 'value', 'plain');
    tree.flush();

    expect(animatedSpan.style.opacity).toBe('');
  });

  it('skips stale attributed text layout callbacks after content is replaced', () => {
    let runPostLayoutCallbacks: (() => void) | undefined;
    tree.setPostLayoutScheduler(callback => {
      runPostLayoutCallbacks = callback;
    });

    let reported = false;
    const attributedText = new AttributedTextBuilder()
      .append('stale', {
        onLayout: () => {
          reported = true;
        },
      })
      .build();
    const id = createRootTestElement('label');

    tree.setAttributeOnElement(id, 'value', attributedText);
    tree.flush();
    tree.setAttributeOnElement(id, 'value', 'plain');
    tree.flush();

    expect(runPostLayoutCallbacks).toBeDefined();
    runPostLayoutCallbacks!();

    expect(reported).toBeFalse();
  });

  it('maps textview textDecoration values through the element class applier', () => {
    const id = createRootTestElement('textview');
    const element = getNode(id).htmlElement;

    tree.setAttributeOnElement(id, 'textDecoration', 'underline');
    tree.flush();
    expect(element.style.textDecorationLine).toBe('underline');
    expect(element.style.textDecorationStyle).toBe('');

    tree.setAttributeOnElement(id, 'textDecoration', 'dashed-underline');
    tree.flush();
    expect(element.style.textDecorationLine).toBe('underline');
    expect(element.style.textDecorationStyle).toBe('dashed');

    tree.setAttributeOnElement(id, 'textDecoration', 'dotted-underline');
    tree.flush();
    expect(element.style.textDecorationLine).toBe('underline');
    expect(element.style.textDecorationStyle).toBe('dotted');

    tree.setAttributeOnElement(id, 'textDecoration', 'strikethrough');
    tree.flush();
    expect(element.style.textDecorationLine).toBe('line-through');
    expect(element.style.textDecorationStyle).toBe('');

    tree.setAttributeOnElement(id, 'textDecoration', 'none');
    tree.flush();
    expect(element.style.textDecorationLine).toBe('none');
    expect(element.style.textDecorationStyle).toBe('');
  });

  it('renders image assets with logical 3x dimensions for objectFit variants', () => {
    const id = createRootTestElement('image');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.rectWidth = 200;
    element.rectHeight = 200;

    tree.setAttributeOnElement(id, 'src', 'test.png');
    tree.flush();
    triggerImageLoad(300, 150);
    const image = getLastImage();

    tree.setAttributeOnElement(id, 'objectFit', 'none');
    tree.flush();
    expect(image.style.width).toBe('300px');
    expect(image.style.height).toBe('150px');

    tree.setAttributeOnElement(id, 'objectFit', 'contain');
    tree.flush();
    expect(image.style.width).toBe('200px');
    expect(image.style.height).toBe('100px');

    tree.setAttributeOnElement(id, 'objectFit', 'cover');
    tree.flush();
    expect(image.style.width).toBe('400px');
    expect(image.style.height).toBe('200px');

    element.rectWidth = 50;
    element.rectHeight = 50;
    tree.setAttributeOnElement(id, 'objectFit', 'scale-down');
    tree.flush();
    expect(image.style.width).toBe('50px');
    expect(image.style.height).toBe('25px');
  });

  it('defers loaded image geometry reads until the centralized layout pass', () => {
    let runLayoutPass: (() => void) | undefined;
    tree.setPostLayoutScheduler(callback => {
      runLayoutPass = callback;
    });
    const id = createRootTestElement('image');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.rectWidth = 200;
    element.rectHeight = 100;

    tree.setAttributeOnElement(id, 'src', 'test.png');
    tree.flush();
    triggerImageLoad(300, 150);

    expect(element.rectReadCount).toBe(0);
    expect(element.childNodes.length).toBe(0);
    expect(runLayoutPass).toBeDefined();

    runLayoutPass!();
    expect(element.rectReadCount).toBe(1);
    expect(element.querySelector('img')).toBe(getLastImage());
  });

  it('uses SVG viewBox dimensions as logical image dimensions', async () => {
    const id = createRootTestElement('image');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.rectWidth = 200;
    element.rectHeight = 200;
    const svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 120 80"></svg>';
    let decodedWidth = -1;
    let decodedHeight = -1;

    tree.setAttributeOnElement(id, 'onImageDecoded', (width: number, height: number) => {
      decodedWidth = width;
      decodedHeight = height;
    });
    tree.setAttributeOnElement(id, 'src', `data:image/svg+xml,${encodeURIComponent(svg)}`);
    tree.setAttributeOnElement(id, 'objectFit', 'none');
    tree.flush();
    triggerImageLoad(300, 150);

    expect(getLastImage().style.width).toBe('360px');
    expect(getLastImage().style.height).toBe('240px');
    await new Promise(resolve => setTimeout(resolve, 0));
    expect(decodedWidth).toBe(360);
    expect(decodedHeight).toBe(240);
  });

  it('resolves renderable path-only Asset objects for image sources', () => {
    const id = createRootTestElement('image');

    tree.setAttributeOnElement(id, 'src', { path: 'asset-from-path.png', width: 120, height: 80 });
    tree.flush();

    expect(getLastImage().src).toBe('asset-from-path.png');

    tree.setAttributeOnElement(id, 'src', { path: 'image', src: { default: 'asset-from-src.png' } });
    tree.flush();

    expect(getLastImage().src).toBe('asset-from-src.png');

    const logicalId = createRootTestElement('image');
    tree.setAttributeOnElement(logicalId, 'src', { path: 'image', width: 120, height: 80 });
    tree.flush();

    expect(getNode(logicalId).htmlElement.childNodes.length).toBe(0);
  });

  it('only applies shape stroke dash attributes for partial strokes', () => {
    const id = createRootTestElement('shape');
    tree.setAttributeOnElement(id, 'path', 'M 0 0 L 100 0');
    tree.flush();

    const path = getNode(id).htmlElement.querySelector('path')!;
    expect(path.getAttribute('stroke-dasharray')).toBeNull();
    expect(path.getAttribute('stroke-dashoffset')).toBeNull();

    tree.setAttributeOnElement(id, 'strokeStart', 0.25);
    tree.setAttributeOnElement(id, 'strokeEnd', 0.75);
    tree.flush();

    expect(path.getAttribute('stroke-dasharray')).toBe('50 100');
    expect(path.getAttribute('stroke-dashoffset')).toBe('-25');
  });

  it('applies image contentRotation as a CSS transform', () => {
    const id = createRootTestElement('image');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.rectWidth = 220;
    element.rectHeight = 140;

    tree.setAttributeOnElement(id, 'src', 'test.png');
    tree.setAttributeOnElement(id, 'contentRotation', 0.24);
    tree.flush();
    triggerImageLoad(300, 150);

    expect(getLastImage().style.transform).toContain('rotate(0.24rad)');
  });

  it('mirrors the image element when flipOnRtl is set under RTL layout', () => {
    const windowStub = (globalThis as { window?: { getComputedStyle?: (element: unknown) => { direction: string } } })
      .window;
    if (windowStub) {
      windowStub.getComputedStyle = () => ({ direction: 'rtl' });
    }
    const id = createRootTestElement('image');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.rectWidth = 220;
    element.rectHeight = 140;

    tree.setAttributeOnElement(id, 'src', 'test.png');
    tree.setAttributeOnElement(id, 'flipOnRtl', true);
    tree.flush();
    triggerImageLoad(300, 150);

    expect(getLastImage().style.transform).toContain('scale(-1, 1)');
  });

  it('reports image decode/load callbacks asynchronously and auto-sizes using logical 3x dimensions', (done: DoneFn) => {
    const id = createRootTestElement('image');
    const element = getNode(id).htmlElement;
    let decodedWidth = -1;
    let decodedHeight = -1;
    let assetLoadSuccess = false;

    tree.setAttributeOnElement(id, 'onAssetLoad', (success: boolean) => {
      assetLoadSuccess = success;
    });
    tree.setAttributeOnElement(id, 'onImageDecoded', (width: number, height: number) => {
      decodedWidth = width;
      decodedHeight = height;
    });
    tree.setAttributeOnElement(id, 'src', 'test.png');
    tree.flush();
    triggerImageLoad(300, 150);

    expect(element.style.width).toBe('100px');
    expect(element.style.height).toBe('50px');
    setTimeout(() => {
      expect(assetLoadSuccess).toBeTrue();
      expect(decodedWidth).toBe(300);
      expect(decodedHeight).toBe(150);
      done();
    }, 0);
  });

  it('tracks image requests on the view-node tree until they finish loading', () => {
    const assetTracker = new ViewNodeAssetTracker();
    tree.setAssetTracker(assetTracker);
    const id = createRootTestElement('image');
    let allAssetsLoaded = false;

    tree.setAttributeOnElement(id, 'src', 'test.png');
    tree.flush();
    assetTracker.onAllAssetsLoaded(() => {
      allAssetsLoaded = true;
    });

    expect(assetTracker.assetsCount).toBe(1);
    expect(allAssetsLoaded).toBeFalse();

    triggerImageLoad(300, 150);

    expect(allAssetsLoaded).toBeTrue();
    expect(assetTracker.collectErrors()).toBeUndefined();
  });

  it('settles failed and canceled tree-level image requests', () => {
    const assetTracker = new ViewNodeAssetTracker();
    tree.setAssetTracker(assetTracker);
    const id = createRootTestElement('image');

    tree.setAttributeOnElement(id, 'src', 'missing.png');
    tree.flush();
    triggerImageError();

    expect(assetTracker.collectErrors()).toEqual(['Failed to load image']);

    tree.setAttributeOnElement(id, 'src', 'replacement.png');
    tree.flush();

    let allAssetsLoaded = false;
    assetTracker.onAllAssetsLoaded(() => {
      allAssetsLoaded = true;
    });
    expect(allAssetsLoaded).toBeFalse();

    tree.destroyElement(id);

    expect(allAssetsLoaded).toBeTrue();
    expect(assetTracker.assetsCount).toBe(0);
  });

  it('tracks a replacement image request when pixel effects require a CORS-safe reload', () => {
    const assetTracker = new ViewNodeAssetTracker();
    tree.setAssetTracker(assetTracker);
    const id = createRootTestElement('image');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.rectWidth = 100;
    element.rectHeight = 50;

    tree.setAttributeOnElement(id, 'src', 'https://example.test/image.png');
    tree.flush();
    triggerImageLoad(300, 150);

    tree.setAttributeOnElement(id, 'tint', '#ff0000');
    tree.flush();

    let allAssetsLoaded = false;
    assetTracker.onAllAssetsLoaded(() => {
      allAssetsLoaded = true;
    });
    expect(allAssetsLoaded).toBeFalse();

    triggerImageLoad(300, 150);

    expect(allAssetsLoaded).toBeTrue();
    expect(assetTracker.assetsCount).toBe(1);
  });

  it('reports image load failures asynchronously', (done: DoneFn) => {
    const id = createRootTestElement('image');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    let assetLoadSuccess = true;
    let assetLoadError = '';

    element.rectWidth = 100;
    element.rectHeight = 50;
    tree.setAttributeOnElement(id, 'src', 'test.png');
    tree.flush();
    triggerImageLoad(300, 150);
    expect(element.querySelector('img')).not.toBeNull();

    tree.setAttributeOnElement(id, 'onAssetLoad', (success: boolean, error: string | undefined) => {
      assetLoadSuccess = success;
      assetLoadError = error ?? '';
    });
    tree.setAttributeOnElement(id, 'src', 'missing.png');
    tree.flush();
    triggerImageError();

    expect(element.childNodes.length).toBe(0);

    setTimeout(() => {
      expect(assetLoadSuccess).toBeFalse();
      expect(assetLoadError).toBe('Failed to load image');
      done();
    }, 0);
  });

  it('loads ordinary remote images directly without CORS', () => {
    const id = createRootTestElement('image');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.rectWidth = 100;
    element.rectHeight = 50;

    tree.setAttributeOnElement(id, 'src', 'https://example.test/image.png');
    tree.flush();
    expect(getLastImage().crossOrigin).toBeNull();
    expect(getLastImage().src).toBe('https://example.test/image.png');

    triggerImageLoad(300, 150);
    expect(element.querySelector('img')).toBe(getLastImage());
    expect(element.querySelector('canvas')).toBeNull();
    expect(getLastImage().style.transform ?? '').toBe('');
  });

  it('treats malformed absolute image URLs as cross-origin without throwing', () => {
    const windowStub = (globalThis as { window?: { location?: { href: string; origin: string } } }).window;
    expect(windowStub).toBeDefined();
    windowStub!.location = { href: 'https://app.example/', origin: 'https://app.example' };
    const warnSpy = spyOn(console, 'warn');
    const id = createRootTestElement('image');

    tree.setAttributeOnElement(id, 'src', 'https://[invalid');
    tree.flush();

    expect(getLastImage().src).toBe('https://[invalid');
    expect(warnSpy).toHaveBeenCalledWith(
      jasmine.stringMatching('Valdi web renderer could not parse image URL for origin comparison'),
    );
  });

  it('coalesces resolved image attributes into one render configuration', () => {
    const id = createRootTestElement('image');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.rectWidth = 100;
    element.rectHeight = 50;

    tree.setAttributeOnElement(id, 'src', 'https://example.test/image.png');
    tree.setAttributeOnElement(id, 'objectFit', 'contain');
    tree.setAttributeOnElement(id, 'contentScaleX', 2);
    tree.setAttributeOnElement(id, 'tint', '#ff0000');
    tree.flush();

    expect(imageConstructionCount).toBe(1);
    expect(getLastImage().crossOrigin).toBe('anonymous');
    expect(element.querySelector('canvas')).not.toBeNull();
  });

  it('reuses a canvas-safe image when a pixel effect is enabled', () => {
    const id = createRootTestElement('image');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.rectWidth = 100;
    element.rectHeight = 50;

    tree.setAttributeOnElement(id, 'src', 'test.png');
    tree.flush();
    triggerImageLoad(300, 150);
    const image = getLastImage();

    tree.setAttributeOnElement(id, 'tint', '#ff0000');
    tree.flush();

    expect(getLastImage()).toBe(image);
    expect(element.querySelector('img')).toBeNull();
    expect(element.querySelector('canvas')!.canvasContext.drawImage).toHaveBeenCalled();
  });

  it('swaps to a CORS-enabled canvas implementation only for pixel effects', () => {
    const id = createRootTestElement('image');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.rectWidth = 100;
    element.rectHeight = 50;

    tree.setAttributeOnElement(id, 'src', 'https://example.test/image.png');
    tree.flush();
    triggerImageLoad(300, 150);
    const displayImage = getLastImage();
    expect(displayImage.crossOrigin).toBeNull();
    expect(element.querySelector('img')).toBe(displayImage);

    tree.setAttributeOnElement(id, 'tint', '#ff0000');
    tree.flush();
    const canvasImage = getLastImage();
    expect(canvasImage).not.toBe(displayImage);
    expect(canvasImage.crossOrigin).toBe('anonymous');
    expect(element.querySelector('canvas')).not.toBeNull();
    expect(element.querySelector('img')).toBeNull();

    triggerImageLoad(300, 150);
    const canvas = element.querySelector('canvas')!;
    expect(canvas.canvasContext.drawImage).toHaveBeenCalled();

    tree.setAttributeOnElement(id, 'tint', undefined);
    tree.flush();
    expect(element.querySelector('img')).toBe(canvasImage);
    expect(element.querySelector('canvas')).toBeNull();
  });

  it('clears the active image implementation when the source is removed', () => {
    const id = createRootTestElement('image');
    const element = getNode(id).htmlElement as unknown as FakeElement;
    element.rectWidth = 100;
    element.rectHeight = 50;

    tree.setAttributeOnElement(id, 'src', 'test.png');
    tree.flush();
    triggerImageLoad(300, 150);
    expect(element.childNodes.length).toBe(1);

    tree.setAttributeOnElement(id, 'src', undefined);
    tree.flush();

    expect(element.childNodes.length).toBe(0);
  });
});
