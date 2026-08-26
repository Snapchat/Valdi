import 'jasmine';
import fs from 'node:fs';
import path from 'node:path';
import { Script } from 'node:vm';

interface DevToolsTreeNode {
  bounds?: { height: number; width: number; x: number; y: number };
  children: DevToolsTreeNode[];
  component?: { elementId?: string; key: string; name: string; properties?: Record<string, unknown> };
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
    target: { capabilities?: string[]; id: string; sessionId: string } | null;
    targetGeneration: number;
  };
  treeContent: TreeStubElement;
  clearTargetPresentation(message: string): void;
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
  value: string;
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

interface PickerTarget {
  attachable: boolean;
  capabilities: string[];
  id: string;
  identityMode: 'inspected-page' | 'target-id';
  name: string;
  platform: string;
  port?: number;
  state: 'attached' | 'available' | 'waiting';
  transport: string;
}

interface PickerStubEvent {
  currentTarget: PickerStubElement;
  key: string;
  target: PickerStubElement;
  preventDefault(): void;
  stopPropagation(): void;
}

interface PickerStubElement {
  checked: boolean;
  children: PickerStubElement[];
  classList: {
    contains(value: string): boolean;
    toggle(value: string, enabled: boolean): void;
  };
  className: string;
  dataset: Record<string, string>;
  disabled: boolean;
  hidden: boolean;
  id: string;
  innerHTML: string;
  open: boolean;
  scrollHeight: number;
  scrollTop: number;
  selected: boolean;
  style: Record<string, string>;
  tabIndex: number;
  textContent: string;
  title: string;
  value: string;
  addEventListener(type: string, listener: (event: PickerStubEvent) => void): void;
  append(child: PickerStubElement): void;
  closest(): PickerStubElement | null;
  contains(): boolean;
  dispatch(type: string, properties?: Partial<PickerStubEvent>): void;
  focus(): void;
  getAttribute(name: string): string | null;
  getBoundingClientRect(): { bottom: number; height: number; left: number; right: number; top: number; width: number };
  querySelector(): PickerStubElement | null;
  querySelectorAll(): PickerStubElement[];
  removeAttribute(name: string): void;
  replaceChildren(): void;
  scrollIntoView(): void;
  setAttribute(name: string, value: string): void;
  setSelectionRange(): void;
}

interface PickerPanel {
  state: {
    activeSection: string;
    consoleEntries: Array<{ kind: string; value: string }>;
    consoleEntryKeys: Set<string>;
    consoleHistory: string[];
    error: string | null;
    expandedNodeIds: Set<string>;
    highlightMayBeActive: boolean;
    highlightRequestTail: Promise<void>;
    highlightTimer: number | null;
    performance: {
      data: Record<string, unknown> | null;
      error: string | null;
      lastTrace: Record<string, unknown> | null;
      ownerIdentity: Record<string, string> | null;
      pending: boolean;
      samples: Record<string, unknown>[];
      snapshotPending: boolean;
      traceActive: boolean;
    };
    refreshPending: boolean;
    registryPending: boolean;
    registryRequestGeneration: number;
    registryTargets: PickerTarget[];
    selectedNodeId: string | null;
    snapshot: { tree: DevToolsTreeNode } | null;
    target: PickerTarget | null;
    targetGeneration: number;
    targetSwitchMessage: string | null;
    unavailableTargetId: string | null;
  };
  applyDirectTargetSelection(target: PickerTarget | null, options?: Record<string, unknown>): boolean;
  connectToInspectedApplication(): Promise<void>;
  evaluateConsoleExpression(expression: string): Promise<void>;
  parseTargetRegistry(payload: unknown): PickerTarget[];
  queueHighlight(nodeId: string | null): void;
  refreshSnapshot(): Promise<void>;
  refreshTargetRegistry(): Promise<void>;
  renderTargetPicker(): void;
  runPerformanceAction(action: string): Promise<void>;
  setActiveSection(section: string): void;
  startConsoleStream(): void;
}

interface DevToolsConsolePanel {
  clearButton: StubElement;
  consoleInput: StubElement;
  consoleMessages: StubElement;
  liveToggle: StubElement;
  state: {
    autoRefresh: boolean;
    consoleEntries: Array<{ kind: string; value: string }>;
    consoleHistory: string[];
    consoleHistoryIndex: number;
    consoleStream: MockConsoleEventSource | null;
    consoleStreamTargetKey: string | null;
    performance: {
      lastTrace: Record<string, unknown> | null;
      ownerIdentity: Record<string, string> | null;
      traceActive: boolean;
    };
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
            component: {
              elementId: '8',
              key: 'nested',
              name: 'NestedExampleComponent',
              properties: { enabled: true, title: '<img src=x onerror=alert(1)>' },
            },
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

function pickerTarget(id: string, overrides: Partial<PickerTarget> = {}): PickerTarget {
  return {
    attachable: true,
    capabilities: ['components', 'snapshot'],
    id,
    identityMode: 'target-id',
    name: `Target ${id}`,
    platform: 'macos',
    port: 9166,
    state: 'available',
    transport: 'valdi-daemon',
    ...overrides,
  };
}

function createPickerStubElement(id: string): PickerStubElement {
  const attributes = new Map<string, string>();
  const classes = new Set<string>();
  const listeners = new Map<string, Array<(event: PickerStubEvent) => void>>();
  const element: PickerStubElement = {
    checked: true,
    children: [],
    classList: {
      contains: value => classes.has(value),
      toggle: (value, enabled) => {
        if (enabled) classes.add(value);
        else classes.delete(value);
      },
    },
    className: '',
    dataset: {},
    disabled: false,
    hidden: false,
    id,
    innerHTML: '',
    open: false,
    scrollHeight: 0,
    scrollTop: 0,
    selected: false,
    style: {},
    tabIndex: -1,
    textContent: '',
    title: '',
    value: '',
    addEventListener(type, listener) {
      const entries = listeners.get(type) ?? [];
      entries.push(listener);
      listeners.set(type, entries);
    },
    append(child) {
      element.children.push(child);
      if (child.selected) element.value = child.value;
    },
    closest: () => null,
    contains: () => true,
    dispatch(type, properties = {}) {
      const event: PickerStubEvent = {
        currentTarget: element,
        key: '',
        preventDefault() {},
        stopPropagation() {},
        target: element,
        ...properties,
      };
      for (const listener of listeners.get(type) ?? []) listener(event);
    },
    focus() {},
    getAttribute: name => attributes.get(name) ?? null,
    getBoundingClientRect: () => ({ bottom: 500, height: 500, left: 0, right: 500, top: 0, width: 500 }),
    querySelector: () => null,
    querySelectorAll: () => [],
    removeAttribute(name) {
      attributes.delete(name);
    },
    replaceChildren() {
      element.children = [];
      element.value = '';
    },
    scrollIntoView() {},
    setAttribute(name, value) {
      attributes.set(name, value);
    },
    setSelectionRange() {},
  };
  return element;
}

interface PickerFetchRequest {
  body?: string;
  method: string;
  url: string;
}

interface PickerResponse {
  ok: boolean;
  status: number;
  json(): Promise<Record<string, unknown>>;
}

interface PickerDeferredResponse {
  reject(error: Error): void;
  resolve(payload: Record<string, unknown>, ok?: boolean, status?: number): void;
}

interface PickerHarness {
  closeTargetIds: Array<string | null>;
  elements: Map<string, PickerStubElement>;
  eventSources: MockConsoleEventSource[];
  fetchRequests: PickerFetchRequest[];
  panel: PickerPanel;
  queueDeferred(pathname: string): PickerDeferredResponse;
  queueResponse(pathname: string, payload: Record<string, unknown>, ok?: boolean, status?: number): void;
  runTimer(timerId: number): void;
}

interface PickerPanelReference {
  value: PickerPanel | undefined;
}

function createPickerHarness(search: string): PickerHarness {
  const treeModelSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'debugger-tree-model.js'), 'utf8');
  const rawPanelSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'devtools-panel.js'), 'utf8');
  const panelSource = rawPanelSource.replace('void connectToInspectedApplication();', 'void 0;');
  const elements = new Map<string, PickerStubElement>();
  const fetchRequests: PickerFetchRequest[] = [];
  const queuedResponses = new Map<string, Array<Promise<PickerResponse>>>();
  const eventSources: MockConsoleEventSource[] = [];
  const closeTargetIds: Array<string | null> = [];
  const timers = new Map<number, () => void>();
  const windowListeners = new Map<string, () => void>();
  const documentDataset: Record<string, string> = {};
  const activePanel: PickerPanelReference = { value: undefined };
  let nextTimerId = 1;
  let activeElement: PickerStubElement | null = null;

  function elementForId(id: string): PickerStubElement {
    let element = elements.get(id);
    if (!element) {
      element = createPickerStubElement(id);
      element.focus = () => {
        activeElement = element ?? null;
      };
      elements.set(id, element);
    }
    return element;
  }

  const mainTabs = ['elements', 'performance', 'console'].map(section => {
    const tab = elementForId(`${section}Tab`);
    tab.dataset['section'] = section;
    return tab;
  });
  const sections = ['elements', 'performance', 'console'].map(section => {
    const panelElement = elementForId(`${section}Section`);
    panelElement.dataset['panel'] = section;
    return panelElement;
  });

  const documentObject = {
    addEventListener() {},
    get activeElement(): PickerStubElement | null {
      return activeElement;
    },
    createElement(type: string): PickerStubElement {
      return createPickerStubElement(type);
    },
    documentElement: { dataset: documentDataset },
    getElementById(id: string): PickerStubElement {
      return elementForId(id);
    },
    hidden: false,
    querySelectorAll(selector: string): PickerStubElement[] {
      if (selector === '.main-tab') return mainTabs;
      if (selector === '.detail-tab') return [];
      if (selector === '.section') return sections;
      return [];
    },
  };

  function response(payload: Record<string, unknown>, ok = true, status = ok ? 200 : 400): PickerResponse {
    return { json: () => Promise.resolve(payload), ok, status };
  }

  function enqueue(pathname: string, plannedResponse: Promise<PickerResponse>): void {
    const entries = queuedResponses.get(pathname) ?? [];
    entries.push(plannedResponse);
    queuedResponses.set(pathname, entries);
  }

  function defaultPayload(pathname: string): Record<string, unknown> {
    if (pathname === '/api/devtools/targets') return { targets: [] };
    if (pathname === '/api/devtools/snapshot') return { tree: componentTree() };
    if (pathname === '/api/devtools/highlight') return { highlighted: true };
    if (pathname === '/api/devtools/evaluate') return { type: 'string', value: 'ok' };
    if (pathname.endsWith('/trace/stop')) return { traceCount: 0, traces: [] };
    return {};
  }

  class PickerEventSource implements MockConsoleEventSource {
    closed = false;
    private readonly listeners = new Map<string, Array<(event: { data: string }) => void>>();

    constructor(readonly url: string) {
      eventSources.push(this);
    }

    addEventListener(type: string, listener: (event: { data: string }) => void): void {
      const entries = this.listeners.get(type) ?? [];
      entries.push(listener);
      this.listeners.set(type, entries);
    }

    close(): void {
      closeTargetIds.push(activePanel.value?.state.target?.id ?? null);
      this.closed = true;
    }

    emit(type: string, payload: Record<string, unknown>): void {
      for (const listener of this.listeners.get(type) ?? []) listener({ data: JSON.stringify(payload) });
    }
  }

  const windowObject = {
    addEventListener(type: string, listener: () => void) {
      windowListeners.set(type, listener);
    },
    clearInterval() {},
    clearTimeout(timerId: number) {
      timers.delete(timerId);
    },
    confirm: () => true,
    location: { origin: 'http://127.0.0.1:18768', search },
    parent: {},
    removeEventListener() {},
    setInterval: () => nextTimerId++,
    setTimeout(callback: () => void): number {
      const timerId = nextTimerId++;
      timers.set(timerId, callback);
      return timerId;
    },
  };

  const panel = new Script(
    `${treeModelSource}\n${panelSource}\n({ applyDirectTargetSelection, connectToInspectedApplication, evaluateConsoleExpression, parseTargetRegistry, queueHighlight, refreshSnapshot, refreshTargetRegistry, renderTargetPicker, runPerformanceAction, setActiveSection, startConsoleStream, state })`,
  ).runInNewContext({
    Blob,
    EventSource: PickerEventSource,
    URL,
    URLSearchParams,
    console,
    document: documentObject,
    fetch: (input: URL, options: { body?: string; method: string }) => {
      const url = new URL(input.toString());
      fetchRequests.push({
        ...(options.body === undefined ? {} : { body: options.body }),
        method: options.method,
        url: url.toString(),
      });
      const entries = queuedResponses.get(url.pathname);
      return entries?.shift() ?? Promise.resolve(response(defaultPayload(url.pathname)));
    },
    navigator: { clipboard: { writeText: () => Promise.resolve() } },
    window: windowObject,
  }) as PickerPanel;
  activePanel.value = panel;

  return {
    closeTargetIds,
    elements,
    eventSources,
    fetchRequests,
    panel,
    queueDeferred(pathname) {
      let resolveResponse: ((value: PickerResponse) => void) | undefined;
      let rejectResponse: ((error: Error) => void) | undefined;
      enqueue(
        pathname,
        new Promise<PickerResponse>((resolve, reject) => {
          resolveResponse = resolve;
          rejectResponse = reject;
        }),
      );
      return {
        reject(error) {
          if (!rejectResponse) throw new Error('Deferred response was not initialized.');
          rejectResponse(error);
        },
        resolve(payload, ok = true, status = ok ? 200 : 400) {
          if (!resolveResponse) throw new Error('Deferred response was not initialized.');
          resolveResponse(response(payload, ok, status));
        },
      };
    },
    queueResponse(pathname, payload, ok = true, status = ok ? 200 : 400) {
      enqueue(pathname, Promise.resolve(response(payload, ok, status)));
    },
    runTimer(timerId) {
      const callback = timers.get(timerId);
      if (!callback) throw new Error(`Unknown timer ${timerId}.`);
      timers.delete(timerId);
      callback();
    },
  };
}

