import { jsx } from '../JSXBootstrap';
import { getModuleLoader } from '../ModuleLoaderGlobal';
import type { ValdiRuntime } from '../ValdiRuntime';
import type { CustomMessageHandler } from './CustomMessageHandler';

declare const module: { readonly path?: string };
declare const runtime: ValdiRuntime;

const DEBUGGER_PROVIDERS_IDENTIFIER = 'ValdiDebuggerProviders';
const DEBUGGER_PROVIDERS_CONTRACT_VERSION = 1;
const MAX_DEBUGGER_CUSTOM_RESPONSE_BYTES = 128 * 1024;
const MAX_DEBUGGER_ACTION_JSON_BYTES = 48 * 1024;
const MAX_DEBUGGER_JSON_DEPTH = 8;
const MAX_DEBUGGER_JSON_VALUES = 10_000;
const MAX_DEBUGGER_JSON_COLLECTION_ITEMS = 100;
const MAX_DEBUGGER_JSON_PROPERTY_NAME_CHARACTERS = 1024;
const MAX_DEBUGGER_JSON_STRING_CHARACTERS = 32 * 1024;
const MAX_DEBUGGER_PROVIDERS = 100;
const MAX_PROVIDER_CONCURRENCY = 4;
const MAX_PROVIDER_PROTOTYPE_DEPTH = 4;
const MAX_PROVIDER_ID_CHARACTERS = 128;
const MAX_PROVIDER_LABEL_CHARACTERS = 256;
const MAX_PROVIDER_DESCRIPTION_CHARACTERS = 4096;
const MAX_PROVIDER_ACTION_CHARACTERS = 128;
const PROVIDER_BRIDGE_GLOBAL_KEY = '__VALDI_DEBUGGER_PROVIDERS_V1__';

export enum DebuggerProviderKind {
  Storage = 'storage',
  Sql = 'sql',
  KeyValue = 'key-value',
  Network = 'network',
}

export interface DebuggerProviderAvailability {
  readonly available: boolean;
  readonly message?: string;
}

/** A bounded request object supplied by the debugger transport. */
export interface DebuggerProviderRequest {
  readonly action: string;
  readonly [key: string]: unknown;
}

/**
 * Providers serialize their own result before crossing the core boundary.
 * The document must contain one JSON object and fit the 48 KiB action-document budget.
 */
export interface DebuggerProviderResult {
  readonly json: string;
}

export interface DebuggerProvider {
  readonly id: string;
  readonly kind: DebuggerProviderKind;
  readonly label: string;
  readonly description?: string;
  readonly availability?: () => boolean | DebuggerProviderAvailability;
  readonly handleRequest: (
    request: DebuggerProviderRequest,
  ) => Promise<DebuggerProviderResult> | DebuggerProviderResult;
}

export interface DebuggerProviderSnapshot {
  readonly available: boolean;
  readonly description?: string;
  readonly id: string;
  readonly kind: DebuggerProviderKind;
  readonly label: string;
  readonly message?: string;
  readonly registrationToken: number;
}

export interface DebuggerProvidersSnapshot {
  readonly contractVersion: number;
  readonly metadataTruncated?: boolean;
  readonly omittedMetadataFields?: number;
  readonly providers: readonly DebuggerProviderSnapshot[];
  readonly revision: number;
}

export interface DebuggerProviderRegistration {
  dispose(): void;
  notifyChange(): void;
}

export interface DebuggerProviderModule {
  readonly path?: string;
}

/** Owns adapter registrations and automatically disposes them when its creating module reloads. */
export interface DebuggerProviderOwner {
  dispose(): void;
  register(provider: DebuggerProvider): DebuggerProviderRegistration;
}

interface DebuggerProvidersMessage {
  readonly action?: string;
  readonly providerId?: string;
  readonly request?: unknown;
}

interface DataPropertyResult {
  readonly found: boolean;
  readonly ok: boolean;
  readonly value?: unknown;
}

interface RegisteredDebuggerProvider {
  readonly availability?: () => boolean | DebuggerProviderAvailability;
  changeRevision: number;
  readonly description?: string;
  disposed: boolean;
  readonly generation: number;
  readonly handleRequest: (
    request: DebuggerProviderRequest,
  ) => Promise<DebuggerProviderResult> | DebuggerProviderResult;
  readonly id: string;
  inFlight: number;
  readonly kind: DebuggerProviderKind;
  readonly label: string;
  readonly token: number;
}

