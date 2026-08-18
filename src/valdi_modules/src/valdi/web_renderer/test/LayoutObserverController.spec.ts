import 'jasmine/src/jasmine';
import { ElementFrame } from 'valdi_tsx/src/Geometry';
import { LayoutObserverController, measureElementFrame } from '../src/LayoutObserverController';
import { FakeResizeObserver, installObserverTestGlobals, makeElement } from './ObserverTestUtils';

interface FakeWindow {
  addEventListener(name: string, listener: () => void): void;
  removeEventListener(name: string, listener: () => void): void;
  dispatchResize(): void;
  listenerCount(name: string): number;
}

function createFakeWindow(): FakeWindow {
  const listeners = new Map<string, Array<() => void>>();
  return {
    addEventListener(name, listener) {
      const callbacks = listeners.get(name) ?? [];
      callbacks.push(listener);
      listeners.set(name, callbacks);
    },
    removeEventListener(name, listener) {
      const callbacks = listeners.get(name);
      const index = callbacks?.indexOf(listener) ?? -1;
      if (callbacks && index >= 0) {
        callbacks.splice(index, 1);
      }
    },
    dispatchResize() {
      const callbacks = listeners.get('resize');
      if (callbacks) {
        for (let i = 0; i < callbacks.length; i++) {
          callbacks[i]();
        }
      }
    },
    listenerCount(name) {
      return listeners.get(name)?.length ?? 0;
    },
  };
}

