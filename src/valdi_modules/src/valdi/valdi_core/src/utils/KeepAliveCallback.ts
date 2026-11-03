import { AnyFunction } from './Callback';

export type KeepAlive = number;
export function beginKeepAlive(): KeepAlive {
  // Janky workaround, would require a runtime change to make this work better.
  return setInterval(() => {}, 100000);
}

export function endKeepAlive(keepAlive: KeepAlive) {
  clearInterval(keepAlive);
}

/**
 * Make a callback that will keep the runtime alive until the callback is called.
 * This can be used in particular for CLI apps which perform asynchronous work.
 * The return value of makeKeepAliveCallback() can be passed to the async services
 * so that the main JS task queue doesn't get empty, which would make the CLI exit.
 */
export function makeKeepAliveCallback<F extends AnyFunction>(callback: F): F {
  const keepAlive = beginKeepAlive();

  const fn: F = function (this: any, ...args: Parameters<F>): ReturnType<F> {
    endKeepAlive(keepAlive);
    return callback.apply(this, args);
  } as F;

  return fn;
}
