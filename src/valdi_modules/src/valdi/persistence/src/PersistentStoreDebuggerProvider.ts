import {
  createDebuggerProviderOwner,
  createDebuggerProviderResult,
  DebuggerProviderKind,
} from 'valdi_core/src/debugging/DebuggerProvider';
import type {
  DebuggerProvider,
  DebuggerProviderModule,
  DebuggerProviderOwner,
  DebuggerProviderRequest,
  DebuggerProviderResult,
} from 'valdi_core/src/debugging/DebuggerProvider';

declare const module: { readonly path?: string };
declare function require(path: string): unknown;

const PERSISTENT_STORE_PROVIDER_ID = 'persistent-store';
const PERSISTENT_STORE_PROVIDER_OWNER_KEY = 'persistence/src/PersistentStoreDebuggerProvider';
const MAX_PROVIDER_DOCUMENT_BYTES = 43 * 1024;
const MAX_PROVIDER_STORES = 100;
const MAX_PROVIDER_ENTRIES_PER_STORE = 100;
const MAX_PROVIDER_ENTRIES = 500;
const MAX_DEBUGGER_STRING_CHARACTERS = 32 * 1024;
const MAX_ERROR_CHARACTERS = 1024;
const MAX_KEY_CHARACTERS = 1024;
const MAX_NAME_CHARACTERS = 512;

interface OwnDataProperty {
  readonly present: boolean;
  readonly valid: boolean;
  readonly value?: unknown;
}

interface BoundedStringProperty {
  readonly originalLength?: number;
  readonly truncated: boolean;
  readonly value?: string;
}

interface ProjectionState {
  invalidFields: number;
  outputEntries: number;
  sourceEntries: number;
  sourceEntryCountIncomplete: boolean;
  sourceStores: number;
  stoppedForBudget: boolean;
  truncatedFields: number;
}

interface SourceStore {
  readonly entries?: readonly unknown[];
  readonly entryCount: number;
  readonly store: object;
}

interface ProjectedEntry {
  readonly output: Record<string, unknown>;
  readonly valueTruncatedByCharacterLimit: boolean;
}

interface PersistentStoreProjectionMetadata {
  entriesOmitted: number;
  readonly entryLimit: number;
  invalidFields: number;
  readonly maxDocumentBytes: number;
  readonly returnedEntries: number;
  readonly returnedStores: number;
  readonly sourceEntries: number;
  readonly sourceEntryCountIncomplete?: boolean;
  readonly sourceStores: number;
  readonly storeLimit: number;
  storesOmitted: number;
  truncated: boolean;
  truncatedFields: number;
}

interface PersistentStoreProjection {
  diagnostics?: Record<string, unknown>;
  inspectedStorageKeys?: number;
  limits?: Record<string, number>;
  projection: PersistentStoreProjectionMetadata;
  rejectedStorageKeys?: number;
  storageError?: string;
  storageErrorLength?: number;
  storageErrorTruncated?: boolean;
  storageInspectionTruncated?: boolean;
  stores: Record<string, unknown>[];
  truncated: boolean;
  usage?: Record<string, number>;
}

function ownDataProperty(value: unknown, key: PropertyKey): OwnDataProperty {
  if ((typeof value !== 'object' && typeof value !== 'function') || value === null) {
    return { present: false, valid: false };
  }
  let descriptor: PropertyDescriptor | undefined;
  try {
    descriptor = Object.getOwnPropertyDescriptor(value, key);
  } catch {
    return { present: false, valid: false };
  }
  if (descriptor === undefined) return { present: false, valid: true };
  if (!('value' in descriptor)) return { present: true, valid: false };
  return { present: true, valid: true, value: descriptor.value };
}

function noteInvalid(state: ProjectionState, property: OwnDataProperty): void {
  if (!property.valid) state.invalidFields++;
}

function safeArray(value: unknown): readonly unknown[] | undefined {
  try {
    return Array.isArray(value) ? value : undefined;
  } catch {
    return undefined;
  }
}

