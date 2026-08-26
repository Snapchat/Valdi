/**
 * Hermes debug WebSocket client — connects to the Hermes JS runtime's debug server
 * and implements the Chrome DevTools Protocol (CDP) Profiler domain.
 *
 * Architecture (confirmed from source):
 *   The Hermes runtime opens a WebSocket server on a random port. The hot-reloader
 *   discovers that port via the daemon protocol and sets up:
 *     adb forward tcp:13595 tcp:<random>
 *   so that external tools can connect via a stable port.
 *
 * Connection flow:
 *   1. HTTP GET http://localhost:<port>/json → [{id, webSocketDebuggerUrl, ...}]
 *   2. Connect via WebSocket to ws://localhost:<port>/<id>
 *   3. Standard CDP Profiler.* calls work on that connection.
 *
 * Requires the hot-reloader to be running (`valdi hotreload android|ios`) so that
 * the adb port forward to the Hermes debug socket is established.
 */

import * as http from 'http';
import { TextDecoder } from 'node:util';
import { CliError } from '../core/errors';
import { ChromiumDevToolsConnection } from './chromiumDevToolsClient';

// ─── Ports ───────────────────────────────────────────────────────────────────

export const HERMES_PORT = 13_595;
const MAX_HERMES_DISCOVERY_BYTES = 1024 * 1024;
const FATAL_UTF8_DECODER = new TextDecoder('utf8', { fatal: true });

// ─── Types ───────────────────────────────────────────────────────────────────

export interface HermesDebuggableDevice {
  id: string;
  title: string;
  webSocketDebuggerUrl: string;
}

export interface CpuProfile {
  nodes: CpuProfileNode[];
  startTime: number;
  endTime: number;
  samples?: number[];
  timeDeltas?: number[];
}

export interface CpuProfileNode {
  id: number;
  callFrame: {
    functionName: string;
    scriptId: string;
    url: string;
    lineNumber: number;
    columnNumber: number;
  };
  hitCount?: number;
  children?: number[];
}

// ─── Errors ──────────────────────────────────────────────────────────────────

export const HERMES_BUILD_FLAG = '--@valdi//bzl/valdi:js_engine=hermes --@valdi//bzl/valdi:js_bytecode_format=hermes';

/**
 * Thrown when the Hermes debug socket is not available — either the app was
 * built with a different JS engine (QuickJS is the default), the Hermes
 * debugger is not enabled, or the hot-reloader is not running.
 */
export class NotHermesError extends CliError {
  constructor(port: number) {
    super(
      `Hermes debug socket not found on port ${port}.\n\n` +
      `CPU profiling requires:\n` +
      `  1. A Hermes build (the default JS engine is QuickJS):\n` +
      `       valdi install android --bazel_args="${HERMES_BUILD_FLAG}"\n` +
      `       valdi install ios     --bazel_args="${HERMES_BUILD_FLAG}"\n\n` +
      `  2. The hot-reloader running (it establishes the debug tunnel on port ${port}):\n` +
      `       valdi hotreload android\n`,
    );
  }
}

// ─── HTTP /json endpoint ──────────────────────────────────────────────────────

export async function listHermesDevices(port: number): Promise<HermesDebuggableDevice[]> {
  // Do NOT run adb forward here — the Companion manages the port-13595 tunnel.
  // Running adb forward would overwrite the Companion's mapping to the random Hermes port.
  return new Promise((resolve, reject) => {
    let settled = false;
    function fail(error: Error): void {
      if (settled) return;
      settled = true;
      reject(error);
    }

    const timer = setTimeout(() => {
      req.destroy();
      fail(new NotHermesError(port));
    }, 3000);

    const req = http.get(`http://127.0.0.1:${port}/json`, (res) => {
      const chunks: Buffer[] = [];
      let bodyBytes = 0;
      res.on('data', (chunk: Buffer) => {
        const buffer = Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk);
        if (buffer.length > MAX_HERMES_DISCOVERY_BYTES - bodyBytes) {
          clearTimeout(timer);
          fail(new CliError(`Hermes discovery exceeded the ${MAX_HERMES_DISCOVERY_BYTES} byte response limit.`));
          res.destroy();
          req.destroy();
          return;
        }
        bodyBytes += buffer.length;
        chunks.push(buffer);
      });
      res.once('error', error => {
        clearTimeout(timer);
        fail(error);
      });
      res.once('end', () => {
        if (settled) return;
        clearTimeout(timer);
        let body: string;
        try {
          body = FATAL_UTF8_DECODER.decode(Buffer.concat(chunks, bodyBytes));
        } catch {
          fail(new CliError(`Hermes debug server on port ${port} returned malformed UTF-8`));
          return;
        }
        try {
          const devices = JSON.parse(body) as HermesDebuggableDevice[];
          settled = true;
          resolve(devices);
        } catch {
          fail(new CliError(`Invalid response from Hermes debug server on port ${port}`));
        }
      });
    });

    req.on('error', (err: NodeJS.ErrnoException) => {
      clearTimeout(timer);
      if (err.code === 'ECONNREFUSED') {
        fail(new NotHermesError(port));
      } else {
        fail(err);
      }
    });
  });
}

