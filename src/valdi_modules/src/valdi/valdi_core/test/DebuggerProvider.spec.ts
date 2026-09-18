import 'jasmine/src/jasmine';
import { jsx } from '../src/JSXBootstrap';
import { ModuleLoader } from '../src/ModuleLoader';
import { getModuleLoader } from '../src/ModuleLoaderGlobal';
import type { IModuleLoader } from '../src/IModuleLoader';
import type { ValdiRuntime } from '../src/ValdiRuntime';
import {
  createDebuggerProviderOwner,
  createDebuggerProviderResult,
  DebuggerProviderKind,
  registerDebuggerProvider,
  reloadDebuggerProviderModuleForTesting,
} from '../src/debugging/DebuggerProvider';
import type { CustomMessageHandler } from '../src/debugging/CustomMessageHandler';
import type {
  DebuggerProvider,
  DebuggerProviderRegistration,
  DebuggerProviderResult,
} from '../src/debugging/DebuggerProvider';

declare const runtime: ValdiRuntime;
declare const global: { moduleLoader: IModuleLoader };

function jsonResult(value: string): DebuggerProviderResult {
  return createDebuggerProviderResult(value);
}

function utf8ByteLength(value: string): number {
  let bytes = 0;
  for (let index = 0; index < value.length; index++) {
    const code = value.charCodeAt(index);
    if (code <= 0x7f) bytes++;
    else if (code <= 0x7ff) bytes += 2;
    else if (code >= 0xd800 && code <= 0xdbff && index + 1 < value.length) {
      const next = value.charCodeAt(index + 1);
      if (next >= 0xdc00 && next <= 0xdfff) {
        bytes += 4;
        index++;
      } else bytes += 3;
    } else bytes += 3;
  }
  return bytes;
}

function finalResponseBytes(data: unknown): number {
  return utf8ByteLength(JSON.stringify({ handled: true, data }));
}