interface RegisteredDebuggerProviderOwner {
  readonly dispose: () => void;
  readonly token: number;
}

interface ProviderBridgeState {
  activeRegistrationCount: number;
  delegate?: (identifier: string, data: unknown) => Promise<unknown> | undefined;
  generation: number;
  readonly handler: CustomMessageHandler;
  handlerInstalled: boolean;
  ownerToken: number;
  readonly owners: Map<string, RegisteredDebuggerProviderOwner>;
  registrationToken: number;
  readonly registrations: Map<string, RegisteredDebuggerProvider[]>;
  revision: number;
}

interface ProviderBridgeGlobal {
  [PROVIDER_BRIDGE_GLOBAL_KEY]?: ProviderBridgeState;
}

class BoundedJsonParser {
  private index = 0;
  private valueCount = 0;

  constructor(private readonly source: string) {}

  parseObjectDocument(): Record<string, unknown> {
    this.skipWhitespace();
    const value = this.parseValue(0);
    this.skipWhitespace();
    if (this.index !== this.source.length) this.fail('contains trailing data');
    if (typeof value !== 'object' || value === null || Array.isArray(value)) {
      this.fail('must contain a JSON object');
    }
    return value as Record<string, unknown>;
  }

  private parseValue(depth: number): unknown {
    if (depth > MAX_DEBUGGER_JSON_DEPTH) this.fail('exceeds the maximum depth');
    this.valueCount++;
    if (this.valueCount > MAX_DEBUGGER_JSON_VALUES) this.fail('contains too many values');
    const character = this.source[this.index];
    if (character === '{') return this.parseObject(depth);
    if (character === '[') return this.parseArray(depth);
    if (character === '"') {
      return this.parseString(MAX_DEBUGGER_JSON_STRING_CHARACTERS);
    }
    if (character === 't') return this.parseLiteral('true', true);
    if (character === 'f') return this.parseLiteral('false', false);
    if (character === 'n') return this.parseLiteral('null', null);
    if (character === '-' || this.isDigit(character)) return this.parseNumber();
    return this.fail('contains an invalid value');
  }

  private parseObject(depth: number): Record<string, unknown> {
    const output = Object.create(null) as Record<string, unknown>;
    this.index++;
    this.skipWhitespace();
    if (this.source[this.index] === '}') {
      this.index++;
      return output;
    }
    let propertyCount = 0;
    for (;;) {
      propertyCount++;
      if (propertyCount > MAX_DEBUGGER_JSON_COLLECTION_ITEMS) this.fail('contains too many object properties');
      const propertyName = this.parseString(MAX_DEBUGGER_JSON_PROPERTY_NAME_CHARACTERS);
      this.skipWhitespace();
      if (this.source[this.index] !== ':') this.fail('contains an object property without a colon');
      this.index++;
      this.skipWhitespace();
      output[propertyName] = this.parseValue(depth + 1);
      this.skipWhitespace();
      const separator = this.source[this.index];
      if (separator === '}') {
        this.index++;
        return output;
      }
      if (separator !== ',') this.fail('contains an invalid object separator');
      this.index++;
      this.skipWhitespace();
    }
  }

  private parseArray(depth: number): unknown[] {
    const output: unknown[] = [];
    this.index++;
    this.skipWhitespace();
    if (this.source[this.index] === ']') {
      this.index++;
      return output;
    }
    let itemCount = 0;
    for (;;) {
      itemCount++;
      if (itemCount > MAX_DEBUGGER_JSON_COLLECTION_ITEMS) this.fail('contains too many array items');
      output.push(this.parseValue(depth + 1));
      this.skipWhitespace();
      const separator = this.source[this.index];
      if (separator === ']') {
        this.index++;
        return output;
      }
      if (separator !== ',') this.fail('contains an invalid array separator');
      this.index++;
      this.skipWhitespace();
    }
  }

