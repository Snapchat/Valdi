import type { VisibilityObserver } from 'valdi_core/src/IRendererDelegate';

interface Viewport {
  x: number;
  y: number;
  width: number;
  height: number;
}

type ViewportTuple = [number, number, number, number];
interface ObservedElement {
  element: HTMLElement;
  isVisible: boolean;
  lastViewport?: ViewportTuple;
}

const EMPTY_ELEMENT_IDS: number[] = [];

export class VisibilityObserverController {
  private observer?: VisibilityObserver;
  private htmlRoot?: HTMLElement | ShadowRoot;
  private readonly observedElementsById = new Map<number, ObservedElement>();
  private readonly elementIdsByHtmlElement = new WeakMap<Element, number>();
  private intersectionObserver?: IntersectionObserver;

  setRoot(htmlRoot: HTMLElement | ShadowRoot): void {
    if (this.htmlRoot === htmlRoot) {
      return;
    }
    this.htmlRoot = htmlRoot;
    this.recreateIntersectionObserver();
    this.scheduleRefresh(false);
  }

  registerObserver(observer: VisibilityObserver): void {
    this.observer = observer;
    this.ensureIntersectionObserver();
    this.scheduleRefresh(false);
  }

  observeElement(id: number, element: HTMLElement): void {
    const observedElement = this.observedElementsById.get(id);
    if (observedElement?.element === element) {
      return;
    }
    this.ensureIntersectionObserver();
    if (observedElement) {
      this.intersectionObserver?.unobserve(observedElement.element);
      this.elementIdsByHtmlElement.delete(observedElement.element);
    }
    this.observedElementsById.set(id, {
      element,
      isVisible: false,
    });
    this.elementIdsByHtmlElement.set(element, id);
    if (this.intersectionObserver) {
      this.intersectionObserver.observe(element);
      if (typeof document !== 'undefined' && document.visibilityState === 'hidden') {
        Promise.resolve().then(() => {
          if (this.observedElementsById.get(id)?.element === element) {
            this.scheduleRefresh(true);
          }
        });
      }
    } else {
      this.scheduleRefresh(false);
    }
  }

  unobserveElement(id: number): void {
    this.destroyElement(id);
  }

  destroyElement(id: number): void {
    const observedElement = this.observedElementsById.get(id);
    if (!observedElement) {
      return;
    }
    this.intersectionObserver?.unobserve(observedElement.element);
    this.elementIdsByHtmlElement.delete(observedElement.element);
    this.observedElementsById.delete(id);
  }

  scheduleRefresh(force: boolean): void {
    this.processPendingIntersectionEntries();
    if (force && typeof document !== 'undefined' && document.visibilityState === 'hidden') {
      this.processHiddenPageIntersections();
    }
  }

  drainScheduledRefresh(force: boolean): void {
    this.processPendingIntersectionEntries();
    if (force && typeof document !== 'undefined' && document.visibilityState === 'hidden') {
      this.processHiddenPageIntersections();
    }
  }

  destroy(): void {
    this.intersectionObserver?.disconnect();
    this.intersectionObserver = undefined;
    this.observer = undefined;
    this.htmlRoot = undefined;
    this.observedElementsById.clear();
  }

  private ensureIntersectionObserver(): void {
    if (this.intersectionObserver || !this.observer || !this.htmlRoot) {
      return;
    }
    const root =
      typeof ShadowRoot !== 'undefined' && this.htmlRoot instanceof ShadowRoot ? null : (this.htmlRoot as HTMLElement);
    this.intersectionObserver = new IntersectionObserver(entries => this.processIntersectionEntries(entries), {
      root,
      threshold: [0, 1],
    });
    for (const observedElement of this.observedElementsById.values()) {
      this.intersectionObserver.observe(observedElement.element);
    }
  }

  private recreateIntersectionObserver(): void {
    this.intersectionObserver?.disconnect();
    this.intersectionObserver = undefined;
    this.ensureIntersectionObserver();
  }

  private processPendingIntersectionEntries(): void {
    const entries = this.intersectionObserver?.takeRecords();
    if (entries?.length) {
      this.processIntersectionEntries(entries);
    }
  }

