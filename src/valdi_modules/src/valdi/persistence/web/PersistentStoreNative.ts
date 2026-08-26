import type { PropertyList } from 'valdi_tsx/src/PropertyList';
import type { PersistentStoreNative } from '../src/PersistentStoreNative';

const enc = new TextEncoder();
const dec = new TextDecoder();

const microtask = (fn: () => void) => { void Promise.resolve().then(fn); };

type Entry = {
  value: ArrayBuffer | string;
  weight?: number;
  /** epoch seconds at which this entry expires (exclusive). undefined = no expiry */
  expiresAt?: number;
};

const memoryStores = new Map<string, Map<string, Entry>>();
// Store names whose in-memory Map has already been hydrated from localStorage.
const hydratedStores = new Set<string>();
let nowOverrideSec: number | undefined;

const nowSec = () => (nowOverrideSec ?? Math.floor(Date.now() / 1000));
const cloneBuf = (b: ArrayBuffer) => b.slice(0);
const toBuf = (s: string) => enc.encode(s).buffer;
const toStr = (b: ArrayBuffer) => dec.decode(new Uint8Array(b));

const storageKey = (_storeName: string, key: string) => key;

const isExpired = (e: Entry) => e.expiresAt !== undefined && nowSec() >= e.expiresAt;

// --- Durable backing -------------------------------------------------------
// The in-memory Map above is only a per-page-load cache, so on web every value
// was lost on reload. We mirror each store into localStorage so it survives.
// One localStorage entry holds the whole store as JSON (binary base64-encoded),
// which is simple and fine for the small key/value data this is meant for
// (preferences, tokens). Large or binary-heavy stores should use IndexedDB; if
// a write exceeds quota (or localStorage is absent/throws) we silently fall
// back to memory-only, keeping the pre-existing behavior.

const LS_PREFIX = 'valdi.PersistentStore.';

interface LocalStorageLike {
  getItem(key: string): string | null;
  key?(index: number): string | null;
  readonly length?: number;
  setItem(key: string, value: string): void;
  removeItem(key: string): void;
}

const getLocalStorage = (): LocalStorageLike | undefined => {
  try {
    const ls = (globalThis as any).localStorage;
    if (ls && typeof ls.getItem === 'function' && typeof ls.setItem === 'function') {
      return ls as LocalStorageLike;
    }
  } catch {
    // Accessing localStorage can throw in sandboxed contexts (e.g. some iframes).
  }
  return undefined;
};

const B64_CHARS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';

function bytesToBase64(bytes: Uint8Array): string {
  let out = '';
  for (let i = 0; i < bytes.length; i += 3) {
    const b0 = bytes[i];
    const b1 = i + 1 < bytes.length ? bytes[i + 1] : 0;
    const b2 = i + 2 < bytes.length ? bytes[i + 2] : 0;
    out += B64_CHARS[b0 >> 2];
    out += B64_CHARS[((b0 & 0x03) << 4) | (b1 >> 4)];
    out += i + 1 < bytes.length ? B64_CHARS[((b1 & 0x0f) << 2) | (b2 >> 6)] : '=';
    out += i + 2 < bytes.length ? B64_CHARS[b2 & 0x3f] : '=';
  }
  return out;
}

function base64ToBytes(b64: string): Uint8Array {
  const clean = b64.replace(/[^A-Za-z0-9+/]/g, '');
  const bytes = new Uint8Array(Math.floor((clean.length * 6) / 8));
  let bitBuf = 0;
  let bitCount = 0;
  let o = 0;
  for (let i = 0; i < clean.length; i++) {
    bitBuf = (bitBuf << 6) | B64_CHARS.indexOf(clean[i]);
    bitCount += 6;
    if (bitCount >= 8) {
      bitCount -= 8;
      bytes[o++] = (bitBuf >> bitCount) & 0xff;
    }
  }
  return bytes;
}

/** On-disk shape of an entry. `s`=string value, `b`=base64 binary, `w`=weight, `e`=expiresAt. */
type PersistedEntry = { s?: string; b?: string; w?: number; e?: number };

const persistStore = (storeName: string, store: Map<string, Entry>): void => {
  const ls = getLocalStorage();
  if (!ls) {
    return;
  }
  // Object.create(null): a plain {} would treat a "__proto__" key as a prototype
  // assignment, which JSON.stringify then drops - silently losing that entry.
  const obj: Record<string, PersistedEntry> = Object.create(null);
  store.forEach((e, k) => {
    const pe: PersistedEntry = typeof e.value === 'string' ? { s: e.value } : { b: bytesToBase64(new Uint8Array(e.value)) };
    if (e.weight !== undefined) {
      pe.w = e.weight;
    }
    if (e.expiresAt !== undefined) {
      pe.e = e.expiresAt;
    }
    obj[k] = pe;
  });
  try {
    ls.setItem(LS_PREFIX + storeName, JSON.stringify(obj));
  } catch {
    // Quota exceeded / storage disabled: keep memory-only, silently.
  }
};

