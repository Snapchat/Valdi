// Web-only coverage for github.com/Snapchat/Valdi#119 and the read-only
// debugger adapter. This runs the real web implementation under Node because
// the standalone runner resolves PersistentStoreNative to the native binding.

import {
  __resetInMemoryForTest,
  getPersistentStoreDiagnostics,
  getPersistentStoreSnapshot,
  newPersistentStore,
} from '../PersistentStoreNative';

declare const process: { exit(code: number): void };

class FakeStorage {
  private readonly entries = new Map<string, string>();
  private readonly orderedKeys: string[] = [];
  keyCalls = 0;

  get length(): number {
    return this.orderedKeys.length;
  }

  clear(): void {
    this.entries.clear();
    this.orderedKeys.splice(0);
    this.keyCalls = 0;
  }

  getItem(key: string): string | null {
    return this.entries.get(key) ?? null;
  }

  key(index: number): string | null {
    this.keyCalls++;
    return this.orderedKeys[index] ?? null;
  }

  removeItem(key: string): void {
    if (!this.entries.delete(key)) {
      return;
    }
    const index = this.orderedKeys.indexOf(key);
    if (index !== -1) {
      this.orderedKeys.splice(index, 1);
    }
  }

  setItem(key: string, value: string): void {
    if (!this.entries.has(key)) {
      this.orderedKeys.push(key);
    }
    this.entries.set(key, String(value));
  }
}

function installFakeLocalStorage(storage = new FakeStorage()): FakeStorage {
  (globalThis as any).localStorage = storage;
  return storage;
}

type NativeStore = ReturnType<typeof newPersistentStore>;

const makeStore = (name: string): NativeStore =>
  newPersistentStore(name, true, false, 0, undefined, undefined, false);

// PersistentStore.ts reaches this binding through Valdi's
// require('PersistentStoreNative') module linker, which raw Node ESM does not
// provide. Keep the unchanged public constructor mapping explicit here:
// deviceGlobal defaults false, so userScoped is true; every other option keeps
// its existing default.
const PUBLIC_DEFAULT_NATIVE_MAPPING = {
  disableBatchWrites: false,
  enableEncryption: undefined,
  maxWeight: 0,
  mockedTime: undefined,
  mockedUserId: undefined,
  userScoped: true,
} as const;

const makeDefaultStyleStore = (name: string): NativeStore =>
  newPersistentStore(
    name,
    PUBLIC_DEFAULT_NATIVE_MAPPING.disableBatchWrites,
    PUBLIC_DEFAULT_NATIVE_MAPPING.userScoped,
    PUBLIC_DEFAULT_NATIVE_MAPPING.maxWeight,
    PUBLIC_DEFAULT_NATIVE_MAPPING.mockedTime,
    PUBLIC_DEFAULT_NATIVE_MAPPING.mockedUserId,
    PUBLIC_DEFAULT_NATIVE_MAPPING.enableEncryption,
  );

const makeStoreAtTime = (name: string, time: number): NativeStore =>
  newPersistentStore(name, true, false, 0, time, undefined, false);

const store = (
  persistentStore: NativeStore,
  key: string,
  value: ArrayBuffer | string,
  ttl?: number,
  weight?: number,
): Promise<void> =>
  new Promise((resolve, reject) =>
    persistentStore.store(key, value, ttl, weight, error => (error ? reject(new Error(error)) : resolve())),
  );

const fetchStr = (persistentStore: NativeStore, key: string): Promise<string> =>
  new Promise((resolve, reject) =>
    persistentStore.fetch(key, (value, error) => (error ? reject(new Error(error)) : resolve(value as string)), true),
  );

const fetchBuf = (persistentStore: NativeStore, key: string): Promise<ArrayBuffer> =>
  new Promise((resolve, reject) =>
    persistentStore.fetch(
      key,
      (value, error) => (error ? reject(new Error(error)) : resolve(value as ArrayBuffer)),
      false,
    ),
  );

