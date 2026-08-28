import { execFile as execFileCallback } from 'node:child_process';
import { createHash } from 'node:crypto';
import http from 'node:http';
import { promisify } from 'node:util';
import { PLATFORM } from '../core/constants';
import { type DaemonConnectedClient, MOBILE_PORT, type RemoteContext } from '../utils/daemonClient';
import { isLoopbackHost } from '../utils/loopbackHost';

/** String values are part of the debugger's local HTTP wire contract. */
export enum DebuggerTargetPlatform {
  Android = 'android',
  IOS = 'ios',
  MacOS = 'macos',
  Web = 'web',
  Unknown = 'unknown',
}

/** Transports describe how the debugger reaches a target, not its operating system. */
export enum DebuggerTargetTransport {
  ChromiumCDP = 'chromium-cdp',
  ValdiDaemon = 'valdi-daemon',
}

/** Capability names are serialized so frontends can hide unsupported tools. */
export enum DebuggerTargetCapability {
  ComponentPropertyEdit = 'component-property-edit',
  ComponentProperties = 'component-properties',
  Components = 'components',
  Console = 'console',
  Highlight = 'highlight',
  JavaScriptDebugger = 'javascript-debugger',
  Performance = 'performance',
  Snapshot = 'snapshot',
  Storage = 'storage',
}

/** Availability is deliberately separate from merely discovering a device. */
export enum DebuggerTargetState {
  Attached = 'attached',
  Available = 'available',
  Waiting = 'waiting',
}

/** The serialized mode tells clients which complete identity contract a target requires. */
export enum DebuggerTargetIdentityMode {
  InspectedPage = 'inspected-page',
  TargetId = 'target-id',
}

export interface DebuggerDaemonEndpoint {
  readonly deviceId?: string;
  readonly port: number;
}

export interface DebuggerDaemonClient extends DaemonConnectedClient {
  readonly contextError: string | null;
  readonly contexts: readonly RemoteContext[];
}

export interface DebuggerPortStatus {
  readonly clients: readonly DebuggerDaemonClient[];
  readonly connected: boolean;
  readonly deviceId?: string;
  readonly error: string | null;
  readonly port: number;
  readonly portName: string;
}

export interface DebuggerProxyTargetMetadata {
  readonly deviceId?: string;
  readonly deviceName?: string;
}

export interface DebuggerProxyTarget {
  readonly adapterType?: string;
  readonly appId?: string;
  readonly id: string;
  readonly metadata?: DebuggerProxyTargetMetadata;
  readonly title?: string;
  readonly webSocketDebuggerUrl: string;
}

export interface DebuggerTargetDescriptor {
  readonly applicationId: string;
  readonly applicationUrl?: string;
  readonly attachable: boolean;
  readonly capabilities: readonly DebuggerTargetCapability[];
  readonly clientId?: string;
  readonly contextId?: string;
  readonly debuggingPort?: number;
  readonly deviceId?: string;
  readonly id: string;
  readonly identityMode: DebuggerTargetIdentityMode;
  readonly javascriptDebuggerUrl?: string;
  readonly name: string;
  readonly owlTarget?: boolean;
  readonly platform: DebuggerTargetPlatform;
  readonly port?: number;
  readonly proxyPort?: number;
  readonly sessionId?: string;
  readonly state: DebuggerTargetState;
  readonly tracingEnabled?: boolean;
  readonly transport: DebuggerTargetTransport;
}

export interface DebuggerTargetRegistryOptions {
  readonly ports: readonly DebuggerPortStatus[];
  readonly proxyTargets: readonly DebuggerProxyTarget[];
  readonly webPreviewTargets: readonly DebuggerTargetDescriptor[];
}

export interface AndroidDaemonDiscovery {
  readonly endpoints: readonly DebuggerDaemonEndpoint[];
  readonly error: string | null;
}

const execFile = promisify(execFileCallback);
const MAX_ANDROID_DAEMON_ENDPOINTS = 8;
const MAX_ADB_FORWARD_OUTPUT_BYTES = 256 * 1024;
const MAX_PROXY_RESPONSE_BYTES = 512 * 1024;
const MAX_PROXY_TARGETS = 128;
const MAX_PROXY_URL_BYTES = 4096;
const MAX_PROXY_STRING_BYTES = 1024;
const MAX_REGISTRY_PORTS = 10;
const MAX_REGISTRY_TARGETS = 256;
const OPAQUE_TARGET_ID_DOMAIN = 'valdi-debugger-target-v1';