const hydrateStore = (storeName: string, store: Map<string, Entry>): void => {
  const ls = getLocalStorage();
  if (!ls) {
    return;
  }
  // localStorage is shared across the origin, so treat its contents as
  // untrusted: guard every shape and keep the whole parse+load in one try so a
  // corrupt or tampered blob is discarded rather than thrown out of a microtask
  // that has no catch (which would drop the completion and hang the caller).
  try {
    const raw = ls.getItem(LS_PREFIX + storeName);
    if (!raw) {
      return;
    }
    const obj = JSON.parse(raw);
    for (const k of Object.keys(obj ?? {})) {
      const pe: PersistedEntry = obj[k] ?? {};
      const value: ArrayBuffer | string =
        typeof pe.b === 'string' ? base64ToBytes(pe.b).buffer : typeof pe.s === 'string' ? pe.s : '';
      const entry: Entry = {
        value,
        weight: typeof pe.w === 'number' ? pe.w : undefined,
        expiresAt: typeof pe.e === 'number' ? pe.e : undefined,
      };
      if (!isExpired(entry)) {
        store.set(k, entry);
      }
    }
  } catch {
    // Corrupt or externally-tampered data: discard and start from an empty store.
  }
};

const removePersistedStore = (storeName: string): void => {
  const ls = getLocalStorage();
  if (ls) {
    try {
      ls.removeItem(LS_PREFIX + storeName);
    } catch {
      // Nothing durable to clean up.
    }
  }
};

const getMemoryStore = (storeName: string): Map<string, Entry> => {
  let store = memoryStores.get(storeName);
  if (!store) {
    store = new Map<string, Entry>();
    memoryStores.set(storeName, store);
  }
  // Hydrate the first time this store name is touched in this JS context (i.e.
  // after a page load) so previously-persisted values are available.
  if (!hydratedStores.has(storeName)) {
    hydratedStores.add(storeName);
    hydrateStore(storeName, store);
  }
  return store;
};

const createStore = (storeName: string): PersistentStoreNative => ({
  store(
    key: string,
    value: ArrayBuffer | string,
    ttlSeconds: number | undefined,
    weight: number | undefined,
    completion: (error?: string) => void,
  ) {
    microtask(() => {
      try {
        const entry: Entry = {
          value: typeof value === 'string' ? value : cloneBuf(value),
          weight,
          expiresAt:
            ttlSeconds === undefined ? undefined :
            ttlSeconds <= 0 ? nowSec() : nowSec() + Math.floor(ttlSeconds),
        };
        const store = getMemoryStore(storeName);
        store.set(storageKey(storeName, key), entry);
        persistStore(storeName, store);
        completion();
      } catch (e: unknown) {
        completion(String(e instanceof Error ? e.message : e ?? 'store failed'));
      }
    });
  },

  fetch(
    key: string,
    completion: (value?: ArrayBuffer | string, error?: string) => void,
    asString: boolean,
  ) {
    microtask(() => {
      const fullKey = storageKey(storeName, key);
      const store = getMemoryStore(storeName);
      sweepIfExpired(store, fullKey);
      const e = store.get(fullKey);
      if (!e) {
        completion(undefined, 'not found');
        return;
      }
      let v: ArrayBuffer | string = e.value;
      if (asString && v instanceof ArrayBuffer) {
        v = toStr(v);
      } else if (!asString && typeof v === 'string') {
        v = toBuf(v);
      }
      completion(v, undefined);
    });
  },

  fetchAll(completion: (value: PropertyList) => void) {
    microtask(() => {
      const out: PropertyList = {};
      const store = getMemoryStore(storeName);
      store.forEach((e, k) => {
        if (!isExpired(e)) {
          out[k] = e.value;
        }
      });
      completion(out);
    });
  },

  setCurrentTime(timeSeconds: number) {
    nowOverrideSec = Math.floor(timeSeconds);
    memoryStores.forEach(store => {
      const keys: string[] = [];
      store.forEach((_e, k) => keys.push(k));
      for (let i = 0; i < keys.length; i++) {
        sweepIfExpired(store, keys[i]);
      }
    });
  },

  exists(key: string, completion: (exists: boolean) => void) {
    microtask(() => {
      const fullKey = storageKey(storeName, key);
      const store = getMemoryStore(storeName);
      sweepIfExpired(store, fullKey);
      completion(store.has(fullKey));
    });
  },

  remove(key: string, completion: (error?: string) => void) {
    microtask(() => {
      try {
        const store = getMemoryStore(storeName);
        store.delete(storageKey(storeName, key));
        persistStore(storeName, store);
        completion();
      } catch (e: unknown) {
        completion(String(e instanceof Error ? e.message : e ?? 'remove failed'));
      }
    });
  },

  removeAll(completion: (error?: string) => void) {
    microtask(() => {
      try {
        memoryStores.delete(storeName);
        hydratedStores.delete(storeName);
        removePersistedStore(storeName);
        completion();
      } catch (e: unknown) {
        completion(String(e instanceof Error ? e.message : e ?? 'removeAll failed'));
      }
    });
  },
});

