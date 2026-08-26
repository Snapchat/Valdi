import http from 'node:http';
import { TextDecoder } from 'node:util';
import { ChromiumDevToolsConnection, type ChromiumDevToolsEvent } from './chromiumDevToolsClient';
import { isLoopbackHost } from './loopbackHost';

const CHROMIUM_DISCOVERY_TIMEOUT_MS = 3000;
const CHROMIUM_COMMAND_TIMEOUT_MS = 8000;
const MAX_CHROMIUM_DISCOVERY_BYTES = 1024 * 1024;
const MAX_CHROMIUM_DISCOVERY_TARGETS = 256;
const VALDI_INJECTED_QUERY_PARAMETERS = ['valdiDebugger', 'valdiDevTools'];
const FATAL_UTF8_DECODER = new TextDecoder('utf8', { fatal: true });
const GUARDED_TARGET_MATCH_PROPERTY = '__valdiDevToolsTargetMatched';
export const OWL_DEVTOOLS_TARGET_NONCE_PROPERTY = '__VALDI_DEVTOOLS_TARGET_NONCE__';

export interface OwlChromiumTarget {
  id: string;
  title: string;
  type: string;
  url: string;
  webSocketDebuggerUrl: string;
}

interface RuntimeEvaluation {
  exceptionDetails?: {
    text?: string;
  };
  result?: {
    description?: string;
    type?: string;
    value?: unknown;
  };
}

function chromiumError(error: unknown, fallback: string): Error {
  if (error instanceof Error) return error;
  if (typeof error === 'string' && error) return new Error(error);
  return new Error(fallback);
}

function isWebSocketForDiscoveryPort(value: string, port: number): boolean {
  try {
    const url = new URL(value);
    return (
      url.protocol === 'ws:' &&
      isLoopbackHost(url.hostname) &&
      !url.username &&
      !url.password &&
      Number.parseInt(url.port || '80', 10) === port
    );
  } catch {
    return false;
  }
}

function canonicalApplicationUrl(value: string, removeInjectedParameters: boolean): string {
  const url = new URL(value);
  if (removeInjectedParameters) {
    for (const parameter of VALDI_INJECTED_QUERY_PARAMETERS) url.searchParams.delete(parameter);
  }
  const entries = Array.from(url.searchParams.entries()).sort(([firstName], [secondName]) => {
    if (firstName !== secondName) return firstName < secondName ? -1 : 1;
    return 0;
  });
  url.search = '';
  for (const [name, value] of entries) url.searchParams.append(name, value);
  return `${url.origin}${url.pathname}${url.search}`;
}

export function matchesOwlApplicationUrl(inspectedUrl: string, applicationUrl: string): boolean {
  try {
    return canonicalApplicationUrl(inspectedUrl, true) === canonicalApplicationUrl(applicationUrl, false);
  } catch (error) {
    console.warn('[Valdi Owl] Ignoring invalid Chromium target URL.', error);
    return false;
  }
}

