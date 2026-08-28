import 'jasmine';
import fs from 'node:fs';
import path from 'node:path';
import { Script } from 'node:vm';

interface RuntimeNode {
  children: RuntimeNode[];
  component?: {
    key?: string;
    name?: string;
    properties?: Record<string, unknown>;
    state?: string;
  };
  element?: { id?: number | string };
  id?: string;
  key?: string;
  tag: string;
}

interface RuntimeStateRecord {
  key: string;
  name: string;
  node: RuntimeNode;
  selectableNodeId: string | null;
  source: string;
  sourceType: string;
  structuralId: string;
}

interface RuntimeStateContext {
  componentId: string;
  expandedPaths: Set<string>;
  limitRendered: boolean;
  rows: number;
  scope: string;
  truncated: boolean;
}

interface RuntimeParseResult {
  error: string | null;
  parsed: boolean;
  value: unknown;
}

interface RuntimePanel {
  elements: {
    inspector: StubElement;
    stateContent: StubElement;
    stateFilter: StubElement;
    stateSection: StubElement;
    stateSummary: StubElement;
  };
  state: {
    activeDetail: string;
    activeSection: string;
    autoRefresh: boolean;
    componentPropertyEdit: { focused: boolean; pending: boolean };
    runtimeState: {
      expandedComponents: Set<string>;
      expandedInspectorValues: Set<string>;
      expandedMainValues: Set<string>;
      inspectGeneration: number;
      inspectorNodeId: string | null;
      search: string;
    };
    selectedNodeId: string | null;
    snapshot: { tree: RuntimeNode } | null;
    snapshotGeneration: number;
    target: RuntimeTarget | null;
  };
  clearTargetPresentation(message: string): void;
  collectRuntimeStateComponents(root: RuntimeNode): { records: RuntimeStateRecord[]; truncated: boolean };
  connectToInspectedPage(): Promise<void>;
  handleDetailTabNavigation(event: StubEvent): void;
  inspectRuntimeStateBinding(binding: Record<string, unknown>): boolean;
  parseRuntimeState(source: string, sourceType: string): RuntimeParseResult;
  parseRuntimeStateRecord(record: RuntimeStateRecord): RuntimeParseResult;
  readRuntimeStateToken(
    source: string,
    parser: { offset: number; sourceType: string; tokens: number },
  ): { type: string; value?: unknown };
  renderRuntimeStateEntries(value: unknown, context: RuntimeStateContext, segments: string[]): string;
  renderRuntimeStateSection(): void;
  refreshSnapshot(): Promise<void>;
  runtimeStateComponentRecord(node: RuntimeNode, structuralId: string): RuntimeStateRecord | null;
  resetRuntimeStateForSelectionChange(selectedNodeId: string | null): void;
  resetRuntimeStateForTargetChange(): void;
  setActiveDetail(detail: string): void;
  setActiveSection(section: string): void;
  startRefreshTimer(): void;
  updateRuntimeStateDisclosure(details: StubElement): void;
}

interface RuntimeTarget {
  applicationUrl?: string;
  capabilities: string[];
  debuggingPort?: number;
  id: string;
  name?: string;
  sessionId?: string;
}

interface StubEvent {
  currentTarget: StubElement;
  key: string;
  target: StubElement;
  preventDefault(): void;
  stopPropagation(): void;
}

class StubElement {
  readonly attributes = new Map<string, string>();
  readonly children: StubElement[] = [];
  readonly classNames = new Set<string>();
  readonly dataset: Record<string, string> = {};
  readonly listeners = new Map<string, Array<(event: StubEvent) => void>>();
  readonly queryResults = new Map<string, StubElement[]>();
  readonly closestResults = new Map<string, StubElement>();
  readonly containedElements = new Set<StubElement>();
  readonly style: Record<string, string> = {};
  readonly classList = {
    contains: (name: string): boolean => this.classNames.has(name),
    toggle: (name: string, enabled?: boolean): void => {
      if (enabled ?? !this.classNames.has(name)) this.classNames.add(name);
      else this.classNames.delete(name);
    },
  };
  checked = true;
  className = '';
  disabled = false;
  focusCount = 0;
  hidden = false;
  innerHTML = '';
  open = false;
  scrollHeight = 0;
  scrollTop = 0;
  tabIndex = -1;
  textContent = '';
  title = '';
  value = '';
  private readonly onFocus: (element: StubElement) => void;

  constructor(
    readonly id: string,
    onFocus: (element: StubElement) => void,
  ) {
    this.onFocus = onFocus;
  }

  addEventListener(type: string, listener: (event: StubEvent) => void): void {
    const listeners = this.listeners.get(type) ?? [];
    listeners.push(listener);
    this.listeners.set(type, listeners);
  }

  append(child: StubElement): void {
    this.children.push(child);
  }

  closest(selector: string): StubElement | null {
    return this.closestResults.get(selector) ?? null;
  }

