import type { ElementFrame } from 'valdi_tsx/src/Geometry';
import type { ElementLayoutObserver } from './core/ElementClass';

type OnLayoutCallback = (frame: ElementFrame) => void;
type PostLayoutScheduler = (callback: () => void) => void;
type RenderCompleteScheduler = (callback: () => void) => void;

interface LayoutObserverEntry {
  readonly attributeName: string;
  readonly layoutElement: LayoutElement;
  readonly observer: ElementLayoutObserver;
  hasSize: boolean;
  height: number;
  preparedCommit: boolean;
  width: number;
}

interface LayoutElement {
  readonly id: number;
  readonly viewClass: string;
  element: HTMLElement;
  attached: boolean;
  observed: boolean;
  observers?: Record<string, LayoutObserverEntry | undefined>;
  observerCount: number;
  sizeObserverCount: number;
  measureObserver?: LayoutObserverEntry;
  onLayout?: OnLayoutCallback;
  lastFrame?: ElementFrame;
  forceOnLayout: boolean;
}

export function measureElementFrame(element: HTMLElement): ElementFrame {
  return measureElementFrameFromRect(element, element.getBoundingClientRect());
}

function measureElementFrameFromRect(element: HTMLElement, rect: DOMRect): ElementFrame {
  const offsetParent = element.offsetParent as HTMLElement | null;

  let x: number;
  let y: number;
  if (offsetParent) {
    const parentRect = offsetParent.getBoundingClientRect();
    const style = getComputedStyle(offsetParent);
    x = rect.left - parentRect.left + offsetParent.scrollLeft - (parseFloat(style.borderLeftWidth) || 0);
    y = rect.top - parentRect.top + offsetParent.scrollTop - (parseFloat(style.borderTopWidth) || 0);
  } else {
    x = rect.left;
    y = rect.top;
  }

  return { x, y, width: rect.width, height: rect.height };
}

function framesEqual(left: ElementFrame, right: ElementFrame): boolean {
  return left.x === right.x && left.y === right.y && left.width === right.width && left.height === right.height;
}

export class LayoutObserverController {
  private readonly elementsById = new Map<number, LayoutElement>();
  private readonly elementIdsByHtmlElement = new WeakMap<Element, number>();
  private readonly elementIdsWithMeasureObserver = new Set<number>();
  private readonly onBrowserResize = () => this.scheduleRefresh();
  private readonly flushScheduledRefresh = () => {
    if (!this.refreshScheduled) {
      return;
    }
    this.refreshScheduled = false;
    this.performUpdates();
  };
  private readonly flushPostLayoutCallbacks = () => {
    this.postLayoutFlushScheduled = false;
    if (this.destroyed) {
      return;
    }
    const callbacks = this.pendingPostLayoutCallbacks;
    if (!callbacks) {
      return;
    }
    this.pendingPostLayoutCallbacks = undefined;
    this.isFlushingPostLayoutCallbacks = true;
    try {
      for (let i = 0; i < callbacks.length; i++) {
        callbacks[i]();
      }
    } finally {
      this.isFlushingPostLayoutCallbacks = false;
    }
    this.schedulePostLayoutCallbacks();
  };
  private resizeObserver?: ResizeObserver;
  private pendingPostLayoutCallbacks?: Array<() => void>;
  private observersToCommit?: LayoutObserverEntry[];
  private postLayoutScheduler?: PostLayoutScheduler;
  private renderCompleteScheduler?: RenderCompleteScheduler;
  private postLayoutDeferralDepth = 0;
  private postLayoutFlushScheduled = false;
  private isFlushingPostLayoutCallbacks = false;
  private refreshScheduled = false;
  private destroyed = false;
  private listeningForBrowserResize = false;

  constructor(private readonly onLayoutPassCommitted: () => void) {
    if (typeof window !== 'undefined' && typeof window.addEventListener === 'function') {
      window.addEventListener('resize', this.onBrowserResize);
      this.listeningForBrowserResize = true;
    }
  }

  setRenderCompleteScheduler(schedule: RenderCompleteScheduler): void {
    this.renderCompleteScheduler = schedule;
  }

  setPostLayoutScheduler(scheduler: PostLayoutScheduler | undefined): void {
    this.postLayoutScheduler = scheduler;
    this.schedulePostLayoutCallbacks();
  }

  beginUpdate(): void {
    this.postLayoutDeferralDepth++;
  }

