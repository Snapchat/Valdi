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
let nowOverrideSec: number | undefined;

const nowSec = () => (nowOverrideSec ?? Math.floor(Date.now() / 1000));
const cloneBuf = (b: ArrayBuffer) => b.slice(0);
const toBuf = (s: string) => enc.encode(s).buffer;
const toStr = (b: ArrayBuffer) => dec.decode(new Uint8Array(b));

const storageKey = (_storeName: string, key: string) => key;

const isExpired = (e: Entry) => e.expiresAt !== undefined && nowSec() >= e.expiresAt;

const getMemoryStore = (storeName: string): Map<string, Entry> => {
  let store = memoryStores.get(storeName);
  if (!store) {
    store = new Map<string, Entry>();
    memoryStores.set(storeName, store);
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
        getMemoryStore(storeName).set(storageKey(storeName, key), entry);
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
        getMemoryStore(storeName).delete(storageKey(storeName, key));
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