  contains(element: StubElement): boolean {
    if (element === this || this.containedElements.has(element)) return true;
    return Array.from(this.containedElements).some(child => child.contains(element));
  }

  dispatch(type: string, properties: Partial<StubEvent> = {}): void {
    const event: StubEvent = {
      currentTarget: this,
      key: '',
      preventDefault() {},
      stopPropagation() {},
      target: this,
      ...properties,
    };
    for (const listener of this.listeners.get(type) ?? []) listener(event);
  }

  focus(): void {
    this.focusCount++;
    this.onFocus(this);
  }

  getAttribute(name: string): string | null {
    return this.attributes.get(name) ?? null;
  }

  getBoundingClientRect(): { bottom: number; height: number; left: number; right: number; top: number; width: number } {
    return { bottom: 600, height: 600, left: 0, right: 800, top: 0, width: 800 };
  }

  querySelector(selector: string): StubElement | null {
    return this.querySelectorAll(selector)[0] ?? null;
  }

  querySelectorAll(selector: string): StubElement[] {
    return this.queryResults.get(selector) ?? [];
  }

  removeAttribute(name: string): void {
    this.attributes.delete(name);
  }

  replaceChildren(...children: StubElement[]): void {
    this.children.splice(0, this.children.length, ...children);
  }

  scrollIntoView(): void {}

  setAttribute(name: string, value: string): void {
    this.attributes.set(name, value);
  }

  setSelectionRange(): void {}
}

interface RuntimeHarness {
  detailTabs: StubElement[];
  fetchPaths: string[];
  panel: RuntimePanel;
  deferNextSnapshot(): void;
  dispatchDocument(type: string): void;
  resolveDeferredSnapshot(): void;
  runIntervals(): void;
  setActiveElement(element: StubElement | null): void;
  setDocumentHidden(hidden: boolean): void;
  setTargetPayload(target: RuntimeTarget): void;
}

function makeRuntimeNode(index: number, source = `{"value":${index}}`): RuntimeNode {
  return {
    children: [],
    component: { key: `key-${index}`, name: `Component${index}`, state: source },
    id: `component-${index}`,
    tag: `Component${index}`,
  };
}

