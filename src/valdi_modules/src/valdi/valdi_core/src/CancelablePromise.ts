/**
 * Represents a promise object that can be canceled.
 */
export interface CancelablePromise<T> extends PromiseLike<T> {
  cancel?(): void;
}

/**
 * Error code stamped on the failure a canceled native promise delivers to its callbacks — the C++
 * Valdi::kPromiseCanceledErrorCode. Native errors carry their code onto the JS Error as `code`.
 */
export const PROMISE_CANCELED_ERROR_CODE = 101;

const PROMISE_CANCELED_MESSAGE = 'Promise canceled';

/**
 * Whether a rejection is a promise cancellation rather than a genuine failure.
 *
 * Canceling a native promise settles it with this failure, so anything that awaits a promise it
 * also cancels will see a rejection where it previously saw nothing at all. Cancellation is
 * intentional: callers generally want to treat it as an abort rather than log it, record it as a
 * failure, or surface it to the user.
 *
 * `code` is the only reliable signal — every boundary a cancellation can cross preserves it
 * (`convertValdiErrorToJSError` into JS, `CppPromiseCallback`/`newValdiException` across JNI,
 * `NSErrorFromError`/`ErrorFromNSError` across Obj-C). The message check is a backstop for a
 * producer that rejects with the canceled message but no code, and it only holds where the message
 * arrives verbatim: on Android `CppPromiseCallback` sends `messageWithCauses()`, which prefixes the
 * exception class, so the message never matches there. Check the raw rejection before wrapping it —
 * a wrapper that copies `.message` into a new Error without `code` defeats both checks.
 */
export function isPromiseCanceledError(error: unknown): boolean {
  if (error === undefined || error === null) {
    return false;
  }
  const candidate = error as { code?: unknown; message?: unknown };
  return candidate.code === PROMISE_CANCELED_ERROR_CODE || candidate.message === PROMISE_CANCELED_MESSAGE;
}

/**
 * Return a CancelablePromise from an existing promise, with a cancel callback that will
 * be invoked when the cancelable promise is canceled.
 */
export function promiseToCancelablePromise<T>(promise: Promise<T>, onCancel: () => void): CancelablePromise<T> {
  return {
    cancel: onCancel,
    then: (onfulfilled, onrejected) => {
      return promise.then(onfulfilled, onrejected);
    },
  };
}

export type PromiseOnCancelFn = () => void;

export class PromiseCanceler {
  /**
   * Returns whether cancel() has been called on the promise canceler.
   */
  get canceled(): boolean {
    return this._canceled === true;
  }

  private _canceled?: boolean;
  private _onCancels?: PromiseOnCancelFn | PromiseOnCancelFn[];

  constructor() {}

  /**
   * Registers a cancel function that will be invoked when cancel() is called.
   */
  onCancel(fn: PromiseOnCancelFn): void {
    if (this._canceled) {
      fn();
      return;
    }

    if (!this._onCancels) {
      this._onCancels = fn;
    } else {
      if (Array.isArray(this._onCancels)) {
        this._onCancels.push(fn);
      } else {
        this._onCancels = [this._onCancels, fn];
      }
    }
  }

  /**
   * Mark the CancelablePromise as canceled, which will notify all the
   * registered onCancel callbacks.
   */
  cancel(): void {
    if (!this._canceled) {
      this._canceled = true;

      const onCancels = this._onCancels;
      if (!onCancels) {
        return;
      }

      this._onCancels = undefined;

      if (Array.isArray(onCancels)) {
        for (const onCancel of onCancels) {
          onCancel();
        }
      } else {
        onCancels();
      }
    }
  }

  clear() {
    this._onCancels = undefined;
  }

  toCancelablePromise<T>(promise: Promise<T>): CancelablePromise<T> {
    const cancelablePromise = promise as CancelablePromise<T>;
    cancelablePromise.cancel = this.cancel.bind(this);
    return cancelablePromise;
  }
}
