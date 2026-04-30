import { setTimeoutInterruptible } from '../SetTimeout';

export type PromiseLikeFunction<T> = (cb: (data?: T, err?: any) => void) => void;

/**
 * Makes a Promise from a PromiseLikeFunction
 * @param fn
 */
export function promisify<T>(fn: PromiseLikeFunction<T>): Promise<T> {
  return new Promise((resolve, reject) => {
    fn((data, err) => {
      if (data) {
        resolve(data);
      } else if (err) {
        reject(err);
      } else {
        reject(new Error('No data returned'));
      }
    });
  });
}

/**
 * Converts a synchronous producer of a value to a Promise, taking
 * in account failures in case of thrown exceptions.
 * @param fn
 */
export function promisifyProducer<T>(fn: () => T): Promise<T> {
  return new Promise((resolve, reject) => {
    try {
      const result = fn();
      resolve(result);
    } catch (err: any) {
      reject(err);
    }
  });
}

/**
 * Converts a setTimeout into a promise, resolving it after the given
 * provided delay in milliseconds.
 */
export function wait(delayMs: number): Promise<void> {
  return new Promise(resolve => {
    // eslint-disable-next-line @snap/valdi/assign-timer-id
    setTimeout(() => {
      resolve();
    }, delayMs);
  });
}

/**
 * Converts a setTimeoutInterruptible into a promise, resolving it after the given
 * provided delay in milliseconds.
 */
export function waitInterruptible(delayMs: number): Promise<void> {
  return new Promise(resolve => {
    // eslint-disable-next-line @snap/valdi/assign-timer-id
    setTimeoutInterruptible(() => {
      resolve();
    }, delayMs);
  });
}

/**
 * Ensures that only one promise is performing at once.
 */
export class PromiseDeduper<T> {
  private promise: Promise<T> | null = null;

  get isPerforming(): boolean {
    return this.promise !== null;
  }

  /**
   * Returns the in-flight promise if possible, otherwise asks the provider for a new
   * promise to consider in-flight and returns it.
   */
  performIfNeeded(provider: () => Promise<T>): Promise<T> {
    if (!this.promise) {
      this.promise = provider()
        .then(result => {
          this.promise = null;
          return result;
        })
        .catch(err => {
          this.promise = null;
          return Promise.reject(err);
        });
    }
    return this.promise;
  }
}

/**
 * Caches and returns a promise (in-flight and resolved) until invalidated.
 */
export class PromiseCache<T> {
  private promise: Promise<T> | null = null;

  invalidate(): void {
    this.promise = null;
  }

  performIfNeeded(provider: () => Promise<T>): Promise<T> {
    if (!this.promise) {
      this.promise = provider();
    }
    return this.promise;
  }
}
