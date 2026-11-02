import { setTimeoutInterruptible } from '../SetTimeout';
import { getValdiRuntime } from '../ValdiRuntimeProvider';
import { makeInterruptibleCallback } from './FunctionUtils';

/**
 * Call the given function when the main thread and the js thread
 * have both finished processing any tasks they may have enqueued.
 * Note: will not be called if the component's context has been destroyed
 * @param cb
 */
export function onIdleInterruptible(cb: () => void) {
  setTimeoutInterruptible(() => {
    getValdiRuntime().onMainThreadIdle(makeInterruptibleCallback(cb));
  });
}
