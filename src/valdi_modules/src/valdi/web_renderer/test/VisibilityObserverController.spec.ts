import 'jasmine/src/jasmine';
import { VisibilityObserverController } from '../src/VisibilityObserverController';
import { FakeIntersectionObserver, installObserverTestGlobals, makeElement } from './ObserverTestUtils';

describe('VisibilityObserverController', () => {
  let uninstallGlobals: () => void;

  beforeEach(() => {
    uninstallGlobals = installObserverTestGlobals();
  });

  afterEach(() => {
    uninstallGlobals();
  });

  it('reports appearing and viewport updates for observed elements', () => {
    const root = makeElement({ left: 0, top: 0, width: 100, height: 100, right: 100, bottom: 100 });
    const element = makeElement({ left: 10, top: 20, width: 50, height: 40, right: 60, bottom: 60 });
    const events: Array<{
      appearing: number[];
      disappearing: number[];
      viewportUpdates: number[];
      eventTime: number;
    }> = [];
    const controller = new VisibilityObserverController();

    controller.setRoot(root);
    controller.registerObserver((appearing, disappearing, viewportUpdates, eventTime) => {
      events.push({ appearing, disappearing, viewportUpdates, eventTime });
    });
    controller.observeElement(7, element);
    FakeIntersectionObserver.lastInstance!.trigger(element);

    expect(events).toEqual([
      {
        appearing: [7],
        disappearing: [],
        viewportUpdates: [7, 0, 0, 50, 40],
        eventTime: 1234,
      },
    ]);
  });

  it('reports viewport clipping changes and disappearing elements', () => {
    const root = makeElement({ left: 0, top: 0, width: 100, height: 100, right: 100, bottom: 100 });
    const element = makeElement({ left: 10, top: 20, width: 50, height: 40, right: 60, bottom: 60 });
    const events: Array<{ appearing: number[]; disappearing: number[]; viewportUpdates: number[] }> = [];
    const controller = new VisibilityObserverController();

    controller.setRoot(root);
    controller.registerObserver((appearing, disappearing, viewportUpdates) => {
      events.push({ appearing, disappearing, viewportUpdates });
    });
    controller.observeElement(9, element);
    const intersectionObserver = FakeIntersectionObserver.lastInstance!;
    intersectionObserver.trigger(element);

    element.rect = { left: -10, top: 20, width: 50, height: 40, right: 40, bottom: 60 };
    intersectionObserver.trigger(element);

    element.rect = { left: 120, top: 20, width: 50, height: 40, right: 170, bottom: 60 };
    intersectionObserver.trigger(element);

    expect(events).toEqual([
      { appearing: [9], disappearing: [], viewportUpdates: [9, 0, 0, 50, 40] },
      { appearing: [], disappearing: [], viewportUpdates: [9, 10, 0, 40, 40] },
      { appearing: [], disappearing: [9], viewportUpdates: [] },
    ]);
  });

  it('uses one intersection observer for all elements and cleans up observations', () => {
    const root = makeElement({ left: 0, top: 0, width: 100, height: 100, right: 100, bottom: 100 });
    const first = makeElement({ left: 10, top: 20, width: 20, height: 20, right: 30, bottom: 40 });
    const second = makeElement({ left: 40, top: 50, width: 20, height: 20, right: 60, bottom: 70 });
    const firstGetBoundingClientRect = first.getBoundingClientRect.bind(first);
    let firstRectReadCount = 0;
    first.getBoundingClientRect = () => {
      firstRectReadCount++;
      return firstGetBoundingClientRect() as DOMRect;
    };
    const events: Array<{ appearing: number[]; viewportUpdates: number[] }> = [];
    const controller = new VisibilityObserverController();

    controller.setRoot(root);
    controller.registerObserver((appearing, _disappearing, viewportUpdates) => {
      events.push({ appearing, viewportUpdates });
    });
    controller.observeElement(1, first);
    controller.observeElement(2, second);

    expect(FakeIntersectionObserver.instances.length).toBe(1);
    expect(FakeIntersectionObserver.lastInstance!.root).toBe(root);
    expect(FakeIntersectionObserver.lastInstance!.thresholds).toEqual([0, 1]);
    expect(FakeIntersectionObserver.lastInstance!.observedElements).toEqual([first, second]);

    FakeIntersectionObserver.lastInstance!.trigger(first, second);
    expect(events).toEqual([
      {
        appearing: [1, 2],
        viewportUpdates: [1, 0, 0, 20, 20, 2, 0, 0, 20, 20],
      },
    ]);
    expect(firstRectReadCount).toBe(1);

    controller.unobserveElement(1);
    expect(FakeIntersectionObserver.lastInstance!.observedElements).toEqual([second]);

    controller.destroy();
    expect(FakeIntersectionObserver.lastInstance!.observedElements).toEqual([]);
  });
});
