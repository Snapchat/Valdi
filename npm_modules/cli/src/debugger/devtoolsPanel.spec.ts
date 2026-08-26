import 'jasmine';
import fs from 'node:fs';
import path from 'node:path';
import { Script } from 'node:vm';

interface DevToolsTreeNode {
  bounds?: { height: number; width: number; x: number; y: number };
  children: DevToolsTreeNode[];
  component?: { elementId?: string; key: string; name: string };
  element?: {
    attributes: Record<string, unknown>;
    dom: { attributes: Record<string, string>; tagName: string; textContent?: string };
    id: number;
  };
  id: string;
  tag: string;
}

interface TreeStubElement {
  checked: boolean;
  className: string;
  innerHTML: string;
  scrollTop: number;
  textContent: string;
  value: string;
  addEventListener(): void;
  contains(): boolean;
  focus(): void;
  removeAttribute(): void;
  setAttribute(): void;
}

interface DevToolsHierarchyPanel {
  inspectorContent: TreeStubElement;
  state: {
    activeDetail: string;
    expandedNodeIds: Set<string>;
    highlightMayBeActive: boolean;
    highlightRequestTail: Promise<void>;
    highlightTimer: number | null;
    search: string;
    selectedNodeId: string | null;
    snapshot: { tree: DevToolsTreeNode } | null;
    snapshotGeneration: number;
    target: { id: string; sessionId: string } | null;
  };
  treeContent: TreeStubElement;
  findNode(id: string): DevToolsTreeNode | null;
  inspectedNodeId(node: DevToolsTreeNode | null): string | null;
  queueHighlight(nodeId: string | null): void;
  refreshSnapshot(): Promise<void>;
  renderInspector(): void;
  renderTree(): void;
  selectNode(id: string): void;
}

interface StubElement {
  checked: boolean;
  className: string;
  innerHTML: string;
  scrollHeight: number;
  scrollTop: number;
  textContent: string;
  addEventListener(type: string, listener: (event: unknown) => void): void;
  dispatch(type: string): void;
}

interface MockConsoleEventSource {
  closed: boolean;
  url: string;
  addEventListener(type: string, listener: (event: { data: string }) => void): void;
  close(): void;
  emit(type: string, payload: Record<string, unknown>): void;
}

interface DevToolsConsolePanel {
  consoleMessages: StubElement;
  liveToggle: StubElement;
  state: {
    autoRefresh: boolean;
    consoleEntries: Array<{ kind: string; value: string }>;
    consoleStream: MockConsoleEventSource | null;
    target: { id: string; sessionId: string } | null;
  };
  evaluateConsoleExpression(expression: string): Promise<void>;
  startConsoleStream(): void;
  stopConsoleStream(): void;
  dispatchWindowEvent(type: string): void;
}

function componentTree(): DevToolsTreeNode {
  return {
    bounds: { height: 80, width: 220, x: 4, y: 8 },
    children: [
      {
        bounds: { height: 80, width: 220, x: 4, y: 8 },
        children: [
          {
            bounds: { height: 20, width: 140, x: 12, y: 24 },
            children: [
              {
                bounds: { height: 20, width: 140, x: 12, y: 24 },
                children: [],
                element: {
                  attributes: { accessibilityLabel: 'Continue' },
                  dom: { attributes: { role: 'button' }, tagName: 'span', textContent: 'Continue' },
                  id: 8,
                },
                id: '8',
                tag: 'label',
              },
            ],
            component: { elementId: '8', key: 'nested', name: 'NestedExampleComponent' },
            id: 'component:["7","nested"]',
            tag: 'NestedExampleComponent',
          },
        ],
        element: {
          attributes: { accessibilityId: 'sample.root' },
          dom: { attributes: { id: 'sample.root' }, tagName: 'div' },
          id: 7,
        },
        id: '7',
        tag: 'view',
      },
    ],
    component: { elementId: '7', key: 'root', name: 'RootExampleComponent' },
    id: 'component:[null,"root"]',
    tag: 'RootExampleComponent',
  };
}

