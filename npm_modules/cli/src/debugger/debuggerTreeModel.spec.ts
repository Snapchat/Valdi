import 'jasmine';
import fs from 'node:fs';
import path from 'node:path';
import { Script } from 'node:vm';

interface DebugTreeNode {
  children?: DebugTreeNode[];
  element?: {
    attributes?: Record<string, unknown>;
    id?: number | string;
  };
  id?: number | string;
  key?: string;
  tag: string;
}

interface DebuggerTreeModel {
  attributes(node: DebugTreeNode | null): Record<string, unknown>;
  children(node: DebugTreeNode | null): DebugTreeNode[];
  findNode(root: DebugTreeNode | null, id: number | string): DebugTreeNode | null;
  formatValue(value: unknown, spacing: number): string;
  hasChildren(node: DebugTreeNode | null): boolean;
  id(node: DebugTreeNode | null): string;
  pathToNode(root: DebugTreeNode | null, id: number | string): DebugTreeNode[];
  projectSnapshot(snapshot: Record<string, unknown>): Record<string, unknown>;
  projectTree(root: DebugTreeNode | null): {
    complete: boolean;
    nodeCount: number;
    nodes: Array<{
      childIndexes: number[];
      data: Record<string, unknown>;
      depth: number;
      index: number;
      parentIndex: number | null;
      sourceChildIndex: number | null;
    }>;
  };
  projectValue(value: unknown): { complete: boolean; value: unknown };
  restoreTree(value: unknown): DebugTreeNode | null;
  stringifyValue(value: unknown, spacing: number): string;
  walk(
    root: DebugTreeNode | undefined,
    visitor: (
      node: DebugTreeNode,
      ancestors: DebugTreeNode[],
      depth: number,
      sourceChildIndex: number | null,
    ) => boolean | void,
    ancestors: DebugTreeNode[],
    depth: number,
  ): boolean;
  walkVisible(
    root: DebugTreeNode | undefined,
    visitor: (node: DebugTreeNode, ancestors: DebugTreeNode[], depth: number) => boolean | void,
    isExpanded: (node: DebugTreeNode) => boolean,
    ancestors: DebugTreeNode[],
    depth: number,
  ): boolean;
}

interface DebuggerModelHarness {
  decorateSnapshot(snapshot: Record<string, unknown>): Record<string, unknown>;
  normalizeLabelValue(value: unknown): string;
}

interface MockElement {
  checked: boolean;
  className: string;
  classList: { toggle(): void };
  contentWindow: null;
  dataset: Record<string, string>;
  innerHTML: string;
  scrollHeight: number;
  scrollTop: number;
  textContent: string;
  title: string;
  value: string;
  addEventListener(): void;
  contains(): boolean;
  focus(): void;
  removeAttribute(): void;
  setAttribute(): void;
}

function createMockElement(): MockElement {
  return {
    checked: false,
    className: '',
    classList: { toggle: () => {} },
    contentWindow: null,
    dataset: {},
    innerHTML: '',
    scrollHeight: 0,
    scrollTop: 0,
    textContent: '',
    title: '',
    value: '',
    addEventListener: () => {},
    contains: () => false,
    focus: () => {},
    removeAttribute: () => {},
    setAttribute: () => {},
  };
}

function loadDebuggerModel(): {
  model: DebuggerModelHarness;
  state: { geometry: { map: { size: number } } | null };
} {
  const treeSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'debugger-tree-model.js'), 'utf8');
  const modelSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'debugger-model.js'), 'utf8');
  const state = { expandedNodeIds: new Set<string>(), geometry: null };
  const model = new Script(
    `${treeSource}\n${modelSource}\n({ decorateSnapshot, normalizeLabelValue })`,
  ).runInNewContext({
    emptyTarget: {},
    state,
  }) as DebuggerModelHarness;
  return { model, state };
}