export async function listOwlChromiumTargets(port: number): Promise<OwlChromiumTarget[]> {
  return await new Promise<OwlChromiumTarget[]>((resolve, reject) => {
    let settled = false;
    let activeResponse: http.IncomingMessage | undefined;
    function fail(error: Error): void {
      if (settled) return;
      settled = true;
      clearTimeout(deadline);
      activeResponse?.destroy();
      request.destroy();
      reject(error);
    }

    function succeed(targets: OwlChromiumTarget[]): void {
      if (settled) return;
      settled = true;
      clearTimeout(deadline);
      resolve(targets);
    }

    const request = http.get(`http://127.0.0.1:${port}/json/list`, response => {
      activeResponse = response;
      if (response.statusCode !== 200) {
        fail(new Error(`Owl Chromium discovery returned HTTP ${String(response.statusCode)}.`));
        return;
      }

      const chunks: Buffer[] = [];
      let bodyBytes = 0;
      response.on('data', (chunk: Buffer) => {
        const buffer = Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk);
        const chunkBytes = buffer.length;
        if (chunkBytes > MAX_CHROMIUM_DISCOVERY_BYTES - bodyBytes) {
          fail(new Error(`Owl Chromium discovery exceeded the ${MAX_CHROMIUM_DISCOVERY_BYTES} byte response limit.`));
          return;
        }
        bodyBytes += chunkBytes;
        chunks.push(buffer);
      });
      response.once('error', error => fail(error));
      response.once('end', () => {
        if (settled) return;
        let body: string;
        try {
          body = FATAL_UTF8_DECODER.decode(Buffer.concat(chunks, bodyBytes));
        } catch {
          fail(new Error('Owl Chromium discovery returned malformed UTF-8.'));
          return;
        }
        try {
          const parsed: unknown = JSON.parse(body);
          if (!Array.isArray(parsed)) {
            fail(new Error('Owl Chromium discovery did not return a target list.'));
            return;
          }
          if (parsed.length > MAX_CHROMIUM_DISCOVERY_TARGETS) {
            fail(new Error(`Owl Chromium discovery returned more than ${MAX_CHROMIUM_DISCOVERY_TARGETS} targets.`));
            return;
          }
          const targets = parsed.filter((item): item is OwlChromiumTarget => {
            if (typeof item !== 'object' || item === null) return false;
            const target = item as Partial<OwlChromiumTarget>;
            return (
              typeof target.id === 'string' &&
              typeof target.title === 'string' &&
              typeof target.type === 'string' &&
              typeof target.url === 'string' &&
              typeof target.webSocketDebuggerUrl === 'string' &&
              isWebSocketForDiscoveryPort(target.webSocketDebuggerUrl, port)
            );
          });
          succeed(targets);
        } catch (error) {
          fail(chromiumError(error, 'Owl Chromium discovery returned invalid JSON.'));
        }
      });
    });

    const deadline = setTimeout(() => {
      fail(new Error(`Timed out discovering Owl Chromium targets on port ${port}.`));
    }, CHROMIUM_DISCOVERY_TIMEOUT_MS);
    request.once('error', error => fail(error));
  });
}

interface GuardedOwlEvaluation {
  matched: boolean;
  value?: unknown;
}

function guardedOwlExpression(applicationUrl: string, targetNonce: string, expression: string): string {
  const expectedUrl = canonicalApplicationUrl(applicationUrl, false);
  return `(async () => {
    const currentUrl = new URL(globalThis.location.href);
    for (const parameter of ${JSON.stringify(VALDI_INJECTED_QUERY_PARAMETERS)}) currentUrl.searchParams.delete(parameter);
    const entries = Array.from(currentUrl.searchParams.entries()).sort(([firstName], [secondName]) =>
      firstName === secondName ? 0 : firstName < secondName ? -1 : 1
    );
    currentUrl.search = '';
    for (const [name, value] of entries) currentUrl.searchParams.append(name, value);
    const canonicalUrl = currentUrl.origin + currentUrl.pathname + currentUrl.search;
    if (
      globalThis[${JSON.stringify(OWL_DEVTOOLS_TARGET_NONCE_PROPERTY)}] !== ${JSON.stringify(targetNonce)} ||
      canonicalUrl !== ${JSON.stringify(expectedUrl)}
    ) {
      return { ${JSON.stringify(GUARDED_TARGET_MATCH_PROPERTY)}: false };
    }
    return {
      ${JSON.stringify(GUARDED_TARGET_MATCH_PROPERTY)}: true,
      value: await (${expression}),
    };
  })()`;
}

async function evaluateGuardedOwlExpression(
  connection: OwlChromiumConnection,
  applicationUrl: string,
  targetNonce: string,
  expression: string,
): Promise<GuardedOwlEvaluation> {
  const result = await connection.evaluate(guardedOwlExpression(applicationUrl, targetNonce, expression));
  if (typeof result !== 'object' || result === null || Array.isArray(result)) {
    throw new Error('The running Owl page returned an invalid guarded evaluation result.');
  }
  const record = result as Record<string, unknown>;
  if (record[GUARDED_TARGET_MATCH_PROPERTY] === false) return { matched: false };
  if (record[GUARDED_TARGET_MATCH_PROPERTY] !== true) {
    throw new Error('The running Owl page omitted its guarded target identity.');
  }
  return { matched: true, value: record['value'] };
}

export class OwlChromiumConnection {
  private constructor(private readonly connection: ChromiumDevToolsConnection) {}

