import type { IRenderer } from 'valdi_core/src/IRenderer';
import type { ValdiWebRendererDelegate, WebRendererDebugSnapshot } from '../ValdiWebRendererDelegate';
import { MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS } from '../ValdiWebRendererDelegate';
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
  source: {
    title: string;
    url: string;
  };
  snapshot: WebRendererDebugSnapshot;
  type: string;
}

export interface StandaloneWebDebuggerRuntime {
  clearHighlight?(): boolean;
  getSnapshot(): StandaloneWebDebuggerSnapshot;
  highlightNode?(nodeId: string): boolean;
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

export class WebDebuggerBridge {
  private destroyed = false;
  private readonly enabled: boolean;
  private highlightedNode?: HTMLDivElement;
  private standaloneRuntime?: StandaloneWebDebuggerRuntime;
  private previousStandaloneRuntime?: StandaloneWebDebuggerRuntime;

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
    this.removeHighlightOverlay();
    this.destroyed = true;

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
    const source = {
      title: captureBoundedSourceMetadata(document.title),
      url: captureBoundedSourceMetadata(window.location.href),
    };
    const envelopeCharacters =
      JSON.stringify({
        channel: WEB_DEBUGGER_CHANNEL,
        source,
        snapshot: null,
        type: 'snapshot',
      }).length - 'null'.length;
    const snapshot = this.delegate.getDebugSnapshot(
      this.renderer,
      Math.max(0, MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS - envelopeCharacters),
    );
    const response: StandaloneWebDebuggerSnapshot = {
      channel: WEB_DEBUGGER_CHANNEL,
      source,
      snapshot,
      type: 'snapshot',
    };
    if (JSON.stringify(response).length <= MAX_WEB_DEBUGGER_SERIALIZED_CHARACTERS) {
      return response;
    }
    return {
      ...response,
      snapshot: {
        tree: null,
        viewport: {
          width: 0,
          height: 0,
        },
      },
    };
  }

  private registerStandaloneRuntime(): void {
    const debuggableWindow = window as DebuggableWindow;
    this.previousStandaloneRuntime = debuggableWindow.__VALDI_WEB_DEBUGGER__;
    const runtime: StandaloneWebDebuggerRuntime = {
      clearHighlight: () => (this.destroyed ? false : this.removeHighlightOverlay()),
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
