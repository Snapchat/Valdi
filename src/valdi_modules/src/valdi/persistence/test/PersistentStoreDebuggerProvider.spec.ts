import 'jasmine/src/jasmine';
import { getModuleLoader } from 'valdi_core/src/ModuleLoaderGlobal';
import { DebuggerProviderKind } from 'valdi_core/src/debugging/DebuggerProvider';
import {
  createPersistentStoreDebuggerProvider,
  createPersistentStoreDebuggerProviderResult,
  registerPersistentStoreDebuggerProvider,
} from '../src/PersistentStoreDebuggerProvider';

interface ProjectedEntry {
  readonly encoding: number | string;
  readonly key: string;
  readonly value: string;
  readonly valueLength?: number;
  readonly valueTruncated?: boolean;
}

interface ProjectedStore {
  readonly backend: string;
  readonly entries: readonly ProjectedEntry[];
  readonly entriesTruncated?: boolean;
  readonly inspectionTruncated?: boolean;
}

interface ProjectionMetadata {
  readonly entriesOmitted: number;
  readonly invalidFields: number;
  readonly returnedEntries: number;
  readonly truncated: boolean;
  readonly truncatedFields: number;
}

interface ProjectedSnapshot {
  readonly projection: ProjectionMetadata;
  readonly storageError?: string;
  readonly storageInspectionTruncated?: boolean;
  readonly stores: readonly ProjectedStore[];
  readonly truncated: boolean;
}

