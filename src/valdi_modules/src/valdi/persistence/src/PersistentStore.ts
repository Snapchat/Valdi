import { toError } from 'valdi_core/src/utils/ErrorUtils';
import { makeSingleCallInterruptibleCallback } from 'valdi_core/src/utils/FunctionUtils';
import { PropertyList } from 'valdi_core/src/utils/PropertyList';
import { PersistentStoreNative } from './PersistentStoreNative';

declare function require(path: string): any;
const nativeCreate: (
  name: string,
  disableBatchWrites: boolean,
  userScoped: boolean,
  maxWeight: number,
  mockedTime: number | undefined,
  mockedUserId: string | undefined,
  enableEncryption: boolean | undefined,
) => PersistentStoreNative = require('PersistentStoreNative').newPersistentStore;

export interface PersistentStoreOptions {
  /**
   * By default, save operations are batched to minimize the number of disk IO.
   * This can be set to true to disable this behavior if you wish writes to
   * happen immediately as soon as store() or remove() are called.
   */
  disableBatchWrites?: boolean;

  /**
   * Whether the store should be available globally across
   * all user sessions, instead of being scoped to the user.
   * The data will not be encrypted when this flag is set.
   */
  deviceGlobal?: boolean;

  /**
   * If set, the store will act like an LRU cache where items are
   * removed as needed to ensure the accumulated weight of all
   * items stay below this value. If you set this value to something else
   * than undefined or zero, you should make sure you also provide the weight
   * when calling store().
   */
  maxWeight?: number;

  /**
   * Set to `true` when storing sensitive data.
   * ie: credit card info, secret keys, authentication cookies
   *
   * Setting to `false` is preffered for most applications as there will be some performance gains.
   */
  enableEncryption?: boolean;
}

/**
 * A persistent store which allows to store data which persists
 * across app sessions.
 */
export class PersistentStore {
  private native: PersistentStoreNative;

  /**
   * Create a new PersistentStore with the given name.
   */
  constructor(readonly name: string, options?: PersistentStoreOptions) {
    const disableBatchWrites = options?.disableBatchWrites;
    const deviceGlobal = options?.deviceGlobal === true;
    const mockedTime = (options as any)?.mockedTime;
    const mockedUserId = (options as any)?.mockedUserId;

    const userScoped = deviceGlobal === false;
    this.native = nativeCreate(
      name,
      disableBatchWrites ?? false,
      userScoped,
      options?.maxWeight ?? 0,
      mockedTime,
      mockedUserId,
      options?.enableEncryption,
    );
  }

  /**
   * Store the blob with the given key
   */
  store(key: string, value: ArrayBuffer, ttlSeconds?: number, weight?: number): Promise<void> {
    return this.storeInner(key, value, ttlSeconds, weight);
  }

  /**
   * Store the string blob with the given key
   */
  storeString(key: string, value: string, ttlSeconds?: number, weight?: number): Promise<void> {
    return this.storeInner(key, value, ttlSeconds, weight);
  }

  /**
   * Fetch the blob for the given key
   */
  fetch(key: string): Promise<ArrayBuffer> {
    return this.fetchInner(key, false) as Promise<ArrayBuffer>;
  }

  /**
   * Fetch the string blob for the given key
   */
  fetchString(key: string): Promise<string> {
    return this.fetchInner(key, true) as Promise<string>;
  }

  private storeInner(key: string, value: ArrayBuffer | string, ttlSeconds?: number, weight?: number): Promise<void> {
    return new Promise<void>((resolve, reject) => {
      this.native.store(
        key,
        value,
        ttlSeconds,
        weight,
        makeSingleCallInterruptibleCallback(err => {
          if (err) {
            reject(toError(err));
          } else {
            resolve();
          }
        }),
      );
    });
  }

  private fetchInner(key: string, asString: boolean): Promise<ArrayBuffer | string> {
    return new Promise((resolve, reject) => {
      this.native.fetch(
        key,
        makeSingleCallInterruptibleCallback((value, err) => {
          if (value !== undefined) {
            resolve(value);
          } else {
            reject(toError(err));
          }
        }),
        asString,
      );
    });
  }

  /**
   * Test whether a blob for the given key exists
   */
  exists(key: string): Promise<boolean> {
    return new Promise<boolean>(resolve => {
      this.native.exists(
        key,
        makeSingleCallInterruptibleCallback(value => {
          resolve(value);
        }),
      );
    });
  }

  /**
   * Remove the blob for the given key
   */
  remove(key: string): Promise<void> {
    return new Promise<void>((resolve, reject) => {
      this.native.remove(
        key,
        makeSingleCallInterruptibleCallback(err => {
          if (!err) {
            resolve();
          } else {
            reject(toError(err));
          }
        }),
      );
    });
  }

  /**
   * Remove all blobs held by this PersistentStore
   */
  removeAll(): Promise<void> {
    return new Promise<void>((resolve, reject) => {
      this.native.removeAll(
        makeSingleCallInterruptibleCallback(err => {
          if (!err) {
            resolve();
          } else {
            reject(toError(err));
          }
        }),
      );
    });
  }

  /**
   * Fetch all the items as a PropertyList.
   */
  fetchAll(): Promise<PropertyList> {
    return new Promise<PropertyList>(resolve => {
      this.native.fetchAll(makeSingleCallInterruptibleCallback(resolve));
    });
  }
}

// A helper function to create a PersistentStore.
// This is useful for testing purposes.
export const createPersistentStoreUtil = {
  createStore: (name: string, options?: PersistentStoreOptions): PersistentStore => {
    return new PersistentStore(name, options);
  },
};
