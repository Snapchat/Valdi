import type { PropertyList } from 'valdi_tsx/src/PropertyList';
import type { PersistentStoreNative } from '../src/PersistentStoreNative';

const STORAGE_PREFIX = 'valdi.persistence.v1:';
const STORAGE_PROBE_KEY = `${STORAGE_PREFIX}__availability_probe__`;

enum StoredValueEncoding {
  String = 0,
  Binary = 1,
}

interface StoredEntry {
  readonly accessedAt: number;
  readonly encoding: StoredValueEncoding;
  readonly expiresAt?: number;
  readonly value: string;
  readonly weight: number;
}

interface StorageBackend {
  getItem(key: string): string | null;
  key(index: number): string | null;
  readonly length: number;
  removeItem(key: string): void;
  setItem(key: string, value: string): void;
}

interface WeightedCacheState {
  readonly backend: StorageBackend;
  readonly entries: Map<string, StoredEntry>;
  totalWeight: number;
}

interface PendingWrite {
  readonly completions: ((error?: string) => void)[];
  readonly run: () => void;
}

export interface WebPersistentStoreDiagnostics {
  readonly batchedFlushes: number;
  readonly coalescedWrites: number;
  readonly durableStores: number;
  readonly evictions: number;
  readonly fallbackStores: number;
  readonly invalidEntries: number;
  readonly reads: number;
  readonly storageWrites: number;
}

const diagnostics = {
  batchedFlushes: 0,
  coalescedWrites: 0,
  durableStores: 0,
  evictions: 0,
  fallbackStores: 0,
  invalidEntries: 0,
  reads: 0,
  storageWrites: 0,
};

class MemoryStorageBackend implements StorageBackend {
  private readonly entries = new Map<string, string>();
  private readonly orderedKeys: string[] = [];

  get length(): number {
    return this.orderedKeys.length;
  }

  getItem(key: string): string | null {
    return this.entries.get(key) ?? null;
  }

  key(index: number): string | null {
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
    this.entries.set(key, value);
  }
}

const fallbackStorage = new MemoryStorageBackend();
const resolvedStorageBackends = new WeakMap<StorageBackend, StorageBackend>();
const weightedCacheStates = new Map<string, WeightedCacheState>();
const pendingWrites = new Map<string, PendingWrite>();
let accessSequence = 0;
let storeSequence = 0;
let batchScheduled = false;
let warnedAboutUnavailableStorage = false;

function resolveStorageBackend(): StorageBackend {
  try {
    if (typeof globalThis.localStorage === 'undefined') {
      warnAboutUnavailableStorage('localStorage is unavailable');
      return fallbackStorage;
    }

    const browserStorage = globalThis.localStorage;
    const resolvedStorage = resolvedStorageBackends.get(browserStorage);
    if (resolvedStorage !== undefined) {
      return resolvedStorage;
    }

    try {
      const previousProbeValue = browserStorage.getItem(STORAGE_PROBE_KEY);
      browserStorage.setItem(STORAGE_PROBE_KEY, '1');
      if (previousProbeValue === null) {
        browserStorage.removeItem(STORAGE_PROBE_KEY);
      } else {
        browserStorage.setItem(STORAGE_PROBE_KEY, previousProbeValue);
      }
      resolvedStorageBackends.set(browserStorage, browserStorage);
      return browserStorage;
    } catch (error) {
      warnAboutUnavailableStorage(error);
      resolvedStorageBackends.set(browserStorage, fallbackStorage);
      return fallbackStorage;
    }
  } catch (error) {
    warnAboutUnavailableStorage(error);
    return fallbackStorage;
  }
}

function warnAboutUnavailableStorage(reason: unknown): void {
  if (warnedAboutUnavailableStorage) {
    return;
  }
  warnedAboutUnavailableStorage = true;
  console.warn(
    `Valdi PersistentStore is using non-persistent memory because browser storage is unavailable: ${String(reason)}`,
  );
}

function errorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

function schedule(callback: () => void): void {
  void Promise.resolve().then(callback);
}

