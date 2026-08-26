import { getValdiRuntime } from './ValdiRuntimeProvider';

const runtime = getValdiRuntime();

/**
 * setTimeout implementation that guarantees that the given callback
 * will be evaluated, even if the current context was destroyed.
 * @param handler - function to evaluate after `timeout` passes.
 * @param timeout - time interval in milliseconds only after which the function should be evaluated.
 * @returns
 */
export function setTimeoutUninterruptible(handler: () => void, timeout?: number): number {
  const disposable = runtime.protectNativeRefs(0);
  return setTimeout(() => {
    try {
      handler();
    } finally {
      disposable();
    }
  }, timeout);
}

/**
 * setTimeout implementation that will no-op when evaluating the given
 * callback if the context was destroyed.
 * @param handler - function to evaluate after `timeout` passes.
 * @param timeout - time interval in milliseconds only after which the function should be evaluated.
 * @returns An identifier that must be passed into `clearTimeout` to cancel the timer.
 */
export function setTimeoutInterruptible(handler: () => void, timeout?: number): number {
  return runtime.scheduleWorkItem(handler, timeout, true);
}

/**
 * Wrapper around our custom setTimeoutInterruptible/setTimeoutUninterruptible calls, accepting a boolean
 * parameter that controls whether the callback should be ignored when the current context is destroyed.
 * @param interruptible - if true, the handler will not be called after the valdi context is destroyed
 * @param handler - function to evaluate after `timeout` passes.
 * @param timeout - time interval in milliseconds only after which the function should be evaluated.
 * @returns An identifier that can be passed into `clearTimeout` to cancel the timer.
 */
export function setTimeoutConfigurable(interruptible: boolean, handler: () => void, timeout?: number): number {
  if (interruptible) {
    return setTimeoutInterruptible(handler, timeout);
  } else {
    return setTimeoutUninterruptible(handler, timeout);
  }
}
