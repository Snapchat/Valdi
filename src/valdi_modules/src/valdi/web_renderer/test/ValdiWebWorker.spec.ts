import 'jasmine/src/jasmine';
import { createValdiWebWorker, registerValdiWebWorker } from '../src/ValdiWebWorker';

class RecordingWorker {
  readonly messages: unknown[] = [];
  onmessage: ((event: MessageEvent) => void) | null = null;
  terminated = false;
  postMessageError: Error | undefined;

  postMessage(data: unknown): void {
    if (this.postMessageError) {
      throw this.postMessageError;
    }
    this.messages.push(data);
  }

  terminate(): void {
    this.terminated = true;
  }
}

describe('ValdiWebWorker', () => {
  let moduleSequence = 0;

  function nextModulePath(): string {
    moduleSequence += 1;
    return `worker/test/Worker${moduleSequence}`;
  }

  it('allows idempotent registration of the same factory', () => {
    const modulePath = nextModulePath();
    const factory = () => new RecordingWorker() as unknown as Worker;

    registerValdiWebWorker(modulePath, factory);
    expect(() => registerValdiWebWorker(modulePath, factory)).not.toThrow();
    expect(() => registerValdiWebWorker(modulePath, () => factory())).toThrowError(
      `Valdi web worker "${modulePath}" is already registered`,
    );
  });

  it('throws synchronously when the logical module is not registered', () => {
    const modulePath = nextModulePath();

    expect(() => createValdiWebWorker(`${modulePath}?value=1`)).toThrowError(
      `Valdi web worker is not registered: ${modulePath}`,
    );
  });

  it('uses the registered factory for requests with query parameters', () => {
    const modulePath = nextModulePath();
    const worker = new RecordingWorker();
    registerValdiWebWorker(modulePath, () => worker as unknown as Worker);

    createValdiWebWorker(`${modulePath}?value=1`);

    expect(worker.terminated).toBeFalse();
  });

  it('forwards messages, browser events, clone errors, and termination', () => {
    const modulePath = nextModulePath();
    const worker = new RecordingWorker();
    registerValdiWebWorker(modulePath, () => worker as unknown as Worker);
    const nativeWorker = createValdiWebWorker(modulePath);
    const event = { data: { response: true } } as MessageEvent;
    let receivedEvent: MessageEvent | undefined;

    nativeWorker.setOnMessage(message => {
      receivedEvent = message as MessageEvent;
    });
    worker.onmessage!(event);
    nativeWorker.postMessage({ request: true });

    expect(receivedEvent).toBe(event);
    expect(worker.messages).toEqual([{ request: true }]);

    const cloneError = new Error('The object could not be cloned');
    worker.postMessageError = cloneError;
    expect(() => nativeWorker.postMessage(() => {})).toThrow(cloneError);

    nativeWorker.terminate();
    expect(worker.terminated).toBeTrue();
  });
});