describe('LayoutObserverController', () => {
  let previousWindow: unknown;
  let uninstallGlobals: () => void;
  let fakeWindow: FakeWindow;
  let scheduledPasses: Array<() => void>;
  let controller: LayoutObserverController;

  beforeEach(() => {
    uninstallGlobals = installObserverTestGlobals();
    previousWindow = (globalThis as { window?: unknown }).window;
    fakeWindow = createFakeWindow();
    (globalThis as { window?: unknown }).window = fakeWindow;
    scheduledPasses = [];
    controller = new LayoutObserverController(() => {});
    controller.setPostLayoutScheduler(callback => scheduledPasses.push(callback));
  });

  afterEach(() => {
    controller.destroy();
    if (previousWindow === undefined) {
      delete (globalThis as { window?: unknown }).window;
    } else {
      (globalThis as { window?: unknown }).window = previousWindow;
    }
    uninstallGlobals();
  });

  function flushPass(): void {
    scheduledPasses.shift()!();
  }

  it('measures element frames on demand without observing layout', () => {
    const parent = makeElement({ left: 100, top: 50, width: 200, height: 200 });
    parent.scrollLeft = 7;
    parent.scrollTop = 9;
    parent.borderLeftWidth = '2px';
    parent.borderTopWidth = '3px';
    const element = makeElement({ left: 140, top: 90, width: 30, height: 20 });
    element.offsetParent = parent;

    expect(FakeResizeObserver.lastInstance).toBeUndefined();
    expect(measureElementFrame(element)).toEqual({ x: 45, y: 46, width: 30, height: 20 });
  });

  it('runs all measurement work before commits and onLayout notifications', () => {
    const order: string[] = [];
    const first = makeElement({ left: 1, top: 2, width: 30, height: 20 });
    const second = makeElement({ left: 3, top: 4, width: 40, height: 25 });
    controller.setLayoutObserver(1, 'view', first, true, 'borderRadius', {
      onSizeChanged() {
        order.push('size');
      },
      onCommit() {
        order.push('size commit');
      },
    });
    controller.setLayoutObserver(2, 'view', second, true, 'onMeasure', {
      onMeasure() {
        order.push('measure');
      },
      onCommit() {
        order.push('measure commit');
      },
    });
    controller.setOnLayoutCallback(1, 'view', first, true, () => order.push('onLayout'));

    expect(scheduledPasses.length).toBe(1);
    flushPass();

    expect(order).toEqual(['measure', 'size', 'size commit', 'measure commit', 'onLayout']);
  });

  it('owns and defers post-layout callbacks during updates', () => {
    const order: string[] = [];
    controller.beginUpdate();
    controller.enqueuePostLayoutCallback(() => order.push('first'));
    controller.enqueuePostLayoutCallback(() => order.push('second'));

    expect(scheduledPasses).toEqual([]);
    controller.endUpdate();
    expect(scheduledPasses.length).toBe(1);

    flushPass();
    expect(order).toEqual(['first', 'second']);
  });

  it('measures each element once and suppresses unchanged size callbacks', () => {
    const element = makeElement({ left: 1, top: 2, width: 30, height: 20 });
    const originalGetBoundingClientRect = element.getBoundingClientRect.bind(element);
    let rectReadCount = 0;
    element.getBoundingClientRect = () => {
      rectReadCount++;
      return originalGetBoundingClientRect() as DOMRect;
    };
    const first = jasmine.createSpy('first');
    const second = jasmine.createSpy('second');
    controller.setLayoutObserver(1, 'view', element, true, 'first', { onSizeChanged: first });
    controller.setLayoutObserver(1, 'view', element, true, 'second', { onSizeChanged: second });

    flushPass();
    expect(rectReadCount).toBe(1);
    expect(first).toHaveBeenCalledOnceWith(30, 20);
    expect(second).toHaveBeenCalledOnceWith(30, 20);

    controller.scheduleRefresh();
    flushPass();
    expect(rectReadCount).toBe(2);
    expect(first).toHaveBeenCalledTimes(1);
    expect(second).toHaveBeenCalledTimes(1);
  });

  it('runs onMeasure on every pass even when the element size is unchanged', () => {
    const element = makeElement({ left: 1, top: 2, width: 30, height: 20 });
    const onMeasure = jasmine.createSpy('onMeasure');
    const onCommit = jasmine.createSpy('onCommit');
    controller.setLayoutObserver(1, 'view', element, true, 'onMeasure', { onMeasure, onCommit });

    flushPass();
    controller.scheduleRefresh();
    flushPass();

    expect(onMeasure).toHaveBeenCalledTimes(2);
    expect(onCommit).toHaveBeenCalledTimes(2);
  });

  it('replaces, resets, and destroys attribute-scoped observers', () => {
    const element = makeElement({ left: 1, top: 2, width: 30, height: 20 });
    const replaced = jasmine.createSpy('replaced');
    const replacement = jasmine.createSpy('replacement');
    controller.setLayoutObserver(1, 'view', element, true, 'radius', { onSizeChanged: replaced });
    controller.setLayoutObserver(1, 'view', element, true, 'radius', { onSizeChanged: replacement });
    flushPass();
    expect(replaced).not.toHaveBeenCalled();
    expect(replacement).toHaveBeenCalledTimes(1);

    controller.setLayoutObserver(1, 'view', element, true, 'radius', undefined);
    expect(FakeResizeObserver.lastInstance!.observedElements).toEqual([]);
    controller.setLayoutObserver(1, 'view', element, true, 'radius', { onSizeChanged: replacement });
    controller.destroyElement(1);
    expect(FakeResizeObserver.lastInstance!.observedElements).toEqual([]);
  });

  it('uses one resize observer and one browser resize listener', () => {
    const first = makeElement({ left: 1, top: 2, width: 30, height: 20 });
    const second = makeElement({ left: 3, top: 4, width: 40, height: 25 });
    controller.setLayoutObserver(1, 'view', first, true, 'first', { onSizeChanged() {} });
    const resizeObserver = FakeResizeObserver.lastInstance;
    controller.setOnLayoutCallback(2, 'view', second, true, (_frame: ElementFrame) => {});

    expect(FakeResizeObserver.lastInstance).toBe(resizeObserver);
    expect(resizeObserver!.observedElements).toEqual([first, second]);
    expect(fakeWindow.listenerCount('resize')).toBe(1);
    flushPass();

    fakeWindow.dispatchResize();
    fakeWindow.dispatchResize();
    expect(scheduledPasses.length).toBe(1);

    controller.destroy();
    expect(fakeWindow.listenerCount('resize')).toBe(0);
  });

  it('isolates measurement and commit failures from other observers', () => {
    const errorSpy = spyOn(console, 'error');
    const first = makeElement({ left: 1, top: 2, width: 30, height: 20 });
    const second = makeElement({ left: 3, top: 4, width: 40, height: 25 });
    const third = makeElement({ left: 5, top: 6, width: 50, height: 35 });
    const successfulCommit = jasmine.createSpy('successfulCommit');
    controller.setLayoutObserver(1, 'view', first, true, 'first', {
      onSizeChanged() {
        throw new Error('measurement failed');
      },
    });
    controller.setLayoutObserver(2, 'view', second, true, 'second', {
      onSizeChanged() {},
      onCommit() {
        throw new Error('commit failed');
      },
    });
    controller.setLayoutObserver(3, 'view', third, true, 'third', {
      onSizeChanged() {},
      onCommit: successfulCommit,
    });

    flushPass();

    expect(successfulCommit).toHaveBeenCalledTimes(1);
    expect(errorSpy).toHaveBeenCalledTimes(2);
  });

  it('rejects onMeasure on attributes other than onMeasure', () => {
    const element = makeElement({ left: 1, top: 2, width: 30, height: 20 });
    expect(() =>
      controller.setLayoutObserver(1, 'view', element, true, 'width', {
        onMeasure() {},
      }),
    ).toThrowError("Only the 'onMeasure' attribute can define ElementLayoutObserver.onMeasure");
  });
});
