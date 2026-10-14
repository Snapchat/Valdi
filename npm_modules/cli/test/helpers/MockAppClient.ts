import { Buffer } from 'node:buffer';
import { connect, createServer, type Socket } from 'node:net';

/**
 * Minimal in-process stand-in for a Valdi app connecting to the hot-reload daemon.
 *
 * The daemon (compiler in --monitor mode, non-USB) opens a plain TCP acceptor and,
 * once a client sends a `configure` request, marks it ready and pushes recompiled
 * modules as `updated_resources` events. A real app is normally attached over
 * USB/simulator; none of that is reachable in CI, so this speaks the wire protocol
 * directly to prove the edit -> recompile -> deliver path without a device.
 *
 * Wire format (see DaemonTCPConnection.swift / ValdiDaemonProtocolClient.swift):
 *   packet = [4-byte magic][uint32-LE payload length][JSON payload]
 * where the JSON is a DaemonClientPayload (client->daemon) or DaemonServerPayload
 * (daemon->client).
 */

// Constants.swift: Magic.valdiMagic
const VALDI_MAGIC = Buffer.from([0x33, 0xc6, 0x00, 0x01]);
const HEADER_LENGTH = 8;

export interface ReceivedResource {
  bundle_name: string;
  file_path_within_bundle: string;
  data_base64?: string;
  data_string?: string;
}

/** Decode a delivered resource's contents to a UTF-8 string regardless of encoding. */
export function resourceText(resource: ReceivedResource): string {
  if (resource.data_string !== undefined) {
    return resource.data_string;
  }
  if (resource.data_base64 !== undefined) {
    return Buffer.from(resource.data_base64, 'base64').toString('utf8');
  }
  return '';
}

interface ServerPayload {
  response?: { request_id?: string; error?: { error_message: string } };
  event?: { updated_resources?: { resources: ReceivedResource[] } };
}

export interface MockAppClient {
  /** Send a configure request and resolve once the daemon acknowledges it (client is now "ready"). */
  configure: (opts?: { applicationId?: string; platform?: 'ios' | 'android' }) => Promise<void>;
  /**
   * Resolve with the resources from the first `updated_resources` event (received after this call)
   * that contains a resource matching `predicate`. Rejects on timeout.
   */
  waitForResources: (predicate: (resource: ReceivedResource) => boolean, timeoutMs?: number) => Promise<ReceivedResource[]>;
  close: () => void;
}

function frame(payload: unknown): Buffer {
  const body = Buffer.from(JSON.stringify(payload), 'utf8');
  const header = Buffer.alloc(HEADER_LENGTH);
  VALDI_MAGIC.copy(header, 0);
  header.writeUInt32LE(body.length, 4);
  return Buffer.concat([header, body]);
}

interface ResourceWaiter {
  predicate: (r: ReceivedResource) => boolean;
  resolve: (rs: ReceivedResource[]) => void;
  reject: (error: Error) => void;
  timer: ReturnType<typeof setTimeout>;
}