const removeAll = (persistentStore: NativeStore): Promise<void> =>
  new Promise((resolve, reject) =>
    persistentStore.removeAll(error => (error ? reject(new Error(error)) : resolve())),
  );

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) {
    throw new Error(`ASSERT FAILED: ${message}`);
  }
}

function assertEqual(actual: unknown, expected: unknown, message: string): void {
  if (actual !== expected) {
    throw new Error(
      `ASSERT FAILED: ${message}; expected ${JSON.stringify(expected)}, received ${JSON.stringify(actual)}`,
    );
  }
}

async function rejects(promise: Promise<unknown>): Promise<boolean> {
  try {
    await promise;
    return false;
  } catch {
    return true;
  }
}

// Resolves to how a promise settled within `ms`: 'resolved', 'rejected', or
// 'hang' if it never settled. Distinguishes a graceful rejection from a dropped
// completion callback.
function settleWithin(promise: Promise<unknown>, ms: number): Promise<'resolved' | 'rejected' | 'hang'> {
  return new Promise(resolve => {
    const timeout = setTimeout(() => resolve('hang'), ms);
    promise.then(
      () => {
        clearTimeout(timeout);
        resolve('resolved');
      },
      () => {
        clearTimeout(timeout);
        resolve('rejected');
      },
    );
  });
}

async function verifyFoundationCompatibility(backing: FakeStorage): Promise<void> {
  assertEqual(
    PUBLIC_DEFAULT_NATIVE_MAPPING.userScoped,
    true,
    'deviceGlobal undefined must retain the public userScoped=true native mapping',
  );
  const defaultStyle = makeDefaultStyleStore('defaultStyle');
  await store(defaultStyle, 'key', 'value');
  assertEqual(
    await fetchStr(defaultStyle, 'key'),
    'value',
    'the default public constructor argument shape must remain usable without a web identity',
  );
  assert(
    backing.getItem('valdi.PersistentStore.defaultStyle') !== null,
    'the default constructor shape must retain the established browser key format',
  );

  const first = makeStore('storeA');
  await store(first, 'greeting', 'hello world');
  assertEqual(
    backing.getItem('valdi.PersistentStore.storeA'),
    '{"greeting":{"s":"hello world"}}',
    'writes must preserve the established whole-store JSON format',
  );

  __resetInMemoryForTest();
  const second = makeStore('storeA');
  assertEqual(await fetchStr(second, 'greeting'), 'hello world', 'strings must restore after reload');

  const ttlAtWrite = makeStoreAtTime('ttlStore', 100);
  await store(ttlAtWrite, 'temporary', 'available', 2);
  const ttlPersisted = JSON.parse(backing.getItem('valdi.PersistentStore.ttlStore') ?? '{}');
  assertEqual(ttlPersisted.temporary?.e, 102, 'TTL writes must retain the established absolute e field');
  __resetInMemoryForTest();
  assertEqual(
    await fetchStr(makeStoreAtTime('ttlStore', 101), 'temporary'),
    'available',
    'a persisted TTL entry must hydrate before its expiry second',
  );
  __resetInMemoryForTest();
  assert(
    await rejects(fetchStr(makeStoreAtTime('ttlStore', 102), 'temporary')),
    'a persisted TTL entry must be absent at its expiry second',
  );

  const buffer = new ArrayBuffer(8);
  const view = new Uint32Array(buffer);
  view[0] = 42;
  view[1] = 84;
  await store(second, 'blob', buffer);
  __resetInMemoryForTest();
  const third = makeStore('storeA');
  const restored = new Uint32Array(await fetchBuf(third, 'blob'));
  assert(restored[0] === 42 && restored[1] === 84, 'binary values must round-trip across reload');

  const isolated = makeStore('storeB');
  assert(await rejects(fetchStr(isolated, 'greeting')), 'stores must remain isolated by name');

  await removeAll(third);
  assertEqual(
    backing.getItem('valdi.PersistentStore.storeA'),
    null,
    'removeAll must delete the established whole-store record',
  );
  __resetInMemoryForTest();
  assert(await rejects(fetchStr(makeStore('storeA'), 'greeting')), 'removeAll must remain cleared after reload');

  const protoStore = makeStore('storeProto');
  await store(protoStore, '__proto__', 'safe');
  __resetInMemoryForTest();
  assertEqual(
    await fetchStr(makeStore('storeProto'), '__proto__'),
    'safe',
    'the reserved __proto__ key must survive reload',
  );

  for (const bad of ['null', 'not json', '{"k":{"b":123}}', '{"k":null}']) {
    backing.setItem('valdi.PersistentStore.storeCorrupt', bad);
    __resetInMemoryForTest();
    const outcome = await settleWithin(fetchStr(makeStore('storeCorrupt'), 'k'), 2000);
    assert(outcome !== 'hang', `corrupt blob ${JSON.stringify(bad)} must not hang callers`);
  }

  delete (globalThis as any).localStorage;
  const fallback = makeStore('storeC');
  await store(fallback, 'k', 'v');
  assertEqual(await fetchStr(fallback, 'k'), 'v', 'memory fallback must work without localStorage');
  installFakeLocalStorage(backing);
}

