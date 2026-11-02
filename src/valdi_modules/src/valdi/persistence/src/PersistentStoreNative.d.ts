import { PropertyList } from 'valdi_core/src/utils/PropertyList';

export interface PersistentStoreNative {
  store(
    key: string,
    value: ArrayBuffer | string,
    ttlSeconds: number | undefined,
    weight: number | undefined,
    completion: (error?: string) => void,
  ): void;
  fetch(key: string, completion: (value?: ArrayBuffer | string, error?: string) => void, asString: boolean): void;
  fetchAll(completion: (value: PropertyList) => void): void;
  exists(key: string, completion: (exists: boolean) => void): void;
  remove(key: string, completion: (error?: string) => void): void;
  removeAll(completion: (error?: string) => void): void;
  setCurrentTime(timeSeconds: number): void;
}
