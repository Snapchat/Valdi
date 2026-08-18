import { IProviderSource } from './IProviderSource';
import { ProviderSourceHead, ProviderSourceList } from './ProviderSource';

export type GlobalProviderSourceId = number;

type GlobalProviderSourceIndex = { [key: GlobalProviderSourceId]: IProviderSource | undefined };

interface GlobalProviderSourceStore {
  index: GlobalProviderSourceIndex;
  sequence: number;
}

interface GlobalProviderSourceStoreAccessor {
  $globalProviderSourceStore?: GlobalProviderSourceStore;
}

const globalProviderSourceStoreAccessor = globalThis as typeof globalThis & GlobalProviderSourceStoreAccessor;

function getOrCreateGlobalProviderSourceStore(): GlobalProviderSourceStore {
  let store = globalProviderSourceStoreAccessor.$globalProviderSourceStore;
  if (!store) {
    store = {
      index: {},
      sequence: 0,
    };
    globalProviderSourceStoreAccessor.$globalProviderSourceStore = store;
  }
  return store;
}

/**
 * Retrieves a previously registered global ProviderSource by its id.
 */
export function getGlobalProviderSource(globalProviderSourceId: GlobalProviderSourceId): IProviderSource | undefined {
  return getOrCreateGlobalProviderSourceStore().index[globalProviderSourceId];
}

/**
 * Retrieves a known previously registered global ProviderSource by its id
 */
export function getGlobalProviderSourceOrThrow(globalProviderSourceId: GlobalProviderSourceId): IProviderSource {
  const globalProviderSource = getGlobalProviderSource(globalProviderSourceId);
  if (!globalProviderSource) {
    throw new Error(`Could not resolve global ProviderSource with ${globalProviderSourceId}`);
  }
  return globalProviderSource;
}

/**
 * Register a ProviderSource as part of the global ProviderSource.
 * @returns the generated id for the Global ProviderSource
 */
export function registerGlobalProviderSource(providerSource: IProviderSource): GlobalProviderSourceId {
  const store = getOrCreateGlobalProviderSourceStore();

  const id = ++store.sequence;
  store.index[id] = providerSource;

  return id;
}

/**
 * @ExportFunction
 * Unregister a previously registered global ProviderSource, making it unreachable through the getGlobalProviderSource() API.
 */
export function unregisterGlobalProviderSource(globalProviderSourceId: GlobalProviderSourceId) {
  const store = getOrCreateGlobalProviderSourceStore();
  delete store.index[globalProviderSourceId];
}

/**
 * Create and register a Global ProviderSource, which can later be retrieved through the getGlobalProviderSource() method
 * @param parentGlobalProviderSourceId if provided, the newly created ProviderSource will inherit from the given parent ProviderSource
 * @param builder A builder function which will be called with the starting ProviderSource. The builder should set any values it wants
 * to set in the ProviderSource and return the result.
 * @returns The id for the newly created global ProviderSource
 */
export function createGlobalProviderSource(
  parentGlobalProviderSourceId: GlobalProviderSourceId | undefined,
  builder: (source: IProviderSource) => IProviderSource,
): GlobalProviderSourceId {
  let startingProviderSource: IProviderSource;
  if (parentGlobalProviderSourceId) {
    startingProviderSource = getGlobalProviderSourceOrThrow(parentGlobalProviderSourceId);
  } else {
    startingProviderSource = new ProviderSourceHead();
  }

  const providerSource = builder(startingProviderSource);
  return registerGlobalProviderSource(providerSource);
}

/**
 * @ExportFunction
 * Combine a list of Global ProviderSource into a new global ProviderSource.
 */
export function combineGlobalProviderSource(globalProviderSourceIds: GlobalProviderSourceId[]): GlobalProviderSourceId {
  const providerSources = globalProviderSourceIds.map(getGlobalProviderSourceOrThrow);
  return registerGlobalProviderSource(new ProviderSourceList(providerSources));
}
