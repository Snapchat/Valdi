import 'jasmine/src/jasmine';
import { ElementLayoutDispatcher } from '../src/ElementLayoutDispatcher';

function frameUpdate(id: number, x: number, y: number, width: number, height: number): Float64Array {
  return new Float64Array([id, x, y, width, height]);
}

describe('ElementLayoutDispatcher', () => {
  let errors: string[];
  let pendingRenderComplete: (() => void)[];
  let dispatcher: ElementLayoutDispatcher;

  function flushRenderComplete() {
    const callbacks = pendingRenderComplete;
    pendingRenderComplete = [];
    callbacks.forEach(callback => callback());
  }

  beforeEach(() => {
    errors = [];
    pendingRenderComplete = [];
    dispatcher = new ElementLayoutDispatcher(message => errors.push(message));
    dispatcher.setRenderCompleteScheduler(callback => pendingRenderComplete.push(callback));
  });

  it('only consumes layout callback attributes', () => {
    dispatcher.onElementCreated(1);

    expect(dispatcher.handleAttribute(1, 'onLayout', () => {})).toBeTrue();
    expect(dispatcher.handleAttribute(1, '$onLayout', () => {})).toBeTrue();
    expect(dispatcher.handleAttribute(1, 'onTap', () => {})).toBeFalse();
    expect(dispatcher.handleAttribute(1, 'width', 10)).toBeFalse();
  });

  it('caches frames and calls onLayout then $onLayout after render completes', () => {
    const calls: string[] = [];
    dispatcher.onElementCreated(1);
    dispatcher.handleAttribute(1, '$onLayout', () => calls.push('injected'));
    dispatcher.handleAttribute(1, 'onLayout', () => calls.push('onLayout'));

    dispatcher.processFrameUpdates(frameUpdate(1, 1, 2, 3, 4));

    expect(dispatcher.getFrame(1)).toEqual({ x: 1, y: 2, width: 3, height: 4 });
    expect(calls).toEqual([]);
    flushRenderComplete();
    expect(calls).toEqual(['onLayout', 'injected']);
  });

  it('replays the last frame to an onLayout set after layout', () => {
    const frames: unknown[] = [];
    dispatcher.onElementCreated(1);
    dispatcher.processFrameUpdates(frameUpdate(1, 0, 0, 10, 20));

    dispatcher.handleAttribute(1, 'onLayout', (frame: unknown) => frames.push(frame));
    flushRenderComplete();

    expect(frames).toEqual([{ x: 0, y: 0, width: 10, height: 20 }]);
  });

  it('reports a throwing replayed onLayout without stopping later completions', () => {
    let laterCompletionCalled = false;
    dispatcher.onElementCreated(1);
    dispatcher.processFrameUpdates(frameUpdate(1, 0, 0, 1, 1));

    dispatcher.handleAttribute(1, 'onLayout', () => {
      throw new Error('boom');
    });
    pendingRenderComplete.push(() => {
      laterCompletionCalled = true;
    });
    flushRenderComplete();

    expect(errors).toEqual(['Failed to call onLayout']);
    expect(laterCompletionCalled).toBeTrue();
  });

  it('does not replay when the element was never laid out', () => {
    dispatcher.onElementCreated(1);
    dispatcher.handleAttribute(1, 'onLayout', () => {});

    expect(pendingRenderComplete.length).toBe(0);
  });

  it('stops calling a removed onLayout', () => {
    let calls = 0;
    dispatcher.onElementCreated(1);
    dispatcher.handleAttribute(1, 'onLayout', () => calls++);
    dispatcher.handleAttribute(1, 'onLayout', undefined);

    dispatcher.processFrameUpdates(frameUpdate(1, 0, 0, 1, 1));
    flushRenderComplete();

    expect(calls).toBe(0);
    expect(dispatcher.getFrame(1)).toEqual({ x: 0, y: 0, width: 1, height: 1 });
  });

  it('ignores frames for elements that are unknown or destroyed', () => {
    dispatcher.onElementCreated(1);
    dispatcher.onElementDestroyed(1);

    dispatcher.processFrameUpdates(frameUpdate(1, 0, 0, 1, 1));
    dispatcher.processFrameUpdates(frameUpdate(2, 0, 0, 1, 1));

    expect(dispatcher.getFrame(1)).toBeUndefined();
    expect(dispatcher.getFrame(2)).toBeUndefined();
  });

  it('reports onLayout errors and keeps calling the remaining callbacks', () => {
    let secondCalled = false;
    dispatcher.onElementCreated(1);
    dispatcher.onElementCreated(2);
    dispatcher.handleAttribute(1, 'onLayout', () => {
      throw new Error('boom');
    });
    dispatcher.handleAttribute(2, 'onLayout', () => {
      secondCalled = true;
    });

    dispatcher.processFrameUpdates(new Float64Array([1, 0, 0, 1, 1, 2, 0, 0, 1, 1]));
    flushRenderComplete();

    expect(errors).toEqual(['Failed to call onLayout']);
    expect(secondCalled).toBeTrue();
  });

  it('calls callbacks immediately with the default scheduler', () => {
    const immediate = new ElementLayoutDispatcher(() => {});
    let calls = 0;
    immediate.onElementCreated(1);
    immediate.handleAttribute(1, 'onLayout', () => calls++);

    immediate.processFrameUpdates(frameUpdate(1, 0, 0, 1, 1));

    expect(calls).toBe(1);
  });
});