  static async connect(webSocketUrl: string): Promise<OwlChromiumConnection> {
    const parsed = new URL(webSocketUrl);
    if (parsed.protocol !== 'ws:' || !isLoopbackHost(parsed.hostname) || parsed.username || parsed.password) {
      throw new Error('Owl Chromium inspection only allows unauthenticated loopback WebSocket targets.');
    }

    return new OwlChromiumConnection(
      await ChromiumDevToolsConnection.connect(webSocketUrl, CHROMIUM_DISCOVERY_TIMEOUT_MS),
    );
  }

  async evaluate(expression: string): Promise<unknown> {
    const result = (await this.connection.call(
      'Runtime.evaluate',
      {
        awaitPromise: true,
        expression,
        returnByValue: true,
      },
      CHROMIUM_COMMAND_TIMEOUT_MS,
    )) as RuntimeEvaluation;
    if (result.exceptionDetails) {
      throw new Error(result.exceptionDetails.text ?? 'Owl Chromium could not evaluate the Valdi inspection request.');
    }
    if (result.result?.type === 'undefined') {
      throw new Error('The running Owl page has not registered the Valdi inspection bridge.');
    }
    return result.result?.value;
  }

  async call(method: string, params: Record<string, unknown>, timeoutMs: number): Promise<unknown> {
    return await this.connection.call(method, params, timeoutMs);
  }

  async matchesTarget(applicationUrl: string, targetNonce: string): Promise<boolean> {
    const evaluation = await evaluateGuardedOwlExpression(this, applicationUrl, targetNonce, 'true');
    return evaluation.matched;
  }

  onClose(listener: (error: Error) => void): () => void {
    return this.connection.onClose(listener);
  }

  onEvent(listener: (event: ChromiumDevToolsEvent) => void): () => void {
    return this.connection.onEvent(listener);
  }

  close(): void {
    this.connection.close();
  }
}

export async function connectToOwlApplication(
  port: number,
  applicationUrl: string,
  targetNonce: string,
): Promise<OwlChromiumConnection> {
  const targets = await listOwlChromiumTargets(port);
  const candidates = targets.filter(
    candidate => candidate.type === 'page' && matchesOwlApplicationUrl(candidate.url, applicationUrl),
  );
  if (candidates.length === 0) {
    throw new Error('No running Owl Chromium page matches this Valdi application.');
  }

  let lastProbeError: Error | null = null;
  for (const candidate of candidates) {
    let connection: OwlChromiumConnection | null = null;
    let matched = false;
    try {
      connection = await OwlChromiumConnection.connect(candidate.webSocketDebuggerUrl);
      const probe = await evaluateGuardedOwlExpression(connection, applicationUrl, targetNonce, 'true');
      if (!probe.matched) continue;
      matched = true;
      return connection;
    } catch (error) {
      lastProbeError = chromiumError(error, 'Could not inspect the candidate Owl Chromium page.');
    } finally {
      if (connection && !matched) connection.close();
    }
  }

  if (candidates.length === 1 && lastProbeError) throw lastProbeError;
  throw new Error('No running Owl Chromium page matches the exact inspected DevTools tab.');
}

async function evaluateOnOwlApplication(
  port: number,
  applicationUrl: string,
  targetNonce: string,
  expression: string,
): Promise<unknown> {
  const connection = await connectToOwlApplication(port, applicationUrl, targetNonce);
  try {
    const evaluation = await evaluateGuardedOwlExpression(connection, applicationUrl, targetNonce, expression);
    if (evaluation.matched) return evaluation.value;
    throw new Error('The inspected Owl page changed while the debugger request was running.');
  } finally {
    connection.close();
  }
}

export async function readOwlDebuggerSnapshot(
  port: number,
  applicationUrl: string,
  targetNonce: string,
): Promise<Record<string, unknown>> {
  const snapshot = await evaluateOwlApplicationExpression(
    port,
    applicationUrl,
    targetNonce,
    'globalThis.__VALDI_WEB_DEBUGGER__?.getSnapshot()',
  );
  if (typeof snapshot !== 'object' || snapshot === null || Array.isArray(snapshot)) {
    throw new Error('The running Owl page returned an invalid Valdi debug snapshot.');
  }
  return snapshot as Record<string, unknown>;
}

export async function evaluateOwlApplicationExpression(
  port: number,
  applicationUrl: string,
  targetNonce: string,
  expression: string,
): Promise<unknown> {
  return await evaluateOnOwlApplication(port, applicationUrl, targetNonce, expression);
}