function sweepIfExpired(store: Map<string, Entry>, key: string): void {
  const e = store.get(key);
  if (e && isExpired(e)) {
    store.delete(key);
  }
}

export function newPersistentStore(
  name: string,
  _disableBatchWrites: boolean,
  _userScoped: boolean,
  _maxWeight: number,
  mockedTime: number | undefined,
  _mockedUserId: string | undefined,
  _enableEncryption: boolean | undefined,
): PersistentStoreNative {
  if (mockedTime !== undefined) {
    nowOverrideSec = Math.floor(mockedTime);
  }
  return createStore(name);
}

/** Exported instance + setter so you can swap in a real native binding later. */
export let persistentStoreNative: PersistentStoreNative = createStore('default');
export function setPersistentStoreNative(newImpl: PersistentStoreNative): void {
  persistentStoreNative = newImpl;
}

/**
 * Test-only: drops all in-memory state so the next access re-hydrates from
 * localStorage, letting tests simulate a page reload. Not part of the
 * PersistentStoreNative contract.
 */
export function __resetInMemoryForTest(): void {
  memoryStores.clear();
  hydratedStores.clear();
}

// --- Read-only debugger adapter -------------------------------------------

const MAX_DEBUG_STORES = 100;
const MAX_DEBUG_ENTRIES_PER_STORE = 100;
const MAX_DEBUG_INSPECTED_ENTRIES_PER_STORE = 200;
const MAX_DEBUG_INSPECTED_STORAGE_KEYS = 1000;
const MAX_DEBUG_NAME_CHARACTERS = 512;
const MAX_DEBUG_STORAGE_KEY_CHARACTERS = LS_PREFIX.length + MAX_DEBUG_NAME_CHARACTERS;
const MAX_DEBUG_KEY_CHARACTERS = 1024;
const MAX_DEBUG_VALUE_CHARACTERS = 32 * 1024;
const MAX_DEBUG_ERROR_CHARACTERS = 1024;
const MAX_DEBUG_SERIALIZED_STORE_CHARACTERS = 128 * 1024;
const MAX_DEBUG_TOTAL_CHARACTERS = 256 * 1024;
const MAX_DEBUG_TOTAL_BYTES = 512 * 1024;
const DEBUG_BASE_STRUCTURAL_CHARACTERS = 1024;
const DEBUG_BASE_STRUCTURAL_BYTES = 2048;
const DEBUG_STORE_STRUCTURAL_CHARACTERS = 512;
const DEBUG_STORE_STRUCTURAL_BYTES = 1024;
const DEBUG_ENTRY_STRUCTURAL_CHARACTERS = 512;
const DEBUG_ENTRY_STRUCTURAL_BYTES = 1024;

interface BoundedString {
  readonly bytes: number;
  readonly characters: number;
  readonly originalLength: number;
  readonly truncated: boolean;
  readonly value: string;
}

export interface WebPersistentStoreDiagnostics {
  readonly hydratedStores: number;
  readonly memoryStores: number;
  readonly storageAvailable: boolean;
}

export interface WebPersistentStoreDebugEntry {
  readonly encoding: number;
  readonly expiresAt?: number;
  readonly key: string;
  readonly keyLength?: number;
  readonly keyTruncated?: boolean;
  readonly unavailableReason?: string;
  readonly value: string;
  readonly valueLength?: number;
  readonly valueTruncated?: boolean;
  readonly weight?: number;
}

export interface WebPersistentStoreDebugStore {
  readonly backend: 'browser' | 'memory';
  readonly entries: readonly WebPersistentStoreDebugEntry[];
  readonly entriesTruncated?: boolean;
  readonly error?: string;
  readonly errorLength?: number;
  readonly errorTruncated?: boolean;
  readonly inspectedEntries: number;
  readonly inspectionTruncated?: boolean;
  readonly name: string;
  readonly nameLength?: number;
  readonly nameTruncated?: boolean;
  readonly serializedLength?: number;
}