  endUpdate(): void {
    if (this.postLayoutDeferralDepth === 0) {
      throw new Error('Unbalanced LayoutObserverController.endUpdate()');
    }
    this.postLayoutDeferralDepth--;
    this.schedulePostLayoutCallbacks();
  }

  enqueuePostLayoutCallback(callback: () => void): void {
    (this.pendingPostLayoutCallbacks ??= []).push(callback);
    if (!this.isFlushingPostLayoutCallbacks) {
      this.schedulePostLayoutCallbacks();
    }
  }

  setLayoutObserver(
    id: number,
    viewClass: string,
    element: HTMLElement,
    attached: boolean,
    attributeName: string,
    observer: ElementLayoutObserver | undefined,
  ): void {
    const existingElement = this.elementsById.get(id);
    const existingEntry = existingElement?.observers?.[attributeName];
    if (!observer) {
      if (existingElement && existingEntry) {
        this.removeLayoutObserver(existingElement, existingEntry);
        this.removeElementIfEmpty(existingElement);
      }
      return;
    }
    if (observer.onMeasure && attributeName !== 'onMeasure') {
      throw new Error(`Only the 'onMeasure' attribute can define ElementLayoutObserver.onMeasure`);
    }

    const layoutElement = existingElement ?? this.createLayoutElement(id, viewClass, element, attached);
    if (layoutElement.element !== element) {
      this.replaceElement(layoutElement, element);
    }
    layoutElement.attached = attached;
    if (existingEntry) {
      this.removeLayoutObserver(layoutElement, existingEntry);
    }

    const entry: LayoutObserverEntry = {
      attributeName,
      layoutElement,
      observer,
      hasSize: false,
      height: 0,
      preparedCommit: false,
      width: 0,
    };
    const observers = layoutElement.observers ?? (layoutElement.observers = Object.create(null));
    observers[attributeName] = entry;
    layoutElement.observerCount++;
    if (observer.onSizeChanged) {
      layoutElement.sizeObserverCount++;
    }
    if (observer.onMeasure) {
      layoutElement.measureObserver = entry;
      this.elementIdsWithMeasureObserver.add(id);
    }
    this.updateResizeObservation(layoutElement);
    this.scheduleRefresh();
  }

  getLayoutObserver(id: number, attributeName: string): ElementLayoutObserver | undefined {
    return this.elementsById.get(id)?.observers?.[attributeName]?.observer;
  }

  setOnLayoutCallback(
    id: number,
    viewClass: string,
    element: HTMLElement,
    attached: boolean,
    callback: OnLayoutCallback | undefined,
  ): void {
    const existingElement = this.elementsById.get(id);
    if (!callback) {
      if (existingElement) {
        existingElement.onLayout = undefined;
        existingElement.lastFrame = undefined;
        existingElement.forceOnLayout = false;
        this.removeElementIfEmpty(existingElement);
      }
      return;
    }

    const layoutElement = existingElement ?? this.createLayoutElement(id, viewClass, element, attached);
    if (layoutElement.element !== element) {
      this.replaceElement(layoutElement, element);
    }
    layoutElement.attached = attached;
    layoutElement.onLayout = callback;
    layoutElement.lastFrame = undefined;
    layoutElement.forceOnLayout = true;
    this.updateResizeObservation(layoutElement);
    this.scheduleRefresh();
  }

  setElementAttached(id: number, attached: boolean): void {
    const layoutElement = this.elementsById.get(id);
    if (!layoutElement || layoutElement.attached === attached) {
      return;
    }
    layoutElement.attached = attached;
    this.updateResizeObservation(layoutElement);
    if (attached) {
      this.scheduleRefresh();
    }
  }

  destroyElement(id: number): void {
    const layoutElement = this.elementsById.get(id);
    if (!layoutElement) {
      return;
    }
    if (layoutElement.observed) {
      this.resizeObserver?.unobserve(layoutElement.element);
    }
    this.elementIdsWithMeasureObserver.delete(id);
    this.elementsById.delete(id);
  }

  scheduleRefresh(): void {
    if (this.destroyed || this.refreshScheduled || this.elementsById.size === 0) {
      return;
    }
    this.refreshScheduled = true;
    this.enqueuePostLayoutCallback(this.flushScheduledRefresh);
  }

  drainScheduledRefresh(): void {
    if (!this.refreshScheduled || this.destroyed) {
      return;
    }
    this.refreshScheduled = false;
    this.performUpdates();
  }