  private parseString(maximumCharacters: number): string {
    if (this.source[this.index] !== '"') this.fail('contains an invalid JSON string');
    const start = this.index;
    this.index++;
    let characters = 0;
    while (this.index < this.source.length) {
      const character = this.source[this.index]!;
      this.index++;
      if (character === '"') return JSON.parse(this.source.slice(start, this.index)) as string;
      if (character === '\\') {
        const escape = this.source[this.index];
        this.index++;
        if (escape === 'u') {
          for (let offset = 0; offset < 4; offset++) {
            const code = this.source.charCodeAt(this.index + offset);
            const hexadecimal = (code >= 48 && code <= 57) || (code >= 65 && code <= 70) || (code >= 97 && code <= 102);
            if (!hexadecimal) this.fail('contains an invalid Unicode escape');
          }
          this.index += 4;
        } else if (!escape || !'"\\/bfnrt'.includes(escape)) {
          this.fail('contains an invalid string escape');
        }
      } else if (character.charCodeAt(0) <= 0x1f) {
        this.fail('contains an unescaped control character');
      }
      characters++;
      if (characters > maximumCharacters) this.fail('contains an oversized string');
    }
    this.fail('contains an unterminated string');
  }

  private parseNumber(): number {
    const start = this.index;
    if (this.source[this.index] === '-') this.index++;
    if (this.source[this.index] === '0') this.index++;
    else {
      if (!this.isNonZeroDigit(this.source[this.index])) this.fail('contains an invalid number');
      while (this.isDigit(this.source[this.index])) this.index++;
    }
    if (this.source[this.index] === '.') {
      this.index++;
      if (!this.isDigit(this.source[this.index])) this.fail('contains an invalid number fraction');
      while (this.isDigit(this.source[this.index])) this.index++;
    }
    const exponent = this.source[this.index];
    if (exponent === 'e' || exponent === 'E') {
      this.index++;
      const sign = this.source[this.index];
      if (sign === '+' || sign === '-') this.index++;
      if (!this.isDigit(this.source[this.index])) this.fail('contains an invalid number exponent');
      while (this.isDigit(this.source[this.index])) this.index++;
    }
    const value = Number(this.source.slice(start, this.index));
    if (!Number.isFinite(value)) this.fail('contains a non-finite number');
    return value;
  }

  private parseLiteral<T>(literal: string, value: T): T {
    if (this.source.slice(this.index, this.index + literal.length) !== literal) {
      this.fail('contains an invalid literal');
    }
    this.index += literal.length;
    return value;
  }

  private skipWhitespace(): void {
    while (this.index < this.source.length) {
      const code = this.source.charCodeAt(this.index);
      if (code !== 0x20 && code !== 0x0a && code !== 0x0d && code !== 0x09) return;
      this.index++;
    }
  }

  private isDigit(character: string | undefined): boolean {
    return character !== undefined && character >= '0' && character <= '9';
  }

  private isNonZeroDigit(character: string | undefined): boolean {
    return character !== undefined && character >= '1' && character <= '9';
  }

  private fail(reason: string): never {
    throw new Error(`Debugger provider JSON ${reason}`);
  }
}

