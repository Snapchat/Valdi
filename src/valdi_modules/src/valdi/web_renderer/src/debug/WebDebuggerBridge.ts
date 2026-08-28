import type { IRenderer } from 'valdi_core/src/IRenderer';
import type { ValdiWebRendererDelegate, WebRendererDebugSnapshot } from '../ValdiWebRendererDelegate';
import { MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS } from '../ValdiWebRendererDelegate';
import type { ComponentPropertyEditCandidate, ComponentPropertyEditRegistrar } from './ComponentHierarchySnapshot';
import { hasWebLocationQueryParameter } from '../utils/LocationQuery';

const WEB_DEBUGGER_CHANNEL = 'valdi-web-debugger';
const WEB_DEBUGGER_QUERY_KEY = 'valdiDebugger';
const WEB_DEBUGGER_QUERY_VALUE = '1';
const DEVTOOLS_QUERY_KEY = 'valdiDevTools';
const OWL_DEBUGGER_QUERY_KEY = 'valdiOwlDebugger';
const MAX_SOURCE_METADATA_SERIALIZED_CHARACTERS = 16_384;
const SOURCE_METADATA_TRUNCATION_MARKER = '... <truncated>';

export interface StandaloneWebDebuggerSnapshot {
  channel: string;
  componentPropertyEditingAvailable: boolean;
  componentPropertyEditProtocolVersion: number | null;
  source: {
    title: string;
    url: string;
  };
  snapshot: WebRendererDebugSnapshot;
  type: string;
}

/**
 * Read/highlight-only compatibility bridge for inspecting a Valdi web renderer
 * directly from browser tooling. It deliberately does not implement the daemon
 * `ValdiDebuggerInput` contract: browser interaction and hit testing remain the
 * responsibility of DOM/DevTools automation, while `valdi inspect input` targets
 * native runtimes connected through the debugger daemon.
 */
export interface StandaloneWebDebuggerRuntime {
  clearHighlight?(): boolean;
  editComponentProperty?(request: unknown): boolean;
  getSnapshot(): StandaloneWebDebuggerSnapshot;
  highlightNode?(nodeId: string): boolean;
}

interface ComponentPropertyEditTokenRecord extends ComponentPropertyEditCandidate {
  readonly expiresAt: number;
  readonly revision: number;
}

interface ComponentPropertyEditCapture {
  accepting: boolean;
  available: boolean;
  readonly captureGeneration: number;
  readonly lifecycleGeneration: number;
  nextToken?: string;
  readonly registry: Map<string, ComponentPropertyEditTokenRecord>;
  readonly revision: number;
}

interface DebuggableWindow extends Window {
  __VALDI_WEB_DEBUGGER__?: StandaloneWebDebuggerRuntime;
}

const BRIDGE_CREATED_STANDALONE_RUNTIMES = new WeakSet<StandaloneWebDebuggerRuntime>();
const LIVE_BRIDGE_STANDALONE_RUNTIMES = new WeakSet<StandaloneWebDebuggerRuntime>();
const PREVIOUS_STANDALONE_RUNTIMES = new WeakMap<
  StandaloneWebDebuggerRuntime,
  StandaloneWebDebuggerRuntime | undefined
>();
const COMPONENT_PROPERTY_TOKEN_BYTES = 16;
const COMPONENT_PROPERTY_TOKEN_LIMIT = 1_000;
const COMPONENT_PROPERTY_TOKEN_LIFETIME_MS = 120_000;
const COMPONENT_PROPERTY_EDIT_PROTOCOL_VERSION = 1;
const COMPONENT_PROPERTY_EDIT_REQUEST_KEYS_BY_PROTOCOL_VERSION = new Map<number, readonly string[]>([
  [
    COMPONENT_PROPERTY_EDIT_PROTOCOL_VERSION,
    ['componentId', 'componentToken', 'propertyName', 'protocolVersion', 'snapshotRevision', 'value'],
  ],
]);
const COMPONENT_PROPERTY_TOKEN_PATTERN = /^[0-9a-f]{32}$/;
const MAX_COMPONENT_ID_CHARACTERS = 4_096;
const MAX_COMPONENT_PROPERTY_NAME_CHARACTERS = 256;
const MAX_COMPONENT_PROPERTY_STRING_BYTES = 65_536;
const COMPONENT_PROPERTY_EDIT_ERROR = 'The component property edit is stale or invalid.';

