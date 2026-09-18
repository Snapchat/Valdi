import 'jasmine/src/jasmine';
import { IComponent } from '../src/IComponent';
import { IRenderedElement } from '../src/IRenderedElement';
import { IRenderedVirtualNode } from '../src/IRenderedVirtualNode';
import {
  fromRenderedVirtualNode,
  fromRenderedVirtualNodeWithOptions,
  IRenderedVirtualNodeData,
} from '../src/IRenderedVirtualNodeData';
import { Messages } from '../src/debugging/Messages';

interface IDebuggableTestComponent extends IComponent {
  readonly state?: unknown;
}

function createComponentNode(viewModel: unknown, state: unknown): IRenderedVirtualNode {
  const component = (state === undefined ? { viewModel } : { viewModel, state }) as unknown as IDebuggableTestComponent;

  return {
    key: 'component',
    uniqueId: 'component',
    parent: undefined,
    element: undefined,
    component,
    children: [],
    parentIndex: 0,
    renderer: component.renderer,
  };
}

function createDetailedData(node: IRenderedVirtualNode): IRenderedVirtualNodeData {
  return fromRenderedVirtualNodeWithOptions(node, {
    includeAttributes: true,
    includeComponentData: true,
    onCreate: undefined,
  });
}

function collectComponentDebugDataLength(node: IRenderedVirtualNodeData): number {
  let length = (node.component?.viewModel?.length ?? 0) + (node.component?.state?.length ?? 0);
  for (const child of node.children ?? []) {
    length += collectComponentDebugDataLength(child);
  }
  return length;
}

function hasOmittedComponentDebugData(node: IRenderedVirtualNodeData): boolean {
  return node.component?.debugDataOmitted === true || (node.children ?? []).some(hasOmittedComponentDebugData);
}

function createElementNode(): IRenderedVirtualNode {
  const element = {
    id: 42,
    tag: 'label',
    frame: { x: 1, y: 2, width: 30, height: 40 },
    getAttributeNames: () => ['value'],
    getAttribute: () => 'Hello',
  } as unknown as IRenderedElement;

  return {
    key: 'element',
    uniqueId: 'element',
    parent: undefined,
    element,
    component: undefined,
    children: [],
    parentIndex: 0,
    renderer: element.renderer,
  };
}

