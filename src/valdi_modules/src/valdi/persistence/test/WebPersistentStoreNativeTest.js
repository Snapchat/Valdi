#!/usr/bin/env node

'use strict';

const assert = require('node:assert/strict');
const path = require('node:path');

class FakeStorage {
  constructor(options) {
    this.entries = new Map();
    this.orderedKeys = [];
    this.rejectWrites = options?.rejectWrites === true;
    this.keyCalls = 0;
    this.writeCalls = 0;
  }

  get length() {
    return this.orderedKeys.length;
  }

  getItem(key) {
    return this.entries.get(key) ?? null;
  }

  key(index) {
    this.keyCalls++;
    return this.orderedKeys[index] ?? null;
  }

  removeItem(key) {
    if (!this.entries.delete(key)) {
      return;
    }
    const index = this.orderedKeys.indexOf(key);
    if (index !== -1) {
      this.orderedKeys.splice(index, 1);
    }
  }

  setItem(key, value) {
    if (this.rejectWrites) {
      throw new Error('SecurityError: browser storage is blocked');
    }
    this.writeCalls++;
    if (!this.entries.has(key)) {
      this.orderedKeys.push(key);
    }
    this.entries.set(key, value);
  }
}

const browserStorage = new FakeStorage();
globalThis.localStorage = browserStorage;

const runfiles = process.env.RUNFILES_DIR || process.env.RUNFILES;
assert.ok(runfiles, 'Bazel RUNFILES_DIR must be available');

const candidateModulePaths = [
  path.join(runfiles, '_main/src/valdi_modules/src/valdi/persistence/web/PersistentStoreNative.js'),
  path.join(runfiles, 'valdi/src/valdi_modules/src/valdi/persistence/web/PersistentStoreNative.js'),
  path.join(runfiles, '+local_repos+valdi/src/valdi_modules/src/valdi/persistence/web/PersistentStoreNative.js'),
];

let binding;
let bindingPath;
for (const candidateModulePath of candidateModulePaths) {
  try {
    binding = require(candidateModulePath);
    bindingPath = candidateModulePath;
    break;
  } catch (error) {
    if (error.code !== 'MODULE_NOT_FOUND' || !String(error.message).includes(candidateModulePath)) {
      throw error;
    }
  }
}
assert.ok(binding, `Compiled persistence module not found in ${candidateModulePaths.join(', ')}`);

function reloadBinding() {
  delete require.cache[require.resolve(bindingPath)];
  binding = require(bindingPath);
}

function makeStore(name, options) {
  const resolved = options ?? {};
  return binding.newPersistentStore(
    name,
    resolved.disableBatchWrites ?? false,
    resolved.userScoped ?? false,
    resolved.maxWeight ?? 0,
    resolved.time,
    resolved.userId,
    resolved.encrypted,
  );
}

function storeValue(store, key, value, ttl, weight) {
  return new Promise((resolve, reject) => {
    store.store(key, value, ttl, weight, error => (error ? reject(new Error(error)) : resolve()));
  });
}

function fetchValue(store, key, asString) {
  return new Promise((resolve, reject) => {
    store.fetch(key, (value, error) => (error ? reject(new Error(error)) : resolve(value)), asString);
  });
}

function exists(store, key) {
  return new Promise(resolve => store.exists(key, resolve));
}

function fetchAll(store) {
  return new Promise(resolve => store.fetchAll(resolve));
}

function removeAll(store) {
  return new Promise((resolve, reject) => {
    store.removeAll(error => (error ? reject(new Error(error)) : resolve()));
  });
}