describe('DebuggerProvider', () => {
  let originalDebugEnabled: boolean;
  let registrations: DebuggerProviderRegistration[];

  beforeEach(() => {
    originalDebugEnabled = runtime.isDebugEnabled;
    runtime.isDebugEnabled = true;
    registrations = [];
  });

  afterEach(() => {
    registrations.forEach(registration => registration.dispose());
    runtime.isDebugEnabled = originalDebugEnabled;
  });

  it('lists and handles independently registered providers through serialized JSON results', async () => {
    const addHandler = spyOn(jsx, 'addCustomMessageHandler').and.callThrough();
    registrations.push(
      registerDebuggerProvider({
        handleRequest: request =>
          jsonResult(JSON.stringify({ action: request.action, stores: [{ name: 'preferences' }] })),
        id: 'test-storage',
        kind: DebuggerProviderKind.Storage,
        label: 'Test storage',
      }),
    );
    registrations.push(
      registerDebuggerProvider({
        handleRequest: request => jsonResult(JSON.stringify({ action: request.action, databases: [{ id: 'main' }] })),
        id: 'test-sql',
        kind: DebuggerProviderKind.Sql,
        label: 'Test SQL',
      }),
    );
    const handler = addHandler.calls.mostRecent().args[0] as CustomMessageHandler;

    const list = await handler.messageReceived('ValdiDebuggerProviders', { action: 'list' });
    const response = await handler.messageReceived('ValdiDebuggerProviders', {
      action: 'request',
      providerId: 'test-storage',
      request: { action: 'snapshot' },
    });

    expect(list.providers).toEqual(
      jasmine.arrayWithExactContents([
        jasmine.objectContaining({ available: true, id: 'test-storage', kind: 'storage' }),
        jasmine.objectContaining({ available: true, id: 'test-sql', kind: 'sql' }),
      ]),
    );
    expect(response.data).toEqual({ action: 'snapshot', stores: [{ name: 'preferences' }] });
    expect(typeof response.registrationToken).toBe('number');
    expect(handler.messageReceived('UnrelatedIdentifier', {})).toBeUndefined();
  });

  it('never enumerates a provider result Proxy that would return 500,000 keys', async () => {
    let ownKeysCalls = 0;
    const result = new Proxy(
      { json: '{"__proto__":{"polluted":true},"nested":{"safe":true}}' },
      {
        ownKeys: () => {
          ownKeysCalls++;
          return Array.from({ length: 500_000 }, (_, index) => `key-${index.toString()}`);
        },
      },
    );
    const addHandler = spyOn(jsx, 'addCustomMessageHandler').and.callThrough();
    registrations.push(
      registerDebuggerProvider({
        handleRequest: () => result,
        id: 'proxy-result',
        kind: DebuggerProviderKind.Storage,
        label: 'Proxy result',
      }),
    );
    const handler = addHandler.calls.mostRecent().args[0] as CustomMessageHandler;

    const response = await handler.messageReceived('ValdiDebuggerProviders', {
      action: 'request',
      providerId: 'proxy-result',
      request: { action: 'snapshot' },
    });

    expect(response.data.nested).toEqual({ safe: true });
    expect(Object.getPrototypeOf(response.data)).toBeNull();
    expect(Object.getPrototypeOf(response.data.nested)).toBeNull();
    expect(({} as { polluted?: boolean }).polluted).toBeUndefined();
    expect(ownKeysCalls).toBe(0);
  });

  it('applies the action-document budget before final response transport', () => {
    const tooManyProperties = `{${Array.from({ length: 101 }, (_, index) => `"k${index.toString()}":${index.toString()}`).join(',')}}`;
    const tooManyItems = `[${Array.from({ length: 101 }, (_, index) => index.toString()).join(',')}]`;
    const tooDeep = '{"a":'.repeat(10) + 'true' + '}'.repeat(10);
    const exactByteLimit = JSON.stringify({
      a: 'x'.repeat(32 * 1024),
      b: 'x'.repeat(16_369),
    });
    const overByteLimit = `${exactByteLimit.slice(0, -2)}x"}`;
    const oversized = JSON.stringify({
      a: 'x'.repeat(32 * 1024),
      b: 'x'.repeat(16_370),
    });

    expect(() => jsonResult('{"valid":[1,true,null,"text"]}')).not.toThrow();
    expect(exactByteLimit.length).toBe(48 * 1024);
    expect(() => jsonResult(exactByteLimit)).not.toThrow();
    expect(() => jsonResult(overByteLimit)).toThrowError(/exceeds 48 KiB/);
    expect(() => jsonResult(tooManyProperties)).toThrowError(/too many object properties/);
    expect(() => jsonResult(`{"items":${tooManyItems}}`)).toThrowError(/too many array items/);
    expect(() => jsonResult(tooDeep)).toThrowError(/maximum depth/);
    expect(() => jsonResult('{"number":1e400}')).toThrowError(/non-finite/);
    expect(() => jsonResult('[1,2,3]')).toThrowError(/must contain a JSON object/);
    expect(() => jsonResult('{invalid}')).toThrowError(/invalid JSON string/);
    expect(() => jsonResult(oversized)).toThrowError(/exceeds 48 KiB/);
  });

  it('bounds the complete exact-limit action response after metadata and reserialization', async () => {
    const exactDocument = JSON.stringify({
      a: 'x'.repeat(32 * 1024),
      b: 'x'.repeat(16_369),
    });
    const addHandler = spyOn(jsx, 'addCustomMessageHandler').and.callThrough();
    registrations.push(
      registerDebuggerProvider({
        availability: () => ({ available: true, message: 'm'.repeat(1024) }),
        description: 'd'.repeat(4096),
        handleRequest: () => jsonResult(exactDocument),
        id: 'i'.repeat(128),
        kind: DebuggerProviderKind.Storage,
        label: 'l'.repeat(256),
      }),
    );
    const handler = addHandler.calls.mostRecent().args[0] as CustomMessageHandler;

    const response = await handler.messageReceived('ValdiDebuggerProviders', {
      action: 'request',
      providerId: 'i'.repeat(128),
      request: { action: 'snapshot' },
    });

    expect(exactDocument.length).toBe(48 * 1024);
    expect(finalResponseBytes(response)).toBeLessThanOrEqual(128 * 1024);
    expect(response.data.b.length).toBe(16_369);
  });

  it('bounds lone-surrogate expansion in the final action response', async () => {
    const rawSurrogates = '\ud800'.repeat(16_000);
    const document = `{"value":"${rawSurrogates}"}`;
    const addHandler = spyOn(jsx, 'addCustomMessageHandler').and.callThrough();
    registrations.push(
      registerDebuggerProvider({
        handleRequest: () => jsonResult(document),
        id: 'surrogates',
        kind: DebuggerProviderKind.Storage,
        label: 'Surrogates',
      }),
    );
    const handler = addHandler.calls.mostRecent().args[0] as CustomMessageHandler;

    const response = await handler.messageReceived('ValdiDebuggerProviders', {
      action: 'request',
      providerId: 'surrogates',
      request: { action: 'snapshot' },
    });
    const reserialized = JSON.stringify({ handled: true, data: response });

    expect(utf8ByteLength(document)).toBeLessThanOrEqual(48 * 1024);
    expect(reserialized.length).toBeGreaterThan(document.length * 5);
    expect(utf8ByteLength(reserialized)).toBeLessThanOrEqual(128 * 1024);
  });

  it('retains all providers while bounding discovery with maximal optional metadata', async () => {
    const addHandler = spyOn(jsx, 'addCustomMessageHandler').and.callThrough();
    for (let index = 0; index < 100; index++) {
      registrations.push(
        registerDebuggerProvider({
          availability: () => ({ available: true, message: 'm'.repeat(1024) }),
          description: 'd'.repeat(4096),
          handleRequest: () => jsonResult('{"ok":true}'),
          id: `provider-${index.toString().padStart(3, '0')}`,
          kind: DebuggerProviderKind.Storage,
          label: `Provider ${index.toString().padStart(3, '0')} ${'l'.repeat(240)}`,
        }),
      );
    }
    const handler = addHandler.calls.mostRecent().args[0] as CustomMessageHandler;

    const response = await handler.messageReceived('ValdiDebuggerProviders', { action: 'list' });

    expect(response.providers.length).toBe(100);
    expect(response.metadataTruncated).toBe(true);
    expect(response.omittedMetadataFields).toBeGreaterThan(0);
    expect(finalResponseBytes(response)).toBeLessThanOrEqual(128 * 1024);
  });

  it('checks availability immediately before dispatch and isolates throwing checks', async () => {
    let available = true;
    const handleRequest = jasmine.createSpy('handleRequest').and.returnValue(jsonResult('{"ok":true}'));
    const addHandler = spyOn(jsx, 'addCustomMessageHandler').and.callThrough();
    registrations.push(
      registerDebuggerProvider({
        availability: () => available,
        handleRequest,
        id: 'conditional',
        kind: DebuggerProviderKind.KeyValue,
        label: 'Conditional',
      }),
    );
    registrations.push(
      registerDebuggerProvider({
        availability: () => {
          throw new Error('availability failure');
        },
        handleRequest,
        id: 'throwing',
        kind: DebuggerProviderKind.Network,
        label: 'Throwing',
      }),
    );
    const handler = addHandler.calls.mostRecent().args[0] as CustomMessageHandler;
    const list = await handler.messageReceived('ValdiDebuggerProviders', { action: 'list' });
    available = false;

    const conditional = await handler.messageReceived('ValdiDebuggerProviders', {
      action: 'request',
      providerId: 'conditional',
      request: { action: 'list' },
    });
    const throwing = await handler.messageReceived('ValdiDebuggerProviders', {
      action: 'request',
      providerId: 'throwing',
      request: { action: 'list' },
    });

    expect(list.providers.find((provider: { id: string }) => provider.id === 'throwing')).toEqual(
      jasmine.objectContaining({ available: false, message: 'Provider availability check failed.' }),
    );
    expect(conditional).toEqual(jasmine.objectContaining({ unavailable: true }));
    expect(throwing).toEqual(jasmine.objectContaining({ unavailable: true }));
    expect(handleRequest).not.toHaveBeenCalled();
  });

  it('bounds concurrency and marks completions stale after replacement or disposal', async () => {
    const resolvers: Array<(value: DebuggerProviderResult) => void> = [];
    const addHandler = spyOn(jsx, 'addCustomMessageHandler').and.callThrough();
    const first = registerDebuggerProvider({
      handleRequest: () => new Promise(resolve => resolvers.push(resolve)),
      id: 'replaceable',
      kind: DebuggerProviderKind.Sql,
      label: 'First',
    });
    registrations.push(first);
    const handler = addHandler.calls.mostRecent().args[0] as CustomMessageHandler;
    const pending = Array.from({ length: 4 }, () =>
      handler.messageReceived('ValdiDebuggerProviders', {
        action: 'request',
        providerId: 'replaceable',
        request: { action: 'list' },
      }),
    );
    await Promise.resolve();

    const busy = await handler.messageReceived('ValdiDebuggerProviders', {
      action: 'request',
      providerId: 'replaceable',
      request: { action: 'list' },
    });
    const replacement = registerDebuggerProvider({
      handleRequest: () => jsonResult('{"source":"replacement"}'),
      id: 'replaceable',
      kind: DebuggerProviderKind.Sql,
      label: 'Replacement',
    });
    registrations.push(replacement);
    replacement.dispose();
    resolvers.forEach(resolve => resolve(jsonResult('{"source":"first"}')));
    const completed = await Promise.all(pending);

    expect(busy).toEqual(jasmine.objectContaining({ busy: true }));
    completed.forEach(result => expect(result).toEqual(jasmine.objectContaining({ stale: true })));
  });

  it('keeps requests current across unrelated registry changes and stales them after their provider changes', async () => {
    const resolvers: Array<(value: DebuggerProviderResult) => void> = [];
    const addHandler = spyOn(jsx, 'addCustomMessageHandler').and.callThrough();
    const slow = registerDebuggerProvider({
      handleRequest: () => new Promise(resolve => resolvers.push(resolve)),
      id: 'slow-storage',
      kind: DebuggerProviderKind.Storage,
      label: 'Slow storage',
    });
    registrations.push(slow);
    const handler = addHandler.calls.mostRecent().args[0] as CustomMessageHandler;
    const first = handler.messageReceived('ValdiDebuggerProviders', {
      action: 'request',
      providerId: 'slow-storage',
      request: { action: 'snapshot' },
    });
    await Promise.resolve();

    const unrelated = registerDebuggerProvider({
      handleRequest: () => jsonResult('{"databases":[]}'),
      id: 'unrelated-sql',
      kind: DebuggerProviderKind.Sql,
      label: 'Unrelated SQL',
    });
    registrations.push(unrelated);
    unrelated.notifyChange();
    unrelated.dispose();
    resolvers.shift()!(jsonResult('{"stores":["preferences"]}'));

    expect(await first).toEqual(
      jasmine.objectContaining({ data: { stores: ['preferences'] }, registrationToken: jasmine.any(Number) }),
    );

    const second = handler.messageReceived('ValdiDebuggerProviders', {
      action: 'request',
      providerId: 'slow-storage',
      request: { action: 'snapshot' },
    });
    await Promise.resolve();
    slow.notifyChange();
    resolvers.shift()!(jsonResult('{"stores":["stale"]}'));

    expect(await second).toEqual(jasmine.objectContaining({ stale: true }));
  });

  it('caps total distinct live registrations and permanently replaces repeated IDs', async () => {
    const addHandler = spyOn(jsx, 'addCustomMessageHandler').and.callThrough();
    const old = registerDebuggerProvider({
      handleRequest: () => jsonResult('{"generation":"old"}'),
      id: 'same-id',
      kind: DebuggerProviderKind.Storage,
      label: 'Old',
    });
    registrations.push(old);
    const replacement = registerDebuggerProvider({
      handleRequest: () => jsonResult('{"generation":"new"}'),
      id: 'same-id',
      kind: DebuggerProviderKind.Storage,
      label: 'New',
    });
    registrations.push(replacement);
    const handler = addHandler.calls.mostRecent().args[0] as CustomMessageHandler;
    replacement.dispose();

    const afterReplacementDispose = await handler.messageReceived('ValdiDebuggerProviders', { action: 'list' });
    expect(afterReplacementDispose.providers).toEqual([]);
    old.notifyChange();
    expect((await handler.messageReceived('ValdiDebuggerProviders', { action: 'list' })).providers).toEqual([]);

    for (let index = 0; index < 100; index++) {
      registrations.push(
        registerDebuggerProvider({
          handleRequest: () => jsonResult('{"ok":true}'),
          id: `provider-${index.toString()}`,
          kind: DebuggerProviderKind.Storage,
          label: `Registration ${index.toString()}`,
        }),
      );
    }

    expect(() =>
      registerDebuggerProvider({
        handleRequest: () => jsonResult('{"ok":true}'),
        id: 'provider-overflow',
        kind: DebuggerProviderKind.Storage,
        label: 'One too many',
      }),
    ).toThrowError(/live registration count/);
  });

  it('supports structural class providers by binding prototype callbacks to their instance', async () => {
    class ClassProvider implements DebuggerProvider {
      readonly id = 'class-provider';
      readonly kind = DebuggerProviderKind.Sql;
      readonly label = 'Class provider';
      private readonly prefix = 'instance';

      availability(): boolean {
        return this.prefix === 'instance';
      }

      handleRequest(request: { readonly action: string }): DebuggerProviderResult {
        return jsonResult(JSON.stringify({ source: this.prefix, action: request.action }));
      }
    }
    const addHandler = spyOn(jsx, 'addCustomMessageHandler').and.callThrough();
    registrations.push(registerDebuggerProvider(new ClassProvider()));
    const handler = addHandler.calls.mostRecent().args[0] as CustomMessageHandler;

    const response = await handler.messageReceived('ValdiDebuggerProviders', {
      action: 'request',
      providerId: 'class-provider',
      request: { action: 'tables' },
    });

    expect(response.data).toEqual({ source: 'instance', action: 'tables' });
  });

  it('rejects accessors and prototype chains beyond the fixed structural bound', () => {
    let getterCalls = 0;
    const accessorProvider = {
      get id(): string {
        getterCalls++;
        return 'unsafe';
      },
      handleRequest: () => jsonResult('{"ok":true}'),
      kind: DebuggerProviderKind.Storage,
      label: 'Unsafe',
    };
    let prototype: object | null = null;
    for (let depth = 0; depth < 7; depth++) prototype = Object.create(prototype) as object;
    const deepProvider = Object.create(prototype) as DebuggerProvider;
    Object.defineProperties(deepProvider, {
      handleRequest: { value: () => jsonResult('{"ok":true}') },
      id: { value: 'deep' },
      kind: { value: DebuggerProviderKind.Storage },
      label: { value: 'Deep' },
    });

    expect(() => registerDebuggerProvider(accessorProvider)).toThrowError(/must be a data property/);
    expect(getterCalls).toBe(0);
    expect(() => registerDebuggerProvider(deepProvider)).toThrowError(/prototype chain is too deep/);
  });

  it('automatically disposes owners from module-loader callbacks without reviving replaced providers', async () => {
    const reloadCallbacks = new Map<string, () => void>();
    const removedCallbacks: string[] = [];
    const observedModules = new Map<string, { path?: string }>();
    spyOn(getModuleLoader(), 'onHotReload').and.callFake((ownerModule, path, callback) => {
      observedModules.set(path, ownerModule);
      reloadCallbacks.set(path, callback);
      return () => {
        removedCallbacks.push(path);
        reloadCallbacks.delete(path);
      };
    });
    const addHandler = spyOn(jsx, 'addCustomMessageHandler').and.callThrough();
    const oldWebModule: { path?: string } = {};
    const oldOwner = createDebuggerProviderOwner(oldWebModule, 'adapters/storage');
    const oldRegistration = oldOwner.register({
      handleRequest: () => jsonResult('{"owner":"old"}'),
      id: 'owned',
      kind: DebuggerProviderKind.Storage,
      label: 'Old',
    });
    registrations.push(oldRegistration);
    const newWebModule: { path?: string } = {};
    const newOwner = createDebuggerProviderOwner(newWebModule, 'adapters/storage');
    const newRegistration = newOwner.register({
      handleRequest: () => jsonResult('{"owner":"new"}'),
      id: 'owned',
      kind: DebuggerProviderKind.Storage,
      label: 'New',
    });
    registrations.push(newRegistration);
    const handler = addHandler.calls.mostRecent().args[0] as CustomMessageHandler;

    expect((await handler.messageReceived('ValdiDebuggerProviders', { action: 'list' })).providers).toEqual([
      jasmine.objectContaining({ id: 'owned', label: 'New' }),
    ]);
    expect(observedModules.get('adapters/storage')).toBe(newWebModule);
    expect(newWebModule.path).toBeUndefined();
    expect(() =>
      oldOwner.register({
        handleRequest: () => jsonResult('{"owner":"stale"}'),
        id: 'owned',
        kind: DebuggerProviderKind.Storage,
        label: 'Stale',
      }),
    ).toThrowError(/owner is disposed/);
    reloadCallbacks.get('adapters/storage')!();
    const afterNewOwnerReload = await handler.messageReceived('ValdiDebuggerProviders', { action: 'list' });

    expect(afterNewOwnerReload.providers).toEqual([]);
    oldRegistration.notifyChange();
    expect((await handler.messageReceived('ValdiDebuggerProviders', { action: 'list' })).providers).toEqual([]);
    expect(removedCallbacks.filter(key => key === 'adapters/storage').length).toBe(2);
  });

  it('observes the native module path when it differs from the stable replacement key', async () => {
    const nativePath = 'test/native/StorageAdapter';
    const stableOwnerKey = 'storage-owner';
    const nativeLoader = new ModuleLoader(() => undefined, undefined, undefined, false);
    nativeLoader.registerModule(nativePath, () => ({}));
    nativeLoader.load(nativePath);
    const originalModuleLoader = global.moduleLoader;
    global.moduleLoader = nativeLoader;
    const addHandler = spyOn(jsx, 'addCustomMessageHandler').and.callThrough();
    const owner = createDebuggerProviderOwner({ path: nativePath }, stableOwnerKey);
    const registration = owner.register({
      handleRequest: () => jsonResult('{"owner":"native"}'),
      id: 'native-owned',
      kind: DebuggerProviderKind.Storage,
      label: 'Native owner',
    });
    registrations.push(registration);
    const handler = addHandler.calls.mostRecent().args[0] as CustomMessageHandler;
    let listAfterUnload: { providers: unknown[] } | undefined;

    try {
      expect(nativeLoader.unload([stableOwnerKey], true, false)).toEqual([]);
      expect((await handler.messageReceived('ValdiDebuggerProviders', { action: 'list' })).providers.length).toBe(1);
      expect(nativeLoader.unload([nativePath], true, false)).toContain(nativePath);
      listAfterUnload = await handler.messageReceived('ValdiDebuggerProviders', { action: 'list' });
    } finally {
      global.moduleLoader = originalModuleLoader;
    }

    expect(listAfterUnload!.providers).toEqual([]);
    expect(() =>
      owner.register({
        handleRequest: () => jsonResult('{"owner":"stale"}'),
        id: 'native-owned',
        kind: DebuggerProviderKind.Storage,
        label: 'Stale native owner',
      }),
    ).toThrowError(/owner is disposed/);
  });

  it('keeps exactly one bridge and invalidates old registrations across provider-module reload', async () => {
    const addHandler = spyOn(jsx, 'addCustomMessageHandler').and.callThrough();
    const oldRegistration = registerDebuggerProvider({
      handleRequest: () => jsonResult('{"module":"old"}'),
      id: 'module-provider',
      kind: DebuggerProviderKind.Sql,
      label: 'Old module',
    });
    registrations.push(oldRegistration);
    const handler = addHandler.calls.mostRecent().args[0] as CustomMessageHandler;

    reloadDebuggerProviderModuleForTesting();
    oldRegistration.notifyChange();
    oldRegistration.dispose();
    const newRegistration = registerDebuggerProvider({
      handleRequest: () => jsonResult('{"module":"new"}'),
      id: 'module-provider',
      kind: DebuggerProviderKind.Sql,
      label: 'New module',
    });
    registrations.push(newRegistration);

    const list = await handler.messageReceived('ValdiDebuggerProviders', { action: 'list' });
    const response = await handler.messageReceived('ValdiDebuggerProviders', {
      action: 'request',
      providerId: 'module-provider',
      request: { action: 'list' },
    });

    expect(addHandler).toHaveBeenCalledTimes(1);
    expect(list.providers).toEqual([jasmine.objectContaining({ id: 'module-provider', label: 'New module' })]);
    expect(response.data).toEqual({ module: 'new' });
  });
});