export class WebDebuggerBridge {
  private destroyed = false;
  private readonly enabled: boolean;
  private highlightedNode?: HTMLDivElement;
  private standaloneRuntime?: StandaloneWebDebuggerRuntime;
  private previousStandaloneRuntime?: StandaloneWebDebuggerRuntime;
  private componentPropertyEditTokens = new Map<string, ComponentPropertyEditTokenRecord>();
  private componentPropertyEditPreviousTokens = new Map<string, ComponentPropertyEditTokenRecord>();
  private componentPropertyEditCaptureGeneration = 0;
  private componentPropertyEditExpiryTimer?: ReturnType<typeof setTimeout>;
  private componentPropertyEditLifecycleGeneration = 0;
  private componentPropertySnapshotRevision = 0;

  constructor(
    _root: HTMLElement | ShadowRoot,
    private readonly delegate: ValdiWebRendererDelegate,
    private readonly renderer: IRenderer,
  ) {
    this.enabled = shouldEnableWebDebuggerBridge();
    if (this.enabled) {
      this.registerStandaloneRuntime();
    }
  }

  destroy(): void {
    if (!this.enabled || this.destroyed) {
      return;
    }
    this.destroyed = true;
    this.componentPropertyEditCaptureGeneration++;
    this.componentPropertyEditLifecycleGeneration++;
    this.clearComponentPropertyEditRegistry();
    this.removeHighlightOverlay();

    const debuggableWindow = window as DebuggableWindow;
    if (this.standaloneRuntime) {
      LIVE_BRIDGE_STANDALONE_RUNTIMES.delete(this.standaloneRuntime);
    }
    if (this.standaloneRuntime && debuggableWindow.__VALDI_WEB_DEBUGGER__ === this.standaloneRuntime) {
      const runtimeToRestore = getRestorableStandaloneRuntime(this.previousStandaloneRuntime);
      if (runtimeToRestore) {
        debuggableWindow.__VALDI_WEB_DEBUGGER__ = runtimeToRestore;
      } else {
        delete debuggableWindow.__VALDI_WEB_DEBUGGER__;
      }
    }
    this.standaloneRuntime = undefined;
    this.previousStandaloneRuntime = undefined;
  }

