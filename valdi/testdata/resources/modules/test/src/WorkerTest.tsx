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
    busyWorker.postMessage('start');
  }

  terminateBusyWorkerWithPort(callback: (res: string) => void) {
    const busyWorker = new Worker('test/src/InfinitePortWorker');
    this.worker = busyWorker;
    const channel = new MessageChannel();
    channel.port1.onmessage = e => {
      if (e.data !== 'ready') {
        callback('message-after-termination');
        return;
      }

      busyWorker.terminate();
      // Termination must remain idempotent while asynchronous teardown is pending.
      busyWorker.terminate();
      // The host end of the transferred channel must survive the worker being aborted mid-loop.
      channel.port1.close();

      const replacementWorker = new Worker('test/src/MyWorker');
      this.worker = replacementWorker;
      replacementWorker.onmessage = replacementEvent => {
        callback(replacementEvent.data as string);
      };
      replacementWorker.postMessage('hi');
    };
    busyWorker.postMessage({ type: 'port' }, [channel.port2]);
  }

  terminateIdleWorker(callback: (res: string) => void) {
    // An idle (non-spinning) worker that acknowledges a message and is then torn down.
    // Unlike terminateBusyWorker this requires no execution interrupt, so it also runs on
    // JavaScriptCore (the iOS engine) — guarding the runtime teardown path
    // (JavaScriptRuntime::teardownOnJsThread -> GCDDispatchQueue::fullTeardown) there, which the
    // busy-worker variants cannot cover because JSCore cannot interrupt a running loop.
    const idleWorker = new Worker('test/src/MyWorker');
    this.worker = idleWorker;
    idleWorker.onmessage = e => {
      if (e.data !== 'works') {
        callback('unexpected-worker-reply');
        return;
      }

      idleWorker.terminate();
      // Termination must remain idempotent while asynchronous teardown is pending.
      idleWorker.terminate();

      // The host must survive the teardown and still be able to run a replacement worker.
      const replacementWorker = new Worker('test/src/MyWorker');
      this.worker = replacementWorker;
      replacementWorker.onmessage = replacementEvent => {
        callback(replacementEvent.data as string);
      };
      replacementWorker.postMessage('hi');
    };
    idleWorker.postMessage('hi');
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
