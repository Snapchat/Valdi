interface WorkerProbeMessageEvent {
  readonly data: unknown;
}

const workerGlobal = globalThis as typeof globalThis & {
  onmessage: ((event: WorkerProbeMessageEvent) => void) | undefined;
  postMessage(message: unknown): void;
};

workerGlobal.onmessage = event => {
  workerGlobal.postMessage({ echoed: event.data, source: 'valdi-web-worker' });
};