async function flushPickerPromises(): Promise<void> {
  for (let index = 0; index < 10; index++) await Promise.resolve();
}

function requiredPickerElement(harness: PickerHarness, id: string): PickerStubElement {
  const element = harness.elements.get(id);
  if (!element) throw new Error(`Expected picker element ${id}.`);
  return element;
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
      `${treeModelSource}\n${panelSource}\n({ clearTargetPresentation, findNode, inspectedNodeId, inspectorContent: elements.inspector, queueHighlight, refreshSnapshot, renderInspector, renderTree, selectNode, state, treeContent: elements.tree })`,
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
    panel.state.target = {
      capabilities: ['components', 'component-properties', 'snapshot', 'highlight', 'console', 'performance'],
      id: 'owl:web-preview',
      sessionId: 'web-preview',
    };
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

  it('renders escaped read-only Valdi props only when the target advertises the capability', () => {
    panel.selectNode('component:["7","nested"]');

    expect(panel.inspectorContent.innerHTML).toContain('aria-label="Valdi props"');
    expect(panel.inspectorContent.innerHTML).toContain('read only');
    expect(panel.inspectorContent.innerHTML).toContain('&lt;img src=x onerror=alert(1)&gt;');
    expect(panel.inspectorContent.innerHTML).not.toContain('<img src=x');

    panel.state.target = { capabilities: [], id: 'native-target', sessionId: 'native-session' };
    panel.renderInspector();

    expect(panel.inspectorContent.innerHTML).not.toContain('aria-label="Valdi props"');
  });

  it('distinguishes omitted properties from a captured empty ViewModel', () => {
    const node = panel.findNode('component:["7","nested"]');
    if (!node?.component) throw new Error('Expected the nested component fixture.');
    delete node.component.properties;
    panel.selectNode(node.id);

    expect(panel.inspectorContent.innerHTML).not.toContain('aria-label="Valdi props"');

    node.component.properties = {};
    panel.renderInspector();

    expect(panel.inspectorContent.innerHTML).toContain('aria-label="Valdi props"');
    expect(panel.inspectorContent.innerHTML).toContain('No properties available.');
  });

  it('clears properties on target change and drops a stale snapshot response', async () => {
    let resolveSnapshot: ((response: { ok: boolean; json(): Promise<Record<string, unknown>> }) => void) | undefined;
    queuedFetchResponses.push(
      new Promise(resolve => {
        resolveSnapshot = resolve;
      }),
    );
    panel.selectNode('component:["7","nested"]');
    const staleRefresh = panel.refreshSnapshot();
    await Promise.resolve();

    panel.state.targetGeneration++;
    panel.clearTargetPresentation('Loading the replacement target…');
    panel.state.target = {
      capabilities: ['components', 'component-properties', 'snapshot', 'highlight', 'console', 'performance'],
      id: 'replacement-target',
      sessionId: 'replacement-session',
    };
    panel.renderInspector();
    expect(panel.state.snapshot).toBeNull();
    expect(panel.inspectorContent.innerHTML).not.toContain('onerror');

    if (resolveSnapshot === undefined) throw new Error('Expected a deferred hierarchy snapshot.');
    resolveSnapshot({ json: () => Promise.resolve({ tree: componentTree() }), ok: true });
    await staleRefresh;

    expect(panel.state.snapshot).toBeNull();
    expect(panel.inspectorContent.innerHTML).not.toContain('onerror');
  });

  it('preserves keyed component selection across updates and safely falls back when it disappears', async () => {
    panel.selectNode('component:["7","nested"]');
    fetchResponse = { tree: componentTree() };

    await panel.refreshSnapshot();

    expect(panel.state.selectedNodeId).toBe('component:["7","nested"]');
    expect(panel.state.expandedNodeIds.has('component:[null,"root"]')).toBeTrue();
    expect(panel.state.expandedNodeIds.has('7')).toBeTrue();

    const elementRoot = componentTree().children[0];
    const nestedElement = elementRoot?.children[0]?.children[0];
    if (!elementRoot || !nestedElement) throw new Error('Expected the component fixture to contain a nested element.');
    elementRoot.children = [nestedElement];
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
    expect(finalRequest['nodeId']).toBeUndefined();
    expect(panel.state.highlightMayBeActive).toBeFalse();
  });
});

