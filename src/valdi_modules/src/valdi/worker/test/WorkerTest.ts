import 'jasmine/src/jasmine';
import { Worker, inWorker } from 'worker/src/Worker';

function timeout(ms: number): Promise<void> {
  // eslint-disable-next-line @snap/valdi/assign-timer-id
  return new Promise(resolve => setTimeout(resolve, ms, 'timeout'));
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
});