// ─── Profile normalisation ────────────────────────────────────────────────────

/**
 * Hermes emits a quirky cpuprofile layout that confuses viewers like Speedscope
 * and Chrome DevTools:
 *
 *   startTime: 0
 *   timeDeltas: [0, <huge monotonic-clock offset>, <real deltas …>]
 *   samples:    [1 (root), 1 (root), <real samples …>]
 *
 * The second timeDelta is the absolute monotonic clock value at the moment
 * profiling started (µs since device boot), which makes the timeline appear
 * to span days. Fix: drop the two synthetic leading entries and set
 * `startTime` to the actual wall-clock origin so the timeline reads correctly.
 */
export function normalizeHermesProfile(profile: CpuProfile): CpuProfile {
  const { samples = [], timeDeltas = [], startTime, endTime, nodes } = profile;
  if (samples.length < 2 || timeDeltas.length < 2) return profile;

  // Compute absolute sample times
  const absTimes: number[] = [];
  let t = startTime;
  for (const d of timeDeltas) {
    t += d;
    absTimes.push(t);
  }

  // absTimes[0] = 0 (fake start), absTimes[1] = real monotonic origin
  const realStart = absTimes[1]!;
  const realAbsTimes = absTimes.slice(1);
  const realSamples = samples.slice(1);

  const newDeltas: number[] = [0];
  for (let i = 1; i < realAbsTimes.length; i++) {
    newDeltas.push(realAbsTimes[i]! - realAbsTimes[i - 1]!);
  }

  return { nodes, startTime: realStart, endTime, samples: realSamples, timeDeltas: newDeltas };
}

// ─── HermesConnection ────────────────────────────────────────────────────────

export class HermesConnection {
  private constructor(private readonly connection: ChromiumDevToolsConnection) {}

  private static encodeDeviceId(deviceId: string): string {
    if (!deviceId || deviceId === '.' || deviceId === '..') {
      throw new CliError('The Hermes debugger device id must be a non-empty WebSocket path segment.');
    }
    try {
      return encodeURIComponent(deviceId);
    } catch {
      throw new CliError('The Hermes debugger device id contains invalid Unicode.');
    }
  }

  /**
   * Connect to the Hermes debug WebSocket for the given device context ID.
   * The path on the WebSocket server is `/<deviceId>`.
   */
  static async connect(port: number, deviceId: string): Promise<HermesConnection> {
    const encodedDeviceId = HermesConnection.encodeDeviceId(deviceId);
    try {
      const connection = await ChromiumDevToolsConnection.connect(
        `ws://127.0.0.1:${port}/${encodedDeviceId}`,
        5000,
      );
      return new HermesConnection(connection);
    } catch (error) {
      if ((error as NodeJS.ErrnoException).code === 'ECONNREFUSED') {
        throw new NotHermesError(port);
      }
      if (error instanceof Error && error.message === 'Timed out connecting to the Chromium DevTools target.') {
        throw new CliError(
          `Timeout connecting to Hermes debug socket on port ${port}. ` +
          'Make sure the Valdi app is running and the hot-reloader is active.',
        );
      }
      throw error;
    }
  }

  // ── Raw CDP call ──────────────────────────────────────────────────────────

  call(method: string, params: object = {}, timeoutMs = 10_000): Promise<unknown> {
    return this.connection.call(method, params as Record<string, unknown>, timeoutMs);
  }

  // ── Profiler ──────────────────────────────────────────────────────────────

  async startProfiling(): Promise<void> {
    await this.call('Profiler.start');
  }

  async stopProfiling(): Promise<CpuProfile> {
    const result = (await this.call('Profiler.stop', {}, 60_000)) as { profile: CpuProfile };
    return normalizeHermesProfile(result.profile);
  }

  // ── Lifecycle ─────────────────────────────────────────────────────────────

  close(): void {
    this.connection.close();
  }
}