function loadDevToolsPanel(): {
  elements: { tree: MockElement };
  renderValue(value: unknown): string;
  renderTree(): void;
  revealPath(id: string): void;
  selectedNodeProjectionJson(): string | null;
  state: {
    expandedNodeIds: Set<string>;
    search: string;
    selectedNodeId: string | null;
    snapshot: { tree: DebugTreeNode } | null;
  };
  visibleSearchIds(root: DebugTreeNode, search: string): Set<string>;
} {
  const treeSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'debugger-tree-model.js'), 'utf8');
  const panelSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'devtools-panel.js'), 'utf8');
  const bootIndex = panelSource.indexOf("\napplyTheme(query.get('theme'));");
  if (bootIndex < 0) throw new Error('Could not isolate the DevTools panel definitions.');
  const mockElements = new Map<string, MockElement>();
  const getElement = (id: string): MockElement => {
    let element = mockElements.get(id);
    if (!element) {
      element = createMockElement();
      mockElements.set(id, element);
    }
    return element;
  };
  const panel = new Script(
    `${treeSource}\n${panelSource.slice(0, bootIndex)}\n({ elements, renderTree, renderValue, revealPath, selectedNodeProjectionJson, state, visibleSearchIds })`,
  ).runInNewContext({
    URL,
    URLSearchParams,
    document: {
      documentElement: { dataset: {} },
      getElementById: getElement,
      querySelectorAll: () => [],
    },
    window: { location: { origin: 'http://127.0.0.1:8765', search: '' } },
  }) as {
    elements: { tree: MockElement };
    renderValue(value: unknown): string;
    renderTree(): void;
    revealPath(id: string): void;
    selectedNodeProjectionJson(): string | null;
    state: {
      expandedNodeIds: Set<string>;
      search: string;
      selectedNodeId: string | null;
      snapshot: { tree: DebugTreeNode } | null;
    };
    visibleSearchIds(root: DebugTreeNode, search: string): Set<string>;
  };
  return panel;
}

function loadPreviewAppender(): {
  appendPreviewNode(node: DebugTreeNode, parent: PreviewElement): void;
  createdElements: PreviewElement[];
  previewValue(value: unknown): string;
} {
  const treeSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'debugger-tree-model.js'), 'utf8');
  const previewSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'debugger-preview-html.js'), 'utf8');
  const createdElements: PreviewElement[] = [];
  const document = {
    createElement: (tag: string): PreviewElement => {
      const element = createPreviewElement(tag);
      createdElements.push(element);
      return element;
    },
  };
  const harness = new Script(`${treeSource}\n${previewSource}\n({ appendPreviewNode, previewValue })`).runInNewContext({
    describeOverlayNode: () => '',
    document,
    getElementIdForNode: (node: DebugTreeNode) => node.element?.id ?? null,
    getNodeAttributes: (node: DebugTreeNode) => node.element?.attributes ?? {},
    getNodeId: (node: DebugTreeNode) => String(node.id ?? node.element?.id ?? node.tag),
    isInteractiveNode: () => false,
    normalizeBounds: (bounds: unknown) => bounds,
    normalizeLabelValue: String,
  }) as { appendPreviewNode(node: DebugTreeNode, parent: PreviewElement): void; previewValue(value: unknown): string };
  return { ...harness, createdElements };
}

interface PreviewElement {
  appendChild(child: PreviewElement): void;
  children: PreviewElement[];
  classList: { add(...names: string[]): void };
  dataset: Record<string, string>;
  disabled: boolean;
  draggable: boolean;
  parent: PreviewElement | null;
  placeholder: string;
  readOnly: boolean;
  scrollLeft: number;
  scrollTop: number;
  style: Record<string, string>;
  tagName: string;
  textContent: string;
  title: string;
  type: string;
  value: string;
}

function createPreviewElement(tag: string): PreviewElement {
  const element: PreviewElement = {
    appendChild: child => {
      element.children.push(child);
      child.parent = element;
    },
    children: [],
    classList: { add: () => {} },
    dataset: {},
    disabled: false,
    draggable: false,
    parent: null,
    placeholder: '',
    readOnly: false,
    scrollLeft: 0,
    scrollTop: 0,
    style: {},
    tagName: tag.toUpperCase(),
    textContent: '',
    title: '',
    type: '',
    value: '',
  };
  return element;
}

function loadFirstElementDescendant(): (node: DebugTreeNode) => DebugTreeNode | null {
  const treeSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'debugger-tree-model.js'), 'utf8');
  const runtimeSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'debugger-runtime.js'), 'utf8');
  return new Script(`${treeSource}\n${runtimeSource}\nfirstElementDescendant`).runInNewContext() as (
    node: DebugTreeNode,
  ) => DebugTreeNode | null;
}

function loadRawInspectorSerializer(): (node: DebugTreeNode, geometry: unknown, target: unknown) => string {
  const treeSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'debugger-tree-model.js'), 'utf8');
  const renderSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'debugger-render.js'), 'utf8');
  return new Script(`${treeSource}\n${renderSource}\nserializeRawInspectorNode`).runInNewContext() as (
    node: DebugTreeNode,
    geometry: unknown,
    target: unknown,
  ) => string;
}

