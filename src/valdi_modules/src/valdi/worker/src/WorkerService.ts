/* eslint-disable @typescript-eslint/no-unsafe-return */
/* eslint-disable @typescript-eslint/no-unsafe-member-access */
import { IWorkerServiceClient } from './IWorkerService';
import { WorkerServiceEntryPoint, WorkerServiceEntryPointConstructor } from './WorkerServiceEntryPoint';
import { ManagedWorker } from './internal/ManagedWorker';
import { ManagedWorkerService } from './internal/ManagedWorkerService';

interface ServiceWorkerExecutorGetters {
  $serviceWorkerExecutors: { [key: string]: ManagedWorker };
}

const serviceWorkerExecutorGlobals = globalThis as typeof globalThis & ServiceWorkerExecutorGetters;
const WORKER_SERVICE_EXECUTOR_KEEP_ALIVE_MS = 30_000;

function throwNotAnnotated<T, A extends unknown[], Service extends WorkerServiceEntryPoint<T, A>>(
  workerEntryPoint: WorkerServiceEntryPointConstructor<T, A, Service>,
): never {
  const className = workerEntryPoint.name;
  throw new Error(`The WorkerServiceEntryPoint ${className} should be annotated with @workerService`);
}

function doStartWorkerService<T, A extends unknown[], Service extends WorkerServiceEntryPoint<T, A>>(
  workerEntryPoint: WorkerServiceEntryPointConstructor<T, A, Service>,
  args: A,
  managedWorker: ManagedWorker | undefined,
): IWorkerServiceClient<T> {
  const file = workerEntryPoint.file;
  const className = workerEntryPoint.name;
  if (!file) {
    throwNotAnnotated(workerEntryPoint);
  }

  const managedService = ManagedWorkerService.create<T>(file, className, args, managedWorker);

  return managedService.makeClient();
}

/**
 * Start a new worker service with the given entry point, passing the given arguments.
 * The service will remain in memory until dispose() is called.
 */
export function startWorkerService<T, A extends unknown[], Service extends WorkerServiceEntryPoint<T, A>>(
  workerEntryPoint: WorkerServiceEntryPointConstructor<T, A, Service>,
  args: A,
): IWorkerServiceClient<T> {
  const executorId = workerEntryPoint.executorIdentifier;
  if (!executorId) {
    throwNotAnnotated(workerEntryPoint);
  }

  let serviceWorkerExecutors = serviceWorkerExecutorGlobals.$serviceWorkerExecutors;
  if (!serviceWorkerExecutors) {
    serviceWorkerExecutors = {};
    serviceWorkerExecutorGlobals.$serviceWorkerExecutors = serviceWorkerExecutors;
  }

  let serviceWorkerExecutor = serviceWorkerExecutors[executorId];
  if (!serviceWorkerExecutor) {
    serviceWorkerExecutor = new ManagedWorker(
      'worker/src/WorkerServiceExecutor',
      WORKER_SERVICE_EXECUTOR_KEEP_ALIVE_MS,
    );
    serviceWorkerExecutors[executorId] = serviceWorkerExecutor;
  }

  return doStartWorkerService(workerEntryPoint, args, serviceWorkerExecutor);
}

function launchArgumentsEqual(left: readonly unknown[], right: readonly unknown[]): boolean {
  if (left.length !== right.length) {
    return false;
  }

  for (let i = 0; i < left.length; i++) {
    if (left[i] !== right[i]) {
      return false;
    }
  }

  return true;
}

/**
 * Use an existing worker service with the given entry point and launch arguments, or start a
 * new one if no existing service can be found. A worker service will only be re-used if the launch
 * arguments matches the launch arguments which were passed to a prior call of startWorkerService() or
 * useOrStartWorkerService().
 * The service will remain in memory until dispose() is called.
 */
export function useOrStartWorkerService<T, A extends unknown[], Service extends WorkerServiceEntryPoint<T, A>>(
  workerEntryPoint: WorkerServiceEntryPointConstructor<T, A, Service>,
  args: A,
): IWorkerServiceClient<T> {
  let existingService: ManagedWorkerService<T> | undefined;

  ManagedWorkerService.forEachWithFileAndClassName(workerEntryPoint.file ?? '', workerEntryPoint.name, (id, worker) => {
    if (launchArgumentsEqual(worker.serviceArguments, args)) {
      if (existingService) {
        throw new Error(`Service ${workerEntryPoint.name} has more than 1 instance`);
      }
      existingService = worker as ManagedWorkerService<T>;
    }
  });

  if (existingService) {
    return existingService.makeClient();
  } else {
    return startWorkerService(workerEntryPoint, args);
  }
}

/**
 * Start a new worker service with the given entry point, passing the given arguments, which will
 * evaluate in the current thread instead of using a background thread.
 * The service will remain in memory until dispose() is called.
 */
export function startWorkerServiceInForeground<T, A extends unknown[], Service extends WorkerServiceEntryPoint<T, A>>(
  workerEntryPoint: WorkerServiceEntryPointConstructor<T, A, Service>,
  args: A,
): IWorkerServiceClient<T> {
  return doStartWorkerService(workerEntryPoint, args, undefined);
}

/**
 * Retrieve a previously created worker service.
 * The service will remain in memory until dispose() is called.
 */
export function useWorkerService<T, A extends unknown[], Service extends WorkerServiceEntryPoint<T, A>>(
  workerEntryPoint: WorkerServiceEntryPointConstructor<T, A, Service>,
  serviceId: number,
): IWorkerServiceClient<T> {
  const service = ManagedWorkerService.fromId(
    serviceId,
    workerEntryPoint.file ?? '',
    workerEntryPoint.name,
  ) as ManagedWorkerService<T>;
  if (!service) {
    throw new Error(`Could not resolve service ${workerEntryPoint.name} with id ${serviceId}`);
  }

  return service.makeClient();
}

/**
 * Return all the worker service ids for the given worker entry point
 */
export function getWorkerServiceIdsForEntryPoint<T, A extends unknown[], Service extends WorkerServiceEntryPoint<T, A>>(
  workerEntryPoint: WorkerServiceEntryPointConstructor<T, A, Service>,
): number[] {
  const file = workerEntryPoint.file;
  const className = workerEntryPoint.name;
  if (!file) {
    return [];
  }

  return ManagedWorkerService.getAllIdsForFileAndClassName(file, className);
}

/**
 * Retrieve a previously created singleton worker service.
 * The service will remain in memory until dispose() is called.
 * Will throw if the service was not created or that more than 1 service exists.
 */
export function useWorkerServiceSingleton<T, A extends unknown[], Service extends WorkerServiceEntryPoint<T, A>>(
  workerEntryPoint: WorkerServiceEntryPointConstructor<T, A, Service>,
): IWorkerServiceClient<T> {
  const serviceIds = getWorkerServiceIdsForEntryPoint(workerEntryPoint);
  if (serviceIds.length === 0) {
    throw new Error(`Could not resolve singleton service ${workerEntryPoint.name}`);
  }
  if (serviceIds.length > 1) {
    throw new Error(`Service ${workerEntryPoint.name} has more than 1 instance, found ${serviceIds.length}`);
  }

  return useWorkerService(workerEntryPoint, serviceIds[0]);
}
