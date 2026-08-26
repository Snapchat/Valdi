import { arrayToString, stringToArray } from 'coreutils/src/Uint8ArrayUtils';
import 'jasmine/src/jasmine';
import { PersistentStore } from 'persistence/src/PersistentStore';

function setPersistentStoreCurrentTime(store: PersistentStore, timeSeconds: number) {
  // Not exposing this through TypeScript to ensure it is not used outside of tests
  (store as any).native.setCurrentTime(timeSeconds);
}

function bufferToString(buf: ArrayBuffer): string {
  return arrayToString(new Uint8Array(buf));
}

function stringToBuffer(str: string): ArrayBuffer {
  return stringToArray(str).buffer;
}

let persistentStoreIndex = 0;
function makePersistentStoreId(encrypted: boolean): string {
  const index = persistentStoreIndex++;
  return `'com.snap.valdi${encrypted ? '.encrypted.' : '.'}PersistentStoreTest_${index}'`;
}

describe('PersistentStore', () => {
  it('canStoreAndRetrieve', async () => {
    const store = new PersistentStore(makePersistentStoreId(false), {
      disableBatchWrites: true,
      deviceGlobal: true,
    });

    const buffer = new ArrayBuffer(8);
    const view = new Uint32Array(buffer);
    view[0] = 42;
    view[1] = 84;
    await store.store('someBlob', buffer);

    const buffer2 = new ArrayBuffer(8);
    const view2 = new Uint32Array(buffer2);
    view2[0] = 1337;
    view2[1] = 7331;
    await store.store('someBlob2', buffer2);

    const blob = await store.fetch('someBlob');

    const fetchedView = new Uint32Array(blob);
    expect(fetchedView[0]).toBe(42);
    expect(fetchedView[1]).toBe(84);

    const blob2 = await store.fetch('someBlob2');

    const fetchedView2 = new Uint32Array(blob2);
    expect(fetchedView2[0]).toBe(1337);
    expect(fetchedView2[1]).toBe(7331);
  });

  it('failsOnUnknownKeys', async () => {
    const store = new PersistentStore(makePersistentStoreId(false), {
      disableBatchWrites: true,
      deviceGlobal: true,
    });

    await expectAsync(store.fetch('badKey')).toBeRejected();
  });

  it('canReplaceEntries', async () => {
    const store = new PersistentStore(makePersistentStoreId(false), {
      disableBatchWrites: true,
      deviceGlobal: true,
    });

    const buffer = new ArrayBuffer(8);
    const view = new Uint32Array(buffer);
    view[0] = 42;
    view[1] = 84;
    await store.store('someBlob', buffer);

    const blob = await store.fetch('someBlob');

    const fetchedView = new Uint32Array(blob);
    expect(fetchedView[0]).toBe(42);
    expect(fetchedView[1]).toBe(84);

    const buffer2 = new ArrayBuffer(8);
    const view2 = new Uint32Array(buffer2);
    view2[0] = 1337;
    view2[1] = 7331;
    await store.store('someBlob', buffer2);

    const blob2 = await store.fetch('someBlob');

    const fetchedView2 = new Uint32Array(blob2);
    expect(fetchedView2[0]).toBe(1337);
    expect(fetchedView2[1]).toBe(7331);
  });

  it('persistAcrossSessions', async () => {
    const storeID = makePersistentStoreId(false);
    const store1 = new PersistentStore(storeID, {
      disableBatchWrites: true,
      deviceGlobal: true,
    });

    const buffer = new ArrayBuffer(8);
    const view = new Uint32Array(buffer);
    view[0] = 42;
    view[1] = 84;
    await store1.store('someBlob', buffer);

    const store2 = new PersistentStore(storeID, {
      disableBatchWrites: true,
      deviceGlobal: true,
    });

    const blob = await store2.fetch('someBlob');

    const fetchedView = new Uint32Array(blob);
    expect(fetchedView[0]).toBe(42);
    expect(fetchedView[1]).toBe(84);
  });

  it('nameIsolateEntries', async () => {
    const storeID = makePersistentStoreId(false);
    const store1 = new PersistentStore(storeID, {
      disableBatchWrites: true,
      deviceGlobal: true,
    });

    const buffer = new ArrayBuffer(8);
    const view = new Uint32Array(buffer);
    view[0] = 42;
    view[1] = 84;
    await store1.store('someBlob', buffer);

    const store2 = new PersistentStore(makePersistentStoreId(false), {
      disableBatchWrites: true,
      deviceGlobal: true,
    });

    await expectAsync(store2.fetch('someBlob')).toBeRejected();
  });

  it('canDelete', async () => {
    const store = new PersistentStore(makePersistentStoreId(false), {
      disableBatchWrites: true,
      deviceGlobal: true,
    });

    const buffer = new ArrayBuffer(8);
    const view = new Uint32Array(buffer);
    view[0] = 42;
    view[1] = 84;
    await store.store('someBlob', buffer);

    // Should succeeed
    await store.fetch('someBlob');

    await store.remove('someBlob');

    // this should fail
    await expectAsync(store.fetch('someBlob')).toBeRejected();
  });

  it('canDeleteAll', async () => {
    const store = new PersistentStore(makePersistentStoreId(false), {
      disableBatchWrites: true,
      deviceGlobal: true,
    });

    const buffer = new ArrayBuffer(8);
    const view = new Uint32Array(buffer);
    view[0] = 42;
    view[1] = 84;
    await store.store('someBlob', buffer);

    const buffer2 = new ArrayBuffer(8);
    const view2 = new Uint32Array(buffer2);
    view2[0] = 1337;
    view2[1] = 7331;
    await store.store('someBlob2', buffer);

    await store.removeAll();

    await expectAsync(store.fetch('someBlob')).toBeRejected();
    await expectAsync(store.fetch('someBlob2')).toBeRejected();
  });

  it('canStoreAndFetchWithBatchEnabled', async () => {
    const store = new PersistentStore(makePersistentStoreId(false), { deviceGlobal: true });

    const buffer = new ArrayBuffer(8);
    const view = new Uint32Array(buffer);
    view[0] = 42;
    view[1] = 84;
    const promise1 = store.store('someBlob', buffer);

    const buffer2 = new ArrayBuffer(8);
    const view2 = new Uint32Array(buffer2);
    view2[0] = 1337;
    view2[1] = 7331;
    const promise2 = store.store('someBlob2', buffer2);

    await promise1;
    await promise2;

    const blob = await store.fetch('someBlob');

    const fetchedView = new Uint32Array(blob);
    expect(fetchedView[0]).toBe(42);
    expect(fetchedView[1]).toBe(84);

    const blob2 = await store.fetch('someBlob2');

    const fetchedView2 = new Uint32Array(blob2);
    expect(fetchedView2[0]).toBe(1337);
    expect(fetchedView2[1]).toBe(7331);
  });

  it('respects ttl when instance is alive', async () => {
    const store = new PersistentStore(makePersistentStoreId(false), { deviceGlobal: true });
    setPersistentStoreCurrentTime(store, 1);

    const buffer = new ArrayBuffer(8);

    await store.store('item', buffer, 1);
    await store.store('item2', buffer, 2);

    // We can fetch when it's still valid
    await store.fetch('item');
    await store.fetch('item2');

    // advance by 1 second
    setPersistentStoreCurrentTime(store, 2);

    // This should still work, since it's not expired
    await store.fetch('item2');

    await expectAsync(store.fetch('item')).toBeRejected();
  });

  it('respects ttl when restoring instance', async () => {
    const storeID = makePersistentStoreId(false);
    let store = new PersistentStore(storeID, {
      mockedTime: 1,
      deviceGlobal: true,
    } as any);

    const buffer = new ArrayBuffer(8);
    await store.store('item', buffer, 2);
    await store.store('item2', buffer, 3);

    store = new PersistentStore(storeID, { mockedTime: 2, deviceGlobal: true } as any);

    // Should still work as the item has not expired
    await store.fetch('item');
    await store.fetch('item2');

    store = new PersistentStore(storeID, { mockedTime: 3, deviceGlobal: true } as any);

    await store.fetch('item2');

    await expectAsync(store.fetch('item')).toBeRejected();
  });

  it('can restore encrypted', async () => {
    const storeID = makePersistentStoreId(true);
    let store = new PersistentStore(storeID, {
      mockedUserId: '424242',
    } as any);

    const buffer = stringToBuffer('Hello world');

    await store.store('item', buffer);

    store = new PersistentStore(storeID, {
      mockedUserId: '424242',
    } as any);

    const fetchedResult = await store.fetch('item');
    const strResult = bufferToString(fetchedResult);

    expect(strResult).toEqual('Hello world');
  });

  it('fails to restore encrypted if user id changes', async () => {
    const storeID = makePersistentStoreId(true);
    let store = new PersistentStore(storeID, {
      mockedUserId: '424242',
    } as any);

    const buffer = stringToBuffer('Hello world');

    await store.store('item', buffer);

    store = new PersistentStore(storeID, {
      mockedUserId: '434343',
    } as any);

    await expectAsync(store.fetch('item')).toBeRejected();
  });

  it('can fetchAll', async () => {
    const store = new PersistentStore(makePersistentStoreId(false), {
      deviceGlobal: true,
    });

    await store.removeAll();

    const data1 = stringToBuffer('test1');
    const data2 = stringToBuffer('test2');
    const data3 = stringToBuffer('test3');

    await store.store('item1', data1);
    await store.store('item2', data2);
    await store.store('item3', data3);

    const items = await store.fetchAll();

    expect(items).toEqual(['item1', data1, 'item2', data2, 'item3', data3]);
  });

  it('can use weight', async () => {
    const store = new PersistentStore(makePersistentStoreId(false), {
      deviceGlobal: true,
      maxWeight: 2,
    });

    await store.removeAll();

    const data1 = stringToBuffer('test1');
    const data2 = stringToBuffer('test2');
    const data3 = stringToBuffer('test3');

    await store.store('item1', data1, undefined, 1);
    await store.store('item2', data2, undefined, 1);
    await store.store('item3', data3, undefined, 1);

    const items = await store.fetchAll();

    expect(items).toEqual(['item2', data2, 'item3', data3]);
  });
});