export interface WebPersistentStoreSnapshotLimits {
  readonly maxEntriesPerStore: number;
  readonly maxErrorCharacters: number;
  readonly maxInspectedEntriesPerStore: number;
  readonly maxInspectedStorageKeys: number;
  readonly maxKeyCharacters: number;
  readonly maxNameCharacters: number;
  readonly maxSerializedStoreCharacters: number;
  readonly maxStorageKeyCharacters: number;
  readonly maxStores: number;
  readonly maxTotalBytes: number;
  readonly maxTotalCharacters: number;
  readonly maxValueCharacters: number;
}

export interface WebPersistentStoreSnapshot {
  readonly diagnostics: WebPersistentStoreDiagnostics;
  readonly inspectedStorageKeys: number;
  readonly limits: WebPersistentStoreSnapshotLimits;
  readonly rejectedStorageKeys: number;
  readonly storageError?: string;
  readonly storageErrorLength?: number;
  readonly storageErrorTruncated?: boolean;
  readonly storageInspectionTruncated?: boolean;
  readonly stores: readonly WebPersistentStoreDebugStore[];
  readonly truncated: boolean;
  readonly usage: { readonly bytes: number; readonly characters: number };
}

const DEBUG_LIMITS: WebPersistentStoreSnapshotLimits = {
  maxEntriesPerStore: MAX_DEBUG_ENTRIES_PER_STORE,
  maxErrorCharacters: MAX_DEBUG_ERROR_CHARACTERS,
  maxInspectedEntriesPerStore: MAX_DEBUG_INSPECTED_ENTRIES_PER_STORE,
  maxInspectedStorageKeys: MAX_DEBUG_INSPECTED_STORAGE_KEYS,
  maxKeyCharacters: MAX_DEBUG_KEY_CHARACTERS,
  maxNameCharacters: MAX_DEBUG_NAME_CHARACTERS,
  maxSerializedStoreCharacters: MAX_DEBUG_SERIALIZED_STORE_CHARACTERS,
  maxStorageKeyCharacters: MAX_DEBUG_STORAGE_KEY_CHARACTERS,
  maxStores: MAX_DEBUG_STORES,
  maxTotalBytes: MAX_DEBUG_TOTAL_BYTES,
  maxTotalCharacters: MAX_DEBUG_TOTAL_CHARACTERS,
  maxValueCharacters: MAX_DEBUG_VALUE_CHARACTERS,
};

class DebugSnapshotBudget {
  bytes = 0;
  characters = 0;

  reserve(characters: number, bytes: number): boolean {
    if (this.characters + characters > MAX_DEBUG_TOTAL_CHARACTERS || this.bytes + bytes > MAX_DEBUG_TOTAL_BYTES) {
      return false;
    }
    this.characters += characters;
    this.bytes += bytes;
    return true;
  }

  take(value: string, maximumCharacters: number): BoundedString | undefined {
    const remainingCharacters = MAX_DEBUG_TOTAL_CHARACTERS - this.characters;
    const remainingBytes = MAX_DEBUG_TOTAL_BYTES - this.bytes;
    if (remainingCharacters < 2 || remainingBytes < 2) {
      return undefined;
    }
    const result = truncateDebugString(value, maximumCharacters, remainingCharacters, remainingBytes);
    this.characters += result.characters;
    this.bytes += result.bytes;
    return result;
  }
}

interface DebugSnapshotBuildState {
  aggregateExhausted: boolean;
  readonly budget: DebugSnapshotBudget;
  readonly stores: WebPersistentStoreDebugStore[];
  truncated: boolean;
}

function utf8BytesForDebugCodePoint(codePoint: number): number {
  if (codePoint <= 0x7f) {
    return 1;
  }
  if (codePoint <= 0x7ff) {
    return 2;
  }
  if (codePoint <= 0xffff) {
    return 3;
  }
  return 4;
}

function debugJsonCost(value: string, index: number, codePoint: number, characterLength: number): {
  bytes: number;
  characters: number;
} {
  const codeUnit = value.charCodeAt(index);
  if (characterLength === 1 && (codeUnit === 0x22 || codeUnit === 0x5c)) {
    return { bytes: 2, characters: 2 };
  }
  if (characterLength === 1 && codeUnit <= 0x1f) {
    const shortEscape =
      codeUnit === 0x08 ||
      codeUnit === 0x09 ||
      codeUnit === 0x0a ||
      codeUnit === 0x0c ||
      codeUnit === 0x0d;
    return shortEscape ? { bytes: 2, characters: 2 } : { bytes: 6, characters: 6 };
  }
  if (characterLength === 1 && codeUnit >= 0xd800 && codeUnit <= 0xdfff) {
    return { bytes: 6, characters: 6 };
  }
  return { bytes: utf8BytesForDebugCodePoint(codePoint), characters: characterLength };
}