describe('integrated DevTools component hierarchy', () => {
  let fetchRequests: Array<{ body?: string; url: string }>;
  let fetchResponse: Record<string, unknown>;
  let queuedFetchResponses: Array<Promise<{ ok: boolean; json(): Promise<Record<string, unknown>> }>>;
  let panel: DevToolsHierarchyPanel;
  let timers: Map<number, () => void>;

  beforeEach(() => {
    fetchRequests = [];
    fetchResponse = { highlighted: true };
    queuedFetchResponses = [];
    timers = new Map();
    let nextTimerId = 1;
    const treeModelSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'debugger-tree-model.js'), 'utf8');
    const rawPanelSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'devtools-panel.js'), 'utf8');
    const panelSource = rawPanelSource.replace('void connectToInspectedApplication();', 'void 0;');
    const elements = new Map<string, TreeStubElement>();
    const document = {
      addEventListener() {},
      documentElement: { dataset: {} },
      getElementById(id: string): TreeStubElement {
        let element = elements.get(id);
        if (element === undefined) {
          element = {
            checked: true,
            className: '',
            innerHTML: '',
            scrollTop: 0,
            textContent: '',
            value: '',
            addEventListener() {},
            contains: () => false,
            focus() {},
            removeAttribute() {},
            setAttribute() {},
          };
          elements.set(id, element);
        }
        return element;
      },
      querySelectorAll(): TreeStubElement[] {
        return [];
      },
    };
    const window = {
      addEventListener() {},
      clearInterval() {},
      clearTimeout(timerId: number) {
        timers.delete(timerId);
      },
      location: {
        origin: 'http://127.0.0.1:18768',
        search:
          '?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Findex.html%3FvaldiDevTools%3D1&targetNonce=panel-target-nonce-123456',
      },
      parent: {},
      setInterval: () => 1,
      setTimeout(callback: () => void): number {
        const timerId = nextTimerId++;
        timers.set(timerId, callback);
        return timerId;
      },
    };

    panel = new Script(
      `${treeModelSource}\n${panelSource}\n({ findNode, inspectedNodeId, inspectorContent: elements.inspector, queueHighlight, refreshSnapshot, renderInspector, renderTree, selectNode, state, treeContent: elements.tree })`,
    ).runInNewContext({
      URL,
      URLSearchParams,
      console,
      document,
      fetch: (url: URL, options: { body?: string }) => {
        fetchRequests.push({ ...(options.body === undefined ? {} : { body: options.body }), url: url.toString() });
        return (
          queuedFetchResponses.shift() ?? Promise.resolve({ json: () => Promise.resolve(fetchResponse), ok: true })
        );
      },
      navigator: { clipboard: { writeText: () => Promise.resolve() } },
      window,
    }) as DevToolsHierarchyPanel;
    panel.state.target = { id: 'owl:web-preview', sessionId: 'web-preview' };
    panel.state.snapshot = { tree: componentTree() };
    panel.state.snapshotGeneration = 1;
  });

  it('renders and searches component rows without hiding their physical descendants', () => {
    panel.state.expandedNodeIds.add('component:[null,"root"]');
    panel.state.expandedNodeIds.add('7');
    panel.state.expandedNodeIds.add('component:["7","nested"]');
    panel.state.selectedNodeId = '8';

    panel.renderTree();

    expect(panel.treeContent.innerHTML).toContain('class="tree-row component-row"');
    expect(panel.treeContent.innerHTML).toContain('<span class="tag-name">RootExampleComponent</span>');
    expect(panel.treeContent.innerHTML).toContain('<span class="tag-name">NestedExampleComponent</span>');
    expect(panel.treeContent.innerHTML).toContain('data-node-id="7"');
    expect(panel.treeContent.innerHTML).toContain('data-node-id="8"');
    expect(panel.treeContent.innerHTML).toContain('<span class="tree-text">Continue</span>');

    panel.state.search = 'nestedexample';
    panel.renderTree();
    expect(panel.treeContent.innerHTML).toContain('RootExampleComponent');
    expect(panel.treeContent.innerHTML).toContain('data-node-id="7"');
    expect(panel.treeContent.innerHTML).toContain('NestedExampleComponent');
    expect(panel.treeContent.innerHTML).not.toContain('data-node-id="8"');
  });

  it('keeps component selection while inspecting the current backing element', () => {
    panel.state.activeDetail = 'dom';

    panel.selectNode('component:["7","nested"]');

    expect(panel.state.selectedNodeId).toBe('component:["7","nested"]');
    expect(panel.inspectorContent.innerHTML).toContain('Valdi component');
    expect(panel.inspectorContent.innerHTML).toContain('NestedExampleComponent');
    expect(panel.inspectorContent.innerHTML).toContain('<span class="property-name">elementId</span>');
    expect(panel.inspectorContent.innerHTML).toContain('Rendered &lt;span&gt;');
    expect(panel.inspectorContent.innerHTML).toContain('Text content');
  });

  it('preserves keyed component selection across updates and safely falls back when it disappears', async () => {
    panel.selectNode('component:["7","nested"]');
    fetchResponse = { tree: componentTree() };

    await panel.refreshSnapshot();

    expect(panel.state.selectedNodeId).toBe('component:["7","nested"]');
    expect(panel.state.expandedNodeIds.has('component:[null,"root"]')).toBeTrue();
    expect(panel.state.expandedNodeIds.has('7')).toBeTrue();

    const elementRoot = componentTree().children[0];
    elementRoot.children = [elementRoot.children[0].children[0]];
    fetchResponse = { tree: elementRoot };
    await panel.refreshSnapshot();

    expect(panel.state.selectedNodeId).toBe('7');
    expect(panel.findNode('component:["7","nested"]')).toBeNull();
  });

  it('maps highlights to backing elements and drops timers from stale snapshot generations', async () => {
    const nestedComponent = panel.findNode('component:["7","nested"]');
    panel.queueHighlight(panel.inspectedNodeId(nestedComponent));
    const currentTimerId = panel.state.highlightTimer;
    if (currentTimerId === null) throw new Error('Expected a current-generation highlight timer id.');
    const currentTimer = timers.get(currentTimerId);
    if (currentTimer === undefined) throw new Error('Expected a current-generation highlight timer.');
    currentTimer();
    await Promise.resolve();

    expect(fetchRequests.length).toBe(1);
    const highlightRequest = fetchRequests[0];
    if (highlightRequest === undefined || highlightRequest.body === undefined) {
      throw new Error('Expected a serialized highlight request.');
    }
    expect(highlightRequest.url).toBe('http://127.0.0.1:18768/api/devtools/highlight');
    expect(JSON.parse(highlightRequest.body)).toEqual({
      inspectedUrl: 'http://127.0.0.1:54321/index.html?valdiDevTools=1',
      nodeId: '8',
      sessionId: 'web-preview',
      targetNonce: 'panel-target-nonce-123456',
    });

    panel.queueHighlight('7');
    const staleTimerId = panel.state.highlightTimer;
    if (staleTimerId === null) throw new Error('Expected a queued highlight timer id.');
    const staleTimer = timers.get(staleTimerId);
    if (staleTimer === undefined) throw new Error('Expected a queued highlight timer.');
    panel.state.snapshotGeneration++;
    staleTimer();
    await Promise.resolve();

    expect(fetchRequests.length).toBe(1);
  });

  it('orders a final clear after an older in-flight highlight request', async () => {
    let resolveHighlight: ((response: { ok: boolean; json(): Promise<Record<string, unknown>> }) => void) | undefined;
    queuedFetchResponses.push(
      new Promise(resolve => {
        resolveHighlight = resolve;
      }),
    );

    panel.queueHighlight('8');
    const highlightTimerId = panel.state.highlightTimer;
    if (highlightTimerId === null) throw new Error('Expected a highlight timer id.');
    const highlightTimer = timers.get(highlightTimerId);
    if (highlightTimer === undefined) throw new Error('Expected a highlight timer.');
    highlightTimer();
    await Promise.resolve();

    panel.queueHighlight(null);
    const clearTimerId = panel.state.highlightTimer;
    if (clearTimerId === null) throw new Error('Expected a clear timer id.');
    const clearTimer = timers.get(clearTimerId);
    if (clearTimer === undefined) throw new Error('Expected a clear timer.');
    clearTimer();
    await Promise.resolve();

    expect(fetchRequests.length).toBe(1);
    expect(panel.state.highlightMayBeActive).toBeTrue();
    if (resolveHighlight === undefined) throw new Error('Expected a deferred highlight response.');
    resolveHighlight({ json: () => Promise.resolve({ highlighted: true }), ok: true });
    await panel.state.highlightRequestTail;

    expect(fetchRequests.length).toBe(2);
    const clearRequest = fetchRequests[1];
    if (clearRequest === undefined || clearRequest.body === undefined) {
      throw new Error('Expected a serialized clear request.');
    }
    expect(JSON.parse(clearRequest.body)).toEqual({
      inspectedUrl: 'http://127.0.0.1:54321/index.html?valdiDevTools=1',
      sessionId: 'web-preview',
      targetNonce: 'panel-target-nonce-123456',
    });
    expect(panel.state.highlightMayBeActive).toBeFalse();
  });

  it('requeues the final clear when a snapshot refresh supersedes a pending clear', async () => {
    let resolveHighlight: ((response: { ok: boolean; json(): Promise<Record<string, unknown>> }) => void) | undefined;
    queuedFetchResponses.push(
      new Promise(resolve => {
        resolveHighlight = resolve;
      }),
    );

    panel.queueHighlight('8');
    const highlightTimerId = panel.state.highlightTimer;
    if (highlightTimerId === null) throw new Error('Expected a highlight timer id.');
    const highlightTimer = timers.get(highlightTimerId);
    if (highlightTimer === undefined) throw new Error('Expected a highlight timer.');
    highlightTimer();
    await Promise.resolve();

    fetchResponse = { tree: componentTree() };
    await panel.refreshSnapshot();
    const firstClearTimerId = panel.state.highlightTimer;
    if (firstClearTimerId === null) throw new Error('Expected the first refresh clear timer.');
    const firstClearTimer = timers.get(firstClearTimerId);
    if (firstClearTimer === undefined) throw new Error('Expected the first refresh clear timer callback.');
    firstClearTimer();

    await panel.refreshSnapshot();
    const finalClearTimerId = panel.state.highlightTimer;
    if (finalClearTimerId === null) throw new Error('Expected the final refresh clear timer.');
    const finalClearTimer = timers.get(finalClearTimerId);
    if (finalClearTimer === undefined) throw new Error('Expected the final refresh clear timer callback.');
    finalClearTimer();
    await Promise.resolve();

    const highlightRequestsBeforeResolution = fetchRequests.filter(request =>
      request.url.endsWith('/api/devtools/highlight'),
    );
    expect(highlightRequestsBeforeResolution.length).toBe(1);
    if (resolveHighlight === undefined) throw new Error('Expected a deferred highlight response.');
    resolveHighlight({ json: () => Promise.resolve({ highlighted: true }), ok: true });
    await panel.state.highlightRequestTail;

    const highlightRequests = fetchRequests.filter(request => request.url.endsWith('/api/devtools/highlight'));
    expect(highlightRequests.length).toBe(2);
    const finalRequestBody = highlightRequests[1]?.body;
    if (finalRequestBody === undefined) throw new Error('Expected a serialized final clear request.');
    const finalRequest = JSON.parse(finalRequestBody) as Record<string, unknown>;
    expect(finalRequest.nodeId).toBeUndefined();
    expect(panel.state.highlightMayBeActive).toBeFalse();
  });
});