function loadStandaloneMetadataHarness(): {
  nodeMatchesSearch(node: DebugTreeNode, search: string): boolean;
  payloadToDisplayString(payload: unknown): string;
  renderAttributesTable(attributes: Record<string, unknown>): string;
} {
  const treeSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'debugger-tree-model.js'), 'utf8');
  const renderSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'debugger-render.js'), 'utf8');
  return new Script(
    `${treeSource}\n${renderSource}\n({ nodeMatchesSearch, payloadToDisplayString, renderAttributesTable })`,
  ).runInNewContext({
    escapeHtml: String,
    getNodeAttributes: (node: DebugTreeNode) => node.element?.attributes ?? {},
    getNodeId: (node: DebugTreeNode) => String(node.id ?? node.tag),
  }) as {
    nodeMatchesSearch(node: DebugTreeNode, search: string): boolean;
    payloadToDisplayString(payload: unknown): string;
    renderAttributesTable(attributes: Record<string, unknown>): string;
  };
}

function loadPreviewSnapshotSerializer(snapshot: Record<string, unknown>): () => string {
  const treeSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'debugger-tree-model.js'), 'utf8');
  const runtimeSource = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'debugger-runtime.js'), 'utf8');
  return new Script(`${treeSource}\n${runtimeSource}\npreviewSnapshotProjectionJson`).runInNewContext({
    state: { snapshot },
  }) as () => string;
}

function makeDeepMetadata(depth: number): Record<string, unknown> {
  const root: Record<string, unknown> = { needle: 'metadata-needle' };
  let current = root;
  for (let index = 0; index < depth; index += 1) {
    const child: Record<string, unknown> = { index };
    current['next'] = child;
    current = child;
  }
  current['cycle'] = root;
  return root;
}

function makeDeepTree(depth: number): { deepest: DebugTreeNode; root: DebugTreeNode } {
  const root: DebugTreeNode = { element: { id: 0 }, id: 0, tag: 'root' };
  let current = root;
  for (let index = 1; index <= depth; index += 1) {
    const child: DebugTreeNode = { element: { id: index }, id: index, tag: 'node' };
    current.children = [child];
    current = child;
  }
  return { deepest: current, root };
}