function truncateDebugString(
  value: string,
  maximumCharacters: number,
  maximumBudgetCharacters: number,
  maximumBudgetBytes: number,
): BoundedString {
  let bytes = 2;
  let characters = 2;
  let end = 0;
  while (end < value.length && end < maximumCharacters) {
    const codePoint = value.codePointAt(end) as number;
    const characterLength = codePoint > 0xffff ? 2 : 1;
    const cost = debugJsonCost(value, end, codePoint, characterLength);
    if (
      end + characterLength > maximumCharacters ||
      characters + cost.characters > maximumBudgetCharacters ||
      bytes + cost.bytes > maximumBudgetBytes
    ) {
      break;
    }
    end += characterLength;
    bytes += cost.bytes;
    characters += cost.characters;
  }
  return { bytes, characters, originalLength: value.length, truncated: end < value.length, value: value.slice(0, end) };
}

function safeErrorMessage(error: unknown): string {
  return typeof error === 'string' ? error : 'Storage inspection failed.';
}

function diagnosticsForStorage(storageAvailable: boolean): WebPersistentStoreDiagnostics {
  return {
    hydratedStores: hydratedStores.size,
    memoryStores: memoryStores.size,
    storageAvailable,
  };
}

/** Return value-free state for an attached debugger without touching persistence data. */
export function getPersistentStoreDiagnostics(): WebPersistentStoreDiagnostics {
  return diagnosticsForStorage(getLocalStorage() !== undefined);
}

function boundedDebugError(state: DebugSnapshotBuildState, error: unknown): BoundedString | undefined {
  const bounded = state.budget.take(safeErrorMessage(error), MAX_DEBUG_ERROR_CHARACTERS);
  if (bounded === undefined) {
    state.aggregateExhausted = true;
    state.truncated = true;
  } else if (bounded.truncated) {
    state.truncated = true;
  }
  return bounded;
}

function memoryDebugValue(entry: Entry): {
  encoding: number;
  originalLength: number;
  preview: string;
  truncated: boolean;
} {
  if (typeof entry.value === 'string') {
    return { encoding: 0, originalLength: entry.value.length, preview: entry.value, truncated: false };
  }
  const byteLength = entry.value.byteLength;
  const originalLength = byteLength === 0 ? 0 : Math.ceil(byteLength / 3) * 4;
  const maximumPreviewBytes = Math.floor(Math.floor(MAX_DEBUG_VALUE_CHARACTERS / 4) * 3 / 3) * 3;
  const previewByteLength = Math.min(byteLength, maximumPreviewBytes);
  const preview = bytesToBase64(new Uint8Array(entry.value, 0, previewByteLength));
  return { encoding: 1, originalLength, preview, truncated: previewByteLength < byteLength };
}

