import 'jasmine';
import http from 'node:http';
import { PLATFORM } from '../core/constants';
import { MOBILE_PORT } from '../utils/daemonClient';
import {
  DebuggerTargetCapability,
  type DebuggerTargetDescriptor,
  DebuggerTargetIdentityMode,
  DebuggerTargetPlatform,
  DebuggerTargetState,
  DebuggerTargetTransport,
  buildDebuggerTargetRegistry,
  discoverDebuggerProxyTargets,
  parseAndroidDaemonForwards,
} from './targetRegistry';

function nativePort(applicationId = 'com.example.android') {
  return {
    clients: [
      {
        application_id: applicationId,
        client_id: 'client-1',
        contextError: null,
        contexts: [{ id: 'context-1', rootComponentName: 'Conversation' }],
        platform: PLATFORM.ANDROID,
      },
    ],
    connected: true,
    deviceId: 'emulator-5554',
    error: null,
    port: 51_001,
    portName: 'mobile',
  };
}

function proxyTarget(adapterType: string, deviceId: string | null) {
  return {
    adapterType,
    appId: 'com.example.android',
    id: adapterType,
    ...(deviceId === null ? {} : { metadata: { deviceId } }),
    webSocketDebuggerUrl: `ws://127.0.0.1:9010/${encodeURIComponent(adapterType)}`,
  };
}

function webPreviewTarget(): DebuggerTargetDescriptor {
  return {
    applicationId: 'http://127.0.0.1:54321/index.html',
    applicationUrl: 'http://127.0.0.1:54321/index.html',
    attachable: true,
    capabilities: [
      DebuggerTargetCapability.Components,
      DebuggerTargetCapability.ComponentProperties,
      DebuggerTargetCapability.Snapshot,
      DebuggerTargetCapability.Console,
    ],
    debuggingPort: 9222,
    id: 'owl:web-preview',
    identityMode: DebuggerTargetIdentityMode.InspectedPage,
    name: 'index.html',
    owlTarget: true,
    platform: DebuggerTargetPlatform.Web,
    sessionId: 'web-preview',
    state: DebuggerTargetState.Available,
    transport: DebuggerTargetTransport.ChromiumCDP,
  };
}

async function serveProxyTargets(payload: unknown): Promise<{ port: number; close(): Promise<void> }> {
  const server = http.createServer((_request, response) => {
    response.writeHead(200, { 'Content-Type': 'application/json' });
    response.end(JSON.stringify(payload));
  });
  await new Promise<void>((resolve, reject) => {
    server.once('error', reject);
    server.listen(0, '127.0.0.1', resolve);
  });
  const address = server.address();
  if (typeof address !== 'object' || address === null) throw new Error('Mock debugging proxy did not bind.');
  return {
    close: async () => {
      await new Promise<void>((resolve, reject) => {
        server.close(error => (error ? reject(error) : resolve()));
      });
    },
    port: address.port,
  };
}

