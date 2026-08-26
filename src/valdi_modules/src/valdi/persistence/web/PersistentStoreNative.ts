import { PropertyList } from 'valdi_tsx/src/PropertyList';
import { PersistentStoreNative } from '../src/PersistentStoreNative';

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