function utf8ByteLength(value: string): number {
  let bytes = 0;
  for (let index = 0; index < value.length; index++) {
    const code = value.charCodeAt(index);
    if (code <= 0x7f) bytes += 1;
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

function validateAndParseProviderJson(json: unknown): Record<string, unknown> {
  if (typeof json !== 'string') throw new Error('Debugger provider results require a JSON string');
  if (json.length > MAX_DEBUGGER_ACTION_JSON_BYTES || utf8ByteLength(json) > MAX_DEBUGGER_ACTION_JSON_BYTES) {
    throw new Error('Debugger provider action JSON exceeds 48 KiB');
  }
  return new BoundedJsonParser(json).parseObjectDocument();
}

function finalCustomResponseByteLength(data: unknown): number {
  return utf8ByteLength(JSON.stringify({ handled: true, data }));
}

function finalCustomResponseFits(data: unknown): boolean {
  return finalCustomResponseByteLength(data) <= MAX_DEBUGGER_CUSTOM_RESPONSE_BYTES;
}

function boundedFinalCustomResponse<T>(data: T): T {
  if (!finalCustomResponseFits(data)) {
    throw new Error('Debugger provider final response exceeds 128 KiB');
  }
  return data;
}

/** Validates a serialized provider result without inspecting a provider-owned object graph. */
export function createDebuggerProviderResult(json: string): DebuggerProviderResult {
  validateAndParseProviderJson(json);
  return { json };
}

function objectPrototypeChain(value: object, label: string): object[] {
  const chain: object[] = [];
  let current: object | null = value;
  for (let depth = 0; current !== null; depth++) {
    if (depth > MAX_PROVIDER_PROTOTYPE_DEPTH) throw new Error(`${label} prototype chain is too deep`);
    chain.push(current);
    try {
      current = Object.getPrototypeOf(current) as object | null;
    } catch {
      throw new Error(`${label} prototype chain could not be inspected safely`);
    }
  }
  return chain;
}

function knownDataProperty(chain: readonly object[], key: PropertyKey, label: string): DataPropertyResult {
  for (const object of chain) {
    let descriptor: PropertyDescriptor | undefined;
    try {
      descriptor = Object.getOwnPropertyDescriptor(object, key);
    } catch {
      return { found: false, ok: false };
    }
    if (descriptor === undefined) continue;
    if (!('value' in descriptor)) throw new Error(`${label} must be a data property`);
    return { found: true, ok: true, value: descriptor.value };
  }
  return { found: false, ok: true };
}

function exactString(
  value: unknown,
  label: string,
  maximumLength: number,
  optional = false,
  maximumJsonBytes?: number,
): string | undefined {
  if (value === undefined && optional) return undefined;
  if (typeof value !== 'string' || value.trim().length === 0 || value.length > maximumLength) {
    throw new Error(`${label} must be a non-empty string of at most ${maximumLength.toString()} characters`);
  }
  if (maximumJsonBytes !== undefined && utf8ByteLength(JSON.stringify(value)) - 2 > maximumJsonBytes) {
    throw new Error(`${label} must serialize to at most ${maximumJsonBytes.toString()} JSON bytes`);
  }
  return value;
}

function exactProviderKind(value: unknown): DebuggerProviderKind {
  switch (value) {
    case DebuggerProviderKind.Storage:
    case DebuggerProviderKind.Sql:
    case DebuggerProviderKind.KeyValue:
    case DebuggerProviderKind.Network:
      return value;
    default:
      throw new Error('Debugger provider kind is unsupported');
  }
}

function captureProvider(provider: DebuggerProvider, generation: number, token: number): RegisteredDebuggerProvider {
  if (typeof provider !== 'object' || provider === null) throw new Error('Debugger provider must be an object');
  const chain = objectPrototypeChain(provider, 'Debugger provider');
  const idValue = knownDataProperty(chain, 'id', 'Debugger provider id');
  const kindValue = knownDataProperty(chain, 'kind', 'Debugger provider kind');
  const labelValue = knownDataProperty(chain, 'label', 'Debugger provider label');
  const descriptionValue = knownDataProperty(chain, 'description', 'Debugger provider description');
  const availabilityValue = knownDataProperty(chain, 'availability', 'Debugger provider availability');
  const handleRequestValue = knownDataProperty(chain, 'handleRequest', 'Debugger provider handleRequest');
  if (
    !idValue.ok ||
    !kindValue.ok ||
    !labelValue.ok ||
    !descriptionValue.ok ||
    !availabilityValue.ok ||
    !handleRequestValue.ok
  ) {
    throw new Error('Debugger provider properties could not be inspected safely');
  }
  const id = exactString(
    idValue.value,
    'Debugger provider id',
    MAX_PROVIDER_ID_CHARACTERS,
    false,
    MAX_PROVIDER_ID_CHARACTERS,
  )!;
  const label = exactString(
    labelValue.value,
    'Debugger provider label',
    MAX_PROVIDER_LABEL_CHARACTERS,
    false,
    MAX_PROVIDER_LABEL_CHARACTERS,
  )!;
  const description = exactString(
    descriptionValue.value,
    'Debugger provider description',
    MAX_PROVIDER_DESCRIPTION_CHARACTERS,
    true,
    MAX_PROVIDER_DESCRIPTION_CHARACTERS,
  );
  if (availabilityValue.found && typeof availabilityValue.value !== 'function') {
    throw new Error('Debugger provider availability must be a function');
  }
  if (!handleRequestValue.found || typeof handleRequestValue.value !== 'function') {
    throw new Error('Debugger provider handleRequest must be a function');
  }
  return {
    availability: availabilityValue.found
      ? (availabilityValue.value as () => boolean | DebuggerProviderAvailability).bind(provider)
      : undefined,
    changeRevision: 0,
    description,
    disposed: false,
    generation,
    handleRequest: (handleRequestValue.value as DebuggerProvider['handleRequest']).bind(provider),
    id,
    inFlight: 0,
    kind: exactProviderKind(kindValue.value),
    label,
    token,
  };
}

function providerAvailability(provider: RegisteredDebuggerProvider): DebuggerProviderAvailability {
  try {
    const availability = provider.availability?.() ?? true;
    if (typeof availability === 'boolean') return { available: availability };
    if (typeof availability !== 'object' || availability === null) {
      return { available: false, message: 'Provider returned invalid availability.' };
    }
    const chain = objectPrototypeChain(availability, 'Provider availability');
    const availableValue = knownDataProperty(chain, 'available', 'Provider availability available');
    const messageValue = knownDataProperty(chain, 'message', 'Provider availability message');
    if (!availableValue.ok || !availableValue.found || typeof availableValue.value !== 'boolean' || !messageValue.ok) {
      return { available: false, message: 'Provider returned invalid availability.' };
    }
    const message = exactString(messageValue.value, 'Provider availability message', 1024, true, 1024);
    return { available: availableValue.value, ...(message === undefined ? {} : { message }) };
  } catch {
    return { available: false, message: 'Provider availability check failed.' };
  }
}

function providerSnapshot(
  provider: RegisteredDebuggerProvider,
  availability = providerAvailability(provider),
): DebuggerProviderSnapshot {
  return {
    available: availability.available,
    ...(provider.description === undefined ? {} : { description: provider.description }),
    id: provider.id,
    kind: provider.kind,
    label: provider.label,
    ...(availability.message === undefined ? {} : { message: availability.message }),
    registrationToken: provider.token,
  };
}

function clearRegistrations(bridge: ProviderBridgeState): void {
  bridge.registrations.forEach(registrations =>
    registrations.forEach(registration => {
      registration.disposed = true;
    }),
  );
  bridge.registrations.clear();
  bridge.activeRegistrationCount = 0;
  bridge.revision++;
}

function createProviderBridge(): ProviderBridgeState {
  let bridge: ProviderBridgeState;
  const handler: CustomMessageHandler = {
    messageReceived(identifier: string, data: unknown): Promise<unknown> | undefined {
      return bridge.delegate?.(identifier, data);
    },
  };
  bridge = {
    activeRegistrationCount: 0,
    generation: 0,
    handler,
    handlerInstalled: false,
    ownerToken: 0,
    owners: new Map<string, RegisteredDebuggerProviderOwner>(),
    registrationToken: 0,
    registrations: new Map<string, RegisteredDebuggerProvider[]>(),
    revision: 0,
  };
  return bridge;
}

function globalProviderBridge(): ProviderBridgeState {
  const globals = globalThis as unknown as ProviderBridgeGlobal;
  let bridge = globals[PROVIDER_BRIDGE_GLOBAL_KEY];
  if (bridge === undefined) {
    bridge = createProviderBridge();
    globals[PROVIDER_BRIDGE_GLOBAL_KEY] = bridge;
  } else if (bridge.owners === undefined) {
    bridge.ownerToken = 0;
    (bridge as { owners: Map<string, RegisteredDebuggerProviderOwner> }).owners = new Map();
  }
  return bridge;
}

const providerBridge = globalProviderBridge();
let moduleGeneration = claimProviderBridgeOwner();

function claimProviderBridgeOwner(): number {
  if (providerBridge.generation !== 0) {
    clearProviderOwners(providerBridge);
    clearRegistrations(providerBridge);
  }
  providerBridge.generation++;
  const generation = providerBridge.generation;
  providerBridge.delegate = (identifier, data) => {
    if (identifier !== DEBUGGER_PROVIDERS_IDENTIFIER) return undefined;
    return handleMessage(providerBridge, generation, data);
  };
  return generation;
}

function releaseProviderBridgeOwner(generation: number): void {
  if (providerBridge.generation !== generation) return;
  clearProviderOwners(providerBridge);
  clearRegistrations(providerBridge);
  providerBridge.delegate = undefined;
  if (providerBridge.handlerInstalled) {
    jsx.removeCustomMessageHandler(providerBridge.handler);
    providerBridge.handlerInstalled = false;
  }
}

function clearProviderOwners(bridge: ProviderBridgeState): void {
  const owners = Array.from(bridge.owners.values());
  bridge.owners.clear();
  owners.forEach(owner => owner.dispose());
}

function registerModuleDisposal(generation: number): void {
  try {
    if (typeof module === 'undefined' || typeof module.path !== 'string') return;
    getModuleLoader().onHotReload(module as { path: string }, module.path, () => {
      releaseProviderBridgeOwner(generation);
    });
  } catch {
    // Some standalone test hosts do not install the Valdi module loader.
  }
}

registerModuleDisposal(moduleGeneration);

function installRegistryBridge(): void {
  if (providerBridge.handlerInstalled) return;
  jsx.addCustomMessageHandler(providerBridge.handler);
  providerBridge.handlerInstalled = true;
}

function removeRegistryBridgeIfUnused(): void {
  if (providerBridge.activeRegistrationCount !== 0 || !providerBridge.handlerInstalled) return;
  jsx.removeCustomMessageHandler(providerBridge.handler);
  providerBridge.handlerInstalled = false;
}

function activeProvider(bridge: ProviderBridgeState, providerId: string): RegisteredDebuggerProvider | undefined {
  const registrations = bridge.registrations.get(providerId);
  const provider = registrations?.[registrations.length - 1];
  return provider?.generation === bridge.generation && !provider.disposed ? provider : undefined;
}

function requiredProviderSnapshot(provider: DebuggerProviderSnapshot): DebuggerProviderSnapshot {
  return {
    available: provider.available,
    id: provider.id,
    kind: provider.kind,
    label: provider.label,
    registrationToken: provider.registrationToken,
  };
}

function debuggerProvidersSnapshot(bridge: ProviderBridgeState): DebuggerProvidersSnapshot {
  const providers: DebuggerProviderSnapshot[] = [];
  bridge.registrations.forEach(registrations => {
    const provider = registrations[registrations.length - 1];
    if (provider !== undefined && !provider.disposed && provider.generation === bridge.generation) {
      providers.push(providerSnapshot(provider));
    }
  });
  providers.sort((left, right) => left.kind.localeCompare(right.kind) || left.label.localeCompare(right.label));
  const complete: DebuggerProvidersSnapshot = {
    contractVersion: DEBUGGER_PROVIDERS_CONTRACT_VERSION,
    providers,
    revision: bridge.revision,
  };
  if (finalCustomResponseFits(complete)) return complete;

  const boundedProviders = providers.map(requiredProviderSnapshot);
  let omittedMetadataFields = 0;
  for (let index = 0; index < providers.length; index++) {
    const completeProvider = providers[index]!;
    const boundedProvider = boundedProviders[index]! as {
      description?: string;
      message?: string;
    };
    for (const key of ['description', 'message'] as const) {
      const value = completeProvider[key];
      if (value === undefined) continue;
      boundedProvider[key] = value;
      const candidate: DebuggerProvidersSnapshot = {
        contractVersion: DEBUGGER_PROVIDERS_CONTRACT_VERSION,
        metadataTruncated: true,
        omittedMetadataFields: providers.length * 2,
        providers: boundedProviders,
        revision: bridge.revision,
      };
      if (!finalCustomResponseFits(candidate)) {
        delete boundedProvider[key];
        omittedMetadataFields++;
      }
    }
  }
  const bounded: DebuggerProvidersSnapshot = {
    contractVersion: DEBUGGER_PROVIDERS_CONTRACT_VERSION,
    metadataTruncated: true,
    omittedMetadataFields,
    providers: boundedProviders,
    revision: bridge.revision,
  };
  return boundedFinalCustomResponse(bounded);
}

function baseProviderResponse(
  bridge: ProviderBridgeState,
  provider: RegisteredDebuggerProvider,
  availability: DebuggerProviderAvailability,
): Record<string, unknown> {
  return {
    contractVersion: DEBUGGER_PROVIDERS_CONTRACT_VERSION,
    provider: providerSnapshot(provider, availability),
    registrationToken: provider.token,
    revision: bridge.revision,
  };
}

async function dispatchProviderRequest(
  bridge: ProviderBridgeState,
  providerId: string,
  request: DebuggerProviderRequest,
): Promise<unknown> {
  const provider = activeProvider(bridge, providerId);
  if (provider === undefined) throw new Error(`Unknown debugger provider: ${providerId}`);
  const availability = providerAvailability(provider);
  const base = baseProviderResponse(bridge, provider, availability);
  if (!availability.available) {
    return boundedFinalCustomResponse({
      ...base,
      message: availability.message ?? 'Provider is unavailable.',
      unavailable: true,
    });
  }
  if (provider.inFlight >= MAX_PROVIDER_CONCURRENCY) {
    return boundedFinalCustomResponse({
      ...base,
      busy: true,
      message: 'Provider request concurrency limit reached.',
    });
  }

  const capturedGeneration = bridge.generation;
  const capturedChangeRevision = provider.changeRevision;
  const capturedToken = provider.token;
  provider.inFlight++;
  try {
    const result = await provider.handleRequest(request);
    if (
      provider.disposed ||
      bridge.generation !== capturedGeneration ||
      provider.changeRevision !== capturedChangeRevision ||
      activeProvider(bridge, providerId)?.token !== capturedToken
    ) {
      return boundedFinalCustomResponse({
        contractVersion: DEBUGGER_PROVIDERS_CONTRACT_VERSION,
        registrationToken: capturedToken,
        revision: bridge.revision,
        stale: true,
      });
    }
    if (typeof result !== 'object' || result === null) {
      throw new Error('Debugger provider results require an object with a JSON data property');
    }
    const resultChain = objectPrototypeChain(result, 'Debugger provider result');
    const jsonValue = knownDataProperty(resultChain, 'json', 'Debugger provider result json');
    if (!jsonValue.ok || !jsonValue.found) {
      throw new Error('Debugger provider results require a JSON data property');
    }
    return boundedFinalCustomResponse({
      ...base,
      data: validateAndParseProviderJson(jsonValue.value),
      revision: bridge.revision,
    });
  } finally {
    provider.inFlight--;
  }
}

function messageValue(message: object, key: PropertyKey, label: string): unknown {
  const value = knownDataProperty(objectPrototypeChain(message, label), key, label);
  if (!value.ok) throw new Error(`${label} could not be inspected safely`);
  return value.value;
}

async function handleMessage(bridge: ProviderBridgeState, generation: number, data: unknown): Promise<unknown> {
  if (bridge.generation !== generation) throw new Error('Debugger provider module instance is stale');
  if (typeof data !== 'object' || data === null || Array.isArray(data)) {
    throw new Error('Debugger provider message must be an object');
  }
  const actionValue = messageValue(data, 'action', 'Debugger provider message action') ?? 'list';
  const action = exactString(actionValue, 'Debugger provider action', MAX_PROVIDER_ACTION_CHARACTERS)!;
  if (action === 'list') return debuggerProvidersSnapshot(bridge);
  if (action !== 'request') throw new Error(`Unsupported debugger provider action: ${action}`);
  const providerId = exactString(
    messageValue(data, 'providerId', 'Debugger provider message providerId'),
    'Debugger provider id',
    MAX_PROVIDER_ID_CHARACTERS,
  )!;
  const requestValue = messageValue(data, 'request', 'Debugger provider message request');
  if (typeof requestValue !== 'object' || requestValue === null || Array.isArray(requestValue)) {
    throw new Error('Debugger provider requests require an object request');
  }
  const requestAction = exactString(
    messageValue(requestValue, 'action', 'Debugger provider request action'),
    'Debugger provider request action',
    MAX_PROVIDER_ACTION_CHARACTERS,
  )!;
  if (requestAction !== messageValue(requestValue, 'action', 'Debugger provider request action')) {
    throw new Error('Debugger provider request action changed during validation');
  }
  return dispatchProviderRequest(bridge, providerId, requestValue as DebuggerProviderRequest);
}

const inactiveRegistration: DebuggerProviderRegistration = {
  dispose(): void {},
  notifyChange(): void {},
};

/** Registers a debugger capability for debug runtimes. Dispose the returned token when the provider owner reloads. */
export function registerDebuggerProvider(provider: DebuggerProvider): DebuggerProviderRegistration {
  if (!runtime.isDebugEnabled) return inactiveRegistration;
  if (providerBridge.generation !== moduleGeneration || providerBridge.delegate === undefined) {
    throw new Error('Debugger provider module instance is stale');
  }
  const registration = captureProvider(provider, moduleGeneration, ++providerBridge.registrationToken);
  const replacedRegistrations = providerBridge.registrations.get(registration.id) ?? [];
  const replacedLiveCount = replacedRegistrations.reduce((count, replaced) => count + (replaced.disposed ? 0 : 1), 0);
  if (providerBridge.activeRegistrationCount - replacedLiveCount >= MAX_DEBUGGER_PROVIDERS) {
    throw new Error('Debugger provider registry exceeds the supported live registration count');
  }
  replacedRegistrations.forEach(replaced => {
    replaced.disposed = true;
  });
  providerBridge.activeRegistrationCount -= replacedLiveCount;
  installRegistryBridge();
  providerBridge.registrations.set(registration.id, [registration]);
  providerBridge.activeRegistrationCount++;
  providerBridge.revision++;

  return {
    dispose(): void {
      if (registration.disposed) return;
      registration.disposed = true;
      const current = providerBridge.registrations.get(registration.id);
      const index = current?.indexOf(registration) ?? -1;
      if (current !== undefined && index >= 0) {
        current.splice(index, 1);
        providerBridge.activeRegistrationCount--;
        if (current.length === 0) providerBridge.registrations.delete(registration.id);
        providerBridge.revision++;
      }
      removeRegistryBridgeIfUnused();
    },
    notifyChange(): void {
      if (
        !registration.disposed &&
        registration.generation === providerBridge.generation &&
        providerBridge.registrations.get(registration.id)?.includes(registration)
      ) {
        registration.changeRevision++;
        providerBridge.revision++;
      }
    },
  };
}

/**
 * Creates an owner whose registrations are automatically removed when the supplied adapter module reloads.
 * ownerKey must be the adapter's stable module identifier and is required because webpack module.path is absent.
 */
export function createDebuggerProviderOwner(
  ownerModule: DebuggerProviderModule,
  ownerKey: string,
): DebuggerProviderOwner {
  if (typeof ownerModule !== 'object' || ownerModule === null) {
    throw new Error('Debugger provider owners require their creating module');
  }
  const stableOwnerKey = exactString(ownerKey, 'Debugger provider owner key', 256, false, 256)!;
  const ownerModuleChain = objectPrototypeChain(ownerModule, 'Debugger provider owner module');
  const modulePathValue = knownDataProperty(ownerModuleChain, 'path', 'Debugger provider owner module path');
  if (!modulePathValue.ok) {
    throw new Error('Debugger provider owner module path could not be inspected safely');
  }
  const reloadPath =
    modulePathValue.found && modulePathValue.value !== undefined
      ? exactString(modulePathValue.value, 'Debugger provider owner module path', 1024)!
      : stableOwnerKey;
  const generation = moduleGeneration;
  const ownerToken = ++providerBridge.ownerToken;
  const registrations: DebuggerProviderRegistration[] = [];
  let disposed = false;
  let removeReloadCallback: (() => void) | undefined;
  const owner: DebuggerProviderOwner = {
    dispose(): void {
      if (disposed) return;
      disposed = true;
      const removeCallback = removeReloadCallback;
      removeReloadCallback = undefined;
      removeCallback?.();
      if (providerBridge.owners.get(stableOwnerKey)?.token === ownerToken) {
        providerBridge.owners.delete(stableOwnerKey);
      }
      registrations.forEach(registration => registration.dispose());
      registrations.length = 0;
    },
    register(provider: DebuggerProvider): DebuggerProviderRegistration {
      if (disposed) throw new Error('Debugger provider owner is disposed');
      if (generation !== providerBridge.generation)
        throw new Error('Debugger provider owner belongs to a stale module');
      if (providerBridge.owners.get(stableOwnerKey)?.token !== ownerToken) {
        throw new Error('Debugger provider owner was replaced');
      }
      const registration = registerDebuggerProvider(provider);
      registrations.push(registration);
      return registration;
    },
  };
  providerBridge.owners.get(stableOwnerKey)?.dispose();
  providerBridge.owners.set(stableOwnerKey, { dispose: () => owner.dispose(), token: ownerToken });
  try {
    removeReloadCallback = getModuleLoader().onHotReload(ownerModule as { path: string }, reloadPath, () =>
      owner.dispose(),
    );
  } catch {
    owner.dispose();
    throw new Error('Debugger provider owner could not bind to its module hot-reload lifecycle');
  }
  return owner;
}

/** @internal Test-only simulation of a DebuggerProvider module replacement. */
export function reloadDebuggerProviderModuleForTesting(): void {
  moduleGeneration = claimProviderBridgeOwner();
}
