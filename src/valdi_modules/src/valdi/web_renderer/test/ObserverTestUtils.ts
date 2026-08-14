export type Rect = {
  left: number;
  top: number;
  width: number;
  height: number;
  right?: number;
  bottom?: number;
};

export type FakeElement = HTMLElement & {
  rect: Rect;
  offsetParent: FakeElement | null;
  scrollLeft: number;
  scrollTop: number;
  borderLeftWidth: string;
  borderTopWidth: string;
};

type ResizeCallback = (entries: ResizeObserverEntry[]) => void;
type IntersectionCallback = (entries: IntersectionObserverEntry[]) => void;

export class FakeResizeObserver {
  static lastInstance?: FakeResizeObserver;

  readonly observedElements: Element[] = [];
  private readonly callback: ResizeCallback;

  constructor(callback: ResizeCallback) {
    this.callback = callback;
    FakeResizeObserver.lastInstance = this;
  }

  observe(element: Element): void {
    this.observedElements.push(element);
  }

  unobserve(element: Element): void {
    const index = this.observedElements.indexOf(element);
    if (index >= 0) {
      this.observedElements.splice(index, 1);
    }
  }

  disconnect(): void {
    this.observedElements.length = 0;
  }

  trigger(element: Element): void {
    this.callback([{ target: element } as ResizeObserverEntry]);
  }
}

export class FakeIntersectionObserver {
  static instances: FakeIntersectionObserver[] = [];
  static lastInstance?: FakeIntersectionObserver;

  readonly observedElements: Element[] = [];
  readonly root: Element | Document | null;
  readonly thresholds: ReadonlyArray<number>;
  private readonly callback: IntersectionCallback;

  constructor(callback: IntersectionCallback, options?: IntersectionObserverInit) {
    this.callback = callback;
    this.root = options?.root ?? null;
    const threshold = options?.threshold ?? 0;
    this.thresholds = typeof threshold === 'number' ? [threshold] : [...threshold];
    FakeIntersectionObserver.instances.push(this);
    FakeIntersectionObserver.lastInstance = this;
  }

  observe(element: Element): void {
    if (!this.observedElements.includes(element)) {
      this.observedElements.push(element);
    }
  }

  unobserve(element: Element): void {
    const index = this.observedElements.indexOf(element);
    if (index >= 0) {
      this.observedElements.splice(index, 1);
    }
  }

  disconnect(): void {
    this.observedElements.length = 0;
  }

  takeRecords(): IntersectionObserverEntry[] {
    return [];
  }

  trigger(...elements: Element[]): void {
    const rootRect = this.root && 'getBoundingClientRect' in this.root ? this.root.getBoundingClientRect() : undefined;
    const entries = elements.map(element => {
      const boundingClientRect = element.getBoundingClientRect();
      const left = rootRect ? Math.max(boundingClientRect.left, rootRect.left) : boundingClientRect.left;
      const top = rootRect ? Math.max(boundingClientRect.top, rootRect.top) : boundingClientRect.top;
      const right = rootRect ? Math.min(boundingClientRect.right, rootRect.right) : boundingClientRect.right;
      const bottom = rootRect ? Math.min(boundingClientRect.bottom, rootRect.bottom) : boundingClientRect.bottom;
      const width = Math.max(0, right - left);
      const height = Math.max(0, bottom - top);
      return {
        target: element,
        isIntersecting: width > 0 && height > 0,
        boundingClientRect,
        intersectionRect: { left, top, right, bottom, width, height },
      } as IntersectionObserverEntry;
    });
    this.callback(entries);
  }
}

export function makeElement(rect: Rect): FakeElement {
  return {
    rect,
    offsetParent: null,
    scrollLeft: 0,
    scrollTop: 0,
    borderLeftWidth: '0',
    borderTopWidth: '0',
    getBoundingClientRect(): Rect {
      return this.rect;
    },
  } as FakeElement;
}

export function installObserverTestGlobals(): () => void {
  const previousRequestAnimationFrame = (globalThis as { requestAnimationFrame?: unknown }).requestAnimationFrame;
  const previousGetComputedStyle = (globalThis as { getComputedStyle?: unknown }).getComputedStyle;
  const previousResizeObserver = (globalThis as { ResizeObserver?: unknown }).ResizeObserver;
  const previousIntersectionObserver = (globalThis as { IntersectionObserver?: unknown }).IntersectionObserver;
  const previousPerformance = (globalThis as { performance?: unknown }).performance;
  const pendingAnimationFrames: Array<() => void> = [];

  FakeResizeObserver.lastInstance = undefined;
  FakeIntersectionObserver.instances = [];
  FakeIntersectionObserver.lastInstance = undefined;

  (globalThis as { requestAnimationFrame?: (callback: () => void) => number }).requestAnimationFrame = callback => {
    pendingAnimationFrames.push(callback);
    return pendingAnimationFrames.length;
  };
  (globalThis as { getComputedStyle?: (element: unknown) => { borderLeftWidth: string; borderTopWidth: string } })
    .getComputedStyle = element => ({
      borderLeftWidth: (element as FakeElement).borderLeftWidth,
      borderTopWidth: (element as FakeElement).borderTopWidth,
    });
  (globalThis as { ResizeObserver?: unknown }).ResizeObserver = FakeResizeObserver;
  (globalThis as { IntersectionObserver?: unknown }).IntersectionObserver = FakeIntersectionObserver;
  (globalThis as { performance?: unknown }).performance = { now: () => 1234 };

  (globalThis as { __flushObserverAnimationFrame?: () => void }).__flushObserverAnimationFrame = () => {
    const callback = pendingAnimationFrames.shift();
    if (callback) {
      callback();
    }
  };

  return () => {
    if (previousRequestAnimationFrame === undefined) {
      delete (globalThis as { requestAnimationFrame?: unknown }).requestAnimationFrame;
    } else {
      (globalThis as { requestAnimationFrame?: unknown }).requestAnimationFrame = previousRequestAnimationFrame;
    }
    if (previousGetComputedStyle === undefined) {
      delete (globalThis as { getComputedStyle?: unknown }).getComputedStyle;
    } else {
      (globalThis as { getComputedStyle?: unknown }).getComputedStyle = previousGetComputedStyle;
    }
    if (previousResizeObserver === undefined) {
      delete (globalThis as { ResizeObserver?: unknown }).ResizeObserver;
    } else {
      (globalThis as { ResizeObserver?: unknown }).ResizeObserver = previousResizeObserver;
    }
    if (previousIntersectionObserver === undefined) {
      delete (globalThis as { IntersectionObserver?: unknown }).IntersectionObserver;
    } else {
      (globalThis as { IntersectionObserver?: unknown }).IntersectionObserver = previousIntersectionObserver;
    }
    if (previousPerformance === undefined) {
      delete (globalThis as { performance?: unknown }).performance;
    } else {
      (globalThis as { performance?: unknown }).performance = previousPerformance;
    }
    delete (globalThis as { __flushObserverAnimationFrame?: unknown }).__flushObserverAnimationFrame;
    FakeResizeObserver.lastInstance = undefined;
    FakeIntersectionObserver.instances = [];
    FakeIntersectionObserver.lastInstance = undefined;
  };
}

export function flushObserverAnimationFrame(): void {
  (globalThis as unknown as { __flushObserverAnimationFrame: () => void }).__flushObserverAnimationFrame();
}