describe('shared debugger tree model', () => {
  let model: DebuggerTreeModel;

  beforeEach(() => {
    const source = fs.readFileSync(path.resolve(process.cwd(), 'debugger', 'debugger-tree-model.js'), 'utf8');
    model = new Script(`${source}\nvaldiDebuggerTreeModel`).runInNewContext() as DebuggerTreeModel;
  });

  it('uses the same stable identities for runtime, element, and keyed nodes', () => {
    expect(model.id({ id: 12, tag: 'view' })).toBe('12');
    expect(model.id({ element: { id: 'element-4' }, tag: 'label' })).toBe('element-4');
    expect(model.id({ key: 'title', tag: 'label' })).toBe('label:title');
    expect(model.id(null)).toBe('');
  });

  it('shares depth, parent paths, lookup, and attributes across debugger frontends', () => {
    const label: DebugTreeNode = { element: { attributes: { value: 'Hello' } }, id: 3, tag: 'label' };
    const container: DebugTreeNode = { children: [label], id: 2, tag: 'view' };
    const root: DebugTreeNode = { children: [container], id: 1, tag: 'view' };
    const visited: Array<{ ancestors: string[]; depth: number; id: string }> = [];

    model.walk(
      root,
      (node, ancestors, depth) => {
        visited.push({ ancestors: ancestors.map(ancestor => model.id(ancestor)), depth, id: model.id(node) });
      },
      [],
      0,
    );

    expect(visited).toEqual([
      { ancestors: [], depth: 0, id: '1' },
      { ancestors: ['1'], depth: 1, id: '2' },
      { ancestors: ['1', '2'], depth: 2, id: '3' },
    ]);
    expect(model.findNode(root, 3)).toBe(label);
    expect(model.pathToNode(root, 3)).toEqual([root, container, label]);
    expect(model.attributes(label)).toEqual({ value: 'Hello' });
  });

  it('returns stable empty values for unavailable targets', () => {
    expect(model.findNode(null, 'missing')).toBeNull();
    expect(model.pathToNode(null, 'missing')).toEqual([]);
    expect(model.attributes(null)).toEqual({});
  });

  it('walks deep trees iteratively without overflowing the call stack', () => {
    const root: DebugTreeNode = { id: 0, tag: 'root' };
    let current = root;
    const depth = 20_000;
    for (let index = 1; index <= depth; index += 1) {
      const child: DebugTreeNode = { id: index, tag: 'node' };
      current.children = [child];
      current = child;
    }

    let visited = 0;
    expect(
      model.walk(
        root,
        (_node, ancestors, currentDepth) => {
          expect(ancestors.length).toBe(currentDepth);
          visited += 1;
        },
        [],
        0,
      ),
    ).toBeTrue();
    expect(visited).toBe(depth + 1);
    expect(model.findNode(root, depth)).toBe(current);
    expect(model.pathToNode(root, depth).length).toBe(depth + 1);
  });

  it('preserves depth-first render order while visiting cycles and shared nodes once', () => {
    const shared: DebugTreeNode = { id: 'shared', tag: 'shared' };
    const first: DebugTreeNode = { children: [shared], id: 'first', tag: 'first' };
    const second: DebugTreeNode = { children: [shared], id: 'second', tag: 'second' };
    const root: DebugTreeNode = { children: [first, second], id: 'root', tag: 'root' };
    shared.children = [root];
    const visited: Array<{ ancestors: string[]; id: string }> = [];

    model.walk(
      root,
      (node, ancestors) => {
        visited.push({ ancestors: ancestors.map(ancestor => model.id(ancestor)), id: model.id(node) });
      },
      [],
      0,
    );

    expect(visited).toEqual([
      { ancestors: [], id: 'root' },
      { ancestors: ['root'], id: 'first' },
      { ancestors: ['root', 'first'], id: 'shared' },
      { ancestors: ['root'], id: 'second' },
    ]);
  });

  it('assigns a shared node to the first parent actually reached in preorder', () => {
    const shared: DebugTreeNode = { id: 'shared', tag: 'shared' };
    const first: DebugTreeNode = { children: [shared], id: 'first', tag: 'first' };
    const root: DebugTreeNode = { children: [first, shared], id: 'root', tag: 'root' };
    const visited: Array<{ ancestors: string[]; depth: number; id: string }> = [];

    model.walk(
      root,
      (node, ancestors, depth) => {
        visited.push({ ancestors: ancestors.map(ancestor => model.id(ancestor)), depth, id: model.id(node) });
      },
      [],
      0,
    );
    const projection = model.projectTree(root);

    expect(visited).toEqual([
      { ancestors: [], depth: 0, id: 'root' },
      { ancestors: ['root'], depth: 1, id: 'first' },
      { ancestors: ['root', 'first'], depth: 2, id: 'shared' },
    ]);
    expect(model.pathToNode(root, 'shared')).toEqual([root, first, shared]);
    expect(projection.nodes[0]?.childIndexes).toEqual([1]);
    expect(projection.nodes[1]?.childIndexes).toEqual([2]);
    expect(projection.nodes[2]).toEqual(jasmine.objectContaining({ depth: 2, parentIndex: 1 }));

    const panel = loadDevToolsPanel();
    panel.state.snapshot = { tree: root };
    panel.state.expandedNodeIds = new Set(['root', 'first']);
    panel.state.selectedNodeId = 'shared';
    panel.renderTree();
    expect(panel.elements.tree.innerHTML).toContain('data-node-id="shared" role="treeitem" aria-level="3"');
  });

  it('stops walking as soon as a visitor, find, or path lookup succeeds', () => {
    const target: DebugTreeNode = { id: 'target', tag: 'target' };
    const unreachable: DebugTreeNode = { tag: 'unreachable' };
    let unreachableIdReads = 0;
    Object.defineProperty(unreachable, 'id', {
      get: () => {
        unreachableIdReads += 1;
        return 'unreachable';
      },
    });
    const root: DebugTreeNode = { children: [target, unreachable], id: 'root', tag: 'root' };
    const visited: string[] = [];

    expect(
      model.walk(
        root,
        node => {
          visited.push(model.id(node));
          return node !== target;
        },
        [],
        0,
      ),
    ).toBeFalse();
    expect(visited).toEqual(['root', 'target']);
    expect(model.findNode(root, 'target')).toBe(target);
    expect(model.pathToNode(root, 'target')).toEqual([root, target]);
    expect(unreachableIdReads).toBe(0);
  });

  it('bounds traversal before an untrusted tree can grow work without limit', () => {
    const root: DebugTreeNode = {
      children: Array.from({ length: 26_000 }, (_, index) => ({ id: index + 1, tag: 'child' })),
      id: 0,
      tag: 'root',
    };
    let visited = 0;

    expect(
      model.walk(
        root,
        () => {
          visited += 1;
        },
        [],
        0,
      ),
    ).toBeFalse();
    expect(visited).toBe(25_000);
  });

  it('decorates, bounds, and computes geometry for a 20k-deep cyclic snapshot iteratively', () => {
    const root: DebugTreeNode = { id: 0, tag: 'root' };
    let current = root;
    const depth = 20_000;
    for (let index = 1; index <= depth; index += 1) {
      const child: DebugTreeNode = { id: index, tag: 'node' };
      current.children = [child];
      current = child;
    }
    current.children = [root];
    const { model: debuggerModel, state } = loadDebuggerModel();
    const snapshot: Record<string, unknown> = { tree: root };

    expect(() => debuggerModel.decorateSnapshot(snapshot)).not.toThrow();
    expect(state.geometry?.map.size).toBe(depth + 1);
    expect(current.element).toBeUndefined();
    expect((current as DebugTreeNode & { bounds?: unknown }).bounds).toBeDefined();
  });

  it('renders a 20k-deep cyclic DevTools tree in preorder without recursive overflow', () => {
    const root: DebugTreeNode = { id: 0, tag: 'root' };
    let current = root;
    const depth = 20_000;
    for (let index = 1; index <= depth; index += 1) {
      const child: DebugTreeNode = { id: index, tag: 'node' };
      current.children = [child];
      current = child;
    }
    current.children = [root];
    const panel = loadDevToolsPanel();
    panel.state.snapshot = { tree: root };
    panel.state.search = 'node';
    panel.state.selectedNodeId = String(depth);

    expect(() => panel.renderTree()).not.toThrow();
    expect((panel.elements.tree.innerHTML.match(/class="tree-row/g) ?? []).length).toBe(depth + 1);
    expect(panel.elements.tree.innerHTML.indexOf('data-node-id="0"')).toBeLessThan(
      panel.elements.tree.innerHTML.indexOf('data-node-id="20000"'),
    );
  });

  it('reveals and renders a 20k-deep path linearly without relying on search expansion', () => {
    const { deepest, root } = makeDeepTree(20_000);
    const panel = loadDevToolsPanel();
    panel.state.snapshot = { tree: root };
    panel.state.search = '';
    panel.state.selectedNodeId = String(deepest.id);

    const startedAt = performance.now();
    panel.revealPath(String(deepest.id));
    panel.renderTree();
    const elapsedMilliseconds = performance.now() - startedAt;

    expect(panel.state.expandedNodeIds.size).toBe(20_000);
    expect((panel.elements.tree.innerHTML.match(/class="tree-row/g) ?? []).length).toBe(20_001);
    expect(elapsedMilliseconds).toBeLessThan(4000);
  });

  it('renders shared DevTools nodes once at their first preorder position and caps visible rows', () => {
    const shared: DebugTreeNode = { id: 'shared', tag: 'shared' };
    const first: DebugTreeNode = { children: [shared], id: 'first', tag: 'first' };
    const second: DebugTreeNode = { children: [shared], id: 'second', tag: 'second' };
    const root: DebugTreeNode = { children: [first, second], id: 'root', tag: 'root' };
    const panel = loadDevToolsPanel();
    panel.state.snapshot = { tree: root };
    panel.state.expandedNodeIds = new Set(['root', 'first', 'second']);
    panel.renderTree();

    const markup = panel.elements.tree.innerHTML;
    expect((markup.match(/data-node-id="shared"/g) ?? []).length).toBe(1);
    expect(markup.indexOf('data-node-id="first"')).toBeLessThan(markup.indexOf('data-node-id="shared"'));
    expect(markup.indexOf('data-node-id="shared"')).toBeLessThan(markup.indexOf('data-node-id="second"'));

    root.children = Array.from({ length: 26_000 }, (_, index) => ({ id: `wide-${index}`, tag: 'node' }));
    panel.state.expandedNodeIds = new Set(['root']);
    panel.renderTree();
    expect((panel.elements.tree.innerHTML.match(/class="tree-row/g) ?? []).length).toBe(25_000);
  });

  it('builds the HTML preview iteratively for deep cyclic/shared graphs and caps rendered nodes', () => {
    const { deepest, root } = makeDeepTree(20_000);
    const firstChild = root.children![0]!;
    const shared: DebugTreeNode = { element: { id: 'shared' }, id: 'shared', tag: 'shared' };
    root.children = [firstChild, shared];
    deepest.children = [shared, root];
    shared.children = [root];
    const preview = loadPreviewAppender();
    const previewRoot = createPreviewElement('main');

    expect(() => preview.appendPreviewNode(root, previewRoot)).not.toThrow();
    expect(preview.createdElements.length).toBe(20_002);
    expect(preview.createdElements.filter(element => element.dataset['previewNodeId'] === 'shared').length).toBe(1);

    const cappedRoot: DebugTreeNode = {
      children: Array.from({ length: 26_000 }, (_, index) => ({ element: { id: index }, id: index, tag: 'node' })),
      element: { id: 'capped-root' },
      id: 'capped-root',
      tag: 'root',
    };
    const cappedPreview = loadPreviewAppender();
    cappedPreview.appendPreviewNode(cappedRoot, createPreviewElement('main'));
    expect(cappedPreview.createdElements.length).toBe(25_000);
  });

  it('finds element descendants iteratively through deep cyclic/shared graphs and respects the cap', () => {
    const findFirstElement = loadFirstElementDescendant();
    const root: DebugTreeNode = { id: 0, tag: 'root' };
    let current = root;
    for (let index = 1; index <= 20_000; index += 1) {
      const child: DebugTreeNode = { id: index, tag: 'node' };
      current.children = [child];
      current = child;
    }
    const shared: DebugTreeNode = { element: { id: 'target' }, id: 'shared', tag: 'shared' };
    current.children = [root, shared];
    shared.children = [root];
    expect(findFirstElement(root)).toBe(shared);

    const first: DebugTreeNode = { children: [shared], id: 'first', tag: 'first' };
    const second: DebugTreeNode = { children: [shared], id: 'second', tag: 'second' };
    expect(findFirstElement({ children: [first, second], id: 'shared-root', tag: 'root' })).toBe(shared);

    const cappedRoot: DebugTreeNode = {
      children: Array.from({ length: 25_000 }, (_, index) =>
        index === 24_999
          ? { element: { id: 'past-cap' }, id: 'past-cap', tag: 'target' }
          : { id: `node-${index}`, tag: 'node' },
      ),
      id: 'capped-root',
      tag: 'root',
    };
    expect(findFirstElement(cappedRoot)).toBeNull();
  });

  it('copies a flat bounded projection for deep cyclic/shared selected nodes', () => {
    const { deepest, root } = makeDeepTree(20_000);
    const firstChild = root.children![0]!;
    const shared: DebugTreeNode = { element: { id: 'shared' }, id: 'shared', tag: 'shared' };
    root.children = [firstChild, shared];
    deepest.children = [shared, root];
    shared.children = [root];
    const panel = loadDevToolsPanel();
    panel.state.snapshot = { tree: root };
    panel.state.selectedNodeId = '0';

    const projection = JSON.parse(panel.selectedNodeProjectionJson()!) as {
      complete: boolean;
      nodeCount: number;
      nodes: Array<{ childIndexes: number[]; data: { id: number | string } }>;
    };
    expect(projection.complete).toBeTrue();
    expect(projection.nodeCount).toBe(20_002);
    expect(projection.nodes.filter(node => node.data.id === 'shared').length).toBe(1);

    const cappedRoot: DebugTreeNode = {
      children: Array.from({ length: 26_000 }, (_, index) => ({ id: index, tag: 'node' })),
      id: 'capped-root',
      tag: 'root',
    };
    panel.state.snapshot = { tree: cappedRoot };
    panel.state.selectedNodeId = 'capped-root';
    const cappedProjection = JSON.parse(panel.selectedNodeProjectionJson()!) as {
      complete: boolean;
      nodeCount: number;
    };
    expect(cappedProjection.complete).toBeFalse();
    expect(cappedProjection.nodeCount).toBe(25_000);
  });

  it('serializes raw inspector data as a flat bounded projection for deep cyclic/shared nodes', () => {
    const { deepest, root } = makeDeepTree(20_000);
    const firstChild = root.children![0]!;
    const shared: DebugTreeNode = { element: { id: 'shared' }, id: 'shared', tag: 'shared' };
    root.children = [firstChild, shared];
    deepest.children = [shared, root];
    shared.children = [root];
    const geometry: Record<string, unknown> = { bounds: { height: 10, width: 20 } };
    geometry['self'] = geometry;
    const target: Record<string, unknown> = { geometry };
    const serialize = loadRawInspectorSerializer();

    const payload = JSON.parse(serialize(root, geometry, target)) as {
      node: { complete: boolean; nodeCount: number; nodes: Array<{ data: { id: number | string } }> };
      projectionComplete: boolean;
    };
    expect(payload.projectionComplete).toBeTrue();
    expect(payload.node.nodeCount).toBe(20_002);
    expect(payload.node.nodes.filter(node => node.data.id === 'shared').length).toBe(1);

    const cappedRoot: DebugTreeNode = {
      children: Array.from({ length: 26_000 }, (_, index) => ({ id: index, tag: 'node' })),
      id: 'capped-root',
      tag: 'root',
    };
    const cappedPayload = JSON.parse(serialize(cappedRoot, {}, {})) as {
      node: { complete: boolean; nodeCount: number };
      projectionComplete: boolean;
    };
    expect(cappedPayload.node.complete).toBeFalse();
    expect(cappedPayload.node.nodeCount).toBe(25_000);
    expect(cappedPayload.projectionComplete).toBeFalse();
  });

  it('routes every standalone and DevTools metadata surface through bounded cycle-safe projection', () => {
    const metadata = makeDeepMetadata(20_000);
    metadata['oversized'] = Array.from({ length: 260_000 }, (_, index) => index);
    const node: DebugTreeNode = {
      element: { attributes: { metadata } },
      id: 'metadata-node',
      tag: 'view',
    };
    const { model: debuggerModel } = loadDebuggerModel();
    const standalone = loadStandaloneMetadataHarness();
    const panel = loadDevToolsPanel();

    expect(() => debuggerModel.normalizeLabelValue(metadata)).not.toThrow();
    expect(() => standalone.nodeMatchesSearch(node, 'metadata-needle')).not.toThrow();
    expect(standalone.nodeMatchesSearch(node, 'metadata-needle')).toBeTrue();
    expect(() => standalone.payloadToDisplayString(metadata)).not.toThrow();
    expect(() => panel.visibleSearchIds(node, 'metadata-needle')).not.toThrow();
    expect(Array.from(panel.visibleSearchIds(node, 'metadata-needle'))).toEqual(['metadata-node']);
    const renderedValue = panel.renderValue(metadata);
    expect(renderedValue.length).toBeLessThan(500);

    const { deepest, root } = makeDeepTree(20_000);
    deepest.children = [root];
    const serializePreview = loadPreviewSnapshotSerializer({ metadata, tree: root });
    const previewProjection = JSON.parse(serializePreview()) as {
      projectionComplete: boolean;
      tree: { nodeCount: number };
    };
    expect(previewProjection.projectionComplete).toBeFalse();
    expect(previewProjection.tree.nodeCount).toBe(20_001);
  });

  it('caps direct strings at every label, preview, payload, and attribute surface', () => {
    const oversized = 'x'.repeat(60_000);
    const projection = model.projectValue(oversized);
    const { model: debuggerModel } = loadDebuggerModel();
    const preview = loadPreviewAppender();
    const standalone = loadStandaloneMetadataHarness();
    const panel = loadDevToolsPanel();

    expect(projection.complete).toBeFalse();
    expect((projection.value as string).length).toBe(50_000);
    expect((projection.value as string).endsWith('…[truncated]')).toBeTrue();
    expect(debuggerModel.normalizeLabelValue(oversized).length).toBe(50_000);
    expect(preview.previewValue(oversized).length).toBe(50_000);
    preview.appendPreviewNode(
      { element: { attributes: { value: oversized }, id: 'oversized-label' }, id: 'oversized-label', tag: 'label' },
      createPreviewElement('main'),
    );
    expect(preview.createdElements[0]?.textContent.length).toBe(50_000);
    expect(standalone.payloadToDisplayString(oversized).length).toBe(50_000);
    const attributes = standalone.renderAttributesTable({ title: oversized });
    expect(attributes.length).toBeLessThan(50_100);
    expect(attributes).toContain('…[truncated]');
    expect(panel.renderValue(oversized).length).toBeLessThan(400);
  });

  it('projects sparse arrays and sparse children by bounded descriptors without invoking accessors', () => {
    const sparse = [] as unknown as unknown[] & Record<string, unknown>;
    sparse[10_000_000] = 'far-value';
    sparse['custom'] = 'custom-value';
    let getterCalls = 0;
    let proxyGets = 0;
    sparse['proxied'] = new Proxy(
      { safe: 'proxy-value' },
      {
        get: () => {
          proxyGets += 1;
          throw new Error('projection must not read through a proxy');
        },
      },
    );
    Object.defineProperty(sparse, 'accessor', {
      enumerable: true,
      get: () => {
        getterCalls += 1;
        return 'unsafe';
      },
    });

    const startedAt = performance.now();
    const projection = model.projectValue(sparse);
    const serialized = JSON.stringify(projection.value);
    const elapsedMilliseconds = performance.now() - startedAt;
    const value = projection.value as {
      $entries: Array<Record<string, unknown>>;
      $length: number;
      $truncated: string;
      $type: string;
    };

    expect(projection.complete).toBeFalse();
    expect(value.$type).toBe('array');
    expect(value.$length).toBe(10_000_001);
    expect(value.$truncated).toBe('sparse-array');
    expect(value.$entries).toContain(jasmine.objectContaining({ $index: 10_000_000, value: 'far-value' }));
    expect(value.$entries).toContain(jasmine.objectContaining({ $key: 'custom', value: 'custom-value' }));
    expect(value.$entries).toContain(
      jasmine.objectContaining({
        $key: 'accessor',
        value: jasmine.objectContaining({ $at: '$.accessor', $truncated: 'accessor' }),
      }),
    );
    expect(getterCalls).toBe(0);
    expect(proxyGets).toBe(0);
    expect(serialized.length).toBeLessThan(1000);
    expect(elapsedMilliseconds).toBeLessThan(1000);

    const sparseChildren: DebugTreeNode[] = [];
    const distantChild: DebugTreeNode = { id: 'distant', tag: 'child' };
    sparseChildren[10_000_000] = distantChild;
    const root: DebugTreeNode = { children: sparseChildren, id: 'root', tag: 'root' };
    const visited: string[] = [];
    const sourceIndexes: Array<number | null> = [];
    expect(
      model.walk(
        root,
        (node, _ancestors, _depth, sourceChildIndex) => {
          visited.push(model.id(node));
          sourceIndexes.push(sourceChildIndex);
        },
        [],
        0,
      ),
    ).toBeFalse();
    expect(visited).toEqual(['root', 'distant']);
    expect(sourceIndexes).toEqual([null, 10_000_000]);
    const treeProjection = model.projectTree(root);
    expect(treeProjection.complete).toBeFalse();
    expect(treeProjection.nodes[1]?.sourceChildIndex).toBe(10_000_000);
  });

  it('preserves array truncation reason and location in serialized projection', () => {
    const projection = model.projectValue(Array.from({ length: 250_001 }, () => 'value'));
    const values = projection.value as Array<Record<string, unknown> | string>;
    const marker = values.at(-1) as Record<string, unknown>;

    expect(projection.complete).toBeFalse();
    expect(marker['$truncated']).toBe('value-limit');
    expect(marker['$at']).toBe('$');
    const serializedValues = JSON.parse(JSON.stringify(values)) as Array<Record<string, unknown> | string>;
    expect(serializedValues.at(-1)).toEqual(marker);
  });

  it('preserves own prototype-named keys without inheriting source sentinels in values or tree data', () => {
    const source = JSON.parse('{"safe":1,"__proto__":{"polluted":true}}') as Record<string, unknown>;
    Object.setPrototypeOf(source, { inheritedSentinel: 'must-not-project' });

    const valueProjection = model.projectValue(source);
    const projectedValue = valueProjection.value as Record<string, unknown>;
    const serializedValue = JSON.parse(JSON.stringify(projectedValue)) as Record<string, unknown>;

    expect(valueProjection.complete).toBeTrue();
    expect(Object.getPrototypeOf(projectedValue)).toBeNull();
    expect(Object.prototype.hasOwnProperty.call(projectedValue, '__proto__')).toBeTrue();
    expect(projectedValue['inheritedSentinel']).toBeUndefined();
    expect((projectedValue['__proto__'] as Record<string, unknown>)['polluted']).toBeTrue();
    expect(serializedValue['__proto__']).toEqual({ polluted: true });

    const root = JSON.parse('{"id":"prototype-node","tag":"view","__proto__":{"treePolluted":true}}') as DebugTreeNode;
    Object.setPrototypeOf(root, { inheritedSentinel: 'must-not-project' });
    const treeProjection = model.projectTree(root);
    const data = treeProjection.nodes[0]?.data;
    const serializedTree = JSON.parse(JSON.stringify(treeProjection)) as {
      nodes: Array<{ data: Record<string, unknown> }>;
    };

    expect(treeProjection.complete).toBeTrue();
    expect(Object.getPrototypeOf(data)).toBeNull();
    expect(Object.prototype.hasOwnProperty.call(data, '__proto__')).toBeTrue();
    expect(data?.['inheritedSentinel']).toBeUndefined();
    expect(serializedTree.nodes[0]?.data['__proto__']).toEqual({
      treePolluted: true,
    });
  });

  it('restores a server flat tree iteratively without changing hierarchy behavior', () => {
    const { deepest, root } = makeDeepTree(20_000);
    deepest.children = [root];
    const projection = model.projectTree(root);

    const restored = model.restoreTree(projection);

    expect(restored).not.toBeNull();
    expect(model.findNode(restored, 20_000)?.id).toBe(20_000);
    expect(model.pathToNode(restored, 20_000).length).toBe(20_001);
    const { model: debuggerModel, state } = loadDebuggerModel();
    expect(() => debuggerModel.decorateSnapshot({ tree: projection })).not.toThrow();
    expect(state.geometry?.map.size).toBe(20_001);
  });
});