  private getSnapshot(): StandaloneWebDebuggerSnapshot {
    if (this.destroyed) {
      throw new Error('Web debugger runtime has been destroyed.');
    }
    const captureGeneration = ++this.componentPropertyEditCaptureGeneration;
    const lifecycleGeneration = this.componentPropertyEditLifecycleGeneration;
    const source = {
      title: captureBoundedSourceMetadata(document.title),
      url: captureBoundedSourceMetadata(window.location.href),
    };
    let componentPropertyEditingAvailable = false;
    try {
      componentPropertyEditingAvailable = typeof this.renderer.editDebugComponentProperty === 'function';
    } catch (_error) {
      componentPropertyEditingAvailable = false;
    }
    const envelopeCharacters =
      JSON.stringify({
        channel: WEB_DEBUGGER_CHANNEL,
        componentPropertyEditingAvailable: false,
        componentPropertyEditProtocolVersion: null,
        source,
        snapshot: null,
        type: 'snapshot',
      }).length - 'null'.length;
    const editCapture = this.createComponentPropertyEditCapture(
      componentPropertyEditingAvailable,
      captureGeneration,
      lifecycleGeneration,
    );
    let snapshot: WebRendererDebugSnapshot;
    try {
      snapshot = this.delegate.getDebugSnapshot(
        this.renderer,
        Math.max(0, MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS - envelopeCharacters),
        editCapture.available ? this.createComponentPropertyEditRegistrar(editCapture) : undefined,
      );
    } catch (error) {
      editCapture.accepting = false;
      editCapture.registry.clear();
      if (this.isComponentPropertyEditCaptureCurrent(editCapture) && !editCapture.available) {
        this.clearComponentPropertyEditRegistry();
      }
      throw error;
    }
    editCapture.accepting = false;
    const captureIsCurrent = this.isComponentPropertyEditCaptureCurrent(editCapture);
    if (!captureIsCurrent || !editCapture.available) {
      stripComponentPropertyEditMetadata(snapshot);
      editCapture.registry.clear();
    }
    if (
      !captureIsCurrent &&
      (this.destroyed || lifecycleGeneration !== this.componentPropertyEditLifecycleGeneration)
    ) {
      throw new Error('Web debugger runtime has been destroyed.');
    }
    const componentPropertyEditingAvailableForResponse = captureIsCurrent && editCapture.available;
    const response: StandaloneWebDebuggerSnapshot = {
      channel: WEB_DEBUGGER_CHANNEL,
      componentPropertyEditingAvailable: componentPropertyEditingAvailableForResponse,
      componentPropertyEditProtocolVersion: componentPropertyEditingAvailableForResponse
        ? COMPONENT_PROPERTY_EDIT_PROTOCOL_VERSION
        : null,
      source,
      snapshot,
      type: 'snapshot',
    };
    if (JSON.stringify(response).length <= MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS) {
      if (captureIsCurrent) {
        if (componentPropertyEditingAvailableForResponse) {
          this.publishComponentPropertyEditRegistry(editCapture);
        } else {
          this.clearComponentPropertyEditRegistry();
        }
      }
      return response;
    }
    editCapture.registry.clear();
    if (captureIsCurrent && !editCapture.available) {
      this.clearComponentPropertyEditRegistry();
    }
    return {
      ...response,
      componentPropertyEditingAvailable: false,
      componentPropertyEditProtocolVersion: null,
      snapshot: {
        tree: null,
        viewport: {
          width: 0,
          height: 0,
        },
      },
    };
  }

  private createComponentPropertyEditCapture(
    componentPropertyEditingAvailable: boolean,
    captureGeneration: number,
    lifecycleGeneration: number,
  ): ComponentPropertyEditCapture {
    this.componentPropertySnapshotRevision =
      this.componentPropertySnapshotRevision >= Number.MAX_SAFE_INTEGER
        ? 1
        : this.componentPropertySnapshotRevision + 1;
    const capture: ComponentPropertyEditCapture = {
      accepting: true,
      available: componentPropertyEditingAvailable,
      captureGeneration,
      lifecycleGeneration,
      registry: new Map(),
      revision: this.componentPropertySnapshotRevision,
    };
    if (!capture.available) return capture;
    capture.nextToken = this.createComponentPropertyToken(capture.registry);
    if (capture.nextToken === undefined) capture.available = false;
    return capture;
  }

  private createComponentPropertyEditRegistrar(capture: ComponentPropertyEditCapture): ComponentPropertyEditRegistrar {
    return candidate => {
      if (!capture.accepting) return undefined;
      if (!this.isComponentPropertyEditCaptureCurrent(capture)) {
        capture.available = false;
        capture.registry.clear();
        return undefined;
      }
      if (!capture.available || capture.registry.size >= COMPONENT_PROPERTY_TOKEN_LIMIT) return undefined;
      const token = capture.nextToken;
      if (token === undefined) {
        capture.available = false;
        capture.registry.clear();
        return undefined;
      }
      capture.registry.set(token, {
        ...candidate,
        descriptor: { ...candidate.descriptor },
        expiresAt: Date.now() + COMPONENT_PROPERTY_TOKEN_LIFETIME_MS,
        revision: capture.revision,
      });
      capture.nextToken = this.createComponentPropertyToken(capture.registry);
      if (capture.nextToken === undefined && capture.registry.size < COMPONENT_PROPERTY_TOKEN_LIMIT) {
        capture.available = false;
        capture.registry.clear();
        return undefined;
      }
      return { componentToken: token, snapshotRevision: capture.revision };
    };
  }

