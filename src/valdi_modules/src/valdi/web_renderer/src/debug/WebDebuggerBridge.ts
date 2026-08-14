import type { ViewNodeTree } from '../core/ViewNodeTree';

const WEB_DEBUGGER_CHANNEL = 'valdi-web-debugger';
const WEB_DEBUGGER_QUERY_KEY = 'valdiDebugger';
const WEB_DEBUGGER_QUERY_VALUE = '1';

interface WebDebuggerMessage {
  channel?: string;
  type?: string;
}

export class WebDebuggerBridge {
  private mutationObserver?: MutationObserver;
  private snapshotScheduled = false;
  private destroyed = false;
  private readonly enabled: boolean;

  constructor(
    private readonly root: HTMLElement | ShadowRoot,
    private readonly viewNodeTree: ViewNodeTree,
  ) {
    this.enabled = shouldEnableWebDebuggerBridge();
    if (!this.enabled) {
      return;
    }

    window.addEventListener('message', this.handleMessage);
    window.addEventListener('resize', this.handleLayoutChange);
    window.addEventListener('scroll', this.handleLayoutChange, true);
    window.addEventListener('input', this.handleLayoutChange, true);
    window.addEventListener('change', this.handleLayoutChange, true);
    window.addEventListener('pointerdown', this.handlePointerDown, true);
    if (typeof MutationObserver !== 'undefined') {
      this.mutationObserver = new MutationObserver(this.handleLayoutChange);
      this.mutationObserver.observe(this.root, {
        attributes: true,
        childList: true,
        subtree: true,
      });
    }
    this.scheduleSnapshot();
  }

  destroy(): void {
    if (!this.enabled || this.destroyed) {
      return;
    }
    this.destroyed = true;
    window.removeEventListener('message', this.handleMessage);
    window.removeEventListener('resize', this.handleLayoutChange);
    window.removeEventListener('scroll', this.handleLayoutChange, true);
    window.removeEventListener('input', this.handleLayoutChange, true);
    window.removeEventListener('change', this.handleLayoutChange, true);
    window.removeEventListener('pointerdown', this.handlePointerDown, true);
    this.mutationObserver?.disconnect();
    this.mutationObserver = undefined;
  }

  private readonly handleMessage = (event: MessageEvent<WebDebuggerMessage>): void => {
    if (this.destroyed || event.source !== window.parent) {
      return;
    }
    const message = event.data;
    if (message?.channel !== WEB_DEBUGGER_CHANNEL || message.type !== 'request-snapshot') {
      return;
    }
    this.scheduleSnapshot();
  };

  private readonly handleLayoutChange = (): void => {
    this.scheduleSnapshot();
  };

  private readonly handlePointerDown = (event: PointerEvent): void => {
    let element = event.target instanceof Element ? event.target : null;
    while (element) {
      const nodeId = this.viewNodeTree.getNodeIdForHtmlElement(element);
      if (nodeId !== undefined) {
        this.postMessage({
          type: 'selection',
          nodeId: String(nodeId),
        });
        return;
      }
      element = element.parentElement;
    }
  };

  private scheduleSnapshot(): void {
    if (this.destroyed || this.snapshotScheduled) {
      return;
    }
    this.snapshotScheduled = true;
    requestAnimationFrame(() => {
      this.snapshotScheduled = false;
      this.postSnapshot();
    });
  }

  private postSnapshot(): void {
    if (this.destroyed || window.parent === window) {
      return;
    }
    this.postMessage({
      type: 'snapshot',
      source: {
        title: document.title,
        url: window.location.href,
      },
      snapshot: this.viewNodeTree.getDebugSnapshot(),
    });
  }

  private postMessage(payload: Record<string, unknown>): void {
    if (this.destroyed || window.parent === window) {
      return;
    }
    window.parent.postMessage(
      {
        channel: WEB_DEBUGGER_CHANNEL,
        ...payload,
      },
      '*',
    );
  }
}

function shouldEnableWebDebuggerBridge(): boolean {
  if (typeof window === 'undefined' || window.parent === window) {
    return false;
  }
  return new URLSearchParams(window.location.search).get(WEB_DEBUGGER_QUERY_KEY) === WEB_DEBUGGER_QUERY_VALUE;
}