const NATIVE_CAPABILITIES: readonly DebuggerTargetCapability[] = [
  DebuggerTargetCapability.Components,
  DebuggerTargetCapability.Snapshot,
];

function containsControlCharacter(value: string): boolean {
  for (const character of value) {
    const codePoint = character.codePointAt(0);
    if (codePoint !== undefined && (codePoint <= 31 || codePoint === 127)) return true;
  }
  return false;
}

function boundedString(value: unknown, maximumBytes: number): string | undefined {
  if (
    typeof value !== 'string' ||
    value.length === 0 ||
    Buffer.byteLength(value, 'utf8') > maximumBytes ||
    containsControlCharacter(value)
  ) {
    return undefined;
  }
  return value;
}

function assertBoundedIdentityString(value: unknown, name: string, allowEmpty: boolean): asserts value is string {
  if (
    typeof value !== 'string' ||
    (!allowEmpty && value.length === 0) ||
    Buffer.byteLength(value, 'utf8') > MAX_PROXY_STRING_BYTES ||
    containsControlCharacter(value)
  ) {
    throw new Error(`Debugger target discovery returned an invalid ${name}.`);
  }
}

function platformFromValue(value: string): DebuggerTargetPlatform {
  switch (value.toLowerCase()) {
    case PLATFORM.ANDROID: {
      return DebuggerTargetPlatform.Android;
    }
    case PLATFORM.IOS: {
      return DebuggerTargetPlatform.IOS;
    }
    case PLATFORM.MACOS:
    case 'standalone': {
      return DebuggerTargetPlatform.MacOS;
    }
    default: {
      return DebuggerTargetPlatform.Unknown;
    }
  }
}

function opaqueTargetId(kind: string, identity: readonly unknown[]): string {
  const serializedIdentity = JSON.stringify([OPAQUE_TARGET_ID_DOMAIN, kind, identity]);
  return `vdt_${createHash('sha256').update(serializedIdentity).digest('base64url').slice(0, 32)}`;
}

function loopbackWebSocketTransportKey(value: string): string | undefined {
  // ChromiumDevToolsConnection omits URL fragments from the WebSocket transport request.
  // Reject the delimiter itself so named and empty fragments cannot bypass endpoint ownership.
  if (value.includes('#') || Buffer.byteLength(value, 'utf8') > MAX_PROXY_URL_BYTES) return undefined;
  try {
    const parsed = new URL(value);
    if (parsed.protocol !== 'ws:' || !isLoopbackHost(parsed.hostname) || parsed.username || parsed.password) {
      return undefined;
    }
    // ChromiumDevToolsConnection sends pathname + search and ignores an empty query delimiter.
    return `${parsed.protocol}//${parsed.host}${parsed.pathname}${parsed.search}`;
  } catch {
    return undefined;
  }
}

function isLoopbackWebSocketUrl(value: string): boolean {
  return loopbackWebSocketTransportKey(value) !== undefined;
}

/** Parse companion-owned ADB forwards without modifying or replacing any tunnel. */
export function parseAndroidDaemonForwards(output: string): DebuggerDaemonEndpoint[] {
  if (Buffer.byteLength(output, 'utf8') > MAX_ADB_FORWARD_OUTPUT_BYTES) {
    throw new Error('ADB returned an oversized forwarding table.');
  }
  const endpoints: DebuggerDaemonEndpoint[] = [];
  const knownEndpoints = new Set<string>();
  const deviceByPort = new Map<number, string>();
  for (const line of output.split(/\r?\n/)) {
    const [deviceId, local, remote, ...extra] = line.trim().split(/\s+/);
    if (
      !deviceId ||
      !/^[\w.:-]{1,128}$/.test(deviceId) ||
      !local ||
      !remote ||
      extra.length > 0 ||
      remote !== `tcp:${MOBILE_PORT}`
    ) {
      continue;
    }
    const match = /^tcp:(\d+)$/.exec(local);
    if (!match) continue;
    const portText = match[1];
    if (!portText) continue;
    const port = Number.parseInt(portText, 10);
    if (!Number.isSafeInteger(port) || port < 1 || port > 65_535) continue;
    const otherDevice = deviceByPort.get(port);
    if (otherDevice !== undefined && otherDevice !== deviceId) {
      throw new Error('ADB reported an ambiguous local Valdi daemon forwarding port.');
    }
    deviceByPort.set(port, deviceId);
    const key = `${deviceId}\0${port.toString()}`;
    if (knownEndpoints.has(key)) continue;
    if (endpoints.length >= MAX_ANDROID_DAEMON_ENDPOINTS) {
      throw new Error(`ADB reported more than ${MAX_ANDROID_DAEMON_ENDPOINTS.toString()} Valdi daemon forwards.`);
    }
    knownEndpoints.add(key);
    endpoints.push({ deviceId, port });
  }
  return endpoints.sort(
    (left, right) => (left.deviceId ?? '').localeCompare(right.deviceId ?? '') || left.port - right.port,
  );
}

