/* eslint-disable @typescript-eslint/no-unsafe-assignment */
/* eslint-disable @typescript-eslint/no-unsafe-member-access */
/* eslint-disable @typescript-eslint/no-explicit-any */

import { CancelablePromise, PromiseCanceler, PromiseOnCancelFn } from 'valdi_core/src/CancelablePromise';
import { toError } from 'valdi_core/src/utils/ErrorUtils';
import { ServiceCallback, ServiceCallbackType, ServiceError } from '../ServiceCallback';
import { attributeWorkerCallback } from '../internal/WorkerAttribution';

function toServiceError(error: any): ServiceError {
  if (typeof error === 'string') {
    return {
      message: error,
    };
  }

  if (error.message) {
    return {
      message: error.message,
      stack: error.stack,
    };
  }

  return {
    message: JSON.stringify(error),
  };
}

export function unpromisify<T, P extends any[], R>(
  receiver: T,
  method: (...args: P) => CancelablePromise<R>,
): (...args: [...P, ServiceCallback<R>]) => void {
  return (...args: [...P, ServiceCallback<R>]): void => {
    const callback = args.pop() as ServiceCallback<R>;

    try {
      const result = method.apply(receiver, args as any as P);

      if (!(result as unknown) || !result.then) {
        throw new Error(
          `Methods exposed through the WorkerService should always return a Promise. Method ${method} returned ${result}`,
        );
      }

      if (result.cancel) {
        // Forward the cancel callback so that the host can call it
        callback(result.cancel.bind(result), undefined, ServiceCallbackType.ForwardCanceler);
      }

      result.then(
        data => {
          callback(data, undefined, ServiceCallbackType.Success);
        },
        error => {
          callback(undefined, toServiceError(error), ServiceCallbackType.Error);
          return undefined;
        },
      );
    } catch (exc) {
      callback(undefined, toError(exc), ServiceCallbackType.Error);
    }
  };
}

export type ServiceFunction<P extends any[], R> = (...args: [...P, ServiceCallback<R>]) => void;

export function forwardCallToService<P extends any[], R>(
  receiver: unknown,
  fn: ServiceFunction<P, R>,
  promiseCanceler: PromiseCanceler,
  params: P,
  attribution?: string,
): Promise<R> {
  return new Promise<R>((resolve, reject) => {
    const serviceCallback: ServiceCallback<R> = (data, error, type) => {
      switch (type) {
        case ServiceCallbackType.Success:
          if (!promiseCanceler.canceled) {
            resolve(data as R);
            promiseCanceler.clear();
          }
          break;
        case ServiceCallbackType.Error:
          if (!promiseCanceler.canceled) {
            const resolvedError = new Error(error!.message);
            resolvedError.stack = error!.stack;
            reject(resolvedError);
            promiseCanceler.clear();
          }
          break;
        case ServiceCallbackType.ForwardCanceler:
          promiseCanceler.onCancel(data as PromiseOnCancelFn);
          break;
      }
    };

    const callback = attributeWorkerCallback(
      attribution ? `worker.receive(${attribution})` : undefined,
      serviceCallback,
    );
    fn.apply(receiver, [...params, callback]);
  });
}

function flattenObject(obj: any, map: (receiver: any, propertyValue: any) => any): any {
  const out: any = {};

  let current = obj;
  while (current) {
    if (
      current === Object.prototype ||
      current === Array.prototype ||
      current === Function.prototype ||
      current === Number.prototype
    ) {
      break;
    }

    const propertyNames = Object.getOwnPropertyNames(current);
    for (const propertyName of propertyNames) {
      if (propertyName === 'constructor' || out[propertyName] !== undefined) {
        continue;
      }

      const propertyDescriptor = Object.getOwnPropertyDescriptor(current, propertyName);
      if (!propertyDescriptor) {
        continue;
      }
      const propertyValue = propertyDescriptor.value;

      if (!(typeof propertyValue === 'function')) {
        continue;
      }

      out[propertyName] = map(obj, propertyValue);
    }

    current = Object.getPrototypeOf(current);
  }

  return out;
}

export function unpromisifyObject(obj: any): any {
  return flattenObject(obj, unpromisify);
}

const blocklistedKeysForProxy: { [key: PropertyKey]: boolean } = {};
for (const proto of [Object.prototype, Array.prototype, Function.prototype, Number.prototype]) {
  for (const propertyName of Object.getOwnPropertyNames(proto)) {
    blocklistedKeysForProxy[propertyName] = true;
  }
}
blocklistedKeysForProxy['jasmineToString'] = true;

export function canBeUsedAsProxyMethod(name: PropertyKey): boolean {
  return blocklistedKeysForProxy[name] === undefined;
}