function safeArrayLength(value: readonly unknown[], state: ProjectionState): number {
  const length = ownDataProperty(value, 'length');
  if (!length.valid || typeof length.value !== 'number' || !Number.isInteger(length.value) || length.value < 0) {
    state.invalidFields++;
    state.sourceEntryCountIncomplete = true;
    return 0;
  }
  return length.value;
}

function safeArrayItem(value: readonly unknown[], index: number, state: ProjectionState): unknown {
  const item = ownDataProperty(value, index.toString());
  noteInvalid(state, item);
  return item.valid ? item.value : undefined;
}

function safeStringPrefix(value: string, maximumCharacters: number): string {
  let end = Math.min(value.length, maximumCharacters);
  if (
    end > 0 &&
    end < value.length &&
    value.charCodeAt(end - 1) >= 0xd800 &&
    value.charCodeAt(end - 1) <= 0xdbff &&
    value.charCodeAt(end) >= 0xdc00 &&
    value.charCodeAt(end) <= 0xdfff
  ) {
    end--;
  }
  return value.slice(0, end);
}

function boundedStringProperty(
  value: object,
  key: PropertyKey,
  maximumCharacters: number,
  state: ProjectionState,
): BoundedStringProperty {
  const property = ownDataProperty(value, key);
  noteInvalid(state, property);
  if (!property.present || !property.valid) return { truncated: false };
  if (typeof property.value !== 'string') {
    state.invalidFields++;
    return { truncated: false };
  }
  const bounded = safeStringPrefix(property.value, maximumCharacters);
  const truncated = bounded.length !== property.value.length;
  if (truncated) state.truncatedFields++;
  return { originalLength: property.value.length, truncated, value: bounded };
}

function stringProperty(
  value: object,
  key: PropertyKey,
  maximumCharacters: number,
  state: ProjectionState,
): string | undefined {
  return boundedStringProperty(value, key, maximumCharacters, state).value;
}

function booleanProperty(value: object, key: PropertyKey, state: ProjectionState): boolean | undefined {
  const property = ownDataProperty(value, key);
  noteInvalid(state, property);
  if (!property.present || !property.valid) return undefined;
  if (typeof property.value !== 'boolean') {
    state.invalidFields++;
    return undefined;
  }
  return property.value;
}

function numberProperty(value: object, key: PropertyKey, state: ProjectionState): number | undefined {
  const property = ownDataProperty(value, key);
  noteInvalid(state, property);
  if (!property.present || !property.valid) return undefined;
  if (typeof property.value !== 'number' || !Number.isFinite(property.value)) {
    state.invalidFields++;
    return undefined;
  }
  return property.value;
}

function copyOptionalString(
  output: Record<string, unknown>,
  source: object,
  key: string,
  maximumCharacters: number,
  state: ProjectionState,
): void {
  const value = stringProperty(source, key, maximumCharacters, state);
  if (value !== undefined) output[key] = value;
}

function copyOptionalBoolean(
  output: Record<string, unknown>,
  source: object,
  key: string,
  state: ProjectionState,
): void {
  const value = booleanProperty(source, key, state);
  if (value !== undefined) output[key] = value;
}

function copyOptionalNumber(
  output: Record<string, unknown>,
  source: object,
  key: string,
  state: ProjectionState,
): void {
  const value = numberProperty(source, key, state);
  if (value !== undefined) output[key] = value;
}

function knownNumberRecord(
  source: object,
  keys: readonly string[],
  state: ProjectionState,
): Record<string, number> | undefined {
  const output: Record<string, number> = Object.create(null) as Record<string, number>;
  let count = 0;
  keys.forEach(key => {
    const value = numberProperty(source, key, state);
    if (value !== undefined) {
      output[key] = value;
      count++;
    }
  });
  return count === 0 ? undefined : output;
}