function createRuntimeHarness(search = '?targetId=runtime-state-test'): RuntimeHarness {
  const treeModelSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'debugger-tree-model.js'), 'utf8');
  const rawPanelSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'devtools-panel.js'), 'utf8');
  const panelSource = rawPanelSource.replace('void connectToInspectedApplication();', 'void 0;');
  const elements = new Map<string, StubElement>();
  const intervals: Array<() => void> = [];
  const fetchPaths: string[] = [];
  const documentListeners = new Map<string, Array<() => void>>();
  const documentDataset: Record<string, string> = {};
  let activeElement: StubElement | null = null;
  let deferredSnapshotResolve: (() => void) | null = null;
  let deferSnapshot = false;
  let documentHidden = false;
  let targetPayload: RuntimeTarget = {
    applicationUrl: 'http://127.0.0.1:54321',
    capabilities: ['components', 'snapshot'],
    debuggingPort: 54_321,
    id: 'new-target',
    name: 'New target',
    sessionId: 'new-session',
  };

  const elementForId = (id: string): StubElement => {
    let element = elements.get(id);
    if (element === undefined) {
      element = new StubElement(id, focused => {
        activeElement = focused;
      });
      elements.set(id, element);
    }
    return element;
  };
  const mainTabs = ['elements', 'state', 'performance', 'console'].map(section => {
    const tab = elementForId(`${section}Tab`);
    tab.dataset['section'] = section;
    return tab;
  });
  const detailTabs = ['styles', 'computed', 'state', 'dom'].map(detail => {
    const tab = elementForId(`${detail}DetailTab`);
    tab.dataset['detail'] = detail;
    return tab;
  });
  const sections = ['elements', 'state', 'performance', 'console'].map(section => {
    const sectionElement = elementForId(`${section}Section`);
    sectionElement.dataset['panel'] = section;
    return sectionElement;
  });
  elementForId('stateSection').containedElements.add(elementForId('stateFilter'));
  elementForId('stateSection').containedElements.add(elementForId('stateContent'));
  const documentObject = {
    addEventListener(type: string, listener: () => void) {
      const listeners = documentListeners.get(type) ?? [];
      listeners.push(listener);
      documentListeners.set(type, listeners);
    },
    createElement(type: string): StubElement {
      return new StubElement(type, focused => {
        activeElement = focused;
      });
    },
    documentElement: { dataset: documentDataset },
    get activeElement(): StubElement | null {
      return activeElement;
    },
    getElementById(id: string): StubElement {
      return elementForId(id);
    },
    get hidden(): boolean {
      return documentHidden;
    },
    querySelectorAll(selector: string): StubElement[] {
      if (selector === '.main-tab') return mainTabs;
      if (selector === '.detail-tab') return detailTabs;
      if (selector === '.section') return sections;
      return [];
    },
  };
  const windowObject = {
    addEventListener() {},
    clearInterval() {},
    clearTimeout() {},
    confirm: () => true,
    location: { origin: 'http://127.0.0.1:18768', search },
    parent: {},
    removeEventListener() {},
    setInterval(callback: () => void): number {
      intervals.push(callback);
      return intervals.length;
    },
    setTimeout(callback: () => void): number {
      callback();
      return 1;
    },
  };
  class StubEventSource {
    addEventListener(): void {}
    close(): void {}
  }
  const panel = new Script(
    `${treeModelSource}\n${panelSource}\n({ clearTargetPresentation, collectRuntimeStateComponents, connectToInspectedPage, elements, handleDetailTabNavigation, inspectRuntimeStateBinding, parseRuntimeState, parseRuntimeStateRecord, readRuntimeStateToken, refreshSnapshot, renderRuntimeStateEntries, renderRuntimeStateSection, resetRuntimeStateForSelectionChange, resetRuntimeStateForTargetChange, runtimeStateComponentRecord, setActiveDetail, setActiveSection, startRefreshTimer, state, updateRuntimeStateDisclosure })`,
  ).runInNewContext({
    Blob,
    EventSource: StubEventSource,
    URL,
    URLSearchParams,
    console,
    debuggerApiHeaders: (headers: Record<string, string>) => ({
      ...headers,
      'X-Valdi-Debugger-Token': 'devtools-runtime-state-test-token',
    }),
    document: documentObject,
    fetch: (input: URL | string) => {
      const url = new URL(input.toString());
      fetchPaths.push(url.pathname);
      const payload =
        url.pathname === '/api/devtools/target'
          ? { target: targetPayload }
          : url.pathname === '/api/devtools/snapshot'
            ? { target: targetPayload, tree: makeRuntimeNode(1) }
            : {};
      const response = { json: () => Promise.resolve(payload), ok: true, status: 200 };
      if (url.pathname === '/api/devtools/snapshot' && deferSnapshot) {
        deferSnapshot = false;
        return new Promise(resolve => {
          deferredSnapshotResolve = () => resolve(response);
        });
      }
      return Promise.resolve(response);
    },
    navigator: { clipboard: { writeText: () => Promise.resolve() } },
    window: windowObject,
  }) as RuntimePanel;

  return {
    detailTabs,
    deferNextSnapshot() {
      deferSnapshot = true;
    },
    dispatchDocument(type) {
      for (const listener of documentListeners.get(type) ?? []) listener();
    },
    fetchPaths,
    panel,
    resolveDeferredSnapshot() {
      deferredSnapshotResolve?.();
      deferredSnapshotResolve = null;
    },
    runIntervals() {
      for (const callback of intervals) callback();
    },
    setActiveElement(element) {
      activeElement = element;
    },
    setDocumentHidden(hidden) {
      documentHidden = hidden;
    },
    setTargetPayload(target) {
      targetPayload = target;
    },
  };
}

async function flushPromises(): Promise<void> {
  for (let index = 0; index < 10; index++) await Promise.resolve();
}

function makeValueContext(): RuntimeStateContext {
  return {
    componentId: 'component',
    expandedPaths: new Set(),
    limitRendered: false,
    rows: 0,
    scope: 'main',
    truncated: false,
  };
}