function scheduleBatchedWrite(token: string, run: () => void, completion: (error?: string) => void): void {
  const previousWrite = pendingWrites.get(token);
  const completions = previousWrite === undefined ? [completion] : [...previousWrite.completions, completion];
  if (previousWrite !== undefined) {
    diagnostics.coalescedWrites++;
    pendingWrites.delete(token);
  }
  pendingWrites.set(token, { completions, run });

  if (batchScheduled) {
    return;
  }
  batchScheduled = true;
  schedule(() => {
    batchScheduled = false;
    diagnostics.batchedFlushes++;
    const writes: PendingWrite[] = [];
    pendingWrites.forEach(write => writes.push(write));
    pendingWrites.clear();
    for (const write of writes) {
      let error: string | undefined;
      try {
        write.run();
      } catch (caughtError) {
        error = errorMessage(caughtError);
      }
      for (const callback of write.completions) {
        callback(error);
      }
    }
  });
}

function nextAccessedAt(): number {
  accessSequence = Math.max(accessSequence + 1, Date.now());
  return accessSequence;
}

function encodeBinary(value: ArrayBuffer): string {
  const bytes = new Uint8Array(value);
  const chunks: string[] = [];
  let chunk = '';
  for (let index = 0; index < bytes.length; index++) {
    chunk += String.fromCharCode(bytes[index]);
    if (chunk.length === 8192) {
      chunks.push(chunk);
      chunk = '';
    }
  }
  if (chunk !== '') {
    chunks.push(chunk);
  }
  return btoa(chunks.join(''));
}

function decodeBinary(value: string): ArrayBuffer {
  const binary = atob(value);
  const bytes = new Uint8Array(binary.length);
  for (let index = 0; index < binary.length; index++) {
    bytes[index] = binary.charCodeAt(index);
  }
  return bytes.buffer;
}

function isStoredEntry(value: unknown): value is StoredEntry {
  if (typeof value !== 'object' || value === null) {
    return false;
  }
  const entry = value as Partial<StoredEntry>;
  return (
    typeof entry.accessedAt === 'number' &&
    Number.isFinite(entry.accessedAt) &&
    (entry.encoding === StoredValueEncoding.String || entry.encoding === StoredValueEncoding.Binary) &&
    (entry.expiresAt === undefined || (typeof entry.expiresAt === 'number' && Number.isFinite(entry.expiresAt))) &&
    typeof entry.value === 'string' &&
    typeof entry.weight === 'number' &&
    Number.isFinite(entry.weight) &&
    entry.weight >= 0
  );
}

class WebPersistentStoreNative implements PersistentStoreNative {
  private readonly backend: StorageBackend;
  private readonly namespace: string;
  private readonly storeId = ++storeSequence;
  private currentTimeSeconds: number | undefined;

  constructor(
    name: string,
    private readonly disableBatchWrites: boolean,
    userScoped: boolean,
    private readonly maxWeight: number,
    mockedTime: number | undefined,
    userId: string | undefined,
    enableEncryption: boolean | undefined,
  ) {
    if (enableEncryption === true) {
      throw new Error('Encrypted PersistentStore is unavailable on web; provide a secure native host implementation.');
    }

    if (userScoped && (userId === undefined || userId.trim() === '')) {
      throw new Error(
        'User-scoped PersistentStore requires an authenticated userId on web; provide userId or set deviceGlobal: true.',
      );
    }

    const scope = userScoped ? `user:${encodeURIComponent(userId as string)}` : 'device';
    this.namespace = `${STORAGE_PREFIX}${scope}:${encodeURIComponent(name)}:`;
    this.backend = resolveStorageBackend();
    this.currentTimeSeconds = mockedTime;
    if (this.backend === fallbackStorage) {
      diagnostics.fallbackStores++;
    } else {
      diagnostics.durableStores++;
    }

  }

  store(
    key: string,
    value: ArrayBuffer | string,
    ttlSeconds: number | undefined,
    weight: number | undefined,
    completion: (error?: string) => void,
  ): void {
    this.scheduleWrite(
      key,
      () => {
        const state = this.maxWeight > 0 ? this.ensureWeightedCacheState() : this.currentWeightedCacheState();
        const isString = typeof value === 'string';
        const entry: StoredEntry = {
          accessedAt: nextAccessedAt(),
          encoding: isString ? StoredValueEncoding.String : StoredValueEncoding.Binary,
          expiresAt: ttlSeconds === undefined ? undefined : this.now() + Math.max(0, Math.floor(ttlSeconds)),
          value: isString ? value : encodeBinary(value),
          weight: Math.max(0, weight ?? 0),
        };
        this.writeEntry(key, entry);
        if (state !== undefined) {
          this.rememberEntry(state, key, entry);
        }
        if (this.maxWeight > 0 && state !== undefined) {
          this.evictIfNeeded(state);
        }
      },
      completion,
    );
  }