async function verifyReadOnlySnapshot(backing: FakeStorage): Promise<void> {
  backing.clear();
  __resetInMemoryForTest();

  backing.setItem(
    'valdi.PersistentStore.persistedOnly',
    JSON.stringify({ alpha: { s: 'one' }, binary: { b: 'AAf/gP8=', w: 3 } }),
  );
  backing.setItem('valdi.PersistentStore.corruptSnapshot', 'not json');
  const persistedBefore = backing.getItem('valdi.PersistentStore.persistedOnly');
  const corruptBefore = backing.getItem('valdi.PersistentStore.corruptSnapshot');
  const diagnosticsBefore = getPersistentStoreDiagnostics();
  const snapshot = getPersistentStoreSnapshot();
  const diagnosticsAfter = getPersistentStoreDiagnostics();

  assertEqual(diagnosticsBefore.memoryStores, 0, 'setup must start without hydrated stores');
  assertEqual(diagnosticsAfter.memoryStores, 0, 'snapshotting must not hydrate persisted stores');
  assertEqual(
    backing.getItem('valdi.PersistentStore.persistedOnly'),
    persistedBefore,
    'snapshotting must not rewrite valid persisted data',
  );
  assertEqual(
    backing.getItem('valdi.PersistentStore.corruptSnapshot'),
    corruptBefore,
    'snapshotting must not remove corrupt persisted data',
  );
  const persisted = snapshot.stores.find(storeSnapshot => storeSnapshot.name === 'persistedOnly');
  assertEqual(persisted?.backend, 'browser', 'unhydrated legacy stores must be inspectable from browser storage');
  assertEqual(persisted?.entries.length, 2, 'valid persisted entries must be inspectable');
  assertEqual(
    persisted?.entries.find(entry => entry.key === 'binary')?.value,
    'AAf/gP8=',
    'binary values must remain in their established persisted representation',
  );
  assert(
    snapshot.stores.find(storeSnapshot => storeSnapshot.name === 'corruptSnapshot')?.error !== undefined,
    'corrupt persisted stores must be represented as bounded error metadata',
  );
  assert(JSON.stringify(snapshot).includes('persistedOnly'), 'snapshot output must be safely serializable');

  const current = makeStore('current');
  await store(current, 'live', 'memory is authoritative');
  const currentSnapshot = getPersistentStoreSnapshot();
  assertEqual(
    currentSnapshot.stores.filter(storeSnapshot => storeSnapshot.name === 'current').length,
    1,
    'current stores must be deduplicated from their persisted copy',
  );
  assertEqual(
    currentSnapshot.stores.find(storeSnapshot => storeSnapshot.name === 'current')?.backend,
    'memory',
    'the current in-memory value must be the inspected source',
  );
}

