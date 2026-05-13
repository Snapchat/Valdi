/**
 * Error thrown when a DeferredPromise exceeds its specified timeoutMs.
 */
export class DeferredPromiseTimeoutError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'DeferredPromiseTimeoutError';
    Object.setPrototypeOf(this, DeferredPromiseTimeoutError.prototype);
  }
}

/**
 * Creates a Promise that can later be resolved or rejected
 */
export class DeferredPromise<T> implements PromiseLike<T> {
  private resolved = false;
  private timeoutId?: any;

  readonly promise: Promise<T>;
  resolve!: (value: T | PromiseLike<T>) => void;
  reject!: (reason?: unknown) => void;

  constructor(timeoutMs?: number) {
    let res: (value: T | PromiseLike<T>) => void;
    let rej: (reason?: unknown) => void;

    this.promise = new Promise<T>((_res, _rej) => {
      res = _res;
      rej = _rej;
    });

    this.resolve = value => this.finalize(res, value);
    this.reject = reason => this.finalize(rej, reason);

    if (timeoutMs !== undefined) {
      this.timeoutId = setTimeout(() => {
        this.reject(new DeferredPromiseTimeoutError(`Promise timed out after ${timeoutMs}ms`));
      }, timeoutMs);
    }
  }

  /**
   * Prevents multiple resolutions and clears any active timeout.
   */
  private finalize(callback: Function, arg: any) {
    if (this.resolved) return;
    this.resolved = true;

    if (this.timeoutId) {
      clearTimeout(this.timeoutId);
      this.timeoutId = undefined;
    }

    callback(arg);
  }

  then<T1 = T, T2 = never>(
    onfulfilled?: (value: T) => T1 | PromiseLike<T1>,
    onrejected?: (reason: any) => T2 | PromiseLike<T2>,
  ): Promise<T1 | T2> {
    return this.promise.then(onfulfilled, onrejected);
  }
}
