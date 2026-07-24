import type { NativeMessagePort, NativeWorker, OnMessageFunc, ValdiRuntime } from 'valdi_core/src/ValdiRuntime';

declare const runtime: ValdiRuntime;

/** This Web Workers API interface represents a background task that can be
 * easily created and can send messages back to its creator. Creating a worker
 * is as simple as calling the Worker() constructor and specifying a script to
 * be run in the worker thread. */
export class Worker {
  private nativeWorker: NativeWorker | null;

  public constructor(url: string) {
    this.nativeWorker = runtime.createWorker(url);
  }

  public set onmessage(func: (event: MessageEvent<unknown>) => void) {
    if (this.nativeWorker) {
      this.nativeWorker.setOnMessage(func as OnMessageFunc<unknown>);
    }
  }

  public postMessage<T>(data: T, transfer?: readonly MessagePort[]): void {
    if (this.nativeWorker) {
      this.nativeWorker.postMessage(data, transfer as readonly NativeMessagePort[] | undefined);
    }
  }

  public terminate(): void {
    if (this.nativeWorker) {
      this.nativeWorker.terminate();
      this.nativeWorker = null;
    }
  }
}

export default Worker;

export function inWorker(): boolean {
  // 'location' is a global variable that is set up in a worker but not in the
  // host runtime.
  return typeof location !== 'undefined';
}