function diagnosticsRecord(source: object, state: ProjectionState): Record<string, unknown> | undefined {
  const hydratedStores = numberProperty(source, 'hydratedStores', state);
  const memoryStores = numberProperty(source, 'memoryStores', state);
  const storageAvailable = booleanProperty(source, 'storageAvailable', state);
  if (hydratedStores === undefined && memoryStores === undefined && storageAvailable === undefined) return undefined;
  return {
    ...(hydratedStores === undefined ? {} : { hydratedStores }),
    ...(memoryStores === undefined ? {} : { memoryStores }),
    ...(storageAvailable === undefined ? {} : { storageAvailable }),
  };
}

function objectProperty(value: object, key: PropertyKey, state: ProjectionState): object | undefined {
  const property = ownDataProperty(value, key);
  noteInvalid(state, property);
  if (!property.present || !property.valid) return undefined;
  if (typeof property.value !== 'object' || property.value === null || safeArray(property.value) !== undefined) {
    state.invalidFields++;
    return undefined;
  }
  return property.value;
}

function sourceStores(snapshot: object, state: ProjectionState): SourceStore[] {
  const storesProperty = ownDataProperty(snapshot, 'stores');
  noteInvalid(state, storesProperty);
  const stores = safeArray(storesProperty.value);
  if (!storesProperty.present || stores === undefined) {
    if (storesProperty.present && storesProperty.valid) state.invalidFields++;
    return [];
  }
  const sourceStoreCount = safeArrayLength(stores, state);
  state.sourceStores = sourceStoreCount;
  if (sourceStoreCount > MAX_PROVIDER_STORES) state.sourceEntryCountIncomplete = true;
  const output: SourceStore[] = [];
  const inspectedStoreCount = Math.min(sourceStoreCount, MAX_PROVIDER_STORES);
  for (let index = 0; index < inspectedStoreCount; index++) {
    const storeValue = safeArrayItem(stores, index, state);
    if (typeof storeValue !== 'object' || storeValue === null) {
      state.invalidFields++;
      state.sourceEntryCountIncomplete = true;
      continue;
    }
    const entriesProperty = ownDataProperty(storeValue, 'entries');
    noteInvalid(state, entriesProperty);
    const entries = safeArray(entriesProperty.value);
    if (entriesProperty.present && entriesProperty.valid && entries === undefined) state.invalidFields++;
    if (entries === undefined) state.sourceEntryCountIncomplete = true;
    const entryCount = entries === undefined ? 0 : safeArrayLength(entries, state);
    state.sourceEntries += entryCount;
    output.push({ entries, entryCount, store: storeValue });
  }
  return output;
}

function projectedEntry(source: unknown, state: ProjectionState): ProjectedEntry {
  const output: Record<string, unknown> = Object.create(null) as Record<string, unknown>;
  if (typeof source !== 'object' || source === null) {
    state.invalidFields++;
    output.encoding = 'unknown';
    output.key = '';
    output.value = '';
    output.unavailableReason = 'invalid-debug-entry';
    return { output, valueTruncatedByCharacterLimit: false };
  }
  const encoding = numberProperty(source, 'encoding', state);
  const key = boundedStringProperty(source, 'key', MAX_KEY_CHARACTERS, state);
  const value = boundedStringProperty(source, 'value', MAX_DEBUGGER_STRING_CHARACTERS, state);
  output.encoding = encoding === undefined ? 'unknown' : encoding;
  output.key = key.value ?? '';
  output.value = value.value ?? '';
  copyOptionalNumber(output, source, 'expiresAt', state);
  copyOptionalNumber(output, source, 'keyLength', state);
  copyOptionalBoolean(output, source, 'keyTruncated', state);
  copyOptionalString(output, source, 'unavailableReason', MAX_ERROR_CHARACTERS, state);
  copyOptionalNumber(output, source, 'valueLength', state);
  copyOptionalBoolean(output, source, 'valueTruncated', state);
  copyOptionalNumber(output, source, 'weight', state);
  if (key.truncated) {
    output.keyLength = Math.max(typeof output.keyLength === 'number' ? output.keyLength : 0, key.originalLength ?? 0);
    output.keyTruncated = true;
  }
  if (value.truncated) {
    output.valueLength = Math.max(
      typeof output.valueLength === 'number' ? output.valueLength : 0,
      value.originalLength ?? 0,
    );
    output.valueTruncated = true;
  }
  return { output, valueTruncatedByCharacterLimit: value.truncated };
}