async function verifySnapshotBounds(): Promise<void> {
  const boundedStorage = installFakeLocalStorage(new FakeStorage());
  __resetInMemoryForTest();

  const expiredEntries: Record<string, { s: string; e: number }> = Object.create(null);
  for (let index = 0; index < 210; index++) {
    expiredEntries[`expired-${index}`] = { s: 'ignored', e: 0 };
  }
  boundedStorage.setItem('valdi.PersistentStore.inspectedEntries', JSON.stringify(expiredEntries));
  const inspectedSnapshot = getPersistentStoreSnapshot();
  const inspectedStore = inspectedSnapshot.stores.find(storeSnapshot => storeSnapshot.name === 'inspectedEntries');
  assertEqual(
    inspectedStore?.inspectedEntries,
    inspectedSnapshot.limits.maxInspectedEntriesPerStore,
    'per-store iteration must stop at the inspected-entry ceiling even when no entries are returned',
  );
  assert(inspectedStore?.inspectionTruncated, 'inspected-entry truncation must be explicit');
  assertEqual(inspectedStore?.entries.length, 0, 'expired entries must remain absent from the debug result');

  boundedStorage.clear();
  __resetInMemoryForTest();
  const validEntries: Record<string, { s: string }> = Object.create(null);
  for (let index = 0; index < 105; index++) {
    validEntries[`entry-${index}`] = { s: 'value' };
  }
  boundedStorage.setItem('valdi.PersistentStore.entryCap', JSON.stringify(validEntries));
  const entryCapSnapshot = getPersistentStoreSnapshot();
  const entryCapStore = entryCapSnapshot.stores[0];
  assertEqual(
    entryCapStore?.entries.length,
    entryCapSnapshot.limits.maxEntriesPerStore,
    'returned entries must stop at the explicit per-store ceiling',
  );
  assert(entryCapStore?.entriesTruncated, 'returned-entry truncation must be explicit');

  boundedStorage.clear();
  __resetInMemoryForTest();
  for (let index = 0; index < 105; index++) {
    boundedStorage.setItem(`valdi.PersistentStore.store-${index}`, '{}');
  }
  const storeCapSnapshot = getPersistentStoreSnapshot();
  assertEqual(
    storeCapSnapshot.stores.length,
    storeCapSnapshot.limits.maxStores,
    'returned stores must stop at the explicit store ceiling',
  );
  assert(storeCapSnapshot.truncated, 'store-count truncation must be explicit');

  boundedStorage.clear();
  __resetInMemoryForTest();
  for (let index = 0; index < 1050; index++) {
    boundedStorage.setItem(`unrelated-${index}`, '{}');
  }
  boundedStorage.setItem('valdi.PersistentStore.afterFlood', '{"key":{"s":"value"}}');
  const keyCallsBefore = boundedStorage.keyCalls;
  const storageFloodSnapshot = getPersistentStoreSnapshot();
  assertEqual(
    storageFloodSnapshot.inspectedStorageKeys,
    storageFloodSnapshot.limits.maxInspectedStorageKeys,
    'browser-key enumeration must stop at its explicit ceiling',
  );
  assertEqual(
    boundedStorage.keyCalls - keyCallsBefore,
    storageFloodSnapshot.limits.maxInspectedStorageKeys,
    'the reported browser-key ceiling must be operational',
  );
  assert(storageFloodSnapshot.storageInspectionTruncated, 'browser-key scan truncation must be explicit');
  assert(
    storageFloodSnapshot.stores.every(storeSnapshot => storeSnapshot.name !== 'afterFlood'),
    'browser keys beyond the inspection ceiling must not be visited',
  );

  boundedStorage.clear();
  __resetInMemoryForTest();
  const limits = getPersistentStoreSnapshot().limits;
  const oversizedName = 'n'.repeat(limits.maxNameCharacters + 20);
  const oversizedKey = 'k'.repeat(limits.maxKeyCharacters + 20);
  const oversizedValue = 'v'.repeat(limits.maxValueCharacters + 20);
  await store(makeStore(oversizedName), oversizedKey, oversizedValue);
  const oversizedSnapshot = getPersistentStoreSnapshot();
  const oversizedStore = oversizedSnapshot.stores[0];
  const oversizedEntry = oversizedStore?.entries[0];
  assertEqual(oversizedStore?.name.length, limits.maxNameCharacters, 'store names must be bounded');
  assert(oversizedStore?.nameTruncated, 'store-name truncation must be explicit');
  assertEqual(oversizedEntry?.key.length, limits.maxKeyCharacters, 'entry keys must be bounded');
  assert(oversizedEntry?.keyTruncated, 'entry-key truncation must be explicit');
  assertEqual(oversizedEntry?.value.length, limits.maxValueCharacters, 'entry values must be bounded');
  assert(oversizedEntry?.valueTruncated, 'entry-value truncation must be explicit');

  boundedStorage.clear();
  __resetInMemoryForTest();
  boundedStorage.setItem(
    'valdi.PersistentStore.oversizedSerialized',
    'x'.repeat(limits.maxSerializedStoreCharacters + 1),
  );
  const serializedSnapshot = getPersistentStoreSnapshot();
  assertEqual(
    serializedSnapshot.stores[0]?.error,
    'serialized-store-too-large',
    'oversized persisted stores must not be parsed',
  );

  boundedStorage.clear();
  __resetInMemoryForTest();
  const aggregateValue = '\n😀"\\'.repeat(3000);
  for (let storeIndex = 0; storeIndex < 20; storeIndex++) {
    const entries: Record<string, { s: string }> = Object.create(null);
    for (let entryIndex = 0; entryIndex < 4; entryIndex++) {
      entries[`entry-${entryIndex}`] = { s: aggregateValue };
    }
    boundedStorage.setItem(`valdi.PersistentStore.aggregate-${storeIndex}`, JSON.stringify(entries));
  }
  const aggregateSnapshot = getPersistentStoreSnapshot();
  const aggregateJson = JSON.stringify(aggregateSnapshot);
  assert(aggregateSnapshot.truncated, 'aggregate budget exhaustion must be explicit');
  assert(
    aggregateSnapshot.stores.some(storeSnapshot => storeSnapshot.entriesTruncated === true),
    'the store cut short by the aggregate budget must report entry truncation',
  );
  assert(
    aggregateSnapshot.usage.characters <= aggregateSnapshot.limits.maxTotalCharacters,
    'reported character usage must remain bounded',
  );
  assert(
    aggregateSnapshot.usage.bytes <= aggregateSnapshot.limits.maxTotalBytes,
    'reported UTF-8 usage must remain bounded',
  );
  assert(
    aggregateJson.length <= aggregateSnapshot.limits.maxTotalCharacters,
    'serialized snapshot characters must remain bounded',
  );
  assert(
    new TextEncoder().encode(aggregateJson).length <= aggregateSnapshot.limits.maxTotalBytes,
    'serialized snapshot bytes must remain bounded',
  );
}

