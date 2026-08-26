import { getValdiRuntime } from '../ValdiRuntimeProvider';
import { AnyFunction } from './Callback';

const runtime = getValdiRuntime();

export const enum CallbackOptions {
  /**
   * Mark that the function has no specific export behavior.
   */
  None = 0,
  /**
   * Mark that when the exported function is called on the main thread, it should
   * lock the JS thread until completion
   */
  SyncWithMainThread = 1 << 0,
  /**
   * Mark that when the exported function is called after its associated Valdi
   * Context is destroyed, its call should be ignored instead of emitting an error.
   */
  Interruptible = 1 << 1,
  /**
   * Mark that when the exported is called once, its underlying function reference
   * should be torn down, which will prevent the function to be called again, as well
   * as help clearing out memory earlier.
   */
  SingleCall = 1 << 2,
}

/**
 * Returns a new configured callback with the given options. The options will impact
 * how the callback will be exported when it is passed to native or platform code.
 * @param options The list of options to set on the callback provided as an union.
 * @param fn The callback to configure
 * @returns A new callback configured with the given options.
 */
export function makeConfiguredCallback<F extends AnyFunction>(options: CallbackOptions, fn: F): F {
  if (options === CallbackOptions.None) {
    return fn;
  }

  const cb = ((...args: unknown[]): unknown => {
    return fn(...args);
  }) as F;
  runtime.configureCallback(options, cb);
  return cb;
}

/**
 * Mark a function as required to run on the main thread meaning that:
 * - it will pause all other javascript and runtime logic until finished
 */
export function makeMainThreadCallback<F extends AnyFunction>(fn: F): F {
  return makeConfiguredCallback(CallbackOptions.SyncWithMainThread, fn);
}

/**
 * Mark a function as interruptible meaning that:
 * - if the valdi context is destroyed
 * - it will not be called and won't trigger an error
 */
export function makeInterruptibleCallback<F extends AnyFunction>(fn: F): F {
  return makeConfiguredCallback(CallbackOptions.Interruptible, fn);
}

/**
 * Mark a function that can be called only once, and which will not emit an error
 * if it called after the associated Valdi Context was destroyed.
 */
export function makeSingleCallInterruptibleCallback<F extends AnyFunction>(fn: F): F {
  return makeConfiguredCallback(CallbackOptions.SingleCall | CallbackOptions.Interruptible, fn);
}

/**
 * Helper function to make a call optionally interruptible
 * @see makeInterruptibleCallback
 */
export function makeInterruptibleCallbackOptionally<F extends AnyFunction>(interruptible: boolean, fn: F): F {
  if (interruptible) {
    return makeInterruptibleCallback(fn);
  } else {
    return fn;
  }
}

/**
 * Mark a function that it can be called only once.
 * When the returned function is passed to native or platform code,
 * and later called by native or platform code, the reference of the
 * given TypeScript function will be torn down after the call. This can
 * help clearing out memory earlier.
 */
export function makeSingleCallCallback<F extends AnyFunction>(fn: F): F {
  return makeConfiguredCallback(CallbackOptions.SingleCall, fn);
}