/** Wire the daemon protocol over an already-connected socket and return the client. */
function createClientOverSocket(socket: Socket): MockAppClient {
  let pending: Buffer = Buffer.alloc(0);
  const configureWaiters = new Map<string, { resolve: () => void; reject: (error: Error) => void }>();
  const resourceWaiters: ResourceWaiter[] = [];

  // Fail (rather than hang) every in-flight waiter, and clear their timers so a failed
  // test doesn't leave a timer pinning the Node event loop. Called on socket drop and close().
  const rejectAllPending = (error: Error) => {
    for (const [, waiter] of configureWaiters) {
      waiter.reject(error);
    }
    configureWaiters.clear();
    for (const waiter of resourceWaiters) {
      clearTimeout(waiter.timer);
      waiter.reject(error);
    }
    resourceWaiters.length = 0;
  };

  const onServerPayload = (payload: ServerPayload) => {
    if (payload.response?.request_id !== undefined) {
      const waiter = configureWaiters.get(payload.response.request_id);
      if (waiter) {
        configureWaiters.delete(payload.response.request_id);
        if (payload.response.error) {
          waiter.reject(new Error(payload.response.error.error_message));
        } else {
          waiter.resolve();
        }
      }
      return;
    }
    const resources = payload.event?.updated_resources?.resources;
    if (resources && resources.length > 0) {
      for (let i = resourceWaiters.length - 1; i >= 0; i--) {
        const waiter = resourceWaiters[i]!;
        if (resources.some(waiter.predicate)) {
          resourceWaiters.splice(i, 1);
          waiter.resolve(resources);
        }
      }
    }
  };

  // A single socket read can carry a partial packet or several packets; buffer until a
  // full [header + payload] is available, then hand off each JSON payload.
  const onData = (chunk: Buffer) => {
    pending = pending.length === 0 ? chunk : Buffer.concat([pending, chunk]);
    while (pending.length >= HEADER_LENGTH) {
      if (!pending.subarray(0, VALDI_MAGIC.length).equals(VALDI_MAGIC)) {
        // Resync by dropping a byte, mirroring the daemon's own discard-on-invalid.
        pending = pending.subarray(1);
        continue;
      }
      const bodyLength = pending.readUInt32LE(4);
      if (pending.length < HEADER_LENGTH + bodyLength) {
        break;
      }
      const body = pending.subarray(HEADER_LENGTH, HEADER_LENGTH + bodyLength);
      pending = pending.subarray(HEADER_LENGTH + bodyLength);
      try {
        onServerPayload(JSON.parse(body.toString('utf8')) as ServerPayload);
      } catch {
        // Ignore anything we can't parse; the smoke only cares about resource events.
      }
    }
  };

  let requestCounter = 0;

  const client: MockAppClient = {
    configure: ({ applicationId = 'valdi-smoke-mock', platform = 'ios' } = {}) =>
      new Promise((resolveConfigure, rejectConfigure) => {
        const requestId = `configure-${++requestCounter}`;
        configureWaiters.set(requestId, { resolve: resolveConfigure, reject: rejectConfigure });
        socket.write(
          frame({ request: { request_id: requestId, configure: { application_id: applicationId, platform } } }),
        );
      }),
    waitForResources: (predicate, timeoutMs = 120_000) =>
      new Promise((resolveResources, rejectResources) => {
        const timer = setTimeout(() => {
          const index = resourceWaiters.findIndex(w => w.resolve === wrappedResolve);
          if (index >= 0) {
            resourceWaiters.splice(index, 1);
          }
          rejectResources(new Error('timed out waiting for a matching updated_resources event'));
        }, timeoutMs);
        const wrappedResolve = (rs: ReceivedResource[]) => {
          clearTimeout(timer);
          resolveResources(rs);
        };
        resourceWaiters.push({ predicate, resolve: wrappedResolve, reject: rejectResources, timer });
      }),
    close: () => {
      rejectAllPending(new Error('mock app client closed'));
      socket.destroy();
    },
  };

  socket.on('data', onData);
  // A drop must fail pending waiters (else configure/waitForResources hang); an attached
  // 'error' handler also stops Node treating it as a fatal unhandled event.
  socket.on('error', err => rejectAllPending(err instanceof Error ? err : new Error(String(err))));
  socket.on('close', () => rejectAllPending(new Error('daemon connection closed')));
  return client;
}

/** Connect to a daemon TCP acceptor (non-USB reloader) and speak the client protocol. */
export function connectMockApp(port: number, host = '127.0.0.1'): Promise<MockAppClient> {
  return new Promise((resolve, reject) => {
    const socket: Socket = connect({ port, host });
    socket.once('error', reject);
    socket.once('connect', () => {
      socket.off('error', reject);
      resolve(createClientOverSocket(socket));
    });
  });
}

/**
 * Mock a simulator/USB device. In --usb mode the reloader daemon does NOT accept
 * connections; its simulator auto-connector dials localhost:<port> (Ports.reloaderStandalone
 * 13591 / reloaderOverUSB 13592) once per second until an app answers. Listen on that port so
 * the daemon connects to us, then speak the same client protocol over the accepted socket.
 */
export function listenMockApp(port: number, host = '127.0.0.1', timeoutMs = 120_000): Promise<MockAppClient> {
  return new Promise((resolve, reject) => {
    const server = createServer();
    const timer = setTimeout(() => {
      server.close();
      reject(new Error(`daemon did not dial the mock on ${host}:${port} within ${timeoutMs}ms`));
    }, timeoutMs);
    server.once('connection', (socket: Socket) => {
      clearTimeout(timer);
      server.close(); // accept only the first connection
      resolve(createClientOverSocket(socket));
    });
    server.once('error', err => {
      clearTimeout(timer);
      reject(err instanceof Error ? err : new Error(String(err)));
    });
    server.listen(port, host);
  });
}
