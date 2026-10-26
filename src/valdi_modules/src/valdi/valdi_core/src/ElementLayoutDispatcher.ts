import { ElementFrame } from 'valdi_tsx/src/Geometry';
import { trace } from './utils/Trace';

type OnLayoutCallback = (frame: ElementFrame) => void;

interface ElementLayoutState {
  frame?: ElementFrame;
  onLayout?: OnLayoutCallback;
  injectedOnLayout?: OnLayoutCallback;
}

/**
 * Caches the frames a platform reports for rendered elements and calls their
 * `onLayout`/`$onLayout` callbacks. Shared by `IRendererDelegate`
 * implementations so every platform delivers layout callbacks the same way.
 */
export class ElementLayoutDispatcher {
  private stateById: { [id: number]: ElementLayoutState | undefined } = {};
  private scheduleRenderComplete: (callback: () => void) => void = callback => callback();

  constructor(private readonly onUncaughtError: (message: string, error: Error) => void) {}

  setRenderCompleteScheduler(schedule: (callback: () => void) => void): void {
    this.scheduleRenderComplete = schedule;
  }

  onElementCreated(id: number): void {
    this.stateById[id] = {};
  }

  onElementDestroyed(id: number): void {
    delete this.stateById[id];
  }

  clear(): void {
    this.stateById = {};
  }

  getFrame(id: number): ElementFrame | undefined {
    return this.stateById[id]?.frame;
  }

  /**
   * Records `onLayout`/`$onLayout` changes. Returns false for any other
   * attribute, which the caller should forward to the platform.
   */
  handleAttribute(id: number, attributeName: string, value: unknown): boolean {
    const isOnLayout = attributeName === 'onLayout';
    if (!isOnLayout && attributeName !== '$onLayout') {
      return false;
    }

    const state = this.stateById[id];
    if (!state) {
      return true;
    }

    const callback = typeof value === 'function' ? (value as OnLayoutCallback) : undefined;
    if (isOnLayout) {
      state.onLayout = callback;
    } else {
      state.injectedOnLayout = callback;
    }

    // A callback set after the element was laid out still receives the last known frame.
    if (callback && state.frame) {
      this.scheduleRenderComplete(() => {
        this.callOnLayout(state, callback);
      });
    }

    return true;
  }

  /**
   * Applies frame updates encoded as `[id, x, y, width, height, ...]`.
   */
  processFrameUpdates(updates: Float64Array): void {
    let elementsWithCallback: ElementLayoutState[] | undefined;

    trace('processFrameUpdates', () => {
      let i = 0;
      while (i < updates.length) {
        const elementId = updates[i++];
        const x = updates[i++];
        const y = updates[i++];
        const width = updates[i++];
        const height = updates[i++];

        const state = this.stateById[elementId];
        if (state) {
          state.frame = { x, y, width, height };
          if (state.onLayout || state.injectedOnLayout) {
            (elementsWithCallback ??= []).push(state);
          }
        }
      }
    });

    if (elementsWithCallback) {
      const states = elementsWithCallback;
      this.scheduleRenderComplete(() => {
        for (const state of states) {
          this.callOnLayout(state, state.onLayout);
          this.callOnLayout(state, state.injectedOnLayout);
        }
      });
    }
  }

  private callOnLayout(state: ElementLayoutState, callback: OnLayoutCallback | undefined): void {
    if (!callback || !state.frame) {
      return;
    }
    try {
      callback(state.frame);
    } catch (err: any) {
      this.onUncaughtError('Failed to call onLayout', err);
    }
  }
}