function projectedStore(source: object, state: ProjectionState): Record<string, unknown> {
  const output: Record<string, unknown> = Object.create(null) as Record<string, unknown>;
  output.backend = stringProperty(source, 'backend', 64, state) ?? 'unknown';
  output.entries = [];
  output.inspectedEntries = numberProperty(source, 'inspectedEntries', state) ?? 0;
  output.name = stringProperty(source, 'name', MAX_NAME_CHARACTERS, state) ?? 'Storage';
  copyOptionalBoolean(output, source, 'entriesTruncated', state);
  copyOptionalString(output, source, 'error', MAX_ERROR_CHARACTERS, state);
  copyOptionalNumber(output, source, 'errorLength', state);
  copyOptionalBoolean(output, source, 'errorTruncated', state);
  copyOptionalBoolean(output, source, 'inspectionTruncated', state);
  copyOptionalNumber(output, source, 'nameLength', state);
  copyOptionalBoolean(output, source, 'nameTruncated', state);
  copyOptionalNumber(output, source, 'serializedLength', state);
  return output;
}

function utf8ByteLength(value: string): number {
  let bytes = 0;
  for (let index = 0; index < value.length; index++) {
    const code = value.charCodeAt(index);
    if (code <= 0x7f) bytes++;
    else if (code <= 0x7ff) bytes += 2;
    else if (code >= 0xd800 && code <= 0xdbff && index + 1 < value.length) {
      const next = value.charCodeAt(index + 1);
      if (next >= 0xdc00 && next <= 0xdfff) {
        bytes += 4;
        index++;
      } else bytes += 3;
    } else bytes += 3;
  }
  return bytes;
}

function projectionFits(output: PersistentStoreProjection): boolean {
  return utf8ByteLength(JSON.stringify(output)) <= MAX_PROVIDER_DOCUMENT_BYTES;
}

function updateProjectionMetadata(
  output: PersistentStoreProjection,
  state: ProjectionState,
  sourceTruncated: boolean,
): void {
  const storesOmitted = Math.max(0, state.sourceStores - output.stores.length);
  const entriesOmitted = Math.max(0, state.sourceEntries - state.outputEntries);
  const truncated =
    sourceTruncated ||
    storesOmitted > 0 ||
    entriesOmitted > 0 ||
    state.invalidFields > 0 ||
    state.sourceEntryCountIncomplete ||
    state.stoppedForBudget ||
    state.truncatedFields > 0;
  output.projection = {
    entriesOmitted,
    entryLimit: MAX_PROVIDER_ENTRIES,
    invalidFields: state.invalidFields,
    maxDocumentBytes: MAX_PROVIDER_DOCUMENT_BYTES,
    returnedEntries: state.outputEntries,
    returnedStores: output.stores.length,
    sourceEntries: state.sourceEntries,
    ...(state.sourceEntryCountIncomplete ? { sourceEntryCountIncomplete: true } : {}),
    sourceStores: state.sourceStores,
    storeLimit: MAX_PROVIDER_STORES,
    storesOmitted,
    truncated,
    truncatedFields: state.truncatedFields,
  };
  output.truncated = truncated;
}

function largestFittingValuePrefix(
  output: PersistentStoreProjection,
  entry: Record<string, unknown>,
  originalValue: string,
  valueTruncationAlreadyCounted: boolean,
  state: ProjectionState,
  sourceTruncated: boolean,
): string | undefined {
  let low = 0;
  let high = originalValue.length;
  let best: string | undefined;
  const sourceValueLength =
    typeof entry.valueLength === 'number' ? Math.max(originalValue.length, entry.valueLength) : originalValue.length;
  if (!valueTruncationAlreadyCounted) state.truncatedFields++;
  while (low <= high) {
    const midpoint = Math.floor((low + high) / 2);
    const candidate = safeStringPrefix(originalValue, midpoint);
    entry.value = candidate;
    entry.valueLength = sourceValueLength;
    entry.valueTruncated = true;
    updateProjectionMetadata(output, state, sourceTruncated);
    if (projectionFits(output)) {
      best = candidate;
      low = midpoint + 1;
    } else {
      high = midpoint - 1;
    }
  }
  if (best === undefined) {
    if (!valueTruncationAlreadyCounted) state.truncatedFields--;
    return undefined;
  }
  entry.value = best;
  return best;
}