describe('integrated DevTools capability-aware target picker', () => {
  it('provides a labeled picker, live status, and accessible tab relationships', () => {
    const html = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'devtools-panel.html'), 'utf8');
    const css = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'devtools-panel.css'), 'utf8');

    expect(html).toContain(
      '<label id="targetSelectLabel" class="target-select-label" for="targetSelect">Target</label>',
    );
    expect(html).toContain('id="targetSelect"');
    expect(html).toMatch(/id="targetPickerStatus"[^>]*role="status"[^>]*aria-live="polite"/);
    expect(html).toMatch(/id="targetStatusDot"[^>]*aria-hidden="true"/);
    expect(html).toMatch(/id="performanceSection"[\S\s]*?role="tabpanel"[\S\s]*?hidden/);
    expect(css).toMatch(/@media \(max-width: 480px\)[\S\s]*?\.target-picker-status\s*{[^}]*clip-path: inset\(50%\)/);
    expect(css).not.toMatch(/\.target-picker-status\s*{[^}]*display:\s*none/);
  });

  it('rejects mixed, partial, empty, and duplicated launch identities without making a request', async () => {
    const invalidSearches = [
      '?targetId=direct&inspectedUrl=http%3A%2F%2F127.0.0.1%3A1234%2F&targetNonce=nonce',
      '?inspectedUrl=http%3A%2F%2F127.0.0.1%3A1234%2F',
      '?targetNonce=nonce',
      '?targetId=',
      '?targetId=one&targetId=two',
      '?inspectedUrl=one&inspectedUrl=two&targetNonce=nonce',
    ];

    for (const search of invalidSearches) {
      const harness = createPickerHarness(search);
      await harness.panel.connectToInspectedApplication();
      expect(harness.fetchRequests).withContext(search).toEqual([]);
      expect(harness.panel.state.error)
        .withContext(search)
        .toMatch(/identity|requires|targetId/);
    }
  });

  it('preserves inspected-page mode and never discovers the target registry', async () => {
    const harness = createPickerHarness(
      '?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Findex.html%3FvaldiDevTools%3D1&targetNonce=panel-target-nonce-123456',
    );
    harness.queueResponse('/api/devtools/target', {
      target: {
        applicationUrl: 'http://127.0.0.1:54321/index.html?valdiDevTools=1',
        debuggingPort: 9222,
        id: 'owl:web-preview',
        name: 'index.html',
        sessionId: 'web-preview',
      },
    });
    harness.queueResponse('/api/devtools/snapshot', { tree: componentTree() });

    await harness.panel.connectToInspectedApplication();
    await flushPickerPromises();

    const paths = harness.fetchRequests.map(request => new URL(request.url).pathname);
    expect(paths).toContain('/api/devtools/target');
    expect(paths).toContain('/api/devtools/snapshot');
    expect(paths).not.toContain('/api/devtools/targets');
    const snapshotRequest = harness.fetchRequests.find(request => request.url.includes('/snapshot'));
    if (!snapshotRequest) throw new Error('Expected an inspected-page snapshot request.');
    const snapshotUrl = new URL(snapshotRequest.url);
    expect(Object.fromEntries(snapshotUrl.searchParams)).toEqual({
      inspectedUrl: 'http://127.0.0.1:54321/index.html?valdiDevTools=1',
      sessionId: 'web-preview',
      targetNonce: 'panel-target-nonce-123456',
    });
    expect(requiredPickerElement(harness, 'targetSelect').hidden).toBeTrue();
  });

  it('clears target-owned Console state when an inspected-page session is replaced', async () => {
    const harness = createPickerHarness(
      '?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Findex.html%3FvaldiDevTools%3D1&targetNonce=panel-target-nonce-123456',
    );
    harness.queueResponse('/api/devtools/target', {
      target: {
        applicationUrl: 'http://127.0.0.1:54321/index.html?valdiDevTools=1',
        debuggingPort: 9222,
        id: 'owl:web-preview',
        name: 'index.html',
        sessionId: 'web-preview',
      },
    });
    await harness.panel.connectToInspectedApplication();
    await flushPickerPromises();
    harness.panel.state.consoleEntries.push({ kind: 'log', value: 'old session output' });
    harness.panel.state.consoleEntryKeys.add('old-session-entry');
    harness.panel.state.consoleHistory.push('oldSessionExpression()');
    requiredPickerElement(harness, 'consoleInput').value = 'old draft';
    harness.queueResponse('/api/devtools/target', {
      target: {
        applicationUrl: 'http://127.0.0.1:54321/replacement.html?valdiDevTools=1',
        debuggingPort: 9222,
        id: 'owl:replacement',
        name: 'replacement.html',
        sessionId: 'replacement',
      },
    });

    await harness.panel.connectToInspectedApplication();
    await flushPickerPromises();

    expect(harness.panel.state.target?.id).toBe('owl:replacement');
    expect(harness.panel.state.consoleEntries.some(entry => entry.value === 'old session output')).toBeFalse();
    expect(harness.panel.state.consoleEntryKeys.has('old-session-entry')).toBeFalse();
    expect(harness.panel.state.consoleHistory).toEqual([]);
    expect(requiredPickerElement(harness, 'consoleInput').value).toBe('');
  });

  it('rerenders cleared Performance state after an inspected-page replacement with Live off', async () => {
    const harness = createPickerHarness(
      '?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Findex.html%3FvaldiDevTools%3D1&targetNonce=panel-target-nonce-123456',
    );
    harness.queueResponse('/api/devtools/target', {
      target: {
        applicationUrl: 'http://127.0.0.1:54321/index.html?valdiDevTools=1',
        debuggingPort: 9222,
        id: 'owl:web-preview',
        name: 'index.html',
        sessionId: 'web-preview',
      },
    });
    await harness.panel.connectToInspectedApplication();
    await flushPickerPromises();
    const liveToggle = requiredPickerElement(harness, 'autoRefreshToggle');
    liveToggle.checked = false;
    liveToggle.dispatch('change');
    harness.panel.state.performance.data = {
      mainThread: { layoutDurationMs: 2, scriptDurationMs: 4, taskDurationMs: 12 },
      memory: { usedBytes: 2048 },
      resourceCount: 4,
      uptimeMs: 100,
    };
    harness.panel.state.performance.pending = true;
    harness.panel.setActiveSection('performance');
    expect(requiredPickerElement(harness, 'performanceContent').innerHTML).toContain('JS heap');
    harness.queueResponse('/api/devtools/target', {
      target: {
        applicationUrl: 'http://127.0.0.1:54321/replacement.html?valdiDevTools=1',
        debuggingPort: 9222,
        id: 'owl:replacement',
        name: 'replacement.html',
        sessionId: 'replacement',
      },
    });

    await harness.panel.connectToInspectedApplication();
    await flushPickerPromises();

    expect(harness.panel.state.performance.data).toBeNull();
    expect(harness.panel.state.performance.pending).toBeFalse();
    expect(requiredPickerElement(harness, 'performanceContent').innerHTML).not.toContain('JS heap');
  });

  it('renders bounded unavailable targets truthfully and never selects the first registry entry', async () => {
    const harness = createPickerHarness('?targetId=missing-target');
    const longUnsafeName = `<img src=x onerror=alert(1)>${'x'.repeat(300)}`;
    harness.queueResponse('/api/devtools/targets', {
      targets: [
        pickerTarget('available-target', { name: longUnsafeName }),
        pickerTarget('web-preview', {
          identityMode: 'inspected-page',
          name: 'Web preview',
          transport: 'chromium-cdp',
        }),
        pickerTarget('waiting-proxy', {
          attachable: true,
          capabilities: ['components', 'snapshot'],
          identityMode: 'target-id',
          name: 'Waiting proxy',
          state: 'waiting',
          transport: 'valdi-daemon',
        }),
      ],
    });

    await harness.panel.connectToInspectedApplication();

    expect(harness.panel.state.target).toBeNull();
    expect(harness.fetchRequests.filter(request => request.url.includes('/snapshot'))).toEqual([]);
    const options = requiredPickerElement(harness, 'targetSelect').children;
    const available = options.find(option => option.value === 'available-target');
    const web = options.find(option => option.value === 'web-preview');
    const waiting = options.find(option => option.value === 'waiting-proxy');
    const missing = options.find(option => option.value === 'missing-target');
    expect(available?.selected).toBeFalse();
    expect(available?.textContent.length).toBeLessThanOrEqual(180);
    expect(available?.textContent).toContain('<img src=x onerror=alert(1)>');
    expect(available?.innerHTML).toBe('');
    expect(web?.disabled).toBeTrue();
    expect(web?.textContent).toContain('Open from the inspected page');
    expect(waiting?.disabled).toBeTrue();
    expect(waiting?.textContent).toContain('Waiting for application');
    expect(missing?.disabled).toBeTrue();
    expect(requiredPickerElement(harness, 'targetPickerStatus').textContent).toContain('unavailable');
  });

  it('fails closed on oversized, duplicated, and malformed registries', () => {
    const harness = createPickerHarness('?targetId=requested');
    const oversized = Array.from({ length: 257 }, (_value, index) => pickerTarget(`target-${index}`));

    for (const [payload, expectedMessage] of [
      [{ targets: oversized }, 'exceeded 256 entries'],
      [{ targets: [pickerTarget('duplicate'), pickerTarget('duplicate')] }, 'duplicate target IDs'],
      [{ targets: [{ ...pickerTarget('bad-capability'), capabilities: ['snapshot', 'snapshot'] }] }, 'malformed entry'],
      [{ targets: [{ ...pickerTarget('bad-port'), port: 0 }] }, 'malformed entry'],
    ] as Array<[Record<string, unknown>, string]>) {
      let message = '';
      try {
        harness.panel.parseTargetRegistry(payload);
      } catch (error) {
        message = error instanceof Error ? error.message : String(error);
      }
      expect(message).toContain(expectedMessage);
    }
  });

  it('selects only the exact requested opaque ID and sends targetId alone for direct snapshots', async () => {
    const harness = createPickerHarness('?targetId=opaque%3Arequested%2Ftarget');
    harness.queueResponse('/api/devtools/targets', {
      targets: [pickerTarget('first-target'), pickerTarget('opaque:requested/target')],
    });
    harness.queueResponse('/api/devtools/snapshot', { tree: componentTree() });

    await harness.panel.connectToInspectedApplication();
    await flushPickerPromises();

    expect(harness.panel.state.target?.id).toBe('opaque:requested/target');
    const snapshotRequest = harness.fetchRequests.find(request => request.url.includes('/api/devtools/snapshot'));
    if (!snapshotRequest) throw new Error('Expected a direct snapshot request.');
    const snapshotUrl = new URL(snapshotRequest.url);
    expect(Array.from(snapshotUrl.searchParams.keys())).toEqual(['targetId']);
    expect(snapshotUrl.searchParams.get('targetId')).toBe('opaque:requested/target');
    expect(harness.fetchRequests.some(request => request.url.includes('/api/devtools/target?'))).toBeFalse();
  });

  it('clears all target-owned presentation and closes Console before an idle user switch', async () => {
    const harness = createPickerHarness('?targetId=target-a');
    const targetA = pickerTarget('target-a', { capabilities: ['components', 'snapshot', 'console'] });
    const targetB = pickerTarget('target-b', { capabilities: ['components', 'snapshot', 'console'] });
    harness.queueResponse('/api/devtools/targets', { targets: [targetA, targetB] });
    await harness.panel.connectToInspectedApplication();
    await flushPickerPromises();

    harness.panel.state.consoleEntries.push({ kind: 'log', value: 'old output' });
    harness.panel.state.consoleEntryKeys.add('old');
    harness.panel.state.consoleHistory.push('oldExpression()');
    harness.panel.state.expandedNodeIds.add('7');
    harness.panel.state.selectedNodeId = '7';
    harness.panel.state.performance.data = { uptimeMs: 1 };
    harness.panel.state.performance.lastTrace = { traceCount: 1 };
    harness.panel.state.performance.samples.push({ uptimeMs: 1 });
    const select = requiredPickerElement(harness, 'targetSelect');
    select.value = 'target-b';
    select.dispatch('change');

    expect(harness.closeTargetIds).toContain('target-a');
    expect(harness.panel.state.target?.id).toBe('target-b');
    expect(harness.panel.state.snapshot).toBeNull();
    expect(harness.panel.state.selectedNodeId).toBeNull();
    expect(harness.panel.state.expandedNodeIds.size).toBe(0);
    expect(harness.panel.state.consoleEntries).toEqual([]);
    expect(harness.panel.state.consoleEntryKeys.size).toBe(0);
    expect(harness.panel.state.consoleHistory).toEqual([]);
    expect(harness.panel.state.performance.data).toBeNull();
    expect(harness.panel.state.performance.lastTrace).toBeNull();
    expect(harness.panel.state.performance.samples).toEqual([]);
  });

  it('blocks pending and owned Performance switches and restores the selected option', async () => {
    const harness = createPickerHarness('?targetId=target-a');
    const targetA = pickerTarget('target-a', { capabilities: ['components', 'snapshot', 'performance'] });
    const targetB = pickerTarget('target-b', { capabilities: ['components', 'snapshot', 'performance'] });
    harness.queueResponse('/api/devtools/targets', { targets: [targetA, targetB] });
    await harness.panel.connectToInspectedApplication();
    await flushPickerPromises();
    const select = requiredPickerElement(harness, 'targetSelect');

    harness.panel.state.performance.pending = true;
    select.value = 'target-b';
    select.dispatch('change');
    expect(harness.panel.state.target?.id).toBe('target-a');
    expect(select.value).toBe('target-a');
    expect(requiredPickerElement(harness, 'targetPickerStatus').textContent).toContain('before switching');

    harness.panel.state.performance.pending = false;
    harness.panel.state.performance.traceActive = true;
    harness.panel.state.performance.ownerIdentity = { targetId: 'target-a' };
    select.value = 'target-b';
    select.dispatch('change');
    expect(harness.panel.state.target?.id).toBe('target-a');
    expect(select.value).toBe('target-a');
  });

  it('detaches on registry removal without fallback and keeps exact-owner Stop reachable', async () => {
    const harness = createPickerHarness('?targetId=target-a');
    const targetA = pickerTarget('target-a', { capabilities: ['components', 'snapshot', 'performance'] });
    const targetB = pickerTarget('target-b', { capabilities: ['components', 'snapshot', 'performance'] });
    harness.queueResponse('/api/devtools/targets', { targets: [targetA, targetB] });
    await harness.panel.connectToInspectedApplication();
    await flushPickerPromises();
    harness.panel.setActiveSection('performance');
    harness.panel.state.performance.traceActive = true;
    harness.panel.state.performance.ownerIdentity = { targetId: 'target-a' };
    harness.queueResponse('/api/devtools/targets', { targets: [targetB] });

    await harness.panel.refreshTargetRegistry();

    expect(harness.panel.state.target).toBeNull();
    expect(harness.panel.state.performance.ownerIdentity).toEqual({ targetId: 'target-a' });
    expect(requiredPickerElement(harness, 'performanceTab').disabled).toBeFalse();
    expect(harness.panel.state.activeSection).toBe('performance');
    harness.queueResponse('/api/devtools/performance/trace/stop', { traceCount: 0, traces: [] });
    await harness.panel.runPerformanceAction('trace-stop');
    const stopRequest = harness.fetchRequests.find(request => request.url.includes('/trace/stop'));
    if (!stopRequest) throw new Error('Expected an exact-owner Performance stop.');
    expect(new URL(stopRequest.url).searchParams.get('targetId')).toBe('target-a');
    expect(harness.panel.state.performance.ownerIdentity).toBeNull();
    expect(requiredPickerElement(harness, 'performanceTab').disabled).toBeTrue();
    expect(harness.panel.state.activeSection).toBe('elements');
  });

  it('renders exact-owner recovery without stale metrics when removal interrupts a capture offscreen', async () => {
    const harness = createPickerHarness('?targetId=target-a');
    const targetA = pickerTarget('target-a', { capabilities: ['components', 'snapshot', 'performance'] });
    const targetB = pickerTarget('target-b', { capabilities: ['components', 'snapshot', 'performance'] });
    harness.queueResponse('/api/devtools/targets', { targets: [targetA, targetB] });
    await harness.panel.connectToInspectedApplication();
    await flushPickerPromises();
    harness.panel.state.performance.data = {
      mainThread: { layoutDurationMs: 2, scriptDurationMs: 4, taskDurationMs: 12 },
      memory: { usedBytes: 2048 },
      resourceCount: 4,
      uptimeMs: 100,
    };
    const oldCapture = harness.queueDeferred('/api/devtools/performance/trace/capture');
    const oldCaptureAction = harness.panel.runPerformanceAction('trace-capture');
    await flushPickerPromises();
    expect(harness.panel.state.activeSection).toBe('elements');
    expect(harness.panel.state.performance.pending).toBeTrue();
    expect(harness.panel.state.performance.ownerIdentity).toEqual({ targetId: 'target-a' });
    expect(requiredPickerElement(harness, 'performanceContent').innerHTML).toContain('JS heap');
    harness.queueResponse('/api/devtools/targets', { targets: [targetB] });

    await harness.panel.refreshTargetRegistry();
    harness.panel.setActiveSection('performance');

    const recoveryContent = requiredPickerElement(harness, 'performanceContent').innerHTML;
    expect(harness.panel.state.target).toBeNull();
    expect(harness.panel.state.performance.data).toBeNull();
    expect(harness.panel.state.performance.ownerIdentity).toEqual({ targetId: 'target-a' });
    expect(recoveryContent).toContain('Stop and retrieve');
    expect(recoveryContent).toContain('data-performance-action="trace-stop"');
    expect(recoveryContent).not.toContain('data-performance-action="trace-stop" disabled');
    expect(recoveryContent).not.toContain('JS heap');

    oldCapture.resolve({ traceCount: 1, traces: [] });
    await oldCaptureAction;
    expect(harness.panel.state.performance.ownerIdentity).toEqual({ targetId: 'target-a' });
    expect(harness.panel.state.performance.pending).toBeFalse();
  });

  it('does not let an old owner Stop result or finally clear a newer recovery Stop', async () => {
    const harness = createPickerHarness('?targetId=target-a');
    const targetA = pickerTarget('target-a', { capabilities: ['components', 'snapshot', 'performance'] });
    const targetB = pickerTarget('target-b', { capabilities: ['components', 'snapshot', 'performance'] });
    harness.queueResponse('/api/devtools/targets', { targets: [targetA, targetB] });
    await harness.panel.connectToInspectedApplication();
    await flushPickerPromises();
    harness.panel.state.performance.traceActive = true;
    harness.panel.state.performance.ownerIdentity = { targetId: 'target-a' };
    const oldStop = harness.queueDeferred('/api/devtools/performance/trace/stop');
    const oldStopAction = harness.panel.runPerformanceAction('trace-stop');
    await flushPickerPromises();
    harness.queueResponse('/api/devtools/targets', { targets: [targetB] });
    await harness.panel.refreshTargetRegistry();
    const recoveryStop = harness.queueDeferred('/api/devtools/performance/trace/stop');
    const recoveryStopAction = harness.panel.runPerformanceAction('trace-stop');
    await flushPickerPromises();
    harness.panel.state.performance.lastTrace = { marker: 'newer recovery state' };

    oldStop.resolve({ marker: 'old stop result', traceCount: 1, traces: [] });
    await oldStopAction;

    expect(harness.panel.state.performance.ownerIdentity).toEqual({ targetId: 'target-a' });
    expect(harness.panel.state.performance.traceActive).toBeTrue();
    expect(harness.panel.state.performance.pending).toBeTrue();
    expect(harness.panel.state.performance.lastTrace).toEqual({ marker: 'newer recovery state' });
    expect(harness.panel.state.performance.error).toBeNull();

    recoveryStop.resolve({ traceCount: 2, traces: [] });
    await recoveryStopAction;
    expect(harness.panel.state.performance.ownerIdentity).toBeNull();
    expect(harness.panel.state.performance.pending).toBeFalse();
  });

  it('does not let an old capture error or finally overwrite a new target capture', async () => {
    const harness = createPickerHarness('?targetId=target-a');
    const targetA = pickerTarget('target-a', { capabilities: ['components', 'snapshot', 'performance'] });
    const targetB = pickerTarget('target-b', { capabilities: ['components', 'snapshot', 'performance'] });
    harness.queueResponse('/api/devtools/targets', { targets: [targetA, targetB] });
    await harness.panel.connectToInspectedApplication();
    await flushPickerPromises();
    const oldCapture = harness.queueDeferred('/api/devtools/performance/trace/capture');
    const oldCaptureAction = harness.panel.runPerformanceAction('trace-capture');
    await flushPickerPromises();
    harness.queueResponse('/api/devtools/targets', { targets: [targetB] });
    await harness.panel.refreshTargetRegistry();
    harness.queueResponse('/api/devtools/performance/trace/stop', { traceCount: 0, traces: [] });
    await harness.panel.runPerformanceAction('trace-stop');
    const select = requiredPickerElement(harness, 'targetSelect');
    select.value = 'target-b';
    select.dispatch('change');
    const newCapture = harness.queueDeferred('/api/devtools/performance/trace/capture');
    const newCaptureAction = harness.panel.runPerformanceAction('trace-capture');
    await flushPickerPromises();
    harness.panel.state.performance.data = { marker: 'new target metrics' };
    harness.panel.state.performance.lastTrace = { marker: 'new target trace' };
    const warn = spyOn(console, 'warn');

    oldCapture.reject(new Error('old capture failure'));
    await oldCaptureAction;

    expect(harness.panel.state.performance.ownerIdentity).toEqual({ targetId: 'target-b' });
    expect(harness.panel.state.performance.traceActive).toBeTrue();
    expect(harness.panel.state.performance.pending).toBeTrue();
    expect(harness.panel.state.performance.error).toBeNull();
    expect(harness.panel.state.performance.data).toEqual({ marker: 'new target metrics' });
    expect(harness.panel.state.performance.lastTrace).toEqual({ marker: 'new target trace' });
    expect(warn).toHaveBeenCalledWith('Ignoring a stale web preview performance action error.', jasmine.anything());

    newCapture.resolve({ traceCount: 3, traces: [] });
    await newCaptureAction;
    expect(harness.panel.state.performance.ownerIdentity).toBeNull();
    expect(harness.panel.state.performance.pending).toBeFalse();
  });

  it('keeps a replacement snapshot pending when an older target resolves or rejects', async () => {
    const harness = createPickerHarness('?targetId=target-a');
    const targetA = pickerTarget('target-a');
    const targetB = pickerTarget('target-b');
    harness.queueResponse('/api/devtools/targets', { targets: [targetA, targetB] });
    await harness.panel.connectToInspectedApplication();
    await flushPickerPromises();

    const oldSnapshot = harness.queueDeferred('/api/devtools/snapshot');
    void harness.panel.refreshSnapshot();
    const replacementSnapshot = harness.queueDeferred('/api/devtools/snapshot');
    const select = requiredPickerElement(harness, 'targetSelect');
    select.value = 'target-b';
    select.dispatch('change');
    oldSnapshot.resolve({ tree: componentTree() });
    await flushPickerPromises();
    expect(harness.panel.state.target?.id).toBe('target-b');
    expect(harness.panel.state.snapshot).toBeNull();
    expect(harness.panel.state.refreshPending).toBeTrue();
    replacementSnapshot.resolve({ tree: componentTree() });
    await flushPickerPromises();
    expect(harness.panel.state.refreshPending).toBeFalse();

    const staleError = harness.queueDeferred('/api/devtools/snapshot');
    void harness.panel.refreshSnapshot();
    const finalSnapshot = harness.queueDeferred('/api/devtools/snapshot');
    select.value = 'target-a';
    select.dispatch('change');
    staleError.reject(new Error('stale target failure'));
    await flushPickerPromises();
    expect(harness.panel.state.error).toBeNull();
    expect(harness.panel.state.refreshPending).toBeTrue();
    finalSnapshot.resolve({ tree: componentTree() });
    await flushPickerPromises();
  });

  it('orders an exact old-target highlight clear before new-target highlight work', async () => {
    const harness = createPickerHarness('?targetId=target-a');
    const capabilities = ['components', 'snapshot', 'highlight'];
    const targetA = pickerTarget('target-a', { capabilities });
    const targetB = pickerTarget('target-b', { capabilities });
    harness.queueResponse('/api/devtools/targets', { targets: [targetA, targetB] });
    await harness.panel.connectToInspectedApplication();
    await flushPickerPromises();
    const oldHighlight = harness.queueDeferred('/api/devtools/highlight');

    harness.panel.queueHighlight('7');
    if (harness.panel.state.highlightTimer === null) throw new Error('Expected a highlight timer.');
    harness.runTimer(harness.panel.state.highlightTimer);
    await flushPickerPromises();
    const select = requiredPickerElement(harness, 'targetSelect');
    select.value = 'target-b';
    select.dispatch('change');
    expect(harness.fetchRequests.filter(request => request.url.includes('/highlight')).length).toBe(1);

    oldHighlight.resolve({ highlighted: true });
    await harness.panel.state.highlightRequestTail;
    const highlightRequests = harness.fetchRequests.filter(request => request.url.includes('/highlight'));
    expect(highlightRequests.length).toBe(2);
    const finalHighlightRequest = highlightRequests[1];
    if (finalHighlightRequest?.body === undefined) throw new Error('Expected a serialized old-target highlight clear.');
    expect(JSON.parse(finalHighlightRequest.body)).toEqual({ targetId: 'target-a' });
  });

  it('drops old Console streams and evaluation success or failure after a switch', async () => {
    const harness = createPickerHarness('?targetId=target-a');
    const capabilities = ['components', 'snapshot', 'console'];
    const targetA = pickerTarget('target-a', { capabilities });
    const targetB = pickerTarget('target-b', { capabilities });
    harness.queueResponse('/api/devtools/targets', { targets: [targetA, targetB] });
    await harness.panel.connectToInspectedApplication();
    await flushPickerPromises();
    const oldStream = harness.eventSources[0];
    if (!oldStream) throw new Error('Expected a Console stream.');
    const oldEvaluation = harness.queueDeferred('/api/devtools/evaluate');
    void harness.panel.evaluateConsoleExpression('oldValue()');
    const select = requiredPickerElement(harness, 'targetSelect');
    select.value = 'target-b';
    select.dispatch('change');

    oldStream.emit('console', { level: 'log', message: 'stale log', targetId: 'target-a' });
    oldStream.emit('stream-warning', { message: 'stale warning', targetId: 'target-a' });
    oldStream.emit('stream-error', { error: 'stale error', targetId: 'target-a' });
    oldEvaluation.resolve({ type: 'string', value: 'stale result' });
    await flushPickerPromises();
    expect(harness.panel.state.consoleEntries).toEqual([]);

    const staleFailure = harness.queueDeferred('/api/devtools/evaluate');
    void harness.panel.evaluateConsoleExpression('secondOldValue()');
    select.value = 'target-a';
    select.dispatch('change');
    staleFailure.reject(new Error('stale evaluation failure'));
    await flushPickerPromises();
    expect(harness.panel.state.consoleEntries).toEqual([]);
  });

  it('gates unsupported tabs and generates no Console or Performance traffic for component-only targets', async () => {
    const harness = createPickerHarness('?targetId=components-only');
    harness.queueResponse('/api/devtools/targets', { targets: [pickerTarget('components-only')] });
    await harness.panel.connectToInspectedApplication();
    await flushPickerPromises();

    expect(requiredPickerElement(harness, 'consoleTab').disabled).toBeTrue();
    expect(requiredPickerElement(harness, 'consoleTab').getAttribute('aria-disabled')).toBe('true');
    expect(requiredPickerElement(harness, 'performanceTab').disabled).toBeTrue();
    expect(harness.eventSources).toEqual([]);
    harness.panel.setActiveSection('console');
    expect(harness.panel.state.activeSection).toBe('elements');
    harness.panel.setActiveSection('performance');
    expect(harness.panel.state.activeSection).toBe('elements');
    await harness.panel.evaluateConsoleExpression('shouldNotRun()');
    expect(
      harness.fetchRequests.some(request =>
        ['/api/devtools/evaluate', '/api/devtools/console/stream', '/api/devtools/performance'].some(pathname =>
          request.url.includes(pathname),
        ),
      ),
    ).toBeFalse();
  });

  it('uses roving tab focus and skips capability-disabled tools during keyboard navigation', async () => {
    const harness = createPickerHarness('?targetId=console-target');
    harness.queueResponse('/api/devtools/targets', {
      targets: [pickerTarget('console-target', { capabilities: ['components', 'snapshot', 'console'] })],
    });
    await harness.panel.connectToInspectedApplication();
    await flushPickerPromises();
    harness.panel.setActiveSection('elements');
    const elementsTab = requiredPickerElement(harness, 'elementsTab');
    const performanceTab = requiredPickerElement(harness, 'performanceTab');
    const consoleTab = requiredPickerElement(harness, 'consoleTab');

    elementsTab.dispatch('keydown', { key: 'ArrowRight' });

    expect(performanceTab.disabled).toBeTrue();
    expect(harness.panel.state.activeSection).toBe('console');
    expect(elementsTab.tabIndex).toBe(-1);
    expect(consoleTab.tabIndex).toBe(0);
    expect(consoleTab.getAttribute('aria-selected')).toBe('true');
    expect(requiredPickerElement(harness, 'elementsSection').hidden).toBeTrue();
    expect(requiredPickerElement(harness, 'consoleSection').hidden).toBeFalse();

    consoleTab.dispatch('keydown', { key: 'ArrowRight' });
    expect(harness.panel.state.activeSection).toBe('elements');
  });

  it('invalidates streams and active tabs when capabilities change for the same target ID', async () => {
    const harness = createPickerHarness('?targetId=target-a');
    const consoleTarget = pickerTarget('target-a', {
      capabilities: ['components', 'snapshot', 'console', 'performance'],
    });
    harness.queueResponse('/api/devtools/targets', { targets: [consoleTarget] });
    await harness.panel.connectToInspectedApplication();
    await flushPickerPromises();
    const oldGeneration = harness.panel.state.targetGeneration;
    const oldStream = harness.eventSources[0];
    if (!oldStream) throw new Error('Expected a Console stream.');
    harness.panel.state.consoleEntries.push({ kind: 'log', value: 'old capability output' });
    harness.panel.state.consoleEntryKeys.add('old-capability-entry');
    harness.panel.state.consoleHistory.push('oldCapabilityExpression()');
    harness.panel.state.expandedNodeIds.add('7');
    harness.panel.state.selectedNodeId = '7';
    harness.panel.state.performance.data = { uptimeMs: 1 };
    harness.panel.state.performance.lastTrace = { traceCount: 1 };
    harness.panel.state.performance.samples.push({ uptimeMs: 1 });
    harness.panel.setActiveSection('console');
    const replacementSnapshot = harness.queueDeferred('/api/devtools/snapshot');
    harness.queueResponse('/api/devtools/targets', {
      targets: [pickerTarget('target-a', { capabilities: ['components', 'snapshot', 'console,performance'] })],
    });

    await harness.panel.refreshTargetRegistry();

    expect(harness.panel.state.target?.id).toBe('target-a');
    expect(harness.panel.state.targetGeneration).toBeGreaterThan(oldGeneration);
    expect(oldStream.closed).toBeTrue();
    expect(harness.panel.state.selectedNodeId).toBeNull();
    expect(harness.panel.state.expandedNodeIds.size).toBe(0);
    expect(harness.panel.state.consoleEntries).toEqual([]);
    expect(harness.panel.state.consoleEntryKeys.size).toBe(0);
    expect(harness.panel.state.consoleHistory).toEqual([]);
    expect(harness.panel.state.performance.data).toBeNull();
    expect(harness.panel.state.performance.lastTrace).toBeNull();
    expect(harness.panel.state.performance.samples).toEqual([]);
    expect(requiredPickerElement(harness, 'consoleTab').disabled).toBeTrue();
    expect(harness.panel.state.activeSection).toBe('elements');
    oldStream.emit('console', { level: 'log', message: 'stale same-id log', targetId: 'target-a' });
    expect(harness.panel.state.consoleEntries).toEqual([]);
    replacementSnapshot.resolve({ tree: componentTree() });
    await flushPickerPromises();
  });

  it('keeps the exact current target when a later registry payload is malformed', async () => {
    const harness = createPickerHarness('?targetId=target-a');
    harness.queueResponse('/api/devtools/targets', { targets: [pickerTarget('target-a'), pickerTarget('target-b')] });
    await harness.panel.connectToInspectedApplication();
    await flushPickerPromises();
    const targetGeneration = harness.panel.state.targetGeneration;
    harness.queueResponse('/api/devtools/targets', {
      targets: [pickerTarget('duplicate'), pickerTarget('duplicate')],
    });

    await harness.panel.refreshTargetRegistry();

    expect(harness.panel.state.target?.id).toBe('target-a');
    expect(harness.panel.state.targetGeneration).toBe(targetGeneration);
    expect(requiredPickerElement(harness, 'targetSelect').value).toBe('target-a');
    expect(requiredPickerElement(harness, 'targetPickerStatus').textContent).toContain('duplicate target IDs');
  });

  it('lets only the newest registry generation publish and still does not auto-select', async () => {
    const harness = createPickerHarness('?targetId=requested-target');
    const first = harness.queueDeferred('/api/devtools/targets');
    void harness.panel.refreshTargetRegistry();
    harness.panel.state.registryPending = false;
    const second = harness.queueDeferred('/api/devtools/targets');
    void harness.panel.refreshTargetRegistry();

    second.resolve({ targets: [pickerTarget('newest-target')] });
    await flushPickerPromises();
    first.resolve({ targets: [pickerTarget('requested-target')] });
    await flushPickerPromises();

    expect(harness.panel.state.registryTargets.map(target => target.id)).toEqual(['newest-target']);
    expect(harness.panel.state.target).toBeNull();
    expect(harness.panel.state.unavailableTargetId).toBe('requested-target');
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
            value: '',
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
      `${source}\n({ clearButton: elements.clearConsoleButton, consoleInput: elements.consoleInput, consoleMessages: elements.consoleMessages, dispatchWindowEvent: type => windowListeners.get(type)?.(), evaluateConsoleExpression, liveToggle: elements.autoRefreshToggle, startConsoleStream, state, stopConsoleStream })`,
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

  it('exposes an accessible local clear action that is distinct from Performance controls', () => {
    const markup = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'devtools-panel.html'), 'utf8');
    const clearButton = markup.match(/<button(?=[^>]*id="clearConsoleButton")[\S\s]*?<\/button>/)?.[0];

    expect(clearButton).toBeDefined();
    expect(clearButton).toContain('type="button"');
    expect(clearButton).toContain('aria-label="Clear console"');
    expect(clearButton).toContain('aria-controls="consoleMessages"');
    expect(clearButton).not.toContain('data-performance-action');
  });

  it('clears only local console output while preserving the stream, target, prompt, history, and Performance', () => {
    panel.startConsoleStream();
    const stream = eventSources[0];
    if (!stream) throw new Error('Expected the Chromium console stream to connect.');
    const target = panel.state.target;
    const streamTargetKey = panel.state.consoleStreamTargetKey;
    const performanceOwner = {
      inspectedUrl: 'http://127.0.0.1:54321/index.html?valdiDevTools=1',
      sessionId: 'web-preview',
      targetNonce: 'panel-target-nonce-123456',
    };
    const lastTrace = { traceCount: 4 };
    panel.state.consoleHistory.push('first()', 'second()');
    panel.state.consoleHistoryIndex = 1;
    panel.consoleInput.value = 'draft expression';
    panel.state.performance.ownerIdentity = performanceOwner;
    panel.state.performance.lastTrace = lastTrace;
    panel.state.performance.traceActive = true;
    const entry = {
      level: 'info',
      message: 'Renderer output',
      sessionId: 'web-preview',
      source: 'console',
      targetId: 'owl:web-preview',
      timestamp: 42,
    };
    stream.emit('console', entry);

    panel.clearButton.dispatch('click');

    expect(panel.state.consoleEntries).toEqual([]);
    expect(panel.consoleMessages.innerHTML).toBe('');
    expect(panel.state.consoleStream).toBe(stream);
    expect(stream.closed).toBeFalse();
    expect(panel.state.target).toBe(target);
    expect(panel.state.consoleStreamTargetKey).toBe(streamTargetKey);
    expect(panel.consoleInput.value).toBe('draft expression');
    expect(panel.state.consoleHistory).toEqual(['first()', 'second()']);
    expect(panel.state.consoleHistoryIndex).toBe(1);
    expect(panel.state.performance.ownerIdentity).toBe(performanceOwner);
    expect(panel.state.performance.lastTrace).toBe(lastTrace);
    expect(panel.state.performance.traceActive).toBeTrue();

    stream.emit('console', entry);

    expect(panel.state.consoleEntries).toEqual([jasmine.objectContaining({ kind: 'info', value: 'Renderer output' })]);
    expect(panel.consoleMessages.innerHTML).toContain('Renderer output');
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

interface DevToolsPerformancePanel {
  document: { activeElement: { id?: string } | null };
  performanceContent: {
    innerHTML: string;
    scrollTop: number;
    querySelector(selector: string): { scrollLeft: number; scrollTop: number } | null;
  };
  state: {
    activeSection: string;
    performance: {
      data: Record<string, unknown> | null;
      durationSeconds: number;
      error: string | null;
      lastTrace: Record<string, unknown> | null;
      ownerIdentity: Record<string, string> | null;
      pending: boolean;
      samples: Array<Record<string, number>>;
      snapshotPending: boolean;
      traceActive: boolean;
      traceScope: string;
      traceSearch: string;
    };
    target: { id: string; sessionId: string } | null;
  };
  buildPerformanceTraceExport(result: Record<string, unknown>): { traceEvents: Array<Record<string, unknown>> };
  dispatchWindowEvent(type: string): void;
  preparePerformanceForTargetChange(): void;
  refreshPerformance(options?: Record<string, unknown>): Promise<void>;
  renderPerformance(data?: Record<string, unknown>): void;
  runPerformanceAction(action: string): Promise<void>;
}

describe('integrated DevTools performance panel', () => {
  let panel: DevToolsPerformancePanel;
  let requests: Array<{ body?: string; keepalive?: boolean; method: string; url: string }>;
  let nextFetchResponse: Promise<{ ok: boolean; status: number; json(): Promise<Record<string, unknown>> }> | undefined;
  let traceRecording: boolean;
  let completionErrorPending: boolean;
  let stopFailure: string | null;
  let snapshotResponse: Record<string, unknown>;

  const snapshot = {
    mainThread: { layoutDurationMs: 2, scriptDurationMs: 4, taskDurationMs: 12 },
    memory: { totalBytes: 4096, usedBytes: 2048 },
    navigation: { domContentLoadedMs: 30, loadMs: 50 },
    paints: [{ name: 'first-contentful-paint', startTime: 25 }],
    rendererTracingEnabled: true,
    resourceCount: 4,
    transferSize: 1024,
    uptimeMs: 100,
  };

  function traceResult(): Record<string, unknown> {
    return {
      browserMetrics: { LayoutCount: 2, TaskDurationMs: 12 },
      browserSummary: { browserEventCount: 1, longTaskCount: 1, rendererEventCount: 1 },
      droppedTraceEventCount: 0,
      elapsedMs: 100,
      perfettoMetadata: { captureScope: 'process-wide' },
      recording: false,
      traceCount: 2,
      traces: [
        { endMicros: 1300, startMicros: 1000, threadId: 1, trace: 'Valdi.Renderer.onRender.Example' },
        { endMicros: 76_500, startMicros: 1500, threadId: 1, trace: 'Browser.MainThread.Task' },
      ],
    };
  }

  beforeEach(() => {
    requests = [];
    nextFetchResponse = undefined;
    traceRecording = false;
    completionErrorPending = false;
    stopFailure = null;
    snapshotResponse = snapshot;
    const treeModelSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'debugger-tree-model.js'), 'utf8');
    const rawPanelSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'devtools-panel.js'), 'utf8');
    const panelSource = rawPanelSource.replace('void connectToInspectedApplication();', 'void 0;');
    const elements = new Map<string, Record<string, unknown>>();
    const document = {
      activeElement: null as { id?: string } | null,
      addEventListener() {},
      createElement() {
        return { click() {} };
      },
      documentElement: { dataset: {} },
      getElementById(id: string): Record<string, unknown> {
        let element = elements.get(id);
        if (element === undefined) {
          element = {
            checked: true,
            className: '',
            classList: { contains: () => false, toggle() {} },
            dataset: {},
            innerHTML: '',
            scrollHeight: 0,
            scrollTop: 0,
            style: {},
            textContent: '',
            value: '',
            addEventListener() {},
            contains: () => false,
            focus() {},
            removeAttribute() {},
            querySelector: () => null,
            setAttribute() {},
            setSelectionRange() {},
          };
          elements.set(id, element);
        }
        return element;
      },
      querySelectorAll(): unknown[] {
        return [];
      },
    };
    const windowListeners = new Map<string, Array<() => void>>();
    const window = {
      addEventListener(type: string, listener: () => void) {
        const listeners = windowListeners.get(type) ?? [];
        listeners.push(listener);
        windowListeners.set(type, listeners);
      },
      clearInterval() {},
      clearTimeout() {},
      confirm: () => true,
      location: {
        origin: 'http://127.0.0.1:18768',
        search:
          '?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321%2Findex.html%3FvaldiDevTools%3D1&targetNonce=panel-target-nonce-123456',
      },
      parent: {},
      dispatch(type: string) {
        for (const listener of windowListeners.get(type) ?? []) listener();
      },
      setInterval: () => 1,
      setTimeout: () => 1,
    };

    panel = new Script(
      `${treeModelSource}\n${panelSource}\n({ buildPerformanceTraceExport, dispatchWindowEvent: type => window.dispatch(type), document, performanceContent: elements.performanceContent, preparePerformanceForTargetChange, refreshPerformance, renderPerformance, runPerformanceAction, state })`,
    ).runInNewContext({
      Blob,
      Date,
      EventSource: class {
        addEventListener() {}
        close() {}
      },
      URL,
      URLSearchParams,
      console,
      document,
      elements,
      fetch: (url: URL, options: { body?: string; keepalive?: boolean; method: string }) => {
        const requestUrl = new URL(url.toString());
        requests.push({
          ...(options.body === undefined ? {} : { body: options.body }),
          ...(options.keepalive === undefined ? {} : { keepalive: options.keepalive }),
          method: options.method,
          url: requestUrl.toString(),
        });
        if (nextFetchResponse) {
          const response = nextFetchResponse;
          nextFetchResponse = undefined;
          return response;
        }
        let payload: Record<string, unknown>;
        if (requestUrl.pathname.endsWith('/snapshot')) {
          payload = snapshotResponse;
        } else if (requestUrl.pathname.endsWith('/status')) {
          payload = {
            completedRecordingAvailable: false,
            ...(completionErrorPending ? { completionError: 'Synthetic retained completion error.' } : {}),
            recording: traceRecording,
            rendererTracingEnabled: true,
            tracingSupported: true,
          };
        } else if (requestUrl.pathname.endsWith('/start')) {
          traceRecording = true;
          payload = { recording: true, rendererTracingEnabled: true, tracingSupported: true };
        } else if (requestUrl.pathname.endsWith('/enable')) {
          payload = { rendererTracingEnabled: true };
        } else if (requestUrl.pathname.endsWith('/stop') && completionErrorPending) {
          completionErrorPending = false;
          return Promise.resolve({
            json: () => Promise.resolve({ error: 'Synthetic retained completion error.' }),
            ok: false,
            status: 500,
          });
        } else if (requestUrl.pathname.endsWith('/stop') && stopFailure) {
          const error = stopFailure;
          stopFailure = null;
          return Promise.resolve({ json: () => Promise.resolve({ error }), ok: false, status: 500 });
        } else {
          traceRecording = false;
          payload = traceResult();
        }
        return Promise.resolve({ json: () => Promise.resolve(payload), ok: true, status: 200 });
      },
      navigator: { clipboard: { writeText: () => Promise.resolve() } },
      window,
    }) as DevToolsPerformancePanel;
    panel.state.activeSection = 'performance';
    panel.state.target = { id: 'owl:web-preview', sessionId: 'web-preview' };
  });

  it('binds snapshots and status polling to the complete inspected-target tuple', async () => {
    await panel.refreshPerformance();

    expect(requests.length).toBe(2);
    for (const request of requests) {
      const url = new URL(request.url);
      expect(url.searchParams.get('sessionId')).toBe('web-preview');
      expect(url.searchParams.get('inspectedUrl')).toBe('http://127.0.0.1:54321/index.html?valdiDevTools=1');
      expect(url.searchParams.get('targetNonce')).toBe('panel-target-nonce-123456');
    }
    expect(panel.state.performance.samples.length).toBe(1);
    expect(panel.performanceContent.innerHTML).toContain('data-performance-scope="valdi"');
    expect(panel.performanceContent.innerHTML).toContain('data-performance-scope="browser"');
    expect(panel.performanceContent.innerHTML).toContain('data-performance-scope="all"');
    expect(panel.performanceContent.innerHTML).not.toContain('data-performance-scope="app"');
    expect(panel.performanceContent.innerHTML).toContain('JavaScript time');
    expect(panel.performanceContent.innerHTML).toContain('Layout time');
    expect(panel.performanceContent.innerHTML).toContain('<button type="button"');
  });

  it('links the Performance tab and panel with tab semantics', () => {
    const html = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'devtools-panel.html'), 'utf8');

    expect(html).toContain('id="performanceTab"');
    expect(html).toContain('aria-controls="performanceSection"');
    expect(html).toContain('id="performanceSection"');
    expect(html).toMatch(/role="tabpanel"\s+aria-labelledby="performanceTab"/);
  });

  it('clamps capture to fifteen seconds and never calls a CPU profiling route', async () => {
    panel.state.performance.data = snapshot;
    panel.state.performance.durationSeconds = 90;

    await panel.runPerformanceAction('trace-capture');

    const capture = requests.find(request => new URL(request.url).pathname.endsWith('/trace/capture'));
    if (!capture?.body) throw new Error('Expected a trace capture request.');
    expect(JSON.parse(capture.body)).toEqual({ durationMs: 15_000 });
    expect(new URL(capture.url).searchParams.get('sessionId')).toBe('web-preview');
    expect(requests.some(request => request.url.includes('/profile/'))).toBeFalse();
    expect(panel.state.performance.lastTrace).toEqual(jasmine.objectContaining({ traceCount: 2 }));
  });

  it('keeps the recording owner tuple when stopping after the selected target changes', async () => {
    panel.state.performance.data = snapshot;
    await panel.runPerformanceAction('trace-start');
    expect(panel.state.performance.traceActive).toBeTrue();
    expect(panel.state.performance.ownerIdentity).toEqual(
      jasmine.objectContaining({ sessionId: 'web-preview', targetNonce: 'panel-target-nonce-123456' }),
    );

    panel.state.performance.lastTrace = traceResult();
    panel.preparePerformanceForTargetChange();
    expect(panel.state.performance.data).toBeNull();
    expect(panel.state.performance.lastTrace).toBeNull();
    expect(panel.state.performance.samples).toEqual([]);
    panel.state.target = { id: 'owl:replacement', sessionId: 'replacement' };
    snapshotResponse = { ...snapshot, resourceCount: 9, uptimeMs: 200 };
    await panel.runPerformanceAction('trace-stop');

    const stop = requests.find(request => new URL(request.url).pathname.endsWith('/trace/stop'));
    if (!stop) throw new Error('Expected a trace stop request.');
    expect(new URL(stop.url).searchParams.get('sessionId')).toBe('web-preview');
    expect(panel.state.performance.traceActive).toBeFalse();
    expect(panel.state.performance.ownerIdentity).toBeNull();
    expect(panel.state.performance.lastTrace).toBeNull();
    expect(panel.state.performance.data).toEqual(jasmine.objectContaining({ resourceCount: 9 }));
    expect(panel.state.performance.samples.length).toBe(1);
  });

  it('acknowledges a retained completion error through Stop before allowing another Start', async () => {
    panel.state.performance.data = snapshot;
    panel.state.performance.traceActive = true;
    panel.state.performance.ownerIdentity = {
      inspectedUrl: 'http://127.0.0.1:54321/index.html?valdiDevTools=1',
      sessionId: 'web-preview',
      targetNonce: 'panel-target-nonce-123456',
    };
    completionErrorPending = true;

    await panel.refreshPerformance();

    const acknowledgement = requests.find(request => new URL(request.url).pathname.endsWith('/trace/stop'));
    if (!acknowledgement) throw new Error('Expected completion-error acknowledgement through Stop.');
    expect(new URL(acknowledgement.url).searchParams.get('sessionId')).toBe('web-preview');
    expect(panel.state.performance.error).toBe('Synthetic retained completion error.');
    expect(completionErrorPending).toBeFalse();
    expect(panel.state.performance.traceActive).toBeFalse();
    expect(panel.state.performance.ownerIdentity).toBeNull();

    await panel.runPerformanceAction('trace-start');
    expect(panel.state.performance.traceActive).toBeTrue();
  });

  it('invalidates an old snapshot without wedging polling when the target changes', async () => {
    let resolveSnapshot:
      ((response: { ok: boolean; status: number; json(): Promise<Record<string, unknown>> }) => void) | undefined;
    nextFetchResponse = new Promise(resolve => {
      resolveSnapshot = resolve;
    });
    const oldRefresh = panel.refreshPerformance();
    await Promise.resolve();
    expect(panel.state.performance.snapshotPending).toBeTrue();

    panel.preparePerformanceForTargetChange();
    panel.state.target = { id: 'owl:replacement', sessionId: 'replacement' };
    expect(panel.state.performance.snapshotPending).toBeFalse();
    if (!resolveSnapshot) throw new Error('Expected a deferred performance snapshot.');
    resolveSnapshot({ json: () => Promise.resolve(snapshot), ok: true, status: 200 });
    await oldRefresh;

    expect(panel.state.performance.data).toBeNull();
    await panel.refreshPerformance();
    expect(panel.state.performance.data).toEqual(snapshot);
    expect(panel.state.performance.snapshotPending).toBeFalse();
  });

  it('does not let a delayed pre-start status response overwrite a successful Start', async () => {
    panel.state.performance.data = snapshot;
    let resolveSnapshot:
      ((response: { ok: boolean; status: number; json(): Promise<Record<string, unknown>> }) => void) | undefined;
    nextFetchResponse = new Promise(resolve => {
      resolveSnapshot = resolve;
    });
    const oldPoll = panel.refreshPerformance({ silent: true });
    await Promise.resolve();

    await panel.runPerformanceAction('trace-start');
    if (!resolveSnapshot) throw new Error('Expected a deferred pre-start snapshot.');
    resolveSnapshot({ json: () => Promise.resolve(snapshot), ok: true, status: 200 });
    await oldPoll;

    expect(panel.state.performance.traceActive).toBeTrue();
    expect(panel.state.performance.ownerIdentity).toEqual(jasmine.objectContaining({ sessionId: 'web-preview' }));
  });

  it('cleans up a stale successful start without overwriting the replacement target', async () => {
    panel.state.performance.data = snapshot;
    let resolveStart:
      ((response: { ok: boolean; status: number; json(): Promise<Record<string, unknown>> }) => void) | undefined;
    nextFetchResponse = new Promise(resolve => {
      resolveStart = resolve;
    });
    const start = panel.runPerformanceAction('trace-start');
    await Promise.resolve();

    panel.preparePerformanceForTargetChange();
    panel.state.target = { id: 'owl:replacement', sessionId: 'replacement' };
    if (!resolveStart) throw new Error('Expected a deferred performance start.');
    resolveStart({
      json: () => Promise.resolve({ recording: true, rendererTracingEnabled: true, tracingSupported: true }),
      ok: true,
      status: 200,
    });
    await start;

    const startRequest = requests.find(request => new URL(request.url).pathname.endsWith('/trace/start'));
    const cleanupRequest = requests.find(request => new URL(request.url).pathname.endsWith('/trace/stop'));
    if (!startRequest || !cleanupRequest) throw new Error('Expected stale-start cleanup requests.');
    expect(new URL(startRequest.url).searchParams.get('sessionId')).toBe('web-preview');
    expect(new URL(cleanupRequest.url).searchParams.get('sessionId')).toBe('web-preview');
    expect(panel.state.target?.sessionId).toBe('replacement');
    expect(panel.state.performance.traceActive).toBeFalse();
    expect(panel.state.performance.ownerIdentity).toBeNull();
    expect(panel.state.performance.pending).toBeFalse();
  });

  it('retains and surfaces a stale Start owner when exact cleanup fails', async () => {
    panel.state.performance.data = snapshot;
    let resolveStart:
      ((response: { ok: boolean; status: number; json(): Promise<Record<string, unknown>> }) => void) | undefined;
    nextFetchResponse = new Promise(resolve => {
      resolveStart = resolve;
    });
    const start = panel.runPerformanceAction('trace-start');
    await Promise.resolve();

    panel.preparePerformanceForTargetChange();
    panel.state.target = { id: 'owl:replacement', sessionId: 'replacement' };
    stopFailure = 'Synthetic stale cleanup failure.';
    if (!resolveStart) throw new Error('Expected a deferred performance start.');
    resolveStart({
      json: () => Promise.resolve({ recording: true, rendererTracingEnabled: true, tracingSupported: true }),
      ok: true,
      status: 200,
    });
    await start;

    expect(panel.state.performance.traceActive).toBeTrue();
    expect(panel.state.performance.ownerIdentity).toEqual(jasmine.objectContaining({ sessionId: 'web-preview' }));
    expect(panel.state.performance.error).toContain('Synthetic stale cleanup failure.');
    expect(panel.performanceContent.innerHTML).toContain('Stop and retrieve');

    await panel.runPerformanceAction('trace-stop');
    const stopRequests = requests.filter(request => new URL(request.url).pathname.endsWith('/trace/stop'));
    expect(stopRequests.length).toBe(2);
    expect(new URL(stopRequests[1]?.url ?? '').searchParams.get('sessionId')).toBe('web-preview');
    expect(panel.state.performance.ownerIdentity).toBeNull();
  });

  it('does not replace a newer owner when stale Start cleanup fails', async () => {
    panel.state.performance.data = snapshot;
    let resolveStart:
      ((response: { ok: boolean; status: number; json(): Promise<Record<string, unknown>> }) => void) | undefined;
    nextFetchResponse = new Promise(resolve => {
      resolveStart = resolve;
    });
    const staleStart = panel.runPerformanceAction('trace-start');
    await Promise.resolve();

    panel.preparePerformanceForTargetChange();
    panel.state.target = { id: 'owl:replacement', sessionId: 'replacement' };
    await panel.runPerformanceAction('trace-start');
    expect(panel.state.performance.ownerIdentity).toEqual(jasmine.objectContaining({ sessionId: 'replacement' }));
    stopFailure = 'Synthetic stale cleanup failure.';
    const warn = spyOn(console, 'warn');
    if (!resolveStart) throw new Error('Expected a deferred stale performance start.');
    resolveStart({
      json: () => Promise.resolve({ recording: true, rendererTracingEnabled: true, tracingSupported: true }),
      ok: true,
      status: 200,
    });
    await staleStart;

    const cleanupRequest = requests.find(
      request =>
        new URL(request.url).pathname.endsWith('/trace/stop') &&
        new URL(request.url).searchParams.get('sessionId') === 'web-preview',
    );
    if (!cleanupRequest) throw new Error('Expected exact stale-owner cleanup.');
    expect(panel.state.performance.ownerIdentity).toEqual(jasmine.objectContaining({ sessionId: 'replacement' }));
    expect(panel.state.performance.traceActive).toBeTrue();
    expect(panel.state.performance.error).toBeNull();
    expect(warn).toHaveBeenCalledWith(
      'Unable to retain a stale Performance recording because a different recording is already owned by a newer operation.',
      jasmine.anything(),
    );
  });

  it('keeps an in-flight Capture owned for exact recovery after a target change', async () => {
    panel.state.performance.data = snapshot;
    let resolveCapture:
      ((response: { ok: boolean; status: number; json(): Promise<Record<string, unknown>> }) => void) | undefined;
    nextFetchResponse = new Promise(resolve => {
      resolveCapture = resolve;
    });
    const capture = panel.runPerformanceAction('trace-capture');
    await Promise.resolve();
    expect(panel.state.performance.traceActive).toBeTrue();
    expect(panel.state.performance.ownerIdentity).toEqual(jasmine.objectContaining({ sessionId: 'web-preview' }));

    panel.preparePerformanceForTargetChange();
    panel.state.target = { id: 'owl:replacement', sessionId: 'replacement' };
    expect(panel.state.performance.data).toBeNull();
    expect(panel.state.performance.ownerIdentity).toEqual(jasmine.objectContaining({ sessionId: 'web-preview' }));
    if (!resolveCapture) throw new Error('Expected a deferred performance capture.');
    resolveCapture({ json: () => Promise.resolve(traceResult()), ok: true, status: 200 });
    await capture;

    expect(panel.state.performance.traceActive).toBeTrue();
    expect(panel.state.performance.ownerIdentity).toEqual(jasmine.objectContaining({ sessionId: 'web-preview' }));
    expect(panel.state.performance.lastTrace).toBeNull();
    expect(panel.state.performance.pending).toBeFalse();

    await panel.runPerformanceAction('trace-stop');
    expect(panel.state.performance.traceActive).toBeFalse();
    expect(panel.state.performance.ownerIdentity).toBeNull();
  });

  it('skips silent polling while a generated Performance input is focused', async () => {
    panel.state.performance.data = snapshot;
    panel.document.activeElement = { id: 'performanceTraceFilter' };

    await panel.refreshPerformance({ silent: true });

    expect(requests).toEqual([]);
    expect(panel.state.performance.data).toBe(snapshot);
  });

  it('preserves panel and timeline scroll positions when polling rerenders', () => {
    const previousTimeline = { scrollLeft: 83, scrollTop: 47 };
    const replacementTimeline = { scrollLeft: 0, scrollTop: 0 };
    let queryCount = 0;
    panel.performanceContent.scrollTop = 129;
    panel.performanceContent.querySelector = () => (queryCount++ === 0 ? previousTimeline : replacementTimeline);

    panel.renderPerformance(snapshot);

    expect(panel.performanceContent.scrollTop).toBe(129);
    expect(replacementTimeline.scrollLeft).toBe(83);
    expect(replacementTimeline.scrollTop).toBe(47);
  });

  it('uses an exact-owner keepalive Stop when the panel unloads', async () => {
    panel.state.performance.data = snapshot;
    await panel.runPerformanceAction('trace-start');
    requests = [];

    panel.dispatchWindowEvent('pagehide');
    await Promise.resolve();

    const stop = requests.find(request => new URL(request.url).pathname.endsWith('/trace/stop'));
    if (!stop) throw new Error('Expected a pagehide trace stop request.');
    expect(stop.keepalive).toBeTrue();
    expect(new URL(stop.url).searchParams.get('sessionId')).toBe('web-preview');
    expect(new URL(stop.url).searchParams.get('targetNonce')).toBe('panel-target-nonce-123456');
  });

  it('bounds samples, timeline rows, and grouped summary rows', () => {
    const traces = Array.from({ length: 180 }, (_, index) => ({
      endMicros: index * 1000 + 500,
      startMicros: index * 1000,
      threadId: 1,
      trace: `Valdi.Operation.${index % 20}`,
    }));
    panel.state.performance.lastTrace = { ...traceResult(), traceCount: traces.length, traces };
    for (let index = 0; index < 125; index++) {
      panel.renderPerformance({ ...snapshot, uptimeMs: index + 1 });
    }

    const timelineRows = panel.performanceContent.innerHTML.match(/class="performance-timeline-row"/g) ?? [];
    const summaryRows = panel.performanceContent.innerHTML.match(/<tr><td>Valdi\.Operation\./g) ?? [];
    expect(panel.state.performance.samples.length).toBe(120);
    expect(timelineRows.length).toBe(120);
    expect(summaryRows.length).toBe(12);
    expect(panel.performanceContent.innerHTML).toContain('Total captured duration by event');
    expect(panel.performanceContent.innerHTML).toContain('Inclusive total');
  });

  it('builds the export only on demand without requiring duplicated raw or Perfetto event lists', () => {
    const result = traceResult();
    const exported = panel.buildPerformanceTraceExport(result);

    expect(result['rawTraceEvents']).toBeUndefined();
    expect(result['perfetto']).toBeUndefined();
    expect(exported.traceEvents.filter(event => event['ph'] === 'X').length).toBe(2);
    expect(exported.traceEvents.length).toBe(4);
  });

  it('escapes filtered trace names in the panel while preserving them in on-demand export', () => {
    const maliciousName = 'Valdi.<img src=x onerror=alert(1)>';
    const result = traceResult();
    const traces = result['traces'] as Array<Record<string, unknown>>;
    traces.push({ endMicros: 77_000, startMicros: 76_700, threadId: 1, trace: maliciousName });
    panel.state.performance.lastTrace = { ...result, traceCount: traces.length };
    panel.state.performance.traceSearch = 'onerror';

    panel.renderPerformance(snapshot);
    const exported = panel.buildPerformanceTraceExport(result);

    expect(panel.performanceContent.innerHTML).not.toContain('<img src=x');
    expect(panel.performanceContent.innerHTML).toContain('&lt;img src=x onerror=alert(1)&gt;');
    expect(exported.traceEvents).toContain(jasmine.objectContaining({ name: maliciousName }));
  });
});
