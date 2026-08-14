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

  scheduleRefresh(_force: boolean): void {
    this.processPendingIntersectionEntries();
  }

  drainScheduledRefresh(_force: boolean): void {
    this.processPendingIntersectionEntries();
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
