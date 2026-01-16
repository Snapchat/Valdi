/**
 * Error thrown when a DeferredPromise exceeds its specified timeout
 */
export class DeferredPromiseTimeoutError extends Error {
  constructor(timeoutMs: number) {
    super(`Promise timed out after ${timeoutMs}ms`);
    this.name = 'DeferredPromiseTimeoutError';
  }
}

/**
 * A manually controllable Promise that can be resolved/rejected later.
 * Supports optional timeout that auto-rejects with a typed error.
 */
export class DeferredPromise<T> implements PromiseLike<T> {
  private _resolve!: (value: T | PromiseLike<T>) => void;
  private _reject!: (reason?: unknown) => void;
  private _isSettled = false;
  private _timeoutId: ReturnType<typeof setTimeout> | undefined;

  public readonly promise: Promise<T>;

  constructor(timeoutMs?: number) {
    this.promise = new Promise<T>((resolve, reject) => {
      this._resolve = resolve;
      this._reject = reject;
    });

    // Prevent accidental late calls after settlement
    this.resolve = this._settle(this._resolve);
    this.reject  = this._settle(this._reject);

    if (timeoutMs !== undefined && Number.isFinite(timeoutMs) && timeoutMs > 0) {
      this._timeoutId = setTimeout(() => {
        this.reject(new DeferredPromiseTimeoutError(timeoutMs));
      }, timeoutMs);
    }
  }

  /**
   * Resolve the promise with a value (idempotent)
   */
  public resolve!: (value: T | PromiseLike<T>) => void;

  /**
   * Reject the promise with a reason (idempotent)
   */
  public reject!: (reason?: unknown) => void;

  /**
   * Check whether the promise has already been resolved or rejected
   */
  public get isSettled(): boolean {
    return this._isSettled;
  }

  /**
   * Clear timeout (if any) and prevent further settlement
   */
  public cancelTimeout(): void {
    if (this._timeoutId !== undefined) {
      clearTimeout(this._timeoutId);
      this._timeoutId = undefined;
    }
  }

  private _settle<TFn extends (...args: any[]) => void>(callback: TFn) {
    return (...args: Parameters<TFn>) => {
      if (this._isSettled) return;
      this._isSettled = true;

      this.cancelTimeout(); // always clean up

      callback(...args);
    };
  }

  // Make it awaitable / thenable
  then<TResult1 = T, TResult2 = never>(
    onfulfilled?: ((value: T) => TResult1 | PromiseLike<TResult1>) | null | undefined,
    onrejected?: ((reason: any) => TResult2 | PromiseLike<TResult2>) | null | undefined,
  ): Promise<TResult1 | TResult2> {
    return this.promise.then(onfulfilled, onrejected);
  }

  // Optional: make catch() and finally() directly available
  catch<TResult = never>(
    onrejected?: ((reason: any) => TResult | PromiseLike<TResult>) | null | undefined,
  ): Promise<T | TResult> {
    return this.promise.catch(onrejected);
  }

  finally(onfinally?: (() => void) | null | undefined): Promise<T> {
    return this.promise.finally(onfinally);
  }
}