function appendMemoryDebugStore(
  state: DebugSnapshotBuildState,
  storeName: string,
  store: Map<string, Entry>,
): boolean {
  if (!state.budget.reserve(DEBUG_STORE_STRUCTURAL_CHARACTERS, DEBUG_STORE_STRUCTURAL_BYTES)) {
    state.aggregateExhausted = true;
    state.truncated = true;
    return false;
  }
  const name = state.budget.take(storeName, MAX_DEBUG_NAME_CHARACTERS);
  if (name === undefined) {
    state.aggregateExhausted = true;
    state.truncated = true;
    return false;
  }

  const entries: WebPersistentStoreDebugEntry[] = [];
  let entriesTruncated = false;
  let inspectedEntries = 0;
  let inspectionTruncated = false;
  const entryIterator = store.entries();
  while (true) {
    const nextEntry = entryIterator.next();
    if (nextEntry.done) {
      break;
    }
    const [key, entry] = nextEntry.value;
    if (entries.length >= MAX_DEBUG_ENTRIES_PER_STORE || inspectedEntries >= MAX_DEBUG_INSPECTED_ENTRIES_PER_STORE) {
      entriesTruncated = entries.length >= MAX_DEBUG_ENTRIES_PER_STORE;
      inspectionTruncated = true;
      state.truncated = true;
      break;
    }
    inspectedEntries++;
    if (isExpired(entry)) {
      continue;
    }
    if (!state.budget.reserve(DEBUG_ENTRY_STRUCTURAL_CHARACTERS, DEBUG_ENTRY_STRUCTURAL_BYTES)) {
      state.aggregateExhausted = true;
      state.truncated = true;
      entriesTruncated = true;
      inspectionTruncated = true;
      break;
    }
    const boundedKey = state.budget.take(key, MAX_DEBUG_KEY_CHARACTERS);
    if (boundedKey === undefined) {
      state.aggregateExhausted = true;
      state.truncated = true;
      entriesTruncated = true;
      inspectionTruncated = true;
      break;
    }

    let debugValue: ReturnType<typeof memoryDebugValue>;
    try {
      debugValue = memoryDebugValue(entry);
    } catch {
      entries.push({
        encoding: typeof entry.value === 'string' ? 0 : 1,
        key: boundedKey.value,
        ...(boundedKey.truncated ? { keyLength: boundedKey.originalLength, keyTruncated: true } : {}),
        unavailableReason: 'unavailable-memory-value',
        value: '',
      });
      state.truncated = true;
      continue;
    }
    const boundedValue = state.budget.take(debugValue.preview, MAX_DEBUG_VALUE_CHARACTERS);
    if (boundedValue === undefined) {
      state.aggregateExhausted = true;
      state.truncated = true;
      entriesTruncated = true;
      inspectionTruncated = true;
      break;
    }
    const valueTruncated = debugValue.truncated || boundedValue.truncated;
    const metadataInvalid =
      (entry.expiresAt !== undefined && !Number.isFinite(entry.expiresAt)) ||
      (entry.weight !== undefined && !Number.isFinite(entry.weight));
    entries.push({
      encoding: debugValue.encoding,
      ...(entry.expiresAt !== undefined && Number.isFinite(entry.expiresAt) ? { expiresAt: entry.expiresAt } : {}),
      key: boundedKey.value,
      ...(boundedKey.truncated ? { keyLength: boundedKey.originalLength, keyTruncated: true } : {}),
      ...(metadataInvalid ? { unavailableReason: 'invalid-entry-metadata' } : {}),
      value: boundedValue.value,
      ...(valueTruncated ? { valueLength: debugValue.originalLength, valueTruncated: true } : {}),
      ...(entry.weight !== undefined && Number.isFinite(entry.weight) ? { weight: entry.weight } : {}),
    });
    state.truncated = state.truncated || boundedKey.truncated || valueTruncated || metadataInvalid;
  }

  state.stores.push({
    backend: 'memory',
    entries,
    ...(entriesTruncated ? { entriesTruncated: true } : {}),
    inspectedEntries,
    ...(inspectionTruncated ? { inspectionTruncated: true } : {}),
    name: name.value,
    ...(name.truncated ? { nameLength: name.originalLength, nameTruncated: true } : {}),
  });
  state.truncated = state.truncated || name.truncated;
  return true;
}