describe('integrated DevTools console panel', () => {
  let eventSources: MockConsoleEventSource[];
  let panel: DevToolsConsolePanel;
  let requests: Array<{ body?: string; url: string }>;

  beforeEach(() => {
    eventSources = [];
    requests = [];
    const rawSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'devtools-panel.js'), 'utf8');
    const source = rawSource.replace('void connectToInspectedApplication();', 'void 0;');
    const elements = new Map<string, StubElement>();
    const windowListeners = new Map<string, () => void>();
    const document = {
      addEventListener() {},
      documentElement: { dataset: {} },
      getElementById(id: string): StubElement {
        let element = elements.get(id);
        if (element === undefined) {
          const listeners = new Map<string, (event: unknown) => void>();
          element = {
            checked: true,
            className: '',
            innerHTML: '',
            scrollHeight: 100,
            scrollTop: 0,
            textContent: '',
            addEventListener(type: string, listener: (event: unknown) => void): void {
              listeners.set(type, listener);
            },
            dispatch(type: string): void {
              listeners.get(type)?.({});
            },
          };
          elements.set(id, element);
        }
        return element;
      },
      querySelectorAll(): StubElement[] {
        return [];
      },
    };
    const window = {
      addEventListener(type: string, listener: () => void): void {
        windowListeners.set(type, listener);
      },
      location: {
        origin: 'http://127.0.0.1:18768',
        search:
          '?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Findex.html%3FvaldiDevTools%3D1&targetNonce=panel-target-nonce-123456',
      },
      parent: {},
    };

    class MockEventSource implements MockConsoleEventSource {
      closed = false;
      private readonly listeners = new Map<string, (event: { data: string }) => void>();

      constructor(readonly url: string) {
        eventSources.push(this);
      }

      addEventListener(type: string, listener: (event: { data: string }) => void): void {
        this.listeners.set(type, listener);
      }

      close(): void {
        this.closed = true;
      }

      emit(type: string, payload: Record<string, unknown>): void {
        this.listeners.get(type)?.({ data: JSON.stringify(payload) });
      }
    }

    panel = new Script(
      `${source}\n({ consoleMessages: elements.consoleMessages, dispatchWindowEvent: type => windowListeners.get(type)?.(), evaluateConsoleExpression, liveToggle: elements.autoRefreshToggle, startConsoleStream, state, stopConsoleStream })`,
    ).runInNewContext({
      EventSource: MockEventSource,
      URL,
      URLSearchParams,
      console,
      document,
      elements,
      fetch: (url: URL, options: { body?: string }) => {
        requests.push({ ...(options.body === undefined ? {} : { body: options.body }), url: url.toString() });
        return Promise.resolve({
          json: () => Promise.resolve({ type: 'number', value: 4 }),
          ok: true,
        });
      },
      window,
      windowListeners,
    }) as DevToolsConsolePanel;
    panel.state.target = { id: 'owl:web-preview', sessionId: 'web-preview' };
  });

  it('binds the stream to the exact target tuple and safely renders only matching events', () => {
    panel.startConsoleStream();
    const stream = eventSources[0];
    if (!stream) throw new Error('Expected the Chromium console stream to connect.');
    const streamUrl = new URL(stream.url);

    expect(streamUrl.pathname).toBe('/api/devtools/console/stream');
    expect(streamUrl.searchParams.get('sessionId')).toBe('web-preview');
    expect(streamUrl.searchParams.get('targetNonce')).toBe('panel-target-nonce-123456');
    expect(streamUrl.searchParams.get('inspectedUrl')).toBe('http://127.0.0.1:54321/index.html?valdiDevTools=1');

    stream.emit('console', {
      level: 'warn',
      message: 'Synthetic <renderer> warning',
      sessionId: 'web-preview',
      source: 'console',
      targetId: 'owl:web-preview',
      timestamp: 42,
    });
    stream.emit('console', {
      level: 'warn',
      message: 'Synthetic <renderer> warning',
      sessionId: 'web-preview',
      source: 'console',
      targetId: 'owl:web-preview',
      timestamp: 42,
    });
    stream.emit('console', {
      level: 'error',
      message: 'Wrong target',
      sessionId: 'another-session',
      targetId: 'owl:web-preview',
      timestamp: 43,
    });

    expect(panel.state.consoleEntries).toEqual([
      jasmine.objectContaining({ kind: 'warn', value: 'Synthetic <renderer> warning' }),
    ]);
    expect(panel.consoleMessages.innerHTML).toContain('class="console-entry warn"');
    expect(panel.consoleMessages.innerHTML).toContain('Synthetic &lt;renderer&gt; warning');
    expect(panel.consoleMessages.innerHTML).not.toContain('<renderer>');
    expect(panel.consoleMessages.innerHTML).not.toContain('Wrong target');
  });

  it('isolates reconnects, honors the Live toggle, and tears down on pagehide', () => {
    panel.startConsoleStream();
    const first = eventSources[0];
    if (!first) throw new Error('Expected the first Chromium console stream.');

    panel.state.target = { id: 'owl:replacement', sessionId: 'replacement' };
    panel.startConsoleStream();
    const second = eventSources[1];
    if (!second) throw new Error('Expected the replacement Chromium console stream.');
    first.emit('console', {
      level: 'error',
      message: 'Stale target',
      sessionId: 'web-preview',
      targetId: 'owl:web-preview',
      timestamp: 1,
    });
    second.emit('console', {
      level: 'info',
      message: 'Replacement target',
      sessionId: 'replacement',
      targetId: 'owl:replacement',
      timestamp: 2,
    });

    expect(first.closed).toBeTrue();
    expect(panel.state.consoleEntries).toEqual([
      jasmine.objectContaining({ kind: 'info', value: 'Replacement target' }),
    ]);

    panel.liveToggle.checked = false;
    panel.liveToggle.dispatch('change');
    expect(second.closed).toBeTrue();
    expect(panel.state.consoleStream).toBeNull();

    panel.liveToggle.checked = true;
    panel.liveToggle.dispatch('change');
    const resumed = eventSources[2];
    if (!resumed) throw new Error('Expected a resumed Chromium console stream.');
    panel.dispatchWindowEvent('pagehide');
    expect(resumed.closed).toBeTrue();
    expect(panel.state.consoleStream).toBeNull();
  });

  it('keeps manual evaluation bound to the same target tuple while streaming', async () => {
    panel.startConsoleStream();
    await panel.evaluateConsoleExpression('2 + 2');

    expect(requests).toContain(
      jasmine.objectContaining({
        body: JSON.stringify({
          expression: '2 + 2',
          inspectedUrl: 'http://127.0.0.1:54321/index.html?valdiDevTools=1',
          sessionId: 'web-preview',
          targetNonce: 'panel-target-nonce-123456',
        }),
        url: 'http://127.0.0.1:18768/api/devtools/evaluate',
      }),
    );
    expect(panel.state.consoleEntries).toEqual([
      jasmine.objectContaining({ kind: 'input', value: '2 + 2' }),
      jasmine.objectContaining({ kind: 'result', value: '4' }),
    ]);
  });
});