  /** Hidden browser pages may suppress IntersectionObserver callbacks even while their layouts remain measurable. */
  private processHiddenPageIntersections(): void {
    const root = this.htmlRoot;
    if (this.observer === undefined || root === undefined) {
      return;
    }

    const rootRect =
      typeof ShadowRoot !== 'undefined' && root instanceof ShadowRoot
        ? { left: 0, top: 0, right: window.innerWidth, bottom: window.innerHeight }
        : (root as HTMLElement).getBoundingClientRect();
    const entries: IntersectionObserverEntry[] = [];
    this.observedElementsById.forEach(observed => {
      const bounds = observed.element.getBoundingClientRect();
      const left = Math.max(bounds.left, rootRect.left);
      const top = Math.max(bounds.top, rootRect.top);
      const right = Math.min(bounds.right, rootRect.right);
      const bottom = Math.min(bounds.bottom, rootRect.bottom);
      const width = Math.max(0, right - left);
      const height = Math.max(0, bottom - top);
      entries.push({
        boundingClientRect: bounds,
        intersectionRatio:
          bounds.width > 0 && bounds.height > 0 ? (width * height) / (bounds.width * bounds.height) : 0,
        intersectionRect: { bottom, height, left, right, top, width, x: left, y: top } as DOMRectReadOnly,
        isIntersecting: width > 0 && height > 0,
        rootBounds: rootRect as DOMRectReadOnly,
        target: observed.element,
        time: performance.now(),
      } as IntersectionObserverEntry);
    });
    this.processIntersectionEntries(entries);
  }

  private processIntersectionEntries(entries: IntersectionObserverEntry[]): void {
    const observer = this.observer;
    if (!observer || !this.htmlRoot) {
      return;
    }

    let appearingElements: number[] | undefined;
    let disappearingElements: number[] | undefined;
    let viewportUpdates: number[] | undefined;
    for (let i = 0; i < entries.length; i++) {
      const entry = entries[i];
      const elementId = this.elementIdsByHtmlElement.get(entry.target);
      if (elementId === undefined) {
        continue;
      }
      const observedElement = this.observedElementsById.get(elementId);
      if (!observedElement || observedElement.element !== entry.target) {
        continue;
      }
      const viewport = this.visibleViewportForIntersectionEntry(entry);
      const wasVisible = observedElement.isVisible;
      const isVisible = !!viewport && viewport.width > 0 && viewport.height > 0;
      if (isVisible) {
        observedElement.isVisible = true;
        if (!wasVisible) {
          (appearingElements ??= []).push(elementId);
        }
        if (!this.hasSameViewport(observedElement, viewport)) {
          observedElement.lastViewport = [viewport.x, viewport.y, viewport.width, viewport.height];
          (viewportUpdates ??= []).push(elementId, viewport.x, viewport.y, viewport.width, viewport.height);
        }
      } else if (wasVisible) {
        observedElement.isVisible = false;
        observedElement.lastViewport = undefined;
        (disappearingElements ??= []).push(elementId);
      }
    }

    if (appearingElements || disappearingElements || viewportUpdates) {
      observer(
        appearingElements ?? EMPTY_ELEMENT_IDS,
        disappearingElements ?? EMPTY_ELEMENT_IDS,
        viewportUpdates ?? EMPTY_ELEMENT_IDS,
        performance.now(),
      );
    }
  }

  private hasSameViewport(observedElement: ObservedElement, viewport: Viewport): boolean {
    const lastViewport = observedElement.lastViewport;
    return (
      !!lastViewport &&
      lastViewport[0] === viewport.x &&
      lastViewport[1] === viewport.y &&
      lastViewport[2] === viewport.width &&
      lastViewport[3] === viewport.height
    );
  }

  private visibleViewportForIntersectionEntry(entry: IntersectionObserverEntry): Viewport | undefined {
    const intersectionRect = entry.intersectionRect;
    if (!entry.isIntersecting || intersectionRect.width <= 0 || intersectionRect.height <= 0) {
      return undefined;
    }
    const elementRect = entry.boundingClientRect;
    return {
      x: intersectionRect.left - elementRect.left,
      y: intersectionRect.top - elementRect.top,
      width: intersectionRect.width,
      height: intersectionRect.height,
    };
  }
}