  fetch(key: string, completion: (value?: ArrayBuffer | string, error?: string) => void, asString: boolean): void {
    schedule(() => {
      try {
        const state = this.maxWeight > 0 ? this.ensureWeightedCacheState() : this.currentWeightedCacheState();
        const entry = this.readEntry(key);
        if (entry === undefined) {
          completion(undefined, 'not found');
          return;
        }

        if (state !== undefined) {
          const accessedEntry: StoredEntry = { ...entry, accessedAt: nextAccessedAt() };
          this.writeEntry(key, accessedEntry);
          this.rememberEntry(state, key, accessedEntry);
        }

        const value = this.decodeValue(entry);
        if (asString) {
          completion(typeof value === 'string' ? value : new TextDecoder().decode(value));
        } else {
          completion(typeof value === 'string' ? new TextEncoder().encode(value).buffer : value);
        }
      } catch (error) {
        completion(undefined, errorMessage(error));
      }
    });
  }

  fetchAll(completion: (value: PropertyList) => void): void {
    schedule(() => {
      const result: PropertyList = [];
      try {
        for (const key of this.keys()) {
          const entry = this.readEntry(key);
          if (entry !== undefined) {
            result.push(key, this.decodeValue(entry));
          }
        }
      } catch (error) {
        console.warn(`Valdi PersistentStore could not enumerate a browser store: ${errorMessage(error)}`);
      }
      completion(result);
    });
  }

  exists(key: string, completion: (exists: boolean) => void): void {
    schedule(() => {
      try {
        completion(this.readEntry(key) !== undefined);
      } catch (error) {
        console.warn(`Valdi PersistentStore could not inspect a browser entry: ${errorMessage(error)}`);
        completion(false);
      }
    });
  }

  remove(key: string, completion: (error?: string) => void): void {
    this.scheduleWrite(
      key,
      () => {
        this.backend.removeItem(this.storageKey(key));
        this.forgetEntry(key);
        diagnostics.storageWrites++;
      },
      completion,
    );
  }

  removeAll(completion: (error?: string) => void): void {
    this.scheduleWrite(
      '__remove_all__',
      () => {
        for (const key of this.keys()) {
          this.backend.removeItem(this.storageKey(key));
          this.forgetEntry(key);
          diagnostics.storageWrites++;
        }
      },
      completion,
    );
  }

  setCurrentTime(timeSeconds: number): void {
    this.currentTimeSeconds = Math.floor(timeSeconds);
    for (const key of this.keys()) {
      this.readEntry(key);
    }
  }

  private scheduleWrite(key: string, operation: () => void, completion: (error?: string) => void): void {
    if (!this.disableBatchWrites) {
      scheduleBatchedWrite(`${this.storeId}:${key}`, operation, completion);
      return;
    }
    schedule(() => {
      try {
        operation();
        completion();
      } catch (error) {
        completion(errorMessage(error));
      }
    });
  }

  private now(): number {
    return this.currentTimeSeconds ?? Math.floor(Date.now() / 1000);
  }

  private storageKey(key: string): string {
    return `${this.namespace}${encodeURIComponent(key)}`;
  }

  private keys(): string[] {
    const result: string[] = [];
    for (let index = 0; index < this.backend.length; index++) {
      const storageKey = this.backend.key(index);
      if (storageKey?.startsWith(this.namespace)) {
        result.push(decodeURIComponent(storageKey.substring(this.namespace.length)));
      }
    }
    return result;
  }

  private readEntry(key: string): StoredEntry | undefined {
    diagnostics.reads++;
    const storageKey = this.storageKey(key);
    const serialized = this.backend.getItem(storageKey);
    if (serialized === null) {
      this.forgetEntry(key);
      return undefined;
    }

    let entry: unknown;
    try {
      entry = JSON.parse(serialized);
    } catch (error) {
      diagnostics.invalidEntries++;
      console.warn(`Valdi PersistentStore removed an invalid browser entry: ${errorMessage(error)}`);
      this.backend.removeItem(storageKey);
      this.forgetEntry(key);
      return undefined;
    }

    if (!isStoredEntry(entry)) {
      diagnostics.invalidEntries++;
      console.warn('Valdi PersistentStore removed a browser entry with unsupported persistence metadata.');
      this.backend.removeItem(storageKey);
      this.forgetEntry(key);
      return undefined;
    }

    if (entry.expiresAt !== undefined && this.now() >= entry.expiresAt) {
      this.backend.removeItem(storageKey);
      this.forgetEntry(key);
      return undefined;
    }
    return entry;
  }