function resultData(snapshot: unknown): ProjectedSnapshot {
  return JSON.parse(createPersistentStoreDebuggerProviderResult(snapshot).json) as ProjectedSnapshot;
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

describe('PersistentStoreDebuggerProvider', () => {
  it('projects a parser-compatible snapshot within global entry and UTF-8 budgets', () => {
    const stores = Array.from({ length: 6 }, (_, storeIndex) => ({
      backend: 'memory',
      entries: Array.from({ length: 100 }, (_, entryIndex) => ({
        encoding: 0,
        key: `${storeIndex.toString()}-${entryIndex.toString()}`,
        value: 'value',
      })),
      inspectedEntries: 100,
      name: `store-${storeIndex.toString()}`,
    }));
    const result = createPersistentStoreDebuggerProviderResult({ stores, truncated: false });
    const data = JSON.parse(result.json) as ProjectedSnapshot;
    const returnedEntries = data.stores.reduce((count, store) => count + store.entries.length, 0);

    expect(utf8ByteLength(result.json)).toBeLessThanOrEqual(43 * 1024);
    expect(returnedEntries).toBeLessThanOrEqual(500);
    expect(data.projection.returnedEntries).toBe(returnedEntries);
    expect(data.projection.entriesOmitted).toBe(600 - returnedEntries);
    expect(data.projection.truncated).toBeTrue();
    expect(data.stores.every(store => store.entries.length <= 100)).toBeTrue();
    expect(data.stores.some(store => store.entriesTruncated === true)).toBeTrue();
  });

  it('truncates Unicode values without splitting a surrogate pair', () => {
    const value = '🙂'.repeat(20_000);
    const data = resultData({
      stores: [{ backend: 'memory', entries: [{ encoding: 0, key: 'emoji', value }], name: 'unicode' }],
      truncated: false,
    });
    const projected = data.stores[0].entries[0];
    const finalCodeUnit = projected.value.charCodeAt(projected.value.length - 1);

    expect(projected.valueTruncated).toBeTrue();
    expect(projected.valueLength).toBe(value.length);
    expect(projected.value.length).toBeLessThan(value.length);
    expect(data.projection.truncatedFields).toBe(1);
    expect(finalCodeUnit >= 0xd800 && finalCodeUnit <= 0xdbff).toBeFalse();
    expect(utf8ByteLength(JSON.stringify(data))).toBeLessThanOrEqual(43 * 1024);
  });

  it('budgets lone surrogates by their escaped JSON byte cost', () => {
    const value = '\ud800'.repeat(10_000);
    const result = createPersistentStoreDebuggerProviderResult({
      stores: [
        {
          backend: 'memory',
          entries: [
            { encoding: 0, key: 'surrogates', value },
            { encoding: 0, key: 'later', value: 'later' },
          ],
          name: 'unicode',
        },
      ],
      truncated: false,
    });
    const data = JSON.parse(result.json) as ProjectedSnapshot;
    const projected = data.stores[0].entries[0];

    expect(utf8ByteLength(result.json)).toBeLessThanOrEqual(43 * 1024);
    expect(projected.valueTruncated).toBeTrue();
    expect(projected.valueLength).toBe(value.length);
    expect(projected.value.length).toBeLessThan(value.length);
    expect(data.projection.truncatedFields).toBe(1);
    expect(data.projection.entriesOmitted).toBe(1);
    expect(data.stores[0].entriesTruncated).toBeTrue();
  });

  it('preserves errors, truncation metadata, and numeric encoding zero', () => {
    const data = resultData({
      storageError: 'denied',
      storageInspectionTruncated: true,
      stores: [
        {
          backend: 'browser',
          entries: [{ encoding: 0, key: 'theme', value: 'dark' }],
          inspectedEntries: 1,
          inspectionTruncated: true,
          name: 'preferences',
        },
      ],
      truncated: true,
    });

    expect(data.storageError).toBe('denied');
    expect(data.storageInspectionTruncated).toBeTrue();
    expect(data.stores[0].backend).toBe('browser');
    expect(data.stores[0].inspectionTruncated).toBeTrue();
    expect(data.stores[0].entries[0].encoding).toBe(0);
    expect(data.truncated).toBeTrue();
  });

  it('reads only known own data properties without invoking getters or enumeration traps', () => {
    let getterCalls = 0;
    let ownKeysCalls = 0;
    const entry = Object.defineProperty({ encoding: 0, key: 'safe' }, 'value', {
      enumerable: true,
      get: () => {
        getterCalls++;
        return 'secret';
      },
    });
    const snapshot = new Proxy(
      { stores: [{ backend: 'memory', entries: [entry], name: 'safe' }], truncated: false },
      {
        ownKeys: value => {
          ownKeysCalls++;
          return Reflect.ownKeys(value);
        },
      },
    );
    const data = resultData(snapshot);

    expect(getterCalls).toBe(0);
    expect(ownKeysCalls).toBe(0);
    expect(data.stores[0].entries[0].value).toBe('');
    expect(data.projection.invalidFields).toBeGreaterThan(0);
  });

  it('exposes only snapshot and reports unsupported native inspectors as unavailable', async () => {
    const available = createPersistentStoreDebuggerProvider({
      getPersistentStoreSnapshot: () => ({ stores: [], truncated: false }),
    });
    const unavailable = createPersistentStoreDebuggerProvider({});

    expect(available.id).toBe('persistent-store');
    expect(available.kind).toBe(DebuggerProviderKind.Storage);
    expect(available.availability!()).toBeTrue();
    const result = await available.handleRequest({ action: 'snapshot' });
    expect((JSON.parse(result.json) as ProjectedSnapshot).stores).toEqual([]);
    expect(() => available.handleRequest({ action: 'clear' })).toThrowError(/Unsupported PersistentStore/);
    expect(unavailable.availability!()).toEqual(jasmine.objectContaining({ available: false }));
    expect(() => unavailable.handleRequest({ action: 'snapshot' })).toThrowError(/unavailable/);
  });

  it('binds pathless web and native registrations to the provider owner lifecycle', () => {
    const callbacks = new Map<string, () => void>();
    const observedModules = new Map<string, { path?: string }>();
    spyOn(getModuleLoader(), 'onHotReload').and.callFake((ownerModule, path, callback) => {
      callbacks.set(path, callback);
      observedModules.set(path, ownerModule);
      return () => callbacks.delete(path);
    });
    const pathlessModule: { path?: string } = {};
    const webOwner = registerPersistentStoreDebuggerProvider(pathlessModule, {
      getPersistentStoreSnapshot: () => ({ stores: [], truncated: false }),
    });

    expect(observedModules.get('persistence/src/PersistentStoreDebuggerProvider')).toBe(pathlessModule);
    callbacks.get('persistence/src/PersistentStoreDebuggerProvider')!();
    expect(() => webOwner.register(createPersistentStoreDebuggerProvider({}))).toThrowError(/owner is disposed/);

    const nativeModule = { path: 'persistence/native/PersistentStoreDebuggerProvider' };
    const nativeOwner = registerPersistentStoreDebuggerProvider(nativeModule, {});
    expect(observedModules.get(nativeModule.path)).toBe(nativeModule);
    callbacks.get(nativeModule.path)!();
    expect(() => nativeOwner.register(createPersistentStoreDebuggerProvider({}))).toThrowError(/owner is disposed/);
  });
});
