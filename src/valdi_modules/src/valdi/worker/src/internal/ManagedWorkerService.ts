/* eslint-disable @typescript-eslint/no-unsafe-member-access */
/* eslint-disable @typescript-eslint/no-explicit-any */
import { CancelablePromise, PromiseCanceler } from 'valdi_core/src/CancelablePromise';
import { protectNativeRefsForCurrentContextId } from 'valdi_core/src/NativeReferences';
import { IWorkerServiceClient, IWorkerService } from '../IWorkerService';
import { processWorkerServiceExecutorTask, WorkerServiceExecutorTask } from '../WorkerServiceExecutor';
import { ServiceFunction, canBeUsedAsProxyMethod, forwardCallToService } from '../utils/WorkerServiceBridgeUtils';
import { ManagedWorker } from './ManagedWorker';

type NativeRefsDisposable = () => void;

interface ManagedServiceWorkersGetters {
  $managedServiceWorkers: { [key: number]: ManagedWorkerService<unknown> };
  $managedServiceWorkerSequence: number;
}

const managedServiceWorkerGlobals = globalThis as typeof globalThis & ManagedServiceWorkersGetters;

export class ManagedWorkerService<T> {
  private retainCount = 0;

  constructor(
    readonly serviceId: number,
    readonly file: string,
    readonly className: string,
    readonly api: T,
    readonly servicePromise: Promise<IWorkerService<T>>,
    readonly serviceArguments: readonly unknown[],
    readonly nativeRefsDisposable: NativeRefsDisposable,
    readonly managedWorker: ManagedWorker | undefined,
  ) {}

  static create<T>(
    file: string,
    className: string,
    args: readonly unknown[],
    managedWorker: ManagedWorker | undefined,
  ): ManagedWorkerService<T> {
    const serviceId = (managedServiceWorkerGlobals.$managedServiceWorkerSequence ?? 0) + 1;
    const nativeRefsDisposable = protectNativeRefsForCurrentContextId();

    if (managedWorker) {
      managedWorker.onUsed();
    }

    const servicePromise: Promise<IWorkerService<T>> = new Promise((resolve, reject) => {
      const task = makeCreateWorkerServiceTask(serviceId, file, className, args, resolve, reject);

      if (managedWorker) {
        managedWorker.worker.postMessage(task);
      } else {
        processWorkerServiceExecutorTask(task);
      }
    });

    const instance = new ManagedWorkerService<T>(
      serviceId,
      file,
      className,
      makeAPIProxy<T>(servicePromise),
      servicePromise,
      args,
      nativeRefsDisposable,
      managedWorker,
    );

    managedServiceWorkerGlobals.$managedServiceWorkerSequence = serviceId;
    if (!managedServiceWorkerGlobals.$managedServiceWorkers) {
      managedServiceWorkerGlobals.$managedServiceWorkers = {};
    }
    managedServiceWorkerGlobals.$managedServiceWorkers[serviceId] = instance;

    return instance;
  }

  static fromId(serviceId: number, file: string, className: string): ManagedWorkerService<unknown> | undefined {
    const service = managedServiceWorkerGlobals.$managedServiceWorkers[serviceId];
    if (!service || service.file !== file || service.className !== className) {
      return undefined;
    }
    return service;
  }

  static forEachWithFileAndClassName(
    file: string,
    className: string,
    cb: (id: number, managedServiceWorker: ManagedWorkerService<unknown>) => void,
  ): void {
    const managedServiceWorkers = managedServiceWorkerGlobals.$managedServiceWorkers;
    if (managedServiceWorkers) {
      for (const id in managedServiceWorkers) {
        const managedServiceWorker = managedServiceWorkers[id];
        if (
          managedServiceWorker &&
          managedServiceWorker.file === file &&
          managedServiceWorker.className === className
        ) {
          cb(Number.parseInt(id), managedServiceWorker);
        }
      }
    }
  }

  static getAllIdsForFileAndClassName(file: string, className: string): number[] {
    const out: number[] = [];

    this.forEachWithFileAndClassName(file, className, id => {
      out.push(id);
    });

    return out;
  }

  retain(): void {
    this.retainCount++;
  }

  release(): void {
    this.retainCount--;

    if (this.retainCount === 0) {
      delete managedServiceWorkerGlobals.$managedServiceWorkers[this.serviceId];

      // eslint-disable-next-line @typescript-eslint/no-floating-promises
      this.servicePromise.then(service => {
        service.dispose();
        this.nativeRefsDisposable();
        this.managedWorker?.onUnused();
      });
    }
  }

  makeClient(): IWorkerServiceClient<T> {
    return new WorkerServiceClient<T>(this);
  }
}

class WorkerServiceClient<T> implements IWorkerServiceClient<T> {
  readonly serviceId: number;

  private client: ManagedWorkerService<T> | undefined;

  constructor(client: ManagedWorkerService<T>) {
    this.serviceId = client.serviceId;
    this.client = client;

    client.retain();
  }

  get api(): T {
    return this.client!.api;
  }

  dispose(): void {
    if (this.client) {
      this.client.release();
      this.client = undefined;
    }
  }
}

type AnyPromiseFunction = (...args: unknown[]) => CancelablePromise<unknown>;

function makeAPIProxyFunction<T>(name: PropertyKey, servicePromise: Promise<IWorkerService<T>>): AnyPromiseFunction {
  function proxyFunction(...args: unknown[]): CancelablePromise<unknown> {
    const promiseCanceler = new PromiseCanceler();

    const promise = servicePromise.then(client => {
      // eslint-disable-next-line @typescript-eslint/no-unsafe-assignment
      const api = client.api as any;
      if (!api) {
        throw new Error(`Cannot call ${name.toString()}: WorkerService was disposed`);
      }

      const fn = api[name] as ServiceFunction<unknown[], unknown>;
      if (!fn) {
        throw new Error(`Cannot call ${name.toString()}: WorkerService does not expose this method`);
      }

      return forwardCallToService(api, fn, promiseCanceler, args);
    });

    return promiseCanceler.toCancelablePromise(promise);
  }

  Object.defineProperty(proxyFunction, 'name', { value: name });

  return proxyFunction;
}

function makeAPIProxy<T>(servicePromise: Promise<IWorkerService<T>>): T {
  return new Proxy(
    {},
    {
      get(target: any, prop: PropertyKey): AnyPromiseFunction | undefined {
        let fn = target[prop] as AnyPromiseFunction | undefined;
        if (!fn) {
          if (canBeUsedAsProxyMethod(prop)) {
            fn = makeAPIProxyFunction(prop, servicePromise);
            target[prop] = fn;
          }
        }

        return fn;
      },
    },
  ) as T;
}

function makeCreateWorkerServiceTask<T, A extends readonly unknown[]>(
  serviceId: number,
  file: string,
  className: string,
  args: A,
  resolve: (data: IWorkerService<T>) => void,
  reject: (err: unknown) => void,
): WorkerServiceExecutorTask {
  return {
    serviceId,
    file,
    className,
    args,
    callback: (data, error) => {
      if (error) {
        reject(error);
      } else {
        const unprocessed = data as IWorkerService<unknown>;
        let out: IWorkerService<T>;

        try {
          out = {
            // eslint-disable-next-line @typescript-eslint/no-unsafe-assignment
            api: unprocessed.api as any,
            dispose: unprocessed.dispose,
          };
        } catch (exc) {
          reject(exc);
          return;
        }
        resolve(out);
      }
    },
  };
}
