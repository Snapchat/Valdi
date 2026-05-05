// eslint-disable-next-line @typescript-eslint/ban-ts-comment
// @ts-ignore
import { RequireFunc } from './IModuleLoader';
import { setTimeoutUninterruptible } from './SetTimeout';

declare const require: RequireFunc;

/**
 * Checks whether microtasks to handle Promise fulfillments are
 * flushed when doing a native call. This will be true on older
 * versions of JavaScriptCore that had a bug with their C API.
 */
export function arePromiseUtterlyBroken(nativeCall: () => void): boolean {
  let thenCalled = false;
  // eslint-disable-next-line @typescript-eslint/no-floating-promises
  new Promise<void>(resolve => {
    resolve();
  }).then(() => {
    thenCalled = true;
  });

  nativeCall();

  return thenCalled;
}

export function polyfillPromise() {
  const promise = require('./Promise', true);
  // eslint-disable-next-line @typescript-eslint/ban-ts-comment
  // @ts-ignore
  if (promise && promise.polyfill) {
    // eslint-disable-next-line @typescript-eslint/ban-ts-comment
    // @ts-ignore
    promise.polyfill();

    if (typeof queueMicrotask !== 'undefined') {
      const ourAsap = (callback: (arg: any) => void, arg: any) => {
        queueMicrotask(() => {
          callback(arg);
        });
      };
      (Promise as any)._setAsap(ourAsap);
    } else {
      const ourAsap = (callback: (arg: any) => void, arg: any) => {
        // eslint-disable-next-line @snap/valdi/assign-timer-id
        setTimeoutUninterruptible(() => {
          callback(arg);
        }, 1);
      };
      (Promise as any)._setAsap(ourAsap);
    }
  }
}
