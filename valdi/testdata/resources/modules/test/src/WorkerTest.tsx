import { Component } from 'valdi_core/src/Component';
import Worker from 'worker/src/Worker';

export class WorkerTest extends Component {
  worker: Worker | null = null;

  callWorker(callback: (res: string) => void) {
    this.worker = new Worker('test/src/MyWorker');
    this.worker.onmessage = e => {
      callback(e.data as string);
    };
    this.worker.postMessage('hi');
  }

  terminateBusyWorker(callback: (res: string) => void) {
    const busyWorker = new Worker('test/src/InfiniteWorker');
    this.worker = busyWorker;
    busyWorker.onmessage = e => {
      if (e.data !== 'ready') {
        callback('message-after-termination');
        return;
      }

      busyWorker.terminate();
      // Termination must remain idempotent while asynchronous teardown is pending.
      busyWorker.terminate();

      const replacementWorker = new Worker('test/src/MyWorker');
      this.worker = replacementWorker;
      replacementWorker.onmessage = replacementEvent => {
        callback(replacementEvent.data as string);
      };
      replacementWorker.postMessage('hi');
    };
  }

  terminateBeforeInitialization(callback: (res: string) => void) {
    const terminatedWorker = new Worker('test/src/MyWorker');
    terminatedWorker.terminate();
    // Termination must also be idempotent before worker initialization completes.
    terminatedWorker.terminate();
    terminatedWorker.postMessage('ignored');

    const replacementWorker = new Worker('test/src/MyWorker');
    this.worker = replacementWorker;
    replacementWorker.onmessage = e => {
      callback(e.data as string);
    };
    replacementWorker.postMessage('hi');
  }

  onRender() {
    <view />;
  }
}