function appendPersistedDebugStore(
  state: DebugSnapshotBuildState,
  storage: LocalStorageLike,
  storageName: string,
): boolean {
  if (!state.budget.reserve(DEBUG_STORE_STRUCTURAL_CHARACTERS, DEBUG_STORE_STRUCTURAL_BYTES)) {
    state.aggregateExhausted = true;
    state.truncated = true;
    return false;
  }
  const name = state.budget.take(storageName, MAX_DEBUG_NAME_CHARACTERS);
  if (name === undefined) {
    state.aggregateExhausted = true;
    state.truncated = true;
    return false;
  }

  let raw: string | null;
  try {
    raw = storage.getItem(LS_PREFIX + storageName);
  } catch (error) {
    const boundedError = boundedDebugError(state, error);
    state.stores.push({
      backend: 'browser',
      entries: [],
      ...(boundedError === undefined ? {} : { error: boundedError.value }),
      ...(boundedError?.truncated ? { errorLength: boundedError.originalLength, errorTruncated: true } : {}),
      inspectedEntries: 0,
      name: name.value,
      ...(name.truncated ? { nameLength: name.originalLength, nameTruncated: true } : {}),
    });
    state.truncated = true;
    return true;
  }
  if (raw === null) {
    return true;
  }
  if (typeof raw !== 'string') {
    state.stores.push({
      backend: 'browser',
      entries: [],
      error: 'unsupported-serialized-store',
      inspectedEntries: 0,
      name: name.value,
      ...(name.truncated ? { nameLength: name.originalLength, nameTruncated: true } : {}),
    });
    state.truncated = true;
    return true;
  }
  if (raw.length > MAX_DEBUG_SERIALIZED_STORE_CHARACTERS) {
    state.stores.push({
      backend: 'browser',
      entries: [],
      error: 'serialized-store-too-large',
      inspectedEntries: 0,
      name: name.value,
      ...(name.truncated ? { nameLength: name.originalLength, nameTruncated: true } : {}),
      serializedLength: raw.length,
    });
    state.truncated = true;
    return true;
  }

  let parsed: unknown;
  try {
    parsed = JSON.parse(raw);
  } catch (error) {
    const boundedError = boundedDebugError(state, error);
    state.stores.push({
      backend: 'browser',
      entries: [],
      ...(boundedError === undefined ? {} : { error: boundedError.value }),
      ...(boundedError?.truncated ? { errorLength: boundedError.originalLength, errorTruncated: true } : {}),
      inspectedEntries: 0,
      name: name.value,
      ...(name.truncated ? { nameLength: name.originalLength, nameTruncated: true } : {}),
      serializedLength: raw.length,
    });
    state.truncated = true;
    return true;
  }

  if (typeof parsed !== 'object' || parsed === null || Array.isArray(parsed)) {
    state.stores.push({
      backend: 'browser',
      entries: [],
      error: 'unsupported-store-metadata',
      inspectedEntries: 0,
      name: name.value,
      ...(name.truncated ? { nameLength: name.originalLength, nameTruncated: true } : {}),
      serializedLength: raw.length,
    });
    state.truncated = true;
    return true;
  }

  const entries: WebPersistentStoreDebugEntry[] = [];
  let entriesTruncated = false;
  let inspectedEntries = 0;
  let inspectionTruncated = false;
  for (const key in parsed) {
    if (!Object.prototype.hasOwnProperty.call(parsed, key)) {
      continue;
    }
    if (entries.length >= MAX_DEBUG_ENTRIES_PER_STORE || inspectedEntries >= MAX_DEBUG_INSPECTED_ENTRIES_PER_STORE) {
      entriesTruncated = entries.length >= MAX_DEBUG_ENTRIES_PER_STORE;
      inspectionTruncated = true;
      state.truncated = true;
      break;
    }
    inspectedEntries++;
    if (!state.budget.reserve(DEBUG_ENTRY_STRUCTURAL_CHARACTERS, DEBUG_ENTRY_STRUCTURAL_BYTES)) {
      state.aggregateExhausted = true;
      state.truncated = true;
      entriesTruncated = true;
      inspectionTruncated = true;
      break;
    }
    const boundedKey = state.budget.take(key, MAX_DEBUG_KEY_CHARACTERS);
    if (boundedKey === undefined) {
      state.aggregateExhausted = true;
      state.truncated = true;
      entriesTruncated = true;
      inspectionTruncated = true;
      break;
    }

    const persisted = (parsed as Record<string, unknown>)[key];
    if (typeof persisted !== 'object' || persisted === null || Array.isArray(persisted)) {
      entries.push({
        encoding: 0,
        key: boundedKey.value,
        ...(boundedKey.truncated ? { keyLength: boundedKey.originalLength, keyTruncated: true } : {}),
        unavailableReason: 'unsupported-entry-metadata',
        value: '',
      });
      state.truncated = true;
      continue;
    }
    const pe = persisted as PersistedEntry;
    const encoding = typeof pe.b === 'string' ? 1 : 0;
    const persistedValue = typeof pe.b === 'string' ? pe.b : typeof pe.s === 'string' ? pe.s : undefined;
    if (persistedValue === undefined) {
      entries.push({
        encoding,
        key: boundedKey.value,
        ...(boundedKey.truncated ? { keyLength: boundedKey.originalLength, keyTruncated: true } : {}),
        unavailableReason: 'unsupported-entry-metadata',
        value: '',
      });
      state.truncated = true;
      continue;
    }
    if (typeof pe.e === 'number' && Number.isFinite(pe.e) && nowSec() >= pe.e) {
      continue;
    }
    const boundedValue = state.budget.take(persistedValue, MAX_DEBUG_VALUE_CHARACTERS);
    if (boundedValue === undefined) {
      state.aggregateExhausted = true;
      state.truncated = true;
      entriesTruncated = true;
      inspectionTruncated = true;
      break;
    }
    const metadataInvalid =
      (pe.e !== undefined && (typeof pe.e !== 'number' || !Number.isFinite(pe.e))) ||
      (pe.w !== undefined && (typeof pe.w !== 'number' || !Number.isFinite(pe.w)));
    entries.push({
      encoding,
      ...(typeof pe.e === 'number' && Number.isFinite(pe.e) ? { expiresAt: pe.e } : {}),
      key: boundedKey.value,
      ...(boundedKey.truncated ? { keyLength: boundedKey.originalLength, keyTruncated: true } : {}),
      ...(metadataInvalid ? { unavailableReason: 'invalid-entry-metadata' } : {}),
      value: boundedValue.value,
      ...(boundedValue.truncated ? { valueLength: boundedValue.originalLength, valueTruncated: true } : {}),
      ...(typeof pe.w === 'number' && Number.isFinite(pe.w) ? { weight: pe.w } : {}),
    });
    state.truncated = state.truncated || boundedKey.truncated || boundedValue.truncated || metadataInvalid;
  }

  state.stores.push({
    backend: 'browser',
    entries,
    ...(entriesTruncated ? { entriesTruncated: true } : {}),
    inspectedEntries,
    ...(inspectionTruncated ? { inspectionTruncated: true } : {}),
    name: name.value,
    ...(name.truncated ? { nameLength: name.originalLength, nameTruncated: true } : {}),
    serializedLength: raw.length,
  });
  state.truncated = state.truncated || name.truncated;
  return true;
}