describe('IRenderedVirtualNodeData', () => {
  it('exports component ViewModel and state for detailed debugger snapshots', () => {
    const data = createDetailedData(createComponentNode({ title: 'Debugger' }, { selectedNodeId: 42 }));

    expect(data.component?.viewModel).toContain('title: "Debugger"');
    expect(data.component?.state).toContain('selectedNodeId: 42');
  });

  it('omits state when a component has no state', () => {
    const data = createDetailedData(createComponentNode({ title: 'Stateless' }, undefined));

    expect(data.component?.viewModel).toContain('title: "Stateless"');
    expect(data.component?.state).toBeUndefined();
  });

  it('serializes circular and callable debug values safely', () => {
    function callback(): void {}

    const viewModel: Record<string, unknown> = { callback };
    viewModel['self'] = viewModel;

    const data = createDetailedData(createComponentNode(viewModel, {}));

    expect(data.component?.viewModel).toContain('<function callback/>');
    expect(data.component?.viewModel).toContain('<circular object/>');
  });

  it('does not invoke component accessors or custom debug representations', () => {
    let invocationCount = 0;
    const viewModel = {
      get secret(): string {
        invocationCount += 1;
        return 'secret';
      },
      toConsoleRepresentation(): string {
        invocationCount += 1;
        return 'custom';
      },
      toString(): string {
        invocationCount += 1;
        return 'custom';
      },
    };

    const data = createDetailedData(createComponentNode(viewModel, {}));

    expect(invocationCount).toBe(0);
    expect(data.component?.viewModel).toContain('secret: <accessor/>');
    expect(data.component?.viewModel).toContain('toConsoleRepresentation: <function toConsoleRepresentation/>');
    expect(data.component?.viewModel).toContain('toString: <function toString/>');
  });

  it('does not invoke accessors on the component itself', () => {
    let invocationCount = 0;
    const node = createComponentNode({}, {});
    const component = node.component as object;
    Object.defineProperties(component, {
      viewModel: {
        enumerable: true,
        get: () => {
          invocationCount += 1;
          return { secret: 'view-model' };
        },
      },
      state: {
        enumerable: true,
        get: () => {
          invocationCount += 1;
          return { secret: 'state' };
        },
      },
    });

    const data = createDetailedData(node);

    expect(invocationCount).toBe(0);
    expect(data.component?.viewModel).toBe('<accessor/>');
    expect(data.component?.state).toBe('<accessor/>');
  });

  it('summarizes array buffer views without enumerating their indexes', () => {
    const data = createDetailedData(createComponentNode({ bytes: new Uint8Array(1_000_000) }, {}));

    expect(data.component?.viewModel).toContain('bytes: <array buffer view/>');
    expect(data.component?.viewModel?.length).toBeLessThan(100);
  });

  it('does not count inherited enumerable keys against the object limit', () => {
    const prototype: Record<string, unknown> = {};
    for (let index = 0; index < 100; index++) {
      prototype[`inherited${index}`] = index;
    }
    const viewModel = Object.create(prototype) as Record<string, unknown>;
    viewModel['own'] = 'value';

    const data = createDetailedData(createComponentNode(viewModel, {}));

    expect(data.component?.viewModel).toContain('own: "value"');
    expect(data.component?.viewModel).not.toContain('more properties');
  });

  it('does not invoke overridden Map or Set size getters', () => {
    let invocationCount = 0;
    const map = new Map<string, string>([['key', 'value']]);
    const set = new Set<string>(['value']);
    for (const collection of [map, set]) {
      Object.defineProperty(collection, 'size', {
        get: () => {
          invocationCount += 1;
          return 1;
        },
      });
    }

    const data = createDetailedData(createComponentNode({ map, set }, {}));

    expect(invocationCount).toBe(0);
    expect(data.component?.viewModel).toContain('Map{');
    expect(data.component?.viewModel).toContain('Set(');
  });

  it('replaces values at the maximum component debug depth with an omission marker', () => {
    const data = createDetailedData(
      createComponentNode(
        {
          level1: { level2: { level3: { level4: 'too-deep' } } },
          sibling: 'visible',
        },
        {},
      ),
    );

    expect(data.component?.viewModel).toContain('level4: ...');
    expect(data.component?.viewModel).not.toContain('too-deep');
    expect(data.component?.viewModel).toContain('sibling: "visible"');
  });

  it('limits arrays and objects to 50 serialized items with omission markers', () => {
    const items = Array.from({ length: 52 }, (_value, index) => `item-${index}`);
    const properties: Record<string, number> = {};
    for (let index = 0; index < 52; index++) {
      properties[`property${index}`] = index;
    }

    const data = createDetailedData(createComponentNode({ items, properties }, {}));

    expect(data.component?.viewModel).toContain('"item-49"');
    expect(data.component?.viewModel).not.toContain('"item-50"');
    expect(data.component?.viewModel).toContain('... 2 more item(s) ...');
    expect(data.component?.viewModel).toContain('property49: 49');
    expect(data.component?.viewModel).not.toContain('property50: 50');
    expect(data.component?.viewModel).toContain('... more properties ...');
  });

  it('truncates one serialized component field to its character cap', () => {
    const data = createDetailedData(createComponentNode('x'.repeat(70_000), {}));

    expect(data.component?.viewModel?.endsWith('\n... <truncated>')).toBeTrue();
    expect(data.component?.viewModel?.length).toBe(65_536);
    expect(data.component?.debugDataOmitted).toBeTrue();
  });

  it('does not expose component values through the compatibility serializer', () => {
    const data = fromRenderedVirtualNode(
      createComponentNode({ token: 'not-exported' }, { secret: 'not-exported' }),
      true,
    );

    expect(data.component).toEqual({});
  });

  it('applies one component debug-data character budget across the tree', () => {
    const root = createComponentNode('root', {});
    for (let index = 0; index < 6; index++) {
      root.children.push(createComponentNode(String(index).repeat(70_000), {}));
    }

    const data = createDetailedData(root);

    expect(collectComponentDebugDataLength(data)).toBeLessThanOrEqual(262_144);
    expect(hasOmittedComponentDebugData(data)).toBeTrue();
  });

  it('preserves existing element snapshot data', () => {
    const data = fromRenderedVirtualNode(createElementNode(), true);

    expect(data).toEqual({
      key: 'element',
      tag: 'label',
      element: {
        id: 42,
        frame: { x: 1, y: 2, width: 30, height: 40 },
        attributes: { value: 'Hello' },
      },
      component: undefined,
      children: undefined,
    });
  });

  it('preserves component fields through the daemon message envelope', () => {
    const data = createDetailedData(createComponentNode({ title: 'Wire' }, { attached: true }));
    const message = Messages.parse(1, Messages.getContextTreeResponse('request-1', data));
    const body = message.body as IRenderedVirtualNodeData;

    expect(body.component?.viewModel).toContain('title: "Wire"');
    expect(body.component?.state).toContain('attached: true');
  });
});