function verifyHostileStorageGetter(): void {
  __resetInMemoryForTest();
  const snapshotForLengthThrow = (thrown: unknown) => {
    (globalThis as any).localStorage = {
      getItem: () => null,
      setItem: () => undefined,
      removeItem: () => undefined,
      get length() {
        throw thrown;
      },
    };
    return getPersistentStoreSnapshot();
  };

  const primitiveSnapshot = snapshotForLengthThrow('primitive storage failure');
  assertEqual(
    primitiveSnapshot.storageError,
    'primitive storage failure',
    'primitive string throws may be retained as bounded diagnostics',
  );

  const symbolMessageError = new Error('ignored');
  (symbolMessageError as any).message = Symbol('attacker-controlled');
  const symbolMessageSnapshot = snapshotForLengthThrow(symbolMessageError);
  assertEqual(
    symbolMessageSnapshot.storageError,
    'Storage inspection failed.',
    'objects with symbol messages must use the constant diagnostic fallback',
  );

  let messageReads = 0;
  const hostileMessage = Object.create(null);
  Object.defineProperty(hostileMessage, 'message', {
    get: () => {
      messageReads++;
      throw new Error('message getter must not run');
    },
  });
  const hostileMessageSnapshot = snapshotForLengthThrow(hostileMessage);
  assertEqual(messageReads, 0, 'snapshot errors must not read attacker-controlled message properties');
  assertEqual(
    hostileMessageSnapshot.storageError,
    'Storage inspection failed.',
    'hostile message getters must use the constant diagnostic fallback',
  );

  let coercions = 0;
  const hostileCoercion = {
    toString: () => {
      coercions++;
      throw new Error('toString must not run');
    },
  };
  const hostileCoercionSnapshot = snapshotForLengthThrow(hostileCoercion);
  assertEqual(coercions, 0, 'snapshot errors must not coerce attacker-controlled thrown objects');
  assertEqual(
    hostileCoercionSnapshot.storageError,
    'Storage inspection failed.',
    'hostile coercion must use the constant diagnostic fallback',
  );
  assert(
    JSON.stringify([primitiveSnapshot, symbolMessageSnapshot, hostileMessageSnapshot, hostileCoercionSnapshot])
      .includes('storageError'),
    'all hostile thrown-value snapshots must remain serializable',
  );

  (globalThis as any).localStorage = {
    getItem: () => {
      throw 'primitive getItem failure';
    },
    setItem: () => undefined,
    removeItem: () => undefined,
    key: () => 'valdi.PersistentStore.hostile',
    length: 1,
  };
  const getItemSnapshot = getPersistentStoreSnapshot();
  assertEqual(
    getItemSnapshot.stores[0]?.error,
    'primitive getItem failure',
    'primitive per-store getter failures must become bounded metadata',
  );
  assert(JSON.stringify(getItemSnapshot).includes('hostile'), 'per-store getter failures must remain serializable');

  const hugeStorageKey = 'valdi.PersistentStore.' + 'x'.repeat(3 * 1024 * 1024);
  let hugeKeyCalls = 0;
  let hugeGetItemCalls = 0;
  (globalThis as any).localStorage = {
    getItem: () => {
      hugeGetItemCalls++;
      return '{}';
    },
    setItem: () => undefined,
    removeItem: () => undefined,
    key: () => {
      hugeKeyCalls++;
      return hugeStorageKey;
    },
    length: 1,
  };
  const hugeKeySnapshot = getPersistentStoreSnapshot();
  assert(
    hugeStorageKey.length > hugeKeySnapshot.limits.maxStorageKeyCharacters,
    'the hostile key fixture must exceed the accepted raw storage-key ceiling',
  );
  assertEqual(hugeKeySnapshot.rejectedStorageKeys, 1, 'oversized raw storage keys must be reported');
  assertEqual(hugeKeyCalls, 1, 'oversized raw storage keys must be read only once');
  assertEqual(hugeGetItemCalls, 0, 'oversized raw storage keys must be rejected before getItem lookup');
  assertEqual(hugeKeySnapshot.stores.length, 0, 'oversized raw storage keys must not enter store retention');
  assert(hugeKeySnapshot.truncated, 'oversized raw storage-key rejection must mark the snapshot');
  assert(
    JSON.stringify(hugeKeySnapshot).length < 4096,
    'multi-megabyte raw storage keys must not inflate the serialized snapshot',
  );
}

async function main(): Promise<void> {
  const backing = installFakeLocalStorage();
  await verifyFoundationCompatibility(backing);
  await verifyReadOnlySnapshot(backing);
  await verifySnapshotBounds();
  verifyHostileStorageGetter();

  // eslint-disable-next-line no-console
  console.log('PersistentStore web compatibility and debugger snapshot: all checks passed');
}

main().catch(error => {
  // eslint-disable-next-line no-console
  console.error(error);
  process.exit(1);
});