  destroy(): void {
    if (this.destroyed) {
      return;
    }
    this.destroyed = true;
    if (this.listeningForBrowserResize) {
      window.removeEventListener('resize', this.onBrowserResize);
      this.listeningForBrowserResize = false;
    }
    this.resizeObserver?.disconnect();
    this.resizeObserver = undefined;
    this.elementsById.clear();
    this.elementIdsWithMeasureObserver.clear();
    this.pendingPostLayoutCallbacks = undefined;
    this.observersToCommit = undefined;
    this.postLayoutFlushScheduled = false;
    this.isFlushingPostLayoutCallbacks = false;
    this.refreshScheduled = false;
  }

  private createLayoutElement(id: number, viewClass: string, element: HTMLElement, attached: boolean): LayoutElement {
    const layoutElement: LayoutElement = {
      id,
      viewClass,
      element,
      attached,
      observed: false,
      observerCount: 0,
      sizeObserverCount: 0,
      forceOnLayout: false,
    };
    this.elementsById.set(id, layoutElement);
    this.elementIdsByHtmlElement.set(element, id);
    return layoutElement;
  }

  private replaceElement(layoutElement: LayoutElement, element: HTMLElement): void {
    if (layoutElement.observed) {
      this.resizeObserver?.unobserve(layoutElement.element);
      layoutElement.observed = false;
    }
    layoutElement.element = element;
    this.elementIdsByHtmlElement.set(element, layoutElement.id);
  }

  private removeLayoutObserver(layoutElement: LayoutElement, entry: LayoutObserverEntry): void {
    delete layoutElement.observers![entry.attributeName];
    layoutElement.observerCount--;
    if (entry.observer.onSizeChanged) {
      layoutElement.sizeObserverCount--;
    }
    if (layoutElement.measureObserver === entry) {
      layoutElement.measureObserver = undefined;
      this.elementIdsWithMeasureObserver.delete(layoutElement.id);
    }
  }

  private removeElementIfEmpty(layoutElement: LayoutElement): void {
    if (layoutElement.observerCount !== 0 || layoutElement.onLayout) {
      this.updateResizeObservation(layoutElement);
      return;
    }
    if (layoutElement.observed) {
      this.resizeObserver?.unobserve(layoutElement.element);
    }
    this.elementsById.delete(layoutElement.id);
  }

  private updateResizeObservation(layoutElement: LayoutElement): void {
    const shouldObserve = layoutElement.attached && (layoutElement.observerCount !== 0 || !!layoutElement.onLayout);
    if (layoutElement.observed === shouldObserve) {
      return;
    }
    if (shouldObserve) {
      this.ensureResizeObserver();
      this.resizeObserver?.observe(layoutElement.element);
    } else {
      this.resizeObserver?.unobserve(layoutElement.element);
    }
    layoutElement.observed = shouldObserve;
  }

  private ensureResizeObserver(): void {
    if (this.resizeObserver || typeof ResizeObserver === 'undefined') {
      return;
    }
    this.resizeObserver = new ResizeObserver(entries => {
      for (let i = 0; i < entries.length; i++) {
        const entry = entries[i];
        const elementId = this.elementIdsByHtmlElement.get(entry.target);
        const layoutElement = elementId === undefined ? undefined : this.elementsById.get(elementId);
        if (layoutElement?.element === entry.target && layoutElement.attached) {
          this.scheduleRefresh();
          return;
        }
      }
    });
  }

  private schedulePostLayoutCallbacks(): void {
    if (
      !this.pendingPostLayoutCallbacks ||
      this.postLayoutFlushScheduled ||
      this.postLayoutDeferralDepth !== 0 ||
      this.destroyed
    ) {
      return;
    }
    this.postLayoutFlushScheduled = true;
    if (this.postLayoutScheduler) {
      this.postLayoutScheduler(this.flushPostLayoutCallbacks);
    } else {
      Promise.resolve().then(this.flushPostLayoutCallbacks);
    }
  }

  private performUpdates(): void {
    this.observersToCommit = undefined;

    // 1. Run the special onMeasure hooks so their DOM reads and pending commits are prepared first.
    this.measureOnMeasureObservers();

    // 2. Measure every attached element once, notify changed size observers, and collect pending work.
    const elementsToNotify = this.measureElementLayouts();

    // 3. Apply observer DOM writes only after every measurement in this pass has completed.
    this.commitLayoutObservers();

    // 4. Deliver public onLayout callbacks after internal observer commits have finished.
    this.notifyOnLayoutCallbacks(elementsToNotify);

    // 5. Refresh downstream layout consumers, such as visibility observation, using committed geometry.
    this.onLayoutPassCommitted();
  }

