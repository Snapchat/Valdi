import 'jasmine/src/jasmine';
import { Worker, inWorker } from 'worker/src/Worker';

interface WorkerPortMessage {
  readonly type: string;
  readonly port: MessagePort;
}

function timeout(ms: number): Promise<void> {
  // eslint-disable-next-line @snap/valdi/assign-timer-id
  return new Promise(resolve => setTimeout(resolve, ms, 'timeout'));
}

function receiveNext<T>(port: MessagePort): Promise<MessageEvent<T>> {
  return new Promise(resolve => {
    port.onmessage = event => resolve(event as MessageEvent<T>);
  });
}

describe('worker', () => {
  it('run_with_worker', async () => {
    const worker = new Worker('worker/test_workers/TestWorker');
    const workerAnswerPromise = new Promise(resolve => {
      worker.onmessage = e => {
        console.log('host: received message from worker');
        resolve(e.data as [string, number]);
      };
      console.log('host: sending message to worker');
      worker.postMessage(['hello', 123]);
    });
    expect(await workerAnswerPromise).toEqual(['world', 456]);
    expect(inWorker()).toBeFalse();
  }, 1000);
  it('test_worker_close', async () => {
    const worker = new Worker('worker/test_workers/TestWorker');
    const workerAnswerPromise = new Promise(resolve => {
      worker.onmessage = e => {
        console.log('host: received message from worker');
        resolve(e.data as [string, number]);
      };
      console.log('host: sending message to worker');
      worker.postMessage(['close', 123]);
    });
    const answer = await Promise.race([workerAnswerPromise, timeout(100)]);
    expect(answer).toEqual(['timeout']);
  });

  it('params are propagated', async () => {
    const echoValue = 'pong';
    const worker = new Worker('worker/test_workers/EchoWorker?echo=' + echoValue);
    const pong = new Promise(resolve => {
      worker.onmessage = e => {
        console.log('host: received message from worker');
        resolve(e.data);
      };
      console.log('host: sending message to worker');
      worker.postMessage('ping');
    });
    expect(await pong).toEqual(echoValue);
  }, 100);

  it('communicates both ways over a channel transferred to a worker', async () => {
    const worker = new Worker('worker/test_workers/MessageChannelWorker');
    const channel = new MessageChannel();
    const replyEvents: MessageEvent<unknown>[] = [];
    const replies = new Promise<void>(resolve => {
      channel.port1.onmessage = event => {
        replyEvents.push(event);
        if (replyEvents.length === 3) {
          resolve();
        }
      };
    });

    try {
      worker.postMessage({ type: 'initialize', port: channel.port2 }, [channel.port2]);
      channel.port1.postMessage('first');
      channel.port1.postMessage('second');
      channel.port1.postMessage('third');

      await replies;
      expect(replyEvents.map(event => event.data)).toEqual([
        ['worker reply', 'first', 0, true],
        ['worker reply', 'second', 0, true],
        ['worker reply', 'third', 0, true],
      ]);
      expect(replyEvents.map(event => event.ports.length)).toEqual([0, 0, 0]);
    } finally {
      worker.terminate();
    }
  });

  it('communicates both ways over a channel transferred from a worker', async () => {
    const worker = new Worker('worker/test_workers/MessageChannelWorker');
    const transferred = new Promise<MessageEvent<unknown>>(resolve => {
      worker.onmessage = event => resolve(event);
    });

    try {
      worker.postMessage('transfer from worker');

      const event = await transferred;
      const data = event.data as WorkerPortMessage;
      expect(data.type).toBe('worker channel');
      expect(event.ports.length).toBe(1);
      expect(data.port).toBe(event.ports[0]);

      const port = data.port;
      expect((await receiveNext<string>(port)).data).toBe('queued in worker');

      const reply = receiveNext<unknown>(port);
      port.postMessage('sent to worker');
      expect((await reply).data).toEqual(['worker channel reply', 'sent to worker']);
    } finally {
      worker.terminate();
    }
  });
});