describe('debugger target registry', () => {
  it('discovers bounded companion-owned Android daemon tunnels without selecting JS debugger forwards', () => {
    const output = [
      'emulator-5554 tcp:51001 tcp:13592',
      'emulator-5554 tcp:51002 tcp:13594',
      'phone-123 tcp:52001 tcp:13592',
      'phone-123 tcp:52001 tcp:13592',
      'invalid tcp:99999 tcp:13592',
      'emulator-5554 localabstract:valdi tcp:13592',
    ].join('\n');

    expect(parseAndroidDaemonForwards(output)).toEqual([
      { deviceId: 'emulator-5554', port: 51_001 },
      { deviceId: 'phone-123', port: 52_001 },
    ]);
    const tooMany = Array.from(
      { length: 9 },
      (_, index) => `device-${index.toString()} tcp:${(51_000 + index).toString()} tcp:${MOBILE_PORT.toString()}`,
    ).join('\n');
    expect(() => parseAndroidDaemonForwards(tooMany)).toThrowError(/more than 8/);
    expect(() => parseAndroidDaemonForwards('device-a tcp:51001 tcp:13592\ndevice-b tcp:51001 tcp:13592')).toThrowError(
      /ambiguous local/,
    );
  });

  it('uses deterministic opaque IDs over the complete native runtime identity', () => {
    const options = { ports: [nativePort()], proxyTargets: [], webPreviewTargets: [webPreviewTarget()] };
    const first = buildDebuggerTargetRegistry(options);
    const second = buildDebuggerTargetRegistry(options);
    const native = first.find(target => target.transport === DebuggerTargetTransport.ValdiDaemon);
    const replacement = buildDebuggerTargetRegistry({
      ...options,
      ports: [nativePort('com.example.replacement')],
    }).find(target => target.transport === DebuggerTargetTransport.ValdiDaemon);
    if (!native || !replacement) throw new Error('Expected native debugger targets.');

    expect(native.id).toMatch(/^vdt_[\w-]{32}$/);
    expect(native.id).not.toContain('51001');
    expect(native.id).not.toContain('client-1');
    expect(native.id).not.toContain('context-1');
    expect(native.id).not.toContain('emulator-5554');
    expect(second.find(target => target.transport === DebuggerTargetTransport.ValdiDaemon)?.id).toBe(native.id);
    expect(replacement.id).not.toBe(native.id);
    expect(native.capabilities).toEqual([DebuggerTargetCapability.Components, DebuggerTargetCapability.Snapshot]);
    expect(native.capabilities).not.toContain(DebuggerTargetCapability.ComponentProperties);
    expect(native.identityMode).toBe(DebuggerTargetIdentityMode.TargetId);
    expect(first).toContain(
      jasmine.objectContaining({
        capabilities: jasmine.arrayContaining([DebuggerTargetCapability.ComponentProperties]),
        id: 'owl:web-preview',
        identityMode: DebuggerTargetIdentityMode.InspectedPage,
      }),
    );
  });

  it('fails closed when independent discovery inputs produce the same target identity', () => {
    expect(() =>
      buildDebuggerTargetRegistry({
        ports: [nativePort(), nativePort()],
        proxyTargets: [],
        webPreviewTargets: [],
      }),
    ).toThrowError(/ambiguous target identity/);
  });

  it('merges a unique live JavaScript proxy without changing native attachability', () => {
    const targets = buildDebuggerTargetRegistry({
      ports: [nativePort()],
      proxyTargets: [
        {
          adapterType: '_android_emulator-5554',
          appId: 'com.example.android',
          id: 'proxy-1',
          metadata: { deviceId: 'emulator-5554' },
          webSocketDebuggerUrl: 'ws://127.0.0.1:9010/android_emulator-5554/1',
        },
      ],
      webPreviewTargets: [],
    });

    expect(targets).toEqual([
      jasmine.objectContaining({
        attachable: true,
        capabilities: [
          DebuggerTargetCapability.Components,
          DebuggerTargetCapability.Snapshot,
          DebuggerTargetCapability.JavaScriptDebugger,
        ],
        javascriptDebuggerUrl: 'ws://127.0.0.1:9010/android_emulator-5554/1',
        transport: DebuggerTargetTransport.ValdiDaemon,
      }),
    ]);
  });

  it('fails closed when differently attributed proxy records claim one canonical transport endpoint', () => {
    const port = nativePort();
    port.clients.push({
      application_id: 'com.example.second',
      client_id: 'client-2',
      contextError: null,
      contexts: [{ id: 'context-2', rootComponentName: 'Second' }],
      platform: PLATFORM.ANDROID,
    });
    const proxyTarget = {
      adapterType: '_android_emulator-5554',
      metadata: { deviceId: 'emulator-5554' },
    };
    const buildWithUrls = (firstUrl: string, secondUrl: string) =>
      buildDebuggerTargetRegistry({
        ports: [port],
        proxyTargets: [
          {
            ...proxyTarget,
            appId: 'com.example.android',
            id: 'proxy-1',
            webSocketDebuggerUrl: firstUrl,
          },
          {
            ...proxyTarget,
            appId: 'com.example.second',
            id: 'proxy-2',
            webSocketDebuggerUrl: secondUrl,
          },
        ],
        webPreviewTargets: [],
      });

    const sharedUrl = 'ws://127.0.0.1:9010/devtools/runtime';
    expect(() => buildWithUrls(sharedUrl, sharedUrl)).toThrowError(/duplicate JavaScript transport endpoint/);
    expect(() => buildWithUrls(sharedUrl, `${sharedUrl}?`)).toThrowError(/duplicate JavaScript transport endpoint/);
    expect(() =>
      buildWithUrls('ws://LOCALHOST:80/debug/../devtools/runtime', 'ws://localhost/devtools/runtime'),
    ).toThrowError(/duplicate JavaScript transport endpoint/);
    expect(() => buildWithUrls(`${sharedUrl}#one`, `${sharedUrl}#two`)).toThrowError(/non-loopback target URL/);
    expect(() => buildWithUrls(`${sharedUrl}#`, 'ws://127.0.0.1:9010/devtools/other')).toThrowError(
      /non-loopback target URL/,
    );
    const distinctQueryTargets = buildWithUrls(`${sharedUrl}?runtime=one`, `${sharedUrl}?runtime=two`);
    expect(
      distinctQueryTargets.filter(target => target.capabilities.includes(DebuggerTargetCapability.JavaScriptDebugger))
        .length,
    ).toBe(2);
  });

  it('merges Snap Android device-scoped placeholder metadata only into one unique native runtime', () => {
    const targets = buildDebuggerTargetRegistry({
      ports: [nativePort()],
      proxyTargets: [
        {
          adapterType: '_android_emulator-5554',
          appId: 'emulator-5554',
          id: 'android-emulator-5554',
          metadata: { deviceId: 'emulator-5554', deviceName: 'Pixel_8_API_35' },
          title: 'Snap Android Runtime',
          webSocketDebuggerUrl: 'ws://127.0.0.1:9010/android_emulator-5554/1',
        },
      ],
      webPreviewTargets: [],
    });

    expect(targets).toEqual([
      jasmine.objectContaining({
        applicationId: 'com.example.android',
        capabilities: [
          DebuggerTargetCapability.Components,
          DebuggerTargetCapability.Snapshot,
          DebuggerTargetCapability.JavaScriptDebugger,
        ],
        identityMode: DebuggerTargetIdentityMode.TargetId,
        transport: DebuggerTargetTransport.ValdiDaemon,
      }),
    ]);
  });

  it('does not merge an ordinary proxy application ID into a different native application', () => {
    const targets = buildDebuggerTargetRegistry({
      ports: [nativePort()],
      proxyTargets: [
        {
          adapterType: '_android_emulator-5554',
          appId: 'com.example.other',
          id: 'wrong-app',
          metadata: { deviceId: 'emulator-5554' },
          webSocketDebuggerUrl: 'ws://127.0.0.1:9010/android_emulator-5554/wrong-app',
        },
      ],
      webPreviewTargets: [],
    });
    const native = targets.find(target => target.transport === DebuggerTargetTransport.ValdiDaemon);
    const waiting = targets.find(target => target.transport === DebuggerTargetTransport.ChromiumCDP);

    expect(native?.capabilities).toEqual([DebuggerTargetCapability.Components, DebuggerTargetCapability.Snapshot]);
    expect(waiting).toEqual(
      jasmine.objectContaining({
        applicationId: 'com.example.other',
        attachable: false,
        identityMode: DebuggerTargetIdentityMode.TargetId,
      }),
    );
  });

  it('does not treat top-level Android adapter metadata as a device-scoped application placeholder', () => {
    for (const adapterType of ['android', '_android']) {
      const targets = buildDebuggerTargetRegistry({
        ports: [nativePort()],
        proxyTargets: [
          {
            adapterType,
            appId: 'emulator-5554',
            id: `top-level-${adapterType}`,
            metadata: { deviceId: 'emulator-5554' },
            webSocketDebuggerUrl: `ws://127.0.0.1:9010/android/${encodeURIComponent(adapterType)}`,
          },
        ],
        webPreviewTargets: [],
      });
      const native = targets.find(target => target.transport === DebuggerTargetTransport.ValdiDaemon);
      const waiting = targets.find(target => target.transport === DebuggerTargetTransport.ChromiumCDP);

      expect(native?.capabilities).toEqual([DebuggerTargetCapability.Components, DebuggerTargetCapability.Snapshot]);
      expect(waiting).toEqual(
        jasmine.objectContaining({
          applicationId: 'emulator-5554',
          attachable: false,
        }),
      );
    }
  });

  it('recognizes only exact proxy adapter identities for the bounded device metadata', () => {
    const recognizedAdapterTypes = ['android', '_android', '_android_emulator-5554'];
    for (const adapterType of recognizedAdapterTypes) {
      const targets = buildDebuggerTargetRegistry({
        ports: [nativePort()],
        proxyTargets: [proxyTarget(adapterType, 'emulator-5554')],
        webPreviewTargets: [],
      });
      expect(targets[0]?.capabilities).toContain(DebuggerTargetCapability.JavaScriptDebugger);
    }

    for (const adapterType of ['not_android', '_androidish_emulator-5554', '_android_other-device', 'ANDROID']) {
      const targets = buildDebuggerTargetRegistry({
        ports: [nativePort()],
        proxyTargets: [proxyTarget(adapterType, 'emulator-5554')],
        webPreviewTargets: [],
      });
      expect(targets).toEqual([
        jasmine.objectContaining({
          capabilities: [DebuggerTargetCapability.Components, DebuggerTargetCapability.Snapshot],
          transport: DebuggerTargetTransport.ValdiDaemon,
        }),
      ]);
    }

    const missingDeviceMetadata = buildDebuggerTargetRegistry({
      ports: [nativePort()],
      proxyTargets: [proxyTarget('android', null)],
      webPreviewTargets: [],
    });
    expect(missingDeviceMetadata).toEqual([
      jasmine.objectContaining({
        capabilities: [DebuggerTargetCapability.Components, DebuggerTargetCapability.Snapshot],
        transport: DebuggerTargetTransport.ValdiDaemon,
      }),
    ]);

    const iosPort = nativePort('com.example.ios');
    iosPort.deviceId = 'iphone-1';
    const iosClient = iosPort.clients[0];
    if (!iosClient) throw new Error('Expected an iOS debugger client.');
    iosClient.platform = PLATFORM.IOS;
    for (const adapterType of ['_ios', '_ios_iphone-1']) {
      const targets = buildDebuggerTargetRegistry({
        ports: [iosPort],
        proxyTargets: [
          {
            ...proxyTarget(adapterType, 'iphone-1'),
            appId: 'com.example.ios',
          },
        ],
        webPreviewTargets: [],
      });
      expect(targets[0]?.capabilities).toContain(DebuggerTargetCapability.JavaScriptDebugger);
    }
    for (const adapterType of ['ios', 'not_ios', '_iosish_iphone-1', '_ios_other-device', 'IOS']) {
      const targets = buildDebuggerTargetRegistry({
        ports: [iosPort],
        proxyTargets: [
          {
            ...proxyTarget(adapterType, 'iphone-1'),
            appId: 'com.example.ios',
          },
        ],
        webPreviewTargets: [],
      });
      expect(targets[0]?.capabilities).not.toContain(DebuggerTargetCapability.JavaScriptDebugger);
    }
  });

  it('keeps ambiguous or proxy-only devices non-attachable instead of guessing a runtime', () => {
    const port = nativePort();
    const client = port.clients[0];
    if (!client) throw new Error('Expected a native debugger client.');
    client.contexts.push({ id: 'context-2', rootComponentName: 'Second' });
    const targets = buildDebuggerTargetRegistry({
      ports: [port],
      proxyTargets: [
        {
          adapterType: '_android_emulator-5554',
          appId: 'emulator-5554',
          id: 'proxy-1',
          metadata: { deviceId: 'emulator-5554', deviceName: 'Emulator' },
          webSocketDebuggerUrl: 'ws://localhost:9010/android_emulator-5554/1',
        },
      ],
      webPreviewTargets: [],
    });
    const nativeTargets = targets.filter(target => target.transport === DebuggerTargetTransport.ValdiDaemon);
    const waiting = targets.find(target => target.transport === DebuggerTargetTransport.ChromiumCDP);
    if (!waiting) throw new Error('Expected an ambiguity-safe waiting proxy target.');

    expect(
      nativeTargets.every(target => !target.capabilities.includes(DebuggerTargetCapability.JavaScriptDebugger)),
    ).toBeTrue();
    expect(waiting).toEqual(
      jasmine.objectContaining({
        attachable: false,
        capabilities: [DebuggerTargetCapability.JavaScriptDebugger],
        state: DebuggerTargetState.Waiting,
      }),
    );
    expect(waiting.id).toMatch(/^vdt_/);
  });

  it('quarantines proxy records with a present but invalid application ID', async () => {
    const maximumApplicationId = 'v'.repeat(1024);
    const oversizedApplicationId = 'a'.repeat(1025);
    const proxy = await serveProxyTargets([
      {
        adapterType: '_android_emulator-5554',
        id: 'absent-app',
        metadata: { deviceId: 'emulator-5554' },
        webSocketDebuggerUrl: 'ws://127.0.0.1:9010/android/absent-app',
      },
      {
        adapterType: '_android_emulator-5554',
        appId: 'com.example.valid',
        id: 'valid-app',
        metadata: { deviceId: 'emulator-5554' },
        webSocketDebuggerUrl: 'ws://127.0.0.1:9010/android/valid-app',
      },
      {
        adapterType: '_android_emulator-5554',
        appId: maximumApplicationId,
        id: 'maximum-app',
        metadata: { deviceId: 'emulator-5554' },
        webSocketDebuggerUrl: 'ws://127.0.0.1:9010/android/maximum-app',
      },
      ...[
        ['', 'empty-app'],
        ['   ', 'blank-app'],
        [17, 'numeric-app'],
        [null, 'null-app'],
        ['bad\0app', 'control-app'],
        [oversizedApplicationId, 'oversized-app'],
      ].map(([appId, id]) => ({
        adapterType: '_android_emulator-5554',
        appId,
        id,
        metadata: { deviceId: 'emulator-5554' },
        webSocketDebuggerUrl: `ws://127.0.0.1:9010/android/${String(id)}`,
      })),
    ]);
    try {
      const targets = await discoverDebuggerProxyTargets(proxy.port);
      expect(targets.map(target => target.id)).toEqual(['absent-app', 'valid-app', 'maximum-app']);
      expect(targets[0]?.appId).toBeUndefined();
      expect(targets[1]?.appId).toBe('com.example.valid');
      expect(targets[2]?.appId).toBe(maximumApplicationId);
    } finally {
      await proxy.close();
    }
  });

  it('accepts only bounded loopback proxy metadata', async () => {
    const proxy = await serveProxyTargets([
      {
        adapterType: '_ios_device',
        appId: 'com.example.ios',
        id: 'local',
        metadata: { deviceId: 'iphone-1' },
        webSocketDebuggerUrl: 'ws://127.0.0.1:9010/ios/1',
      },
      {
        adapterType: '_android_device',
        id: 'remote',
        webSocketDebuggerUrl: 'ws://example.com/private',
      },
    ]);
    try {
      expect(await discoverDebuggerProxyTargets(proxy.port)).toEqual([
        {
          adapterType: '_ios_device',
          appId: 'com.example.ios',
          id: 'local',
          metadata: { deviceId: 'iphone-1' },
          webSocketDebuggerUrl: 'ws://127.0.0.1:9010/ios/1',
        },
      ]);
    } finally {
      await proxy.close();
    }

    const oversized = await serveProxyTargets(
      Array.from({ length: 129 }, (_, index) => ({
        adapterType: '_ios_device',
        id: index.toString(),
        webSocketDebuggerUrl: `ws://127.0.0.1:9010/ios/${index.toString()}`,
      })),
    );
    try {
      await expectAsync(discoverDebuggerProxyTargets(oversized.port)).toBeRejectedWithError(/more than 128/);
    } finally {
      await oversized.close();
    }
  });
});