/** Discover existing tunnels. The companion remains their sole lifecycle owner. */
export async function discoverAndroidDaemonEndpoints(): Promise<AndroidDaemonDiscovery> {
  try {
    const { stdout } = await execFile('adb', ['forward', '--list'], {
      maxBuffer: MAX_ADB_FORWARD_OUTPUT_BYTES,
      timeout: 2500,
    });
    return { endpoints: parseAndroidDaemonForwards(stdout), error: null };
  } catch (error) {
    return {
      endpoints: [],
      error: error instanceof Error ? error.message : String(error),
    };
  }
}

function parseDebuggerProxyTarget(candidate: unknown): DebuggerProxyTarget | undefined {
  if (typeof candidate !== 'object' || candidate === null || Array.isArray(candidate)) return undefined;
  const value = candidate as Record<string, unknown>;
  const id = boundedString(value['id'], MAX_PROXY_STRING_BYTES);
  const webSocketDebuggerUrl = boundedString(value['webSocketDebuggerUrl'], MAX_PROXY_URL_BYTES);
  if (!id || !webSocketDebuggerUrl || !isLoopbackWebSocketUrl(webSocketDebuggerUrl)) return undefined;

  const rawMetadata = value['metadata'];
  const metadataValue =
    typeof rawMetadata === 'object' && rawMetadata !== null && !Array.isArray(rawMetadata)
      ? (rawMetadata as Record<string, unknown>)
      : undefined;
  const adapterType = boundedString(value['adapterType'], MAX_PROXY_STRING_BYTES);
  const hasAppId = Object.prototype.hasOwnProperty.call(value, 'appId');
  const rawAppId = hasAppId ? value['appId'] : undefined;
  const appId = boundedString(rawAppId, MAX_PROXY_STRING_BYTES);
  if (hasAppId && (appId === undefined || appId.trim().length === 0)) return undefined;
  const title = boundedString(value['title'], MAX_PROXY_STRING_BYTES);
  const deviceId = boundedString(metadataValue?.['deviceId'], MAX_PROXY_STRING_BYTES);
  const deviceName = boundedString(metadataValue?.['deviceName'], MAX_PROXY_STRING_BYTES);
  return {
    id,
    webSocketDebuggerUrl,
    ...(adapterType === undefined ? {} : { adapterType }),
    ...(appId === undefined ? {} : { appId }),
    ...(title === undefined ? {} : { title }),
    ...(deviceId === undefined && deviceName === undefined
      ? {}
      : {
          metadata: {
            ...(deviceId === undefined ? {} : { deviceId }),
            ...(deviceName === undefined ? {} : { deviceName }),
          },
        }),
  };
}

/** Only consume loopback proxy metadata; never trust arbitrary URLs advertised by a device. */
export async function discoverDebuggerProxyTargets(port: number): Promise<DebuggerProxyTarget[]> {
  return await new Promise((resolve, reject) => {
    let settled = false;
    let body = '';
    let bodyBytes = 0;
    const finishWithError = (error: Error): void => {
      if (settled) return;
      settled = true;
      reject(error);
    };
    const request = http.get(`http://127.0.0.1:${port.toString()}/json/list`, response => {
      response.setEncoding('utf8');
      response.on('data', (chunk: string) => {
        bodyBytes += Buffer.byteLength(chunk, 'utf8');
        if (bodyBytes > MAX_PROXY_RESPONSE_BYTES) {
          request.destroy(new Error('The Valdi debugging proxy returned an oversized target list.'));
          return;
        }
        body += chunk;
      });
      response.once('error', error => finishWithError(error));
      response.once('end', () => {
        if (settled) return;
        if (response.statusCode !== 200) {
          finishWithError(new Error(`The Valdi debugging proxy returned HTTP ${String(response.statusCode)}.`));
          return;
        }
        try {
          const parsed: unknown = JSON.parse(body);
          if (!Array.isArray(parsed)) {
            finishWithError(new Error('The Valdi debugging proxy did not return a target list.'));
            return;
          }
          if (parsed.length > MAX_PROXY_TARGETS) {
            finishWithError(
              new Error(`The Valdi debugging proxy returned more than ${MAX_PROXY_TARGETS.toString()} targets.`),
            );
            return;
          }
          const targets = parsed
            .map(candidate => parseDebuggerProxyTarget(candidate))
            .filter((candidate): candidate is DebuggerProxyTarget => candidate !== undefined);
          settled = true;
          resolve(targets);
        } catch (error) {
          finishWithError(error instanceof Error ? error : new Error('The debugging proxy returned invalid JSON.'));
        }
      });
    });
    request.setTimeout(2500, () => request.destroy(new Error('Timed out discovering Valdi debugging proxy targets.')));
    request.once('error', error => finishWithError(error));
  });
}