  private editComponentProperty(request: unknown): boolean {
    if (this.destroyed) throw new Error(COMPONENT_PROPERTY_EDIT_ERROR);
    const parsed = parseComponentPropertyEditRequest(request);
    if (parsed !== undefined) this.pruneExpiredComponentPropertyEditTokens(Date.now());
    const registry =
      parsed === undefined
        ? undefined
        : this.componentPropertyEditTokens.has(parsed.componentToken)
          ? this.componentPropertyEditTokens
          : this.componentPropertyEditPreviousTokens.has(parsed.componentToken)
            ? this.componentPropertyEditPreviousTokens
            : undefined;
    const record = parsed === undefined ? undefined : registry?.get(parsed.componentToken);
    if (
      parsed === undefined ||
      record === undefined ||
      record.revision !== parsed.snapshotRevision ||
      record.componentId !== parsed.componentId ||
      record.propertyName !== parsed.propertyName
    ) {
      throw new Error(COMPONENT_PROPERTY_EDIT_ERROR);
    }

    registry!.delete(parsed.componentToken);
    if (this.componentPropertyEditTokenCount === 0) this.cancelComponentPropertyEditExpiry();
    try {
      const edit = this.renderer.editDebugComponentProperty;
      const updated =
        typeof edit === 'function' &&
        edit.call(this.renderer, {
          component: record.component,
          expectedDescriptor: record.descriptor,
          expectedViewModel: record.viewModel,
          expectedViewModelExtensible: record.viewModelExtensible,
          newValue: parsed.value,
          node: record.node,
          propertyName: record.propertyName,
        });
      if (!updated) throw new Error(COMPONENT_PROPERTY_EDIT_ERROR);
      return true;
    } catch (_error) {
      throw new Error(COMPONENT_PROPERTY_EDIT_ERROR);
    }
  }

  private isComponentPropertyEditCaptureCurrent(capture: ComponentPropertyEditCapture): boolean {
    return (
      !this.destroyed &&
      capture.captureGeneration === this.componentPropertyEditCaptureGeneration &&
      capture.lifecycleGeneration === this.componentPropertyEditLifecycleGeneration
    );
  }

  private cancelComponentPropertyEditExpiry(): void {
    if (this.componentPropertyEditExpiryTimer === undefined) return;
    clearTimeout(this.componentPropertyEditExpiryTimer);
    this.componentPropertyEditExpiryTimer = undefined;
  }

  private clearComponentPropertyEditRegistry(): void {
    this.cancelComponentPropertyEditExpiry();
    this.componentPropertyEditTokens.clear();
    this.componentPropertyEditPreviousTokens.clear();
  }

  private get componentPropertyEditTokenCount(): number {
    return this.componentPropertyEditTokens.size + this.componentPropertyEditPreviousTokens.size;
  }

  private createComponentPropertyToken(
    captureRegistry: Map<string, ComponentPropertyEditTokenRecord>,
  ): string | undefined {
    return createComponentPropertyToken(
      token =>
        captureRegistry.has(token) ||
        this.componentPropertyEditTokens.has(token) ||
        this.componentPropertyEditPreviousTokens.has(token),
    );
  }

  private publishComponentPropertyEditRegistry(capture: ComponentPropertyEditCapture): void {
    this.cancelComponentPropertyEditExpiry();
    this.componentPropertyEditPreviousTokens.clear();
    this.componentPropertyEditPreviousTokens = this.componentPropertyEditTokens;
    this.componentPropertyEditTokens = capture.registry;
    this.pruneExpiredComponentPropertyEditTokens(Date.now());
    this.scheduleComponentPropertyEditExpiry();
  }

  private pruneExpiredComponentPropertyEditTokens(now: number): void {
    for (const registry of [this.componentPropertyEditTokens, this.componentPropertyEditPreviousTokens]) {
      for (const [token, record] of registry) {
        if (record.expiresAt <= now) registry.delete(token);
      }
    }
    if (this.componentPropertyEditTokenCount === 0) this.cancelComponentPropertyEditExpiry();
  }