  private measureOnMeasureObservers(): void {
    for (const elementId of this.elementIdsWithMeasureObserver) {
      const entry = this.elementsById.get(elementId)?.measureObserver;
      if (!entry || !entry.layoutElement.attached) {
        continue;
      }
      entry.preparedCommit = false;
      try {
        entry.observer.onMeasure!(entry.layoutElement.element);
        entry.preparedCommit = !!entry.observer.onCommit;
      } catch (error) {
        this.logObserverError('measure', entry, error);
      }
    }
  }

  private measureElementLayouts(): LayoutElement[] | undefined {
    let elementsToNotify: LayoutElement[] | undefined;
    for (const layoutElement of this.elementsById.values()) {
      if (!layoutElement.attached) {
        continue;
      }
      const shouldMeasureSize = layoutElement.sizeObserverCount !== 0;
      const shouldMeasureFrame = !!layoutElement.onLayout;
      const rect = shouldMeasureSize || shouldMeasureFrame ? layoutElement.element.getBoundingClientRect() : undefined;
      this.measureElementLayoutObservers(layoutElement, rect);
      if (rect && shouldMeasureFrame) {
        const frame = measureElementFrameFromRect(layoutElement.element, rect);
        const frameChanged = !layoutElement.lastFrame || !framesEqual(layoutElement.lastFrame, frame);
        if (layoutElement.forceOnLayout || frameChanged) {
          layoutElement.forceOnLayout = false;
          layoutElement.lastFrame = frame;
          (elementsToNotify ??= []).push(layoutElement);
        }
      }
    }
    return elementsToNotify;
  }

  private measureElementLayoutObservers(layoutElement: LayoutElement, rect: DOMRect | undefined): void {
    const observers = layoutElement.observers;
    if (!observers) {
      return;
    }
    for (const attributeName in observers) {
      const entry = observers[attributeName];
      if (!entry) {
        continue;
      }
      let shouldCommit = entry.preparedCommit;
      entry.preparedCommit = false;
      if (
        rect &&
        entry.observer.onSizeChanged &&
        (!entry.hasSize || entry.width !== rect.width || entry.height !== rect.height)
      ) {
        try {
          entry.observer.onSizeChanged(rect.width, rect.height);
          entry.width = rect.width;
          entry.height = rect.height;
          entry.hasSize = true;
          shouldCommit = shouldCommit || !!entry.observer.onCommit;
        } catch (error) {
          this.logObserverError('notify size change', entry, error);
        }
      }
      if (shouldCommit) {
        (this.observersToCommit ??= []).push(entry);
      }
    }
  }

  private commitLayoutObservers(): void {
    const observersToCommit = this.observersToCommit;
    this.observersToCommit = undefined;
    if (!observersToCommit) {
      return;
    }
    for (let i = 0; i < observersToCommit.length; i++) {
      const entry = observersToCommit[i];
      if (entry.layoutElement.observers?.[entry.attributeName] !== entry) {
        continue;
      }
      try {
        entry.observer.onCommit!(entry.layoutElement.element);
      } catch (error) {
        this.logObserverError('commit', entry, error);
      }
    }
  }

  private notifyOnLayoutCallbacks(elementsToNotify: LayoutElement[] | undefined): void {
    if (!elementsToNotify) {
      return;
    }
    const notify = () => {
      for (let i = 0; i < elementsToNotify.length; i++) {
        const layoutElement = elementsToNotify[i];
        const callback = layoutElement.onLayout;
        const frame = layoutElement.lastFrame;
        if (!callback || !frame) {
          continue;
        }
        try {
          callback(frame);
        } catch (error) {
          const message = error instanceof Error ? error.message : String(error);
          console.error(`Valdi web renderer failed to call 'onLayout' on node ${layoutElement.id}: ${message}`);
        }
      }
    };
    if (this.renderCompleteScheduler) {
      this.renderCompleteScheduler(notify);
    } else {
      notify();
    }
  }

  private logObserverError(phase: string, entry: LayoutObserverEntry, error: unknown): void {
    const message = error instanceof Error ? error.message : String(error);
    console.error(
      `Valdi web renderer failed to ${phase} layout observer '${entry.attributeName}' on node ${entry.layoutElement.id} (${entry.layoutElement.viewClass}): ${message}`,
    );
  }
}