function baseProjection(snapshot: object, state: ProjectionState): PersistentStoreProjection {
  const output: PersistentStoreProjection = {
    projection: {
      entriesOmitted: 0,
      entryLimit: MAX_PROVIDER_ENTRIES,
      invalidFields: 0,
      maxDocumentBytes: MAX_PROVIDER_DOCUMENT_BYTES,
      returnedEntries: 0,
      returnedStores: 0,
      sourceEntries: 0,
      sourceStores: 0,
      storeLimit: MAX_PROVIDER_STORES,
      storesOmitted: 0,
      truncated: false,
      truncatedFields: 0,
    },
    stores: [],
    truncated: false,
  };
  const outputRecord = output as unknown as Record<string, unknown>;
  const diagnostics = objectProperty(snapshot, 'diagnostics', state);
  if (diagnostics !== undefined) output.diagnostics = diagnosticsRecord(diagnostics, state);
  copyOptionalNumber(outputRecord, snapshot, 'inspectedStorageKeys', state);
  const limits = objectProperty(snapshot, 'limits', state);
  if (limits !== undefined) {
    output.limits = knownNumberRecord(
      limits,
      [
        'maxEntriesPerStore',
        'maxErrorCharacters',
        'maxInspectedEntriesPerStore',
        'maxInspectedStorageKeys',
        'maxKeyCharacters',
        'maxNameCharacters',
        'maxSerializedStoreCharacters',
        'maxStorageKeyCharacters',
        'maxStores',
        'maxTotalBytes',
        'maxTotalCharacters',
        'maxValueCharacters',
      ],
      state,
    );
  }
  copyOptionalNumber(outputRecord, snapshot, 'rejectedStorageKeys', state);
  copyOptionalString(outputRecord, snapshot, 'storageError', MAX_ERROR_CHARACTERS, state);
  copyOptionalNumber(outputRecord, snapshot, 'storageErrorLength', state);
  copyOptionalBoolean(outputRecord, snapshot, 'storageErrorTruncated', state);
  copyOptionalBoolean(outputRecord, snapshot, 'storageInspectionTruncated', state);
  const usage = objectProperty(snapshot, 'usage', state);
  if (usage !== undefined) output.usage = knownNumberRecord(usage, ['bytes', 'characters'], state);
  return output;
}