  private scheduleComponentPropertyEditExpiry(): void {
    if (this.destroyed || this.componentPropertyEditTokenCount === 0) return;
    let nextExpiry = Number.POSITIVE_INFINITY;
    for (const registry of [this.componentPropertyEditTokens, this.componentPropertyEditPreviousTokens]) {
      for (const record of registry.values()) nextExpiry = Math.min(nextExpiry, record.expiresAt);
    }
    const delay = Math.max(0, nextExpiry - Date.now());
    let timer: ReturnType<typeof setTimeout>;
    timer = setTimeout(() => {
      if (this.componentPropertyEditExpiryTimer !== timer || this.destroyed) return;
      this.componentPropertyEditExpiryTimer = undefined;
      this.pruneExpiredComponentPropertyEditTokens(Date.now());
      this.scheduleComponentPropertyEditExpiry();
    }, delay);
    this.componentPropertyEditExpiryTimer = timer;
  }

  private registerStandaloneRuntime(): void {
    const debuggableWindow = window as DebuggableWindow;
    this.previousStandaloneRuntime = debuggableWindow.__VALDI_WEB_DEBUGGER__;
    const runtime: StandaloneWebDebuggerRuntime = {
      clearHighlight: () => (this.destroyed ? false : this.removeHighlightOverlay()),
      editComponentProperty: request => this.editComponentProperty(request),
      getSnapshot: () => this.getSnapshot(),
      highlightNode: nodeId => this.highlightNode(nodeId),
    };
    this.standaloneRuntime = runtime;
    BRIDGE_CREATED_STANDALONE_RUNTIMES.add(runtime);
    LIVE_BRIDGE_STANDALONE_RUNTIMES.add(runtime);
    PREVIOUS_STANDALONE_RUNTIMES.set(runtime, this.previousStandaloneRuntime);
    debuggableWindow.__VALDI_WEB_DEBUGGER__ = runtime;
  }

  private highlightNode(nodeId: string): boolean {
    if (this.destroyed || !/^[0-9]{1,16}$/.test(nodeId)) {
      return false;
    }
    const numericNodeId = Number(nodeId);
    if (!Number.isSafeInteger(numericNodeId) || numericNodeId < 0) {
      return false;
    }
    const node = this.delegate.getDebugNode(numericNodeId);
    if (!node || !document.body) {
      return false;
    }

    this.removeHighlightOverlay();
    const bounds = node.htmlElement.getBoundingClientRect();
    const overlay = document.createElement('div');
    overlay.setAttribute('aria-hidden', 'true');
    overlay.dataset['valdiDebuggerOverlay'] = nodeId;
    Object.assign(overlay.style, {
      backgroundColor: 'rgba(26, 115, 232, 0.16)',
      border: '2px solid rgb(26, 115, 232)',
      boxSizing: 'border-box',
      height: `${bounds.height}px`,
      left: `${bounds.left}px`,
      pointerEvents: 'none',
      position: 'fixed',
      top: `${bounds.top}px`,
      width: `${bounds.width}px`,
      zIndex: '2147483647',
    });

    const label = document.createElement('div');
    label.textContent = `${node.type} · ${Math.round(bounds.width)} × ${Math.round(bounds.height)}`;
    Object.assign(label.style, {
      backgroundColor: 'rgb(26, 115, 232)',
      borderRadius: '2px',
      color: 'white',
      font: '11px -apple-system, BlinkMacSystemFont, sans-serif',
      left: '-2px',
      padding: '3px 6px',
      position: 'absolute',
      top: bounds.top > 24 ? '-23px' : '0',
      whiteSpace: 'nowrap',
    });
    overlay.appendChild(label);
    document.body.appendChild(overlay);
    this.highlightedNode = overlay;
    return true;
  }

  private removeHighlightOverlay(): boolean {
    if (!this.highlightedNode) {
      return false;
    }
    this.highlightedNode.remove();
    this.highlightedNode = undefined;
    return true;
  }
}

interface ParsedComponentPropertyEditRequest {
  readonly componentId: string;
  readonly componentToken: string;
  readonly propertyName: string;
  readonly protocolVersion: number;
  readonly snapshotRevision: number;
  readonly value: boolean | number | string;
}