function nativeTarget(
  port: DebuggerPortStatus,
  client: DebuggerDaemonClient,
  context: RemoteContext,
): DebuggerTargetDescriptor {
  if (!Number.isSafeInteger(port.port) || port.port < 1 || port.port > 65_535) {
    throw new Error('Debugger target discovery returned an invalid daemon port.');
  }
  if (port.deviceId !== undefined) assertBoundedIdentityString(port.deviceId, 'device ID', false);
  assertBoundedIdentityString(client.client_id, 'client ID', false);
  assertBoundedIdentityString(client.application_id, 'application ID', false);
  assertBoundedIdentityString(client.platform, 'platform', true);
  assertBoundedIdentityString(context.id, 'context ID', false);
  assertBoundedIdentityString(context.rootComponentName, 'root component name', true);
  const platform = platformFromValue(client.platform || port.portName);
  return {
    applicationId: client.application_id,
    attachable: true,
    capabilities: NATIVE_CAPABILITIES,
    clientId: client.client_id,
    contextId: context.id,
    ...(port.deviceId === undefined ? {} : { deviceId: port.deviceId }),
    id: opaqueTargetId('valdi-daemon', [
      platform,
      client.platform,
      port.portName,
      port.deviceId ?? null,
      port.port,
      client.client_id,
      client.application_id,
      context.id,
    ]),
    identityMode: DebuggerTargetIdentityMode.TargetId,
    name: context.rootComponentName || client.application_id || `Client ${client.client_id}`,
    platform,
    port: port.port,
    proxyPort: port.port,
    state: DebuggerTargetState.Available,
    transport: DebuggerTargetTransport.ValdiDaemon,
  };
}

function proxyTargetPlatform(target: DebuggerProxyTarget): DebuggerTargetPlatform {
  const deviceId = target.metadata?.deviceId;
  if (deviceId === undefined) return DebuggerTargetPlatform.Unknown;
  assertBoundedIdentityString(deviceId, 'proxy device ID', false);

  const adapterType = target.adapterType;
  if (adapterType === 'android' || adapterType === '_android' || adapterType === `_android_${deviceId}`) {
    return DebuggerTargetPlatform.Android;
  }
  if (adapterType === '_ios' || adapterType === `_ios_${deviceId}`) {
    return DebuggerTargetPlatform.IOS;
  }
  return DebuggerTargetPlatform.Unknown;
}

function waitingProxyTarget(target: DebuggerProxyTarget, platform: DebuggerTargetPlatform): DebuggerTargetDescriptor {
  const deviceId = target.metadata?.deviceId;
  const applicationId = target.appId ?? deviceId ?? 'unknown';
  return {
    applicationId,
    attachable: false,
    capabilities: [DebuggerTargetCapability.JavaScriptDebugger],
    ...(deviceId === undefined ? {} : { deviceId }),
    id: opaqueTargetId('javascript-proxy', [
      platform,
      target.adapterType ?? null,
      target.id,
      target.appId ?? null,
      deviceId ?? null,
      target.webSocketDebuggerUrl,
    ]),
    identityMode: DebuggerTargetIdentityMode.TargetId,
    javascriptDebuggerUrl: target.webSocketDebuggerUrl,
    name: target.title ?? target.metadata?.deviceName ?? deviceId ?? target.appId ?? 'JavaScript runtime',
    platform,
    state: DebuggerTargetState.Waiting,
    transport: DebuggerTargetTransport.ChromiumCDP,
  };
}

