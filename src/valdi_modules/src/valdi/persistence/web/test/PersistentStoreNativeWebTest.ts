// Web-only test for github.com/Snapchat/Valdi#119: the web PersistentStore was
// in-memory only, so data vanished on page reload. This runs the real web impl
// under node with a fake localStorage and asserts durability across a simulated
// reload (all in-memory state dropped, localStorage kept).
//
// It runs via js_test (see BUILD.bazel), not the standalone TestsRunner, because
// the standalone runner resolves 'PersistentStoreNative' to the native binding,
// not this web implementation.

import { newPersistentStore, __resetInMemoryForTest } from '../PersistentStoreNative';

declare const process: { exit(code: number): void };

function installFakeLocalStorage(): Map<string, string> {
  const m = new Map<string, string>();
  (globalThis as any).localStorage = {
    getItem: (k: string) => (m.has(k) ? m.get(k)! : null),
    setItem: (k: string, v: string) => {
      m.set(k, String(v));
    },
    removeItem: (k: string) => {
      m.delete(k);
    },
    clear: () => m.clear(),
    key: (i: number) => Array.from(m.keys())[i] ?? null,
    get length() {
      return m.size;
    },
  };
  return m;
}

type NativeStore = ReturnType<typeof newPersistentStore>;

const makeStore = (name: string): NativeStore =>
  newPersistentStore(name, true, false, 0, undefined, undefined, false);

const store = (s: NativeStore, k: string, v: ArrayBuffer | string): Promise<void> =>
  new Promise((res, rej) => s.store(k, v, undefined, undefined, e => (e ? rej(new Error(e)) : res())));
const fetchStr = (s: NativeStore, k: string): Promise<string> =>
  new Promise((res, rej) => s.fetch(k, (v, e) => (e ? rej(new Error(e)) : res(v as string)), true));
const fetchBuf = (s: NativeStore, k: string): Promise<ArrayBuffer> =>
  new Promise((res, rej) => s.fetch(k, (v, e) => (e ? rej(new Error(e)) : res(v as ArrayBuffer)), false));
const removeAll = (s: NativeStore): Promise<void> =>
  new Promise((res, rej) => s.removeAll(e => (e ? rej(new Error(e)) : res())));

function assert(cond: boolean, message: string): void {
  if (!cond) {
    throw new Error(`ASSERT FAILED: ${message}`);
  }
}

async function rejects(p: Promise<unknown>): Promise<boolean> {
  try {
    await p;
    return false;
  } catch {
    return true;
  }
}

// Resolves to how a promise settled within `ms`: 'resolved', 'rejected', or
// 'hang' if it never settled. Distinguishes a graceful rejection (good) from a
// dropped completion (bad).
function settleWithin(p: Promise<unknown>, ms: number): Promise<'resolved' | 'rejected' | 'hang'> {
  return new Promise(resolve => {
    const t = setTimeout(() => resolve('hang'), ms);
    p.then(
      () => {
        clearTimeout(t);
        resolve('resolved');
      },
      () => {
        clearTimeout(t);
        resolve('rejected');
      },
    );
  });
}

async function main(): Promise<void> {
  // 1. A string write is durable and restored after a reload.
  const backing = installFakeLocalStorage();
  const a1 = makeStore('storeA');
  await store(a1, 'greeting', 'hello world');
  assert(backing.size > 0, 'write landed in localStorage, not just memory');

  __resetInMemoryForTest(); // simulate page reload
  const a2 = makeStore('storeA');
  assert((await fetchStr(a2, 'greeting')) === 'hello world', 'string restored after reload');

  // 2. Binary values round-trip across a reload.
  const buf = new ArrayBuffer(8);
  const view = new Uint32Array(buf);
  view[0] = 42;
  view[1] = 84;
  await store(a2, 'blob', buf);
  __resetInMemoryForTest();
  const a3 = makeStore('storeA');
  const restored = new Uint32Array(await fetchBuf(a3, 'blob'));
  assert(restored[0] === 42 && restored[1] === 84, 'binary round-trips across reload');

  // 3. Stores are isolated by name.
  const b = makeStore('storeB');
  assert(await rejects(fetchStr(b, 'greeting')), 'stores are isolated by name');

  // 4. removeAll clears the persisted copy too.
  await removeAll(a3);
  __resetInMemoryForTest();
  const a4 = makeStore('storeA');
  assert(await rejects(fetchStr(a4, 'greeting')), 'removeAll cleared the persisted copy');

  // 5. The reserved key "__proto__" persists across a reload (a plain {} would
  //    drop it via prototype assignment + JSON.stringify).
  const p1 = makeStore('storeProto');
  await store(p1, '__proto__', 'safe');
  __resetInMemoryForTest();
  const p2 = makeStore('storeProto');
  assert((await fetchStr(p2, '__proto__')) === 'safe', '"__proto__" key survives a reload');

  // 6. Corrupt / externally-tampered localStorage is discarded gracefully -
  //    fetch must still settle (reject not-found), never hang the caller.
  for (const bad of ['null', 'not json', '{"k":{"b":123}}', '{"k":null}']) {
    (globalThis as any).localStorage.setItem('valdi.PersistentStore.storeCorrupt', bad);
    __resetInMemoryForTest();
    const corrupt = makeStore('storeCorrupt');
    // Either outcome is fine (not-found reject, or a coerced empty value); the
    // point is the completion fires and the caller never hangs.
    const outcome = await settleWithin(fetchStr(corrupt, 'k'), 2000);
    assert(outcome !== 'hang', `corrupt blob ${JSON.stringify(bad)} must not hang the caller, got ${outcome}`);
  }

  // 7. Falls back to memory (no throw) when localStorage is unavailable.
  delete (globalThis as any).localStorage;
  const c = makeStore('storeC');
  await store(c, 'k', 'v');
  assert((await fetchStr(c, 'k')) === 'v', 'memory fallback works without localStorage');

  // eslint-disable-next-line no-console
  console.log('PersistentStore web durability: all checks passed');
}

main().catch(err => {
  // eslint-disable-next-line no-console
  console.error(err);
  process.exit(1);
});
