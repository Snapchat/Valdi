import 'jasmine/src/jasmine';

interface NestedTransferredPortData {
  readonly ports: readonly MessagePort[];
}

interface TransferredPortData {
  readonly nested: NestedTransferredPortData;
}

function receiveNext<T>(port: MessagePort): Promise<MessageEvent<T>> {
  return new Promise(resolve => {
    port.onmessage = event => resolve(event as MessageEvent<T>);
  });
}

function nextTask(): Promise<void> {
  return new Promise(resolve => {
    // eslint-disable-next-line @snap/valdi/assign-timer-id
    setTimeout(resolve, 0);
  });
}

describe('MessageChannel', () => {
  it('is backed by native MessageChannel and MessagePort classes', () => {
    const channel = new MessageChannel();

    expect(channel instanceof MessageChannel).toBeTrue();
    expect(channel.port1).toBe(channel.port1);
    expect(Object.prototype.hasOwnProperty.call(channel, 'port1')).toBeTrue();
    expect(Object.prototype.hasOwnProperty.call(channel, 'port2')).toBeTrue();
    expect(Object.getOwnPropertyDescriptor(channel, 'port1')?.writable).toBeTrue();
    expect(Object.getOwnPropertyDescriptor(channel, 'port2')?.writable).toBeTrue();
    expect(Object.prototype.hasOwnProperty.call(channel.port1, 'postMessage')).toBeFalse();
    expect(typeof Object.getPrototypeOf(channel.port1).postMessage).toBe('function');
    expect(channel.port1.onmessage).toBeNull();

    const onmessage = () => {};
    channel.port1.onmessage = onmessage;
    expect(channel.port1.onmessage).toBe(onmessage);
    channel.port1.onmessage = null;
    expect(channel.port1.onmessage).toBeNull();
  });

  it('queues messages until onmessage starts the port and preserves FIFO order', async () => {
    const channel = new MessageChannel();
    const messages: unknown[] = [];

    channel.port1.postMessage('first');
    channel.port1.postMessage('second', []);
    channel.port1.postMessage('third');

    const received = new Promise<void>(resolve => {
      channel.port2.onmessage = event => {
        messages.push(event.data);
        expect(event.ports).toEqual([]);
        if (messages.length === 3) {
          resolve();
        }
      };
    });

    await received;
    expect(messages).toEqual(['first', 'second', 'third']);
  });

  it('delivers ArrayBuffer values', async () => {
    const channel = new MessageChannel();
    const received = receiveNext<ArrayBuffer>(channel.port2);
    const buffer = new Uint8Array([1, 2, 3]).buffer;

    channel.port1.postMessage(buffer);

    const event = await received;
    expect(Array.from(new Uint8Array(event.data))).toEqual([1, 2, 3]);
  });

  it('discards messages delivered after start when there is no handler', async () => {
    const channel = new MessageChannel();
    channel.port2.start();
    channel.port1.postMessage('discarded');
    await nextTask();

    const received = receiveNext<string>(channel.port2);
    channel.port1.postMessage('delivered');
    expect((await received).data).toBe('delivered');
  });

  it('can transfer a port over another port and queue messages during transfer', async () => {
    const control = new MessageChannel();
    const payload = new MessageChannel();
    const transferredEvent = receiveNext<string>(control.port2);

    control.port1.postMessage('transfer', [payload.port1]);
    payload.port2.postMessage('queued immediately');

    const event = await transferredEvent;
    expect(event.data).toBe('transfer');
    expect(event.ports.length).toBe(1);
    expect((await receiveNext<string>(event.ports[0])).data).toBe('queued immediately');

    // Detached handles are inert, including repeated close calls.
    payload.port1.postMessage('ignored');
    payload.port1.start();
    payload.port1.close();
    payload.port1.close();
    expect(() => control.port1.postMessage('invalid', [payload.port1])).toThrowError(
      'MessagePort in transfer list is already detached',
    );
  });

  it('preserves transferred ports embedded in message data', async () => {
    const control = new MessageChannel();
    const payload = new MessageChannel();
    const transferredEvent = receiveNext<TransferredPortData>(control.port2);

    control.port1.postMessage({ nested: { ports: [payload.port1, payload.port1] } }, [payload.port1]);

    const event = await transferredEvent;
    expect(event.ports.length).toBe(1);
    expect(event.data.nested.ports[0]).toBe(event.ports[0]);
    expect(event.data.nested.ports[1]).toBe(event.ports[0]);

    const received = receiveNext<string>(event.data.nested.ports[0]);
    payload.port2.postMessage('sent through data port');
    expect((await received).data).toBe('sent through data port');
  });

  it('rejects duplicate transfers atomically', async () => {
    const control = new MessageChannel();
    const payload = new MessageChannel();

    expect(() => control.port1.postMessage('invalid', [payload.port1, payload.port1])).toThrowError(
      'Transfer list contains duplicate MessagePort',
    );

    const received = receiveNext<string>(payload.port2);
    payload.port1.postMessage('still attached');
    expect((await received).data).toBe('still attached');
  });

  it('rejects transferring the sending port without detaching it', async () => {
    const channel = new MessageChannel();

    expect(() => channel.port1.postMessage('invalid', [channel.port1])).toThrowError(
      'Transfer list contains source MessagePort',
    );

    const received = receiveNext<string>(channel.port2);
    channel.port1.postMessage('still attached');
    expect((await received).data).toBe('still attached');
  });

  it('closes ports idempotently and discards later messages', async () => {
    const channel = new MessageChannel();
    let delivered = false;
    channel.port2.onmessage = () => {
      delivered = true;
    };
    channel.port2.close();
    channel.port2.close();
    channel.port2.start();
    channel.port1.postMessage('ignored');

    await nextTask();
    expect(delivered).toBeFalse();
  });
});