function parseComponentPropertyEditRequest(request: unknown): ParsedComponentPropertyEditRequest | undefined {
  if (typeof request !== 'object' || request === null || Array.isArray(request)) return undefined;
  try {
    if (Object.getOwnPropertySymbols(request).length !== 0) return undefined;
    const descriptors = Object.getOwnPropertyDescriptors(request);
    const protocolVersionDescriptor = descriptors['protocolVersion'];
    if (
      protocolVersionDescriptor === undefined ||
      protocolVersionDescriptor.enumerable !== true ||
      !Object.prototype.hasOwnProperty.call(protocolVersionDescriptor, 'value')
    ) {
      return undefined;
    }
    const protocolVersion = protocolVersionDescriptor.value;
    if (!Number.isSafeInteger(protocolVersion)) return undefined;
    const expectedNames = COMPONENT_PROPERTY_EDIT_REQUEST_KEYS_BY_PROTOCOL_VERSION.get(protocolVersion as number);
    if (expectedNames === undefined) return undefined;
    const names = Object.keys(descriptors).sort();
    if (names.length !== expectedNames.length || names.some((name, index) => name !== expectedNames[index])) {
      return undefined;
    }
    const values = Object.create(null) as Record<string, unknown>;
    for (const name of names) {
      const descriptor = descriptors[name];
      if (
        descriptor === undefined ||
        descriptor.enumerable !== true ||
        !Object.prototype.hasOwnProperty.call(descriptor, 'value')
      ) {
        return undefined;
      }
      values[name] = descriptor.value;
    }
    const componentId = values['componentId'];
    const componentToken = values['componentToken'];
    const propertyName = values['propertyName'];
    const snapshotRevision = values['snapshotRevision'];
    const value = values['value'];
    if (
      typeof componentId !== 'string' ||
      componentId.length === 0 ||
      componentId.length > MAX_COMPONENT_ID_CHARACTERS ||
      typeof componentToken !== 'string' ||
      !COMPONENT_PROPERTY_TOKEN_PATTERN.test(componentToken) ||
      typeof propertyName !== 'string' ||
      propertyName.trim().length === 0 ||
      propertyName.length > MAX_COMPONENT_PROPERTY_NAME_CHARACTERS ||
      !Number.isSafeInteger(snapshotRevision) ||
      (snapshotRevision as number) <= 0 ||
      !isEditableComponentPropertyValue(value)
    ) {
      return undefined;
    }
    return {
      componentId,
      componentToken,
      propertyName,
      protocolVersion: protocolVersion as number,
      snapshotRevision: snapshotRevision as number,
      value,
    };
  } catch (_error) {
    return undefined;
  }
}

function isEditableComponentPropertyValue(value: unknown): value is boolean | number | string {
  return (
    (typeof value === 'string' && isUtf8ByteLengthAtMost(value, MAX_COMPONENT_PROPERTY_STRING_BYTES)) ||
    typeof value === 'boolean' ||
    (typeof value === 'number' && Number.isFinite(value) && !Object.is(value, -0))
  );
}

function isUtf8ByteLengthAtMost(value: string, maximumBytes: number): boolean {
  let bytes = 0;
  for (let index = 0; index < value.length; index++) {
    const codePoint = value.codePointAt(index)!;
    if (codePoint <= 0x7f) bytes += 1;
    else if (codePoint <= 0x7ff) bytes += 2;
    else if (codePoint <= 0xffff) bytes += 3;
    else {
      bytes += 4;
      index++;
    }
    if (bytes > maximumBytes) return false;
  }
  return true;
}

function createComponentPropertyToken(hasToken: (token: string) => boolean): string | undefined {
  try {
    const getRandomValues = globalThis.crypto?.getRandomValues;
    if (typeof getRandomValues !== 'function') return undefined;
    for (let attempt = 0; attempt < 8; attempt++) {
      const bytes = new Uint8Array(COMPONENT_PROPERTY_TOKEN_BYTES);
      getRandomValues.call(globalThis.crypto, bytes);
      const token = Array.from(bytes, byte => byte.toString(16).padStart(2, '0')).join('');
      if (COMPONENT_PROPERTY_TOKEN_PATTERN.test(token) && !hasToken(token)) return token;
    }
    return undefined;
  } catch (_error) {
    return undefined;
  }
}