  private decodeValue(entry: StoredEntry): ArrayBuffer | string {
    return entry.encoding === StoredValueEncoding.Binary ? decodeBinary(entry.value) : entry.value;
  }

  private writeEntry(key: string, entry: StoredEntry): void {
    this.backend.setItem(this.storageKey(key), JSON.stringify(entry));
    diagnostics.storageWrites++;
  }

  private currentWeightedCacheState(): WeightedCacheState | undefined {
    const state = weightedCacheStates.get(this.namespace);
    return state?.backend === this.backend ? state : undefined;
  }

  private ensureWeightedCacheState(): WeightedCacheState {
    const existingState = this.currentWeightedCacheState();
    if (existingState !== undefined) {
      return existingState;
    }

    const state: WeightedCacheState = {
      backend: this.backend,
      entries: new Map<string, StoredEntry>(),
      totalWeight: 0,
    };
    weightedCacheStates.set(this.namespace, state);

    const entries: { key: string; entry: StoredEntry }[] = [];
    for (const key of this.keys()) {
      const entry = this.readEntry(key);
      if (entry !== undefined) {
        entries.push({ key, entry });
        accessSequence = Math.max(accessSequence, entry.accessedAt);
      }
    }
    entries.sort((left, right) => left.entry.accessedAt - right.entry.accessedAt);
    for (const storedEntry of entries) {
      state.entries.set(storedEntry.key, storedEntry.entry);
      state.totalWeight += storedEntry.entry.weight;
    }
    return state;
  }

  private rememberEntry(state: WeightedCacheState, key: string, entry: StoredEntry): void {
    const previousEntry = state.entries.get(key);
    if (previousEntry !== undefined) {
      state.totalWeight -= previousEntry.weight;
      state.entries.delete(key);
    }
    state.entries.set(key, entry);
    state.totalWeight += entry.weight;
  }

  private forgetEntry(key: string): void {
    const state = this.currentWeightedCacheState();
    const previousEntry = state?.entries.get(key);
    if (state === undefined || previousEntry === undefined) {
      return;
    }
    state.entries.delete(key);
    state.totalWeight -= previousEntry.weight;
  }

  private evictIfNeeded(state: WeightedCacheState): void {
    if (state.totalWeight <= this.maxWeight) {
      return;
    }
    const entries = state.entries.keys();
    while (state.totalWeight > this.maxWeight) {
      const nextEntry = entries.next();
      if (nextEntry.done) {
        break;
      }
      const key = nextEntry.value;
      const entry = state.entries.get(key);
      if (entry === undefined) {
        continue;
      }
      if (entry.weight === 0) {
        continue;
      }
      this.backend.removeItem(this.storageKey(key));
      state.entries.delete(key);
      state.totalWeight -= entry.weight;
      diagnostics.evictions++;
      diagnostics.storageWrites++;
    }
  }
}

/** Return aggregate, value-free browser persistence counters. */
export function getPersistentStoreDiagnostics(): WebPersistentStoreDiagnostics {
  return { ...diagnostics };
}

/** Browser implementation of the native factory consumed by PersistentStore.ts. */
export function newPersistentStore(
  name: string,
  disableBatchWrites: boolean,
  userScoped: boolean,
  maxWeight: number,
  mockedTime: number | undefined,
  userId: string | undefined,
  enableEncryption: boolean | undefined,
): PersistentStoreNative {
  return new WebPersistentStoreNative(
    name,
    disableBatchWrites,
    userScoped,
    maxWeight,
    mockedTime,
    userId,
    enableEncryption,
  );
}

/** Retain the legacy injectable binding for existing browser hosts. */
export let persistentStoreNative: PersistentStoreNative = newPersistentStore(
  'legacy',
  false,
  false,
  0,
  undefined,
  undefined,
  false,
);

export function setPersistentStoreNative(newImplementation: PersistentStoreNative): void {
  persistentStoreNative = newImplementation;
}