function assertUniqueTargetIds(targets: readonly DebuggerTargetDescriptor[]): void {
  const ids = new Set<string>();
  for (const target of targets) {
    if (ids.has(target.id)) {
      throw new Error('Debugger target discovery produced an ambiguous target identity.');
    }
    ids.add(target.id);
  }
}

/** Merge independently discovered native, explicit web-preview, and JS-debugger targets. */
export function buildDebuggerTargetRegistry(options: DebuggerTargetRegistryOptions): DebuggerTargetDescriptor[] {
  if (options.ports.length > MAX_REGISTRY_PORTS) {
    throw new Error(`Debugger target discovery exceeded ${MAX_REGISTRY_PORTS.toString()} daemon endpoints.`);
  }
  if (options.proxyTargets.length > MAX_PROXY_TARGETS || options.webPreviewTargets.length > 1) {
    throw new Error('Debugger target discovery exceeded its bounded target sources.');
  }
  const targets: DebuggerTargetDescriptor[] = [];
  for (const port of options.ports) {
    if (!port.connected) continue;
    for (const client of port.clients) {
      for (const context of client.contexts) {
        if (targets.length >= MAX_REGISTRY_TARGETS) {
          throw new Error(`Debugger target discovery exceeded ${MAX_REGISTRY_TARGETS.toString()} targets.`);
        }
        targets.push(nativeTarget(port, client, context));
      }
    }
  }

  if (targets.length + options.webPreviewTargets.length > MAX_REGISTRY_TARGETS) {
    throw new Error(`Debugger target discovery exceeded ${MAX_REGISTRY_TARGETS.toString()} targets.`);
  }
  targets.push(...options.webPreviewTargets);

  const claimedProxyTransportKeys = new Set<string>();
  for (const proxyTarget of options.proxyTargets) {
    const proxyTransportKey = loopbackWebSocketTransportKey(proxyTarget.webSocketDebuggerUrl);
    if (proxyTransportKey === undefined) {
      throw new Error('Debugger proxy discovery returned a non-loopback target URL.');
    }
    if (claimedProxyTransportKeys.has(proxyTransportKey)) {
      throw new Error('Debugger proxy discovery produced duplicate JavaScript transport endpoint ownership.');
    }
    claimedProxyTransportKeys.add(proxyTransportKey);
    const platform = proxyTargetPlatform(proxyTarget);
    if (platform === DebuggerTargetPlatform.Unknown) continue;
    const deviceId = proxyTarget.metadata?.deviceId;
    const deviceScopedAndroidPlaceholder =
      platform === DebuggerTargetPlatform.Android &&
      deviceId !== undefined &&
      proxyTarget.adapterType === `_android_${deviceId}` &&
      proxyTarget.appId !== undefined &&
      proxyTarget.appId === deviceId;
    const matchingNativeTargets = targets.filter(candidate => {
      if (candidate.transport !== DebuggerTargetTransport.ValdiDaemon || deviceId === undefined) return false;
      if (candidate.deviceId !== deviceId || candidate.platform !== platform) return false;
      if (deviceScopedAndroidPlaceholder) return true;
      return proxyTarget.appId === undefined || candidate.applicationId === proxyTarget.appId;
    });
    if (matchingNativeTargets.length === 1) {
      const existing = matchingNativeTargets[0];
      if (!existing) throw new Error('Debugger proxy discovery lost a unique native target match.');
      if (existing.capabilities.includes(DebuggerTargetCapability.JavaScriptDebugger)) {
        throw new Error('Debugger proxy discovery produced an ambiguous JavaScript target identity.');
      }
      const index = targets.indexOf(existing);
      targets[index] = {
        ...existing,
        capabilities: [...existing.capabilities, DebuggerTargetCapability.JavaScriptDebugger],
        javascriptDebuggerUrl: proxyTarget.webSocketDebuggerUrl,
      };
      continue;
    }
    if (targets.length >= MAX_REGISTRY_TARGETS) {
      throw new Error(`Debugger target discovery exceeded ${MAX_REGISTRY_TARGETS.toString()} targets.`);
    }
    targets.push(waitingProxyTarget(proxyTarget, platform));
  }

  assertUniqueTargetIds(targets);
  return targets.sort(
    (left, right) => left.platform.localeCompare(right.platform) || left.name.localeCompare(right.name),
  );
}