function stripComponentPropertyEditMetadata(snapshot: WebRendererDebugSnapshot): void {
  if (snapshot.tree === null) return;
  const pending = [snapshot.tree];
  const visited = new Set<object>();
  while (pending.length > 0) {
    const node = pending.pop()!;
    if (visited.has(node)) continue;
    visited.add(node);
    if (node.component !== undefined) delete node.component.propertyEdits;
    pending.push(...node.children);
  }
}

function captureBoundedSourceMetadata(value: string): string {
  const maximumContentCharacters = MAX_SOURCE_METADATA_SERIALIZED_CHARACTERS - 2;
  const markerCharacters = getJsonStringContentCharacterLength(SOURCE_METADATA_TRUNCATION_MARKER);
  const contentCharacterLimit = maximumContentCharacters - markerCharacters;
  const output: string[] = [];
  let outputCharacters = 0;
  let index = 0;
  while (index < value.length) {
    const characterCode = value.charCodeAt(index);
    let inputCharacters = 1;
    let serializedCharacters: number;
    if (characterCode >= 0xd800 && characterCode <= 0xdbff) {
      const nextCharacterCode = value.charCodeAt(index + 1);
      if (nextCharacterCode >= 0xdc00 && nextCharacterCode <= 0xdfff) {
        inputCharacters = 2;
        serializedCharacters = 2;
      } else {
        serializedCharacters = 6;
      }
    } else if (characterCode >= 0xdc00 && characterCode <= 0xdfff) {
      serializedCharacters = 6;
    } else if (
      characterCode === 0x22 ||
      characterCode === 0x5c ||
      characterCode === 0x08 ||
      characterCode === 0x09 ||
      characterCode === 0x0a ||
      characterCode === 0x0c ||
      characterCode === 0x0d
    ) {
      serializedCharacters = 2;
    } else {
      serializedCharacters = characterCode < 0x20 ? 6 : 1;
    }
    if (outputCharacters + serializedCharacters > contentCharacterLimit) {
      break;
    }
    output.push(value.slice(index, index + inputCharacters));
    outputCharacters += serializedCharacters;
    index += inputCharacters;
  }
  return index === value.length ? output.join('') : `${output.join('')}${SOURCE_METADATA_TRUNCATION_MARKER}`;
}

function getJsonStringContentCharacterLength(value: string): number {
  return JSON.stringify(value).length - 2;
}

function getRestorableStandaloneRuntime(
  runtime: StandaloneWebDebuggerRuntime | undefined,
): StandaloneWebDebuggerRuntime | undefined {
  const visited = new Set<StandaloneWebDebuggerRuntime>();
  let candidate = runtime;
  while (
    candidate !== undefined &&
    BRIDGE_CREATED_STANDALONE_RUNTIMES.has(candidate) &&
    !LIVE_BRIDGE_STANDALONE_RUNTIMES.has(candidate)
  ) {
    if (visited.has(candidate)) {
      return undefined;
    }
    visited.add(candidate);
    candidate = PREVIOUS_STANDALONE_RUNTIMES.get(candidate);
  }
  return candidate;
}

function shouldEnableWebDebuggerBridge(): boolean {
  if (typeof window === 'undefined' || window.parent !== window) {
    return false;
  }
  if (!hasWebLocationQueryParameter(window.location.search, WEB_DEBUGGER_QUERY_KEY, WEB_DEBUGGER_QUERY_VALUE)) {
    return false;
  }
  return (
    hasWebLocationQueryParameter(window.location.search, DEVTOOLS_QUERY_KEY, WEB_DEBUGGER_QUERY_VALUE) ||
    hasWebLocationQueryParameter(window.location.search, OWL_DEBUGGER_QUERY_KEY, WEB_DEBUGGER_QUERY_VALUE)
  );
}