function projectPersistentStoreSnapshot(snapshotValue: unknown): PersistentStoreProjection {
  const state: ProjectionState = {
    invalidFields: 0,
    outputEntries: 0,
    sourceEntries: 0,
    sourceEntryCountIncomplete: false,
    sourceStores: 0,
    stoppedForBudget: false,
    truncatedFields: 0,
  };
  const snapshot =
    typeof snapshotValue === 'object' && snapshotValue !== null
      ? snapshotValue
      : (Object.create(null) as Record<string, unknown>);
  if (snapshot !== snapshotValue) state.invalidFields++;
  const sourceTruncated = booleanProperty(snapshot, 'truncated', state) ?? false;
  const stores = sourceStores(snapshot, state);
  const output = baseProjection(snapshot, state);
  updateProjectionMetadata(output, state, sourceTruncated);

  outer: for (const sourceStore of stores) {
    const store = projectedStore(sourceStore.store, state);
    const inspectedEntries = Math.min(sourceStore.entryCount, MAX_PROVIDER_ENTRIES_PER_STORE);
    if (sourceStore.entryCount > inspectedEntries || state.outputEntries + inspectedEntries > MAX_PROVIDER_ENTRIES) {
      store.entriesTruncated = true;
    }
    output.stores.push(store);
    updateProjectionMetadata(output, state, sourceTruncated);
    if (!projectionFits(output)) {
      output.stores.pop();
      state.stoppedForBudget = true;
      break;
    }
    if (sourceStore.entries === undefined) continue;
    const entries = store.entries as Record<string, unknown>[];
    for (let index = 0; index < inspectedEntries; index++) {
      if (state.outputEntries >= MAX_PROVIDER_ENTRIES) break outer;
      const projected = projectedEntry(safeArrayItem(sourceStore.entries, index, state), state);
      const entry = projected.output;
      entries.push(entry);
      state.outputEntries++;
      updateProjectionMetadata(output, state, sourceTruncated);
      if (projectionFits(output)) continue;

      const originalValue = typeof entry.value === 'string' ? entry.value : '';
      if (index + 1 < sourceStore.entryCount) store.entriesTruncated = true;
      if (
        largestFittingValuePrefix(
          output,
          entry,
          originalValue,
          projected.valueTruncatedByCharacterLimit,
          state,
          sourceTruncated,
        ) !== undefined
      ) {
        state.stoppedForBudget = true;
        break outer;
      }
      entries.pop();
      state.outputEntries--;
      store.entriesTruncated = true;
      state.stoppedForBudget = true;
      updateProjectionMetadata(output, state, sourceTruncated);
      if (!projectionFits(output)) {
        state.outputEntries -= entries.length;
        output.stores.pop();
      }
      break outer;
    }
  }
  updateProjectionMetadata(output, state, sourceTruncated);
  return output;
}

/** @internal Produces the pre-serialized, provider-parser-compatible read-only Storage result. */
export function createPersistentStoreDebuggerProviderResult(snapshot: unknown): DebuggerProviderResult {
  const output = projectPersistentStoreSnapshot(snapshot);
  const json = JSON.stringify(output);
  if (utf8ByteLength(json) > MAX_PROVIDER_DOCUMENT_BYTES) {
    throw new Error('PersistentStore debugger projection exceeded its 43 KiB budget');
  }
  return createDebuggerProviderResult(json);
}

function snapshotReader(nativeModule: unknown): (() => unknown) | undefined {
  const property = ownDataProperty(nativeModule, 'getPersistentStoreSnapshot');
  if (!property.valid || typeof property.value !== 'function') return undefined;
  const reader = property.value as () => unknown;
  return () => reader.call(nativeModule);
}

/** @internal Creates the adapter separately from ownership so its behavior can be regression-tested. */
export function createPersistentStoreDebuggerProvider(nativeModule: unknown): DebuggerProvider {
  const readSnapshot = snapshotReader(nativeModule);
  return {
    availability: () =>
      readSnapshot === undefined
        ? { available: false, message: 'PersistentStore inspection is unavailable on this platform.' }
        : true,
    description: 'Bounded, read-only PersistentStore snapshots.',
    handleRequest: (request: DebuggerProviderRequest): DebuggerProviderResult => {
      if (request.action !== 'snapshot') {
        throw new Error(`Unsupported PersistentStore debugger action: ${request.action}`);
      }
      if (readSnapshot === undefined) {
        throw new Error('PersistentStore inspection is unavailable on this platform.');
      }
      return createPersistentStoreDebuggerProviderResult(readSnapshot());
    },
    id: PERSISTENT_STORE_PROVIDER_ID,
    kind: DebuggerProviderKind.Storage,
    label: 'PersistentStore',
  };
}

/** @internal Registers the adapter with module-owned native and pathless-web reload cleanup. */
export function registerPersistentStoreDebuggerProvider(
  ownerModule: DebuggerProviderModule,
  nativeModule: unknown,
): DebuggerProviderOwner {
  const owner = createDebuggerProviderOwner(ownerModule, PERSISTENT_STORE_PROVIDER_OWNER_KEY);
  owner.register(createPersistentStoreDebuggerProvider(nativeModule));
  return owner;
}

registerPersistentStoreDebuggerProvider(module, require('PersistentStoreNative'));
