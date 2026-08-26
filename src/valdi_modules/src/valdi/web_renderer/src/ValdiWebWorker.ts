import type {
  NativeMessageEvent,
  NativeMessagePort,
  NativeWorker,
  OnMessageFunc,
} from 'valdi_core/src/ValdiRuntime';

export type ValdiWebWorkerFactory = () => Worker;

const WORKER_FACTORIES = new Map<string, ValdiWebWorkerFactory>();

function getLogicalModulePath(modulePath: string): string {
  const queryIndex = modulePath.indexOf('?');
  const logicalModulePath = queryIndex < 0 ? modulePath : modulePath.substring(0, queryIndex);
  if (!logicalModulePath) {
    throw new Error('Valdi web worker module path must not be empty');
  }
  return logicalModulePath;
}

export function registerValdiWebWorker(modulePath: string, factory: ValdiWebWorkerFactory): void {
  const logicalModulePath = getLogicalModulePath(modulePath);
  const existingFactory = WORKER_FACTORIES.get(logicalModulePath);

  if (existingFactory !== undefined && existingFactory !== factory) {
    throw new Error(`Valdi web worker "${logicalModulePath}" is already registered`);
  }

  WORKER_FACTORIES.set(logicalModulePath, factory);
}

export function createValdiWebWorker(modulePath: string): NativeWorker {
  const logicalModulePath = getLogicalModulePath(modulePath);
  const factory = WORKER_FACTORIES.get(logicalModulePath);
  if (factory === undefined) {
    throw new Error(`Valdi web worker is not registered: ${logicalModulePath}`);
  }

  const worker = factory();
  return {
    postMessage<T>(data: T, transfer?: readonly NativeMessagePort[]): void {
      if (transfer === undefined) {
        worker.postMessage(data);
      } else {
        worker.postMessage(data, transfer as unknown as Transferable[]);
      }
    },
    setOnMessage<T>(func: OnMessageFunc<T>): void {
      worker.onmessage = event => {
        func(event as unknown as NativeMessageEvent<T>);
      };
    },
    terminate(): void {
      worker.terminate();
    },
  };
}