describe('DevTools bounded runtime state', () => {
  it('parses strict JSON into null-prototype records without prototype pollution', () => {
    const { panel } = createRuntimeHarness();
    const parsed = panel.parseRuntimeState(
      '{"__proto__":{"polluted":true},"constructor":{"safe":1},"prototype":{"safe":2}}',
      'web',
    );
    const value = parsed.value as Record<string, unknown>;
    const emptyRecord: Record<string, unknown> = {};

    expect(parsed.parsed).toBeTrue();
    expect(Object.getPrototypeOf(value)).toBeNull();
    expect(Object.getPrototypeOf(value['__proto__'] as object)).toBeNull();
    expect(Object.prototype.hasOwnProperty.call(value, '__proto__')).toBeTrue();
    expect(Object.prototype.hasOwnProperty.call(value, 'constructor')).toBeTrue();
    expect(Object.prototype.hasOwnProperty.call(value, 'prototype')).toBeTrue();
    expect(emptyRecord['polluted']).toBeUndefined();
  });

  it('treats every native debug document as escaped raw-only input', () => {
    const { panel } = createRuntimeHarness();
    const nativeDocuments = [
      '{ count: 1, ready: true }',
      // An actual key of `real: 0, forged` is indistinguishable from two fields
      // because the native debug serializer does not escape property names.
      '{ real: 0, forged: true }',
      'Map{first: 1, second: 2}',
      'Set(true, false)',
      '<function callback"/>, forged: true/>',
      '<error "real: 0, forged: true"/>',
    ];
    const node = makeRuntimeNode(1);
    panel.state.snapshot = { tree: node };
    panel.state.runtimeState.expandedComponents.add('[]');

    for (const source of nativeDocuments) {
      const parsed = panel.parseRuntimeState(source, 'native');
      expect(parsed.parsed).withContext(source).toBeFalse();
      expect(parsed.value).withContext(source).toBeNull();
      expect(parsed.error).withContext(source).toContain('escaped raw text');
      node.component = { state: source };
      panel.renderRuntimeStateSection();
      expect(panel.elements.stateContent.innerHTML).withContext(source).toContain('class="runtime-state-raw"');
      expect(panel.elements.stateContent.innerHTML).withContext(source).not.toContain('runtime-state-value-key');
    }

    node.component = { state: '<error "real: 0, forged: true"/>' };
    panel.renderRuntimeStateSection();

    expect(panel.elements.stateContent.innerHTML).toContain('class="runtime-state-raw"');
    expect(panel.elements.stateContent.innerHTML).toContain('&lt;error &quot;real: 0, forged: true&quot;/&gt;');
    expect(panel.elements.stateContent.innerHTML).not.toContain('runtime-state-value-key');
  });

  it('keeps web parsing strict', () => {
    const { panel } = createRuntimeHarness();

    expect(panel.parseRuntimeState('{"same":1,"same":2}', 'web').parsed).toBeFalse();
    expect(panel.parseRuntimeState('{ value: 1 }', 'web').parsed).toBeFalse();
    expect(panel.parseRuntimeState('{"value":"safe"}', 'web').parsed).toBeTrue();
    expect(panel.parseRuntimeState('{"value":"safe"}', 'unknown').parsed).toBeFalse();
  });

  it('reuses parsed state by node and invalidates it when source or source type changes', () => {
    const { panel } = createRuntimeHarness();
    const node = makeRuntimeNode(1);
    const record = panel.runtimeStateComponentRecord(node, '[]');
    if (record === null) throw new Error('Expected a runtime state record.');
    const webRecord = { ...record, sourceType: 'web' };

    const first = panel.parseRuntimeStateRecord(webRecord);
    const reused = panel.parseRuntimeStateRecord({ ...webRecord, structuralId: '[0]' });
    const changedSource = panel.parseRuntimeStateRecord({ ...webRecord, source: '{"value":2}' });
    const changedSourceType = panel.parseRuntimeStateRecord({
      ...webRecord,
      source: '{"value":2}',
      sourceType: 'native',
    });

    expect(reused).toBe(first);
    expect(changedSource).not.toBe(first);
    expect((changedSource.value as Record<string, unknown>)['value']).toBe(2);
    expect(changedSourceType).not.toBe(changedSource);
    expect(changedSourceType.parsed).toBeFalse();
  });

  it('enforces exact raw, depth, entry, key, and token boundaries', () => {
    const { panel } = createRuntimeHarness();
    const depth12 = `${'['.repeat(12)}0${']'.repeat(12)}`;
    const depth13 = `${'['.repeat(13)}0${']'.repeat(13)}`;
    const entries100 = `{${Array.from({ length: 100 }, (_value, index) => `"k${index}":${index}`).join(',')}}`;
    const entries101 = `{${Array.from({ length: 101 }, (_value, index) => `"k${index}":${index}`).join(',')}}`;
    const key1024 = `{${JSON.stringify('k'.repeat(1024))}:1}`;
    const key1025 = `{${JSON.stringify('k'.repeat(1025))}:1}`;
    const raw65536 = `"${'x'.repeat(65_534)}"`;
    const nearTokenLimit = `[${[
      `[${Array.from({ length: 8 }, () => '0').join(',')}]`,
      ...Array.from({ length: 99 }, () => `[${Array.from({ length: 9 }, () => '0').join(',')}]`),
    ].join(',')}]`;
    const overTokenLimit = `[${Array.from({ length: 100 }, () => `[${Array.from({ length: 9 }, () => '0').join(',')}]`).join(',')}]`;

    expect(panel.parseRuntimeState(depth12, 'web').parsed).toBeTrue();
    expect(panel.parseRuntimeState(depth13, 'web').parsed).toBeFalse();
    expect(panel.parseRuntimeState(entries100, 'web').parsed).toBeTrue();
    expect(panel.parseRuntimeState(entries101, 'web').parsed).toBeFalse();
    expect(panel.parseRuntimeState(key1024, 'web').parsed).toBeTrue();
    expect(panel.parseRuntimeState(key1025, 'web').parsed).toBeFalse();
    expect(raw65536.length).toBe(65_536);
    expect(panel.parseRuntimeState(raw65536, 'web').parsed).toBeTrue();
    expect(panel.parseRuntimeState(`${raw65536} `, 'web').parsed).toBeFalse();
    expect(panel.parseRuntimeState(nearTokenLimit, 'web').parsed).toBeTrue();
    expect(panel.parseRuntimeState(overTokenLimit, 'web').parsed).toBeFalse();
  });

  it('allows exactly 2,000 lexical tokens and rejects token 2,001', () => {
    const { panel } = createRuntimeHarness();
    const source = Array.from({ length: 2001 }, () => '0').join(' ');
    const parser = { offset: 0, sourceType: 'web', tokens: 0 };

    for (let index = 0; index < 2000; index++) {
      expect(panel.readRuntimeStateToken(source, parser).type).toBe('value');
    }
    expect(parser.tokens).toBe(2000);
    expect(() => panel.readRuntimeStateToken(source, parser)).toThrowError(/token limit/);
  });

  it('caps component rows at 500 and rendered value rows at 1,000', () => {
    const { panel } = createRuntimeHarness();
    const root = makeRuntimeNode(0);
    root.children = Array.from({ length: 500 }, (_value, index) => makeRuntimeNode(index + 1));

    const components = panel.collectRuntimeStateComponents(root);
    const context = makeValueContext();
    const markup = panel.renderRuntimeStateEntries(
      Array.from({ length: 1001 }, (_value, index) => index),
      context,
      [],
    );

    expect(components.records.length).toBe(500);
    expect(components.truncated).toBeTrue();
    expect(context.rows).toBe(1000);
    expect(context.truncated).toBeTrue();
    expect(markup).toContain('Additional state rows were omitted.');
    expect(markup).not.toContain('[1000]');
  });

  it('keeps repeated native subtrees distinct without offering path-only Inspect actions', () => {
    const { panel } = createRuntimeHarness();
    const makeNativeStateNode = (key: string, source: string, children: RuntimeNode[]): RuntimeNode => ({
      children,
      component: { state: source },
      key,
      tag: key === 'shared-key' ? 'RepeatedComponent' : 'BranchComponent',
    });
    const firstRepeated = makeNativeStateNode('shared-key', '{ value: 1 }', []);
    const secondRepeated = makeNativeStateNode('shared-key', '{ value: 2 }', []);
    const root: RuntimeNode = {
      children: [
        makeNativeStateNode('left-branch', '{ branch: 1 }', [firstRepeated]),
        makeNativeStateNode('right-branch', '{ branch: 2 }', [secondRepeated]),
      ],
      key: 'root',
      tag: 'Root',
    };
    panel.state.snapshot = { tree: root };

    const records = panel.collectRuntimeStateComponents(root).records;
    const repeatedRecords = records.filter(record => record.key === 'shared-key');
    expect(repeatedRecords.map(record => record.structuralId)).toEqual(['[0,0]', '[1,0]']);
    expect(repeatedRecords.every(record => record.selectableNodeId === null)).toBeTrue();
    expect(repeatedRecords.every(record => record.sourceType === 'native')).toBeTrue();
    expect(records.map(record => record.key)).toEqual(['left-branch', 'shared-key', 'right-branch', 'shared-key']);

    panel.state.runtimeState.expandedComponents.add('[0,0]');
    panel.renderRuntimeStateSection();
    const markup = panel.elements.stateContent.innerHTML;
    expect(markup).toContain('data-runtime-state-component-id="[0,0]" open');
    expect(markup).toContain('data-runtime-state-component-id="[1,0]"');
    expect(markup).not.toContain('data-runtime-state-component-id="[1,0]" open');
    expect((markup.match(/shared-key/g) ?? []).length).toBe(2);
    expect(markup).not.toContain('runtime-state-inspect');
  });

  it('omits Inspect when an explicit snapshot identity is duplicated', () => {
    const { panel } = createRuntimeHarness();
    const first = makeRuntimeNode(1);
    const second = makeRuntimeNode(2);
    first.id = 'duplicate';
    second.id = 'duplicate';
    const root: RuntimeNode = { children: [first, second], id: 'root', tag: 'Root' };
    panel.state.snapshot = { tree: root };

    const records = panel.collectRuntimeStateComponents(root).records;
    expect(records.map(record => record.selectableNodeId)).toEqual([null, null]);
    panel.renderRuntimeStateSection();
    expect(panel.elements.stateContent.innerHTML).not.toContain('runtime-state-inspect');
  });

  it('keeps native parsing independent from an exact element identity', () => {
    const { panel } = createRuntimeHarness();
    const node: RuntimeNode = {
      children: [],
      component: { state: '{"value":"ambiguous native string"}' },
      element: { id: 42 },
      key: 'native-key',
      tag: 'NativeComponent',
    };
    panel.state.snapshot = { tree: node };
    panel.state.runtimeState.expandedComponents.add('[]');

    const record = panel.runtimeStateComponentRecord(node, '[]');
    expect(record?.selectableNodeId).toBe('42');
    expect(record?.sourceType).toBe('native');
    expect(record?.key).toBe('native-key');
    panel.renderRuntimeStateSection();
    expect(panel.elements.stateContent.innerHTML).toContain('bounded raw snapshot');
    expect(panel.elements.stateContent.innerHTML).toContain('component 42');
  });

  it('does not fall back to element identity when an own node identity is malformed', () => {
    const { panel } = createRuntimeHarness();
    const node: RuntimeNode = {
      children: [],
      component: { state: '{ count: 1 }' },
      element: { id: 42 },
      id: '',
      key: 'malformed-node-id',
      tag: 'NativeComponent',
    };
    panel.state.snapshot = { tree: node };

    expect(panel.runtimeStateComponentRecord(node, '[]')?.selectableNodeId).toBeNull();
    panel.renderRuntimeStateSection();
    expect(panel.elements.stateContent.innerHTML).not.toContain('runtime-state-inspect');
  });

  it('does not read inherited or accessor-backed snapshot fields in the State UI', () => {
    const { panel } = createRuntimeHarness();
    const getter = jasmine.createSpy('stateGetter').and.throwError('must not execute');
    const accessorComponent: Record<string, unknown> = { key: 'key', name: 'Accessor' };
    Object.defineProperty(accessorComponent, 'state', { enumerable: true, get: getter });
    const accessorNode = makeRuntimeNode(1);
    accessorNode.component = accessorComponent as NonNullable<RuntimeNode['component']>;
    const inheritedComponent = Object.create({ state: '{"unsafe":true}' }) as NonNullable<RuntimeNode['component']>;
    Object.defineProperties(inheritedComponent, {
      key: { enumerable: true, value: 'key' },
      name: { enumerable: true, value: 'Inherited' },
    });
    const inheritedNode = makeRuntimeNode(2);
    inheritedNode.component = inheritedComponent;

    expect(panel.runtimeStateComponentRecord(accessorNode, 'accessor')).toBeNull();
    expect(panel.runtimeStateComponentRecord(inheritedNode, 'inherited')).toBeNull();
    expect(getter).not.toHaveBeenCalled();
  });

  it('keeps State search independent and escapes component names, keys, and raw fallback text', () => {
    const { panel } = createRuntimeHarness(
      '?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321&targetNonce=runtime-state-nonce',
    );
    const matching = makeRuntimeNode(1, '{ malformed: <img src=x> }');
    matching.component = { key: '<key>', name: '<script>needle</script>', state: '{ malformed: <img src=x> }' };
    const other = makeRuntimeNode(2, '{"value":"other"}');
    matching.children = [other];
    panel.state.snapshot = { tree: matching };
    panel.state.runtimeState.search = 'needle';
    panel.state.runtimeState.expandedComponents.add('[]');

    panel.renderRuntimeStateSection();

    expect(panel.elements.stateSummary.textContent).toContain('1 of 2 components');
    expect(panel.elements.stateContent.innerHTML).toContain('&lt;script&gt;needle&lt;/script&gt;');
    expect(panel.elements.stateContent.innerHTML).toContain('&lt;key&gt;');
    expect(panel.elements.stateContent.innerHTML).toContain('&lt;img src=x&gt;');
    expect(panel.elements.stateContent.innerHTML).not.toContain('<script>');
    expect(panel.elements.stateContent.innerHTML).not.toContain('<img src=x>');
    expect(panel.elements.stateContent.innerHTML).toContain('component component-1');
  });

  it('rejects Inspect bindings after snapshot, target, selection, filter, disclosure, or node replacement', () => {
    const { panel } = createRuntimeHarness();
    const node = makeRuntimeNode(1);
    panel.state.snapshot = { tree: node };
    panel.state.snapshotGeneration = 4;
    panel.state.runtimeState.inspectGeneration = 9;
    const binding = () => ({
      generation: panel.state.runtimeState.inspectGeneration,
      node,
      selectableNodeId: node.id,
      snapshotGeneration: panel.state.snapshotGeneration,
      source: node.component?.state,
      structuralId: '[]',
    });

    const snapshotBinding = binding();
    panel.state.snapshotGeneration++;
    expect(panel.inspectRuntimeStateBinding(snapshotBinding)).toBeFalse();

    const targetBinding = binding();
    panel.resetRuntimeStateForTargetChange();
    expect(panel.inspectRuntimeStateBinding(targetBinding)).toBeFalse();

    const selectionBinding = binding();
    panel.resetRuntimeStateForSelectionChange('different-component');
    expect(panel.inspectRuntimeStateBinding(selectionBinding)).toBeFalse();

    const filterBinding = binding();
    panel.state.runtimeState.search = 'value';
    panel.renderRuntimeStateSection();
    expect(panel.inspectRuntimeStateBinding(filterBinding)).toBeFalse();

    const disclosureBinding = binding();
    const details = new StubElement('details', () => {});
    details.dataset['runtimeStateComponentId'] = '[]';
    details.open = true;
    panel.updateRuntimeStateDisclosure(details);
    expect(panel.inspectRuntimeStateBinding(disclosureBinding)).toBeFalse();

    const replacementBinding = binding();
    panel.state.snapshot = { tree: makeRuntimeNode(1) };
    expect(panel.inspectRuntimeStateBinding(replacementBinding)).toBeFalse();
  });

  it('restores disclosure focus after a State subtree rerender', () => {
    const harness = createRuntimeHarness();
    const { panel } = harness;
    const node = makeRuntimeNode(1);
    panel.state.snapshot = { tree: node };
    const oldSummary = new StubElement('old-summary', () => {});
    const details = new StubElement('old-details', () => {});
    details.dataset['runtimeStateComponentId'] = '[]';
    details.open = true;
    details.queryResults.set('summary', [oldSummary]);
    const replacementSummary = new StubElement('replacement-summary', element => harness.setActiveElement(element));
    const replacementDetails = new StubElement('replacement-details', () => {});
    replacementDetails.dataset['runtimeStateComponentId'] = '[]';
    replacementDetails.queryResults.set('summary', [replacementSummary]);
    panel.elements.stateContent.queryResults.set('details', [replacementDetails]);
    harness.setActiveElement(oldSummary);

    panel.updateRuntimeStateDisclosure(details);

    expect(panel.state.runtimeState.expandedComponents.has('[]')).toBeTrue();
    expect(replacementSummary.focusCount).toBe(1);
    expect(panel.elements.stateContent.querySelector('details')?.querySelector('summary')).toBe(replacementSummary);
  });

  it('provides roving, keyboard-operable detail tabs and labels the shared tabpanel', () => {
    const harness = createRuntimeHarness();
    const { detailTabs, panel } = harness;
    const [stylesTab, computedTab, stateTab, domTab] = detailTabs;

    panel.setActiveDetail('styles');
    stylesTab.dispatch('keydown', { currentTarget: stylesTab, key: 'ArrowRight' });
    expect(panel.state.activeDetail).toBe('computed');
    expect(stylesTab.tabIndex).toBe(-1);
    expect(computedTab.tabIndex).toBe(0);
    expect(panel.elements.inspector.getAttribute('aria-labelledby')).toBe(computedTab.id);

    computedTab.dispatch('keydown', { currentTarget: computedTab, key: 'End' });
    expect(panel.state.activeDetail).toBe('dom');
    domTab.dispatch('keydown', { currentTarget: domTab, key: 'Home' });
    expect(panel.state.activeDetail).toBe('styles');
    panel.setActiveDetail('state');
    expect(stateTab.getAttribute('aria-selected')).toBe('true');
    expect(stateTab.tabIndex).toBe(0);
  });

  it('blocks immediate, timer, and visibility refreshes while either State view owns focus', () => {
    const harness = createRuntimeHarness();
    const { fetchPaths, panel } = harness;
    panel.state.target = { capabilities: ['components', 'snapshot'], id: 'runtime-state-test' };
    panel.state.activeSection = 'state';
    panel.startRefreshTimer();
    const mainSummary = new StubElement('main-summary', element => harness.setActiveElement(element));
    const inspectButton = new StubElement('inspect-button', element => harness.setActiveElement(element));
    panel.elements.stateContent.containedElements.add(mainSummary);
    panel.elements.stateContent.containedElements.add(inspectButton);

    for (const control of [panel.elements.stateFilter, mainSummary, inspectButton]) {
      control.focus();
      void panel.refreshSnapshot();
      harness.runIntervals();
      harness.dispatchDocument('visibilitychange');
    }

    panel.state.activeSection = 'elements';
    panel.state.activeDetail = 'state';
    const inspectorSummary = new StubElement('inspector-summary', element => harness.setActiveElement(element));
    panel.elements.inspector.containedElements.add(inspectorSummary);
    inspectorSummary.focus();
    void panel.refreshSnapshot();
    harness.runIntervals();
    harness.dispatchDocument('visibilitychange');
    expect(fetchPaths.filter(pathname => pathname === '/api/devtools/snapshot').length).toBe(0);

    harness.setActiveElement(null);
    harness.runIntervals();
    expect(fetchPaths.filter(pathname => pathname === '/api/devtools/snapshot').length).toBe(1);
  });

  it('discards an ordinary snapshot response when State takes focus in flight', async () => {
    const harness = createRuntimeHarness();
    const { panel } = harness;
    const summary = new StubElement('main-summary', element => harness.setActiveElement(element));
    panel.elements.stateContent.containedElements.add(summary);
    panel.state.target = { capabilities: ['components', 'snapshot'], id: 'runtime-state-test' };
    panel.state.activeSection = 'state';
    panel.state.snapshotGeneration = 7;
    harness.deferNextSnapshot();

    const refresh = panel.refreshSnapshot();
    summary.focus();
    harness.resolveDeferredSnapshot();
    await refresh;

    expect(panel.state.snapshotGeneration).toBe(7);
  });

  it('keeps collapsed Inspect visible and transfers click focus to the State detail tab', () => {
    const harness = createRuntimeHarness(
      '?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321&targetNonce=runtime-state-nonce',
    );
    const { detailTabs, panel } = harness;
    const node = makeRuntimeNode(1);
    const inspectButton = new StubElement('inspect-button', element => harness.setActiveElement(element));
    inspectButton.dataset['runtimeStateInspectSlot'] = '0';
    inspectButton.closestResults.set('[data-runtime-state-inspect]', inspectButton);
    panel.elements.stateContent.queryResults.set('[data-runtime-state-inspect-slot]', [inspectButton]);
    panel.elements.stateContent.containedElements.add(inspectButton);
    panel.state.target = { capabilities: ['components', 'snapshot'], id: 'runtime-state-test' };
    panel.state.snapshot = { tree: node };
    panel.state.snapshotGeneration = 3;
    panel.state.activeSection = 'state';
    inspectButton.focus();

    panel.renderRuntimeStateSection();

    const markup = panel.elements.stateContent.innerHTML;
    expect(markup.indexOf('</details>')).toBeLessThan(markup.indexOf('class="runtime-state-inspect"'));
    expect(markup).not.toContain('<details class="runtime-state-component-details" open');
    panel.elements.stateContent.dispatch('click', { target: inspectButton });

    const stateDetailTab = detailTabs.find(tab => tab.dataset['detail'] === 'state');
    expect(panel.state.activeSection).toBe('elements');
    expect(panel.state.activeDetail).toBe('state');
    expect(panel.state.selectedNodeId).toBe(node.id);
    expect(stateDetailTab?.focusCount).toBe(1);
  });

  it('suppresses State polling while a component property editor is focused or pending', () => {
    const harness = createRuntimeHarness();
    const { fetchPaths, panel } = harness;
    panel.state.activeSection = 'state';
    panel.state.target = { capabilities: ['components', 'snapshot'], id: 'runtime-state-test' };
    panel.startRefreshTimer();

    panel.state.componentPropertyEdit.focused = true;
    panel.setActiveSection('state');
    harness.runIntervals();
    panel.state.componentPropertyEdit.focused = false;
    panel.state.componentPropertyEdit.pending = true;
    panel.setActiveSection('state');
    harness.runIntervals();
    expect(fetchPaths.filter(pathname => pathname === '/api/devtools/snapshot').length).toBe(0);

    panel.state.componentPropertyEdit.pending = false;
    harness.runIntervals();
    expect(fetchPaths.filter(pathname => pathname === '/api/devtools/snapshot').length).toBe(1);
  });

  it('clears State search and expansions on target clear and inspected-page reconnect', async () => {
    const directHarness = createRuntimeHarness();
    const directRuntimeState = directHarness.panel.state.runtimeState;
    directRuntimeState.search = 'stale';
    directRuntimeState.expandedComponents.add('component');
    directRuntimeState.expandedInspectorValues.add('inspector');
    directRuntimeState.expandedMainValues.add('main');
    directHarness.panel.elements.stateFilter.value = 'stale';

    directHarness.panel.clearTargetPresentation('cleared');

    expect(directRuntimeState.search).toBe('');
    expect(directRuntimeState.expandedComponents.size).toBe(0);
    expect(directRuntimeState.expandedInspectorValues.size).toBe(0);
    expect(directRuntimeState.expandedMainValues.size).toBe(0);
    expect(directHarness.panel.elements.stateFilter.value).toBe('');

    const reconnectHarness = createRuntimeHarness(
      '?inspectedUrl=http%3A%2F%2F127.0.0.1%3A54321&targetNonce=runtime-state-nonce',
    );
    reconnectHarness.panel.state.target = {
      capabilities: ['components', 'snapshot'],
      id: 'old-target',
      sessionId: 'old-session',
    };
    reconnectHarness.panel.state.runtimeState.search = 'stale';
    reconnectHarness.panel.state.runtimeState.expandedComponents.add('component');
    reconnectHarness.setTargetPayload({
      applicationUrl: 'http://127.0.0.1:54321',
      capabilities: ['components', 'snapshot'],
      debuggingPort: 54_321,
      id: 'new-target',
      name: 'New target',
      sessionId: 'new-session',
    });

    await reconnectHarness.panel.connectToInspectedPage();
    await flushPromises();

    expect(reconnectHarness.panel.state.runtimeState.search).toBe('');
    expect(reconnectHarness.panel.state.runtimeState.expandedComponents.size).toBe(0);
    expect(reconnectHarness.panel.elements.stateFilter.value).toBe('');
  });

  it('declares State tabs, live summary semantics, and tabpanel relationships in static markup', () => {
    const html = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'devtools-panel.html'), 'utf8');

    expect(html).toContain('id="stateTab"');
    expect(html).toContain('aria-controls="stateSection"');
    expect(html).toContain('id="stateDetailTab"');
    expect(html).toContain('aria-controls="inspector"');
    expect(html).toContain('id="stateSummary" class="runtime-state-summary" role="status" aria-live="polite"');
    expect(html).toContain('id="inspector" class="inspector" role="tabpanel"');
  });
});
