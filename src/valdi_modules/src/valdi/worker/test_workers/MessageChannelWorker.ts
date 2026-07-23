interface WorkerPortMessage {
  readonly type: string;
  readonly port: MessagePort;
}

onmessage = workerEvent => {
  const event = workerEvent as unknown as MessageEvent<unknown>;
  if (event.data === 'transfer from worker') {
    const channel = new MessageChannel();
    const postWorkerMessage = postMessage as (data: unknown, transfer?: readonly MessagePort[]) => void;
    channel.port1.onmessage = portEvent => {
      channel.port1.postMessage(['worker channel reply', portEvent.data]);
    };
    postWorkerMessage({ type: 'worker channel', port: channel.port2 }, [channel.port2]);
    channel.port1.postMessage('queued in worker');
    return;
  }

  const port = (event.data as WorkerPortMessage).port;
  const dataPortMatchesTransferredPort = port === event.ports[0];

  // The transferred port becomes the worker's only message receiver.
  delete (globalThis as { onmessage?: unknown }).onmessage;
  port.onmessage = portEvent => {
    port.postMessage(['worker reply', portEvent.data, portEvent.ports.length, dataPortMatchesTransferredPort]);
  };
};