async function run() {
  assert.equal(typeof binding.newPersistentStore, 'function', 'web binding must export the native factory');
  assert.equal(typeof binding.getPersistentStoreDiagnostics, 'function', 'persistence diagnostics must be available');

  const settings = makeStore('settings');
  await storeValue(settings, 'theme', 'dark');
  assert.equal(await fetchValue(makeStore('settings'), 'theme', true), 'dark', 'values must survive new instances');
  assert.ok(globalThis.localStorage.length > 0, 'values must reach durable browser storage');

  const writesBeforeUnweightedRead = browserStorage.writeCalls;
  await fetchValue(settings, 'theme', true);
  assert.equal(browserStorage.writeCalls, writesBeforeUnweightedRead, 'unweighted reads must not rewrite storage');

  const history = makeStore('history');
  await storeValue(history, 'theme', 'conversation');
  assert.equal(await fetchValue(settings, 'theme', true), 'dark', 'named stores must not collide');
  await removeAll(settings);
  assert.equal(await fetchValue(history, 'theme', true), 'conversation', 'removeAll must stay inside its namespace');

  assert.throws(
    () => makeStore('messages', { userScoped: true }),
    /requires an authenticated userId/,
    'user-scoped stores must fail closed without an authenticated identity',
  );
  assert.throws(
    () => makeStore('messages', { userScoped: true, userId: '  ' }),
    /requires an authenticated userId/,
    'blank identities must not share a default user namespace',
  );
  const firstUser = makeStore('messages', { userScoped: true, userId: 'alice' });
  const secondUser = makeStore('messages', { userScoped: true, userId: 'bob' });
  await storeValue(firstUser, 'draft', 'private');
  await assert.rejects(fetchValue(secondUser, 'draft', true), /not found/, 'user scopes must not share entries');
  assert.equal(
    await fetchValue(makeStore('messages', { userScoped: true, userId: 'alice' }), 'draft', true),
    'private',
  );

  const bytes = new Uint8Array([0, 7, 127, 128, 255]).buffer;
  await storeValue(history, 'binary', bytes);
  assert.deepEqual(new Uint8Array(await fetchValue(makeStore('history'), 'binary', false)), new Uint8Array(bytes));

  const expiring = makeStore('expiration', { time: 100 });
  await storeValue(expiring, 'short', 'temporary', 2);
  assert.equal(await fetchValue(makeStore('expiration', { time: 101 }), 'short', true), 'temporary');
  await assert.rejects(fetchValue(makeStore('expiration', { time: 102 }), 'short', true), /not found/);

  const cache = makeStore('cache', { maxWeight: 2 });
  await storeValue(cache, 'first', 'one', undefined, 1);
  await storeValue(cache, 'second', 'two', undefined, 1);
  await fetchValue(cache, 'first', true);
  await storeValue(cache, 'third', 'three', undefined, 1);
  await assert.rejects(fetchValue(cache, 'second', true), /not found/, 'the least recently used entry must be evicted');
  assert.equal(await fetchValue(cache, 'first', true), 'one');
  assert.equal(await fetchValue(cache, 'third', true), 'three');
  assert.deepEqual(await fetchAll(cache), ['first', 'one', 'third', 'three']);

  const reloadCache = makeStore('reload-cache', { maxWeight: 2 });
  await storeValue(reloadCache, 'older', 'old', undefined, 1);
  await storeValue(reloadCache, 'recent', 'recent', undefined, 1);
  await fetchValue(reloadCache, 'older', true);
  reloadBinding();
  const restoredCache = makeStore('reload-cache', { maxWeight: 2 });
  await fetchValue(restoredCache, 'recent', true);
  await storeValue(restoredCache, 'new', 'new', undefined, 1);
  await assert.rejects(fetchValue(restoredCache, 'older', true), /not found/, 'reload must preserve true LRU order');
  assert.equal(await fetchValue(restoredCache, 'recent', true), 'recent', 'recently read entries must survive reload');

  const indexedCache = makeStore('indexed-cache', { maxWeight: 256 });
  await storeValue(indexedCache, 'initial', 'value', undefined, 1);
  const keyCallsAfterInitialization = browserStorage.keyCalls;
  for (let index = 0; index < 96; index++) {
    await storeValue(indexedCache, `item-${index}`, 'value', undefined, 1);
  }
  assert.equal(browserStorage.keyCalls, keyCallsAfterInitialization, 'bounded writes must not rescan browser storage');

  const batchingStore = makeStore('batching');
  const writesBeforeBatch = browserStorage.writeCalls;
  await Promise.all([
    storeValue(batchingStore, 'same-key', 'first'),
    storeValue(batchingStore, 'same-key', 'second'),
    storeValue(batchingStore, 'same-key', 'final'),
  ]);
  assert.equal(browserStorage.writeCalls - writesBeforeBatch, 1, 'same-turn writes must coalesce');
  assert.equal(await fetchValue(batchingStore, 'same-key', true), 'final');

  const immediateStore = makeStore('immediate', { disableBatchWrites: true });
  const writesBeforeImmediate = browserStorage.writeCalls;
  await Promise.all([
    storeValue(immediateStore, 'same-key', 'first'),
    storeValue(immediateStore, 'same-key', 'second'),
  ]);
  assert.equal(browserStorage.writeCalls - writesBeforeImmediate, 2, 'disableBatchWrites must preserve each write');

  const diagnostics = binding.getPersistentStoreDiagnostics();
  assert.ok(diagnostics.coalescedWrites >= 2, 'diagnostics must report write coalescing');
  assert.ok(diagnostics.evictions >= 1, 'diagnostics must report LRU eviction');

  const blockedStorage = new FakeStorage({ rejectWrites: true });
  globalThis.localStorage = blockedStorage;
  const warnings = [];
  const previousWarn = console.warn;
  console.warn = warning => warnings.push(String(warning));
  try {
    const restrictedStore = makeStore('restricted');
    await storeValue(restrictedStore, 'value', 'available in memory');
    assert.equal(await fetchValue(makeStore('restricted'), 'value', true), 'available in memory');
    assert.equal(warnings.length, 1, 'restricted storage must produce one actionable warning');
    assert.match(warnings[0], /non-persistent memory.*SecurityError/);
    assert.ok(binding.getPersistentStoreDiagnostics().fallbackStores >= 2);
  } finally {
    console.warn = previousWarn;
    globalThis.localStorage = browserStorage;
  }

  assert.throws(() => makeStore('secrets', { encrypted: true }), /Encrypted PersistentStore is unavailable on web/);

  console.log(
    'PASS: browser durability, authenticated isolation, reload-safe indexed LRU, batching, fallback, and diagnostics',
  );
}

run().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