/** Return a bounded, read-only view over current and persisted legacy web stores. */
export function getPersistentStoreSnapshot(): WebPersistentStoreSnapshot {
  const storage = getLocalStorage();
  const budget = new DebugSnapshotBudget();
  budget.reserve(DEBUG_BASE_STRUCTURAL_CHARACTERS, DEBUG_BASE_STRUCTURAL_BYTES);
  const state: DebugSnapshotBuildState = {
    aggregateExhausted: false,
    budget,
    stores: [],
    truncated: false,
  };
  const seenStoreNames = new Set<string>();

  const storeIterator = memoryStores.entries();
  while (true) {
    const nextStore = storeIterator.next();
    if (nextStore.done) {
      break;
    }
    const [storeName, store] = nextStore.value;
    if (state.stores.length >= MAX_DEBUG_STORES) {
      state.truncated = true;
      break;
    }
    if (!appendMemoryDebugStore(state, storeName, store)) {
      break;
    }
    seenStoreNames.add(storeName);
    if (state.aggregateExhausted) {
      break;
    }
  }

  if (state.stores.length >= MAX_DEBUG_STORES && storage !== undefined) {
    state.truncated = true;
  }

  let inspectedStorageKeys = 0;
  let rejectedStorageKeys = 0;
  let storageInspectionTruncated = false;
  let storageError: BoundedString | undefined;
  if (storage !== undefined && !state.aggregateExhausted && state.stores.length < MAX_DEBUG_STORES) {
    let storageLength = 0;
    try {
      const length = storage.length;
      storageLength = typeof length === 'number' && Number.isFinite(length) ? Math.max(0, Math.floor(length)) : 0;
    } catch (error) {
      storageError = boundedDebugError(state, error);
      state.truncated = true;
      storageInspectionTruncated = true;
    }

    for (let index = 0; index < storageLength; index++) {
      if (state.stores.length >= MAX_DEBUG_STORES || state.aggregateExhausted) {
        state.truncated = true;
        storageInspectionTruncated = true;
        break;
      }
      if (inspectedStorageKeys >= MAX_DEBUG_INSPECTED_STORAGE_KEYS) {
        state.truncated = true;
        storageInspectionTruncated = true;
        break;
      }
      inspectedStorageKeys++;
      let fullStorageKey: string | null;
      try {
        fullStorageKey = storage.key?.(index) ?? null;
      } catch (error) {
        storageError = boundedDebugError(state, error);
        state.truncated = true;
        storageInspectionTruncated = true;
        break;
      }
      if (typeof fullStorageKey !== 'string') {
        continue;
      }
      if (fullStorageKey.length > MAX_DEBUG_STORAGE_KEY_CHARACTERS) {
        rejectedStorageKeys++;
        state.truncated = true;
        continue;
      }
      if (!fullStorageKey.startsWith(LS_PREFIX)) {
        continue;
      }
      const storeName = fullStorageKey.slice(LS_PREFIX.length);
      if (seenStoreNames.has(storeName)) {
        continue;
      }
      if (!appendPersistedDebugStore(state, storage, storeName)) {
        storageInspectionTruncated = true;
        break;
      }
      seenStoreNames.add(storeName);
    }
  }

  return {
    diagnostics: diagnosticsForStorage(storage !== undefined),
    inspectedStorageKeys,
    limits: { ...DEBUG_LIMITS },
    rejectedStorageKeys,
    ...(storageError === undefined ? {} : { storageError: storageError.value }),
    ...(storageError?.truncated
      ? { storageErrorLength: storageError.originalLength, storageErrorTruncated: true }
      : {}),
    ...(storageInspectionTruncated ? { storageInspectionTruncated: true } : {}),
    stores: state.stores,
    truncated: state.truncated,
    usage: { bytes: budget.bytes, characters: budget.characters },
  };
}
