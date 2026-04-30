import { Component } from 'valdi_core/src/Component';
import { ComponentPrototype } from 'valdi_core/src/ComponentPrototype';
import { ComponentRef } from 'valdi_core/src/ComponentRef';
import { ElementRef } from 'valdi_core/src/ElementRef';
import { IComponent } from 'valdi_core/src/IComponent';
import { IRenderer } from 'valdi_core/src/IRenderer';
import { IRendererDelegate } from 'valdi_core/src/IRendererDelegate';
import { NodePrototype } from 'valdi_core/src/NodePrototype';
import { Renderer } from 'valdi_core/src/Renderer';
import { RendererEventRecorder } from 'valdi_core/src/debugging/RendererEventRecorder';
import { createReusableCallback } from 'valdi_core/src/utils/Callback';
import { tryReuseCallback } from 'valdi_core/src/utils/CallbackInternal';
import { getNodeTag, toXML } from 'valdi_core/src/utils/RenderedVirtualNodeUtils';
import { RendererError } from 'valdi_core/src/utils/RendererError';
import 'jasmine/src/jasmine';
import { PropertyList } from 'valdi_tsx/src/PropertyList';
import { StringMap } from 'coreutils/src/StringMap';
import { RawRenderRequestEntryType, RendererTestDelegate } from './RendererTestDelegate';

function makeRenderer(
  delegate: IRendererDelegate,
  allowedRootElementTypes?: string[],
  disableProxy?: boolean,
  useTopDownMoveOrder?: boolean,
): Renderer {
  const renderer = new Renderer('', allowedRootElementTypes, delegate, useTopDownMoveOrder);

  // return renderer;
  if (disableProxy) {
    return renderer;
  }

  return new Proxy(renderer, {
    get(target: any, property: string): any {
      const value = target[property];

      if (typeof value === 'function') {
        return function () {
          // eslint-disable-next-line prefer-rest-params
          const retValue = value.apply(target, arguments);

          if (property === 'end') {
            renderer.verifyIntegrity();
          }

          return retValue;
        };
      }

      return value;
    },
  });
}

function sanitize(text: string): string {
  const outLines: string[] = [];

  for (const line of text.split('\n')) {
    const trimmed = line.trim();

    if (trimmed) {
      outLines.push(trimmed);
    }
  }

  return outLines.join('\n');
}

function makeNodeProtoype(viewClass: string, attributes?: StringMap<any>): NodePrototype {
  return new NodePrototype(viewClass, viewClass, attributes);
}

function makeComponentPrototype(attributes?: StringMap<any>): ComponentPrototype {
  return ComponentPrototype.instanceWithNewId(attributes);
}

class TestComponent implements IComponent {
  readonly renderer: IRenderer;

  viewModel: any;

  onCreateCount = 0;
  onDestroyCount = 0;
  onRenderCount = 0;
  onViewModelUpdateCount = 0;
  context: any;

  constructor(renderer: IRenderer, viewModel: any, context: any) {
    this.renderer = renderer;
    this.viewModel = viewModel;
    this.context = context;
  }

  onCreate() {
    this.onCreateCount++;
  }

  onDestroy() {
    this.onDestroyCount++;
  }

  onRender() {
    this.onRenderCount++;
  }

  onViewModelUpdate(): void {
    this.onViewModelUpdateCount++;
  }

  static disallowNullViewModel = true;
}

describe('Renderer', () => {
  it('doesnt render on empty body', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    renderer.begin();
    renderer.end();

    expect(output.requests).toEqual([]);
  });

  it('can render simple elements', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const node1 = makeNodeProtoype('layout');
    const node2 = makeNodeProtoype('view');
    const node3 = makeNodeProtoype('label');

    renderer.begin();

    renderer.beginElement(node1);

    renderer.beginElement(node2);
    renderer.endElement();

    renderer.beginElement(node3);
    renderer.endElement();

    renderer.endElement();

    renderer.end();

    expect(output.requests.length).toBe(1);
    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 1,
            className: 'layout',
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 2,
            className: 'view',
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 3,
            className: 'label',
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 2,
            parentId: 1,
            parentIndex: 0,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 3,
            parentId: 1,
            parentIndex: 1,
          },
          {
            type: RawRenderRequestEntryType.setRootElement,
            id: 1,
          },
        ],
      },
    ]);
  });

  it('with useTopDownMoveOrder emits setRootElement first then moves in top-down order', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output, undefined, true, true);

    const node1 = makeNodeProtoype('layout');
    const node2 = makeNodeProtoype('view');
    const node3 = makeNodeProtoype('label');

    renderer.begin();
    renderer.beginElement(node1);
    renderer.beginElement(node2);
    renderer.endElement();
    renderer.beginElement(node3);
    renderer.endElement();
    renderer.endElement();
    renderer.end();

    expect(output.requests.length).toBe(1);
    const entries = output.requests[0].entries;
    const setRootIdx = entries.findIndex(e => e.type === RawRenderRequestEntryType.setRootElement);
    const moveIndices = entries
      .map((e, i) => (e.type === RawRenderRequestEntryType.moveElementToParent ? i : -1))
      .filter(i => i >= 0);
    // setRootElement must appear before any move (top-down: root first)
    expect(setRootIdx).toBeGreaterThanOrEqual(0);
    moveIndices.forEach(moveIdx => expect(moveIdx).toBeGreaterThan(setRootIdx));
    // For every move (id, parentId, index), the parent must have been "attached" before this move.
    // Root (id 1) is attached via setRootElement; others via an earlier move.
    const attachedBefore = new Set<number>();
    for (const e of entries) {
      if (e.type === RawRenderRequestEntryType.setRootElement && e.id != null) {
        attachedBefore.add(e.id);
      }
      if (e.type === RawRenderRequestEntryType.moveElementToParent && e.id != null && e.parentId != null) {
        expect(attachedBefore.has(e.parentId)).toBeTrue();
        attachedBefore.add(e.id);
      }
    }
  });

  it('can apply attributes', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const node1 = makeNodeProtoype('layout');
    const node2 = makeNodeProtoype('layout');

    renderer.begin();

    renderer.beginElement(node1);
    renderer.setAttribute('text', 'hello');

    renderer.beginElement(node2);
    renderer.setAttribute('width', 42);
    renderer.endElement();

    renderer.endElement();

    renderer.end();

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 1,
            className: 'layout',
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 1,
            name: 'text',
            value: 'hello',
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 2,
            className: 'layout',
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 2,
            name: 'width',
            value: 42,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 2,
            parentId: 1,
            parentIndex: 0,
          },
          {
            type: RawRenderRequestEntryType.setRootElement,
            id: 1,
          },
        ],
      },
    ]);
  });

  it('can render forEach without keys', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const node1 = makeNodeProtoype('layout');
    const node2 = makeNodeProtoype('view');

    renderer.begin();

    renderer.beginElement(node1);

    renderer.beginElement(node2);
    renderer.setAttribute('index', 1);
    renderer.endElement();

    renderer.beginElement(node2);
    renderer.setAttribute('index', 2);
    renderer.endElement();

    renderer.endElement();

    renderer.end();

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 1,
            className: 'layout',
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 2,
            className: 'view',
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 2,
            name: 'index',
            value: 1,
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 3,
            className: 'view',
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 3,
            name: 'index',
            value: 2,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 2,
            parentId: 1,
            parentIndex: 0,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 3,
            parentId: 1,
            parentIndex: 1,
          },
          {
            type: RawRenderRequestEntryType.setRootElement,
            id: 1,
          },
        ],
      },
    ]);
  });

  it('doesnt rerender forEach without keys', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const node1 = makeNodeProtoype('layout');
    const node2 = makeNodeProtoype('view');

    renderer.begin();

    renderer.beginElement(node1);

    renderer.beginElement(node2);
    renderer.setAttribute('index', 1);
    renderer.endElement();

    renderer.beginElement(node2);
    renderer.setAttribute('index', 2);
    renderer.endElement();

    renderer.beginElement(node2);
    renderer.setAttribute('index', 3);
    renderer.endElement();

    renderer.endElement();

    renderer.end();

    expect(output.requests.length).toBe(1);
    output.clear();

    renderer.begin();

    renderer.beginElement(node1);

    renderer.beginElement(node2);
    renderer.setAttribute('index', 1);
    renderer.endElement();

    renderer.beginElement(node2);
    renderer.setAttribute('index', 2);
    renderer.endElement();

    renderer.beginElement(node2);
    renderer.setAttribute('index', 3);
    renderer.endElement();

    renderer.endElement();

    renderer.end();

    expect(output.requests).toEqual([]);
  });

  it('handle forEach with non forEach neigbhoors', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const node1 = makeNodeProtoype('layout');
    const node2 = makeNodeProtoype('view');
    const node3 = makeNodeProtoype('label');
    const node4 = makeNodeProtoype('layout');

    renderer.begin();

    renderer.beginElement(node1);

    renderer.beginElement(node2);
    renderer.endElement();

    renderer.beginElement(node3);
    renderer.setAttribute('index', 1);
    renderer.endElement();

    renderer.beginElement(node3);
    renderer.setAttribute('index', 2);
    renderer.endElement();

    renderer.beginElement(node3);
    renderer.setAttribute('index', 3);
    renderer.endElement();

    renderer.beginElement(node4);
    renderer.endElement();

    renderer.endElement();

    renderer.end();

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 1,
            className: 'layout',
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 2,
            className: 'view',
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 3,
            className: 'label',
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 3,
            name: 'index',
            value: 1,
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 4,
            className: 'label',
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 4,
            name: 'index',
            value: 2,
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 5,
            className: 'label',
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 5,
            name: 'index',
            value: 3,
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 6,
            className: 'layout',
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 2,
            parentId: 1,
            parentIndex: 0,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 3,
            parentId: 1,
            parentIndex: 1,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 4,
            parentId: 1,
            parentIndex: 2,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 5,
            parentId: 1,
            parentIndex: 3,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 6,
            parentId: 1,
            parentIndex: 4,
          },
          {
            type: RawRenderRequestEntryType.setRootElement,
            id: 1,
          },
        ],
      },
    ]);
  });

  it('handle move forEach with non forEach neighbor', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const node1 = makeNodeProtoype('layout');
    const node2 = makeNodeProtoype('view');
    const node3 = makeNodeProtoype('label');
    const node4 = makeNodeProtoype('layout');

    function renderFunc(firstNode: NodePrototype, lastNode: NodePrototype) {
      renderer.begin();

      renderer.beginElement(node1);

      renderer.beginElement(firstNode);
      renderer.endElement();

      renderer.beginElement(node3);
      renderer.setAttribute('index', 1);
      renderer.endElement();

      renderer.beginElement(node3);
      renderer.setAttribute('index', 2);
      renderer.endElement();

      renderer.beginElement(node3);
      renderer.setAttribute('index', 3);
      renderer.endElement();

      renderer.beginElement(lastNode);
      renderer.endElement();

      renderer.endElement();

      renderer.end();
    }

    renderFunc(node2, node4);

    expect(output.requests.length).toBe(1);
    output.clear();

    renderFunc(node4, node2);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 6,
            parentId: 1,
            parentIndex: 0,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 3,
            parentId: 1,
            parentIndex: 1,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 4,
            parentId: 1,
            parentIndex: 2,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 5,
            parentId: 1,
            parentIndex: 3,
          },
        ],
      },
    ]);
  });

  it('handle attribute change on forEach without keys', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const node1 = makeNodeProtoype('layout');
    const node2 = makeNodeProtoype('view');

    function renderFunc(items: number[]) {
      renderer.begin();

      renderer.beginElement(node1);

      for (const item of items) {
        renderer.beginElement(node2);
        renderer.setAttribute('index', item);
        renderer.endElement();
      }

      renderer.endElement();

      renderer.end();
    }

    renderFunc([1, 2, 3]);

    expect(output.requests.length).toBe(1);
    output.clear();

    renderFunc([2, 3, 1]);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 2,
            name: 'index',
            value: 2,
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 3,
            name: 'index',
            value: 3,
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 4,
            name: 'index',
            value: 1,
          },
        ],
      },
    ]);
  });

  it('can render for each with keys', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const node1 = makeNodeProtoype('layout');
    const node2 = makeNodeProtoype('view');

    renderer.begin();

    renderer.beginElement(node1);

    renderer.beginElement(node2, 'key1');
    renderer.setAttribute('index', 1);
    renderer.endElement();

    renderer.beginElement(node2, 'key2');
    renderer.setAttribute('index', 2);
    renderer.endElement();

    renderer.endElement();

    renderer.end();

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 1,
            className: 'layout',
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 2,
            className: 'view',
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 2,
            name: 'index',
            value: 1,
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 3,
            className: 'view',
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 3,
            name: 'index',
            value: 2,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 2,
            parentId: 1,
            parentIndex: 0,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 3,
            parentId: 1,
            parentIndex: 1,
          },
          {
            type: RawRenderRequestEntryType.setRootElement,
            id: 1,
          },
        ],
      },
    ]);
  });

  it('detect move with forEach with keys', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const node1 = makeNodeProtoype('layout');
    const node2 = makeNodeProtoype('view');

    renderer.begin();

    renderer.beginElement(node1);

    renderer.beginElement(node2, 'key1');
    renderer.setAttribute('index', 1);
    renderer.endElement();

    renderer.beginElement(node2, 'key2');
    renderer.setAttribute('index', 2);
    renderer.endElement();

    renderer.beginElement(node2, 'key3');
    renderer.setAttribute('index', 3);
    renderer.endElement();

    renderer.endElement();

    renderer.end();

    expect(output.requests.length).toBe(1);
    output.clear();

    renderer.begin();

    renderer.beginElement(node1);

    renderer.beginElement(node2, 'key2');
    renderer.setAttribute('index', 2);
    renderer.endElement();

    renderer.beginElement(node2, 'key3');
    renderer.setAttribute('index', 3);
    renderer.endElement();

    renderer.beginElement(node2, 'key1');
    renderer.setAttribute('index', 1);
    renderer.endElement();

    renderer.endElement();

    renderer.end();

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 3,
            parentId: 1,
            parentIndex: 0,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 4,
            parentId: 1,
            parentIndex: 1,
          },
        ],
      },
    ]);
  });

  it('fails on nested render', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output, undefined, true);

    renderer.begin();

    expect(() => {
      renderer.begin();
    }).toThrowError();
  });

  it('fails on unbalanced begin element', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output, undefined, true);

    const node = makeNodeProtoype('root');

    renderer.begin();

    renderer.beginElement(node);
    renderer.endElement();
    renderer.endElement();

    expect(() => {
      renderer.end();
    }).toThrowError();
  });

  it('detects attribute change', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const node1 = makeNodeProtoype('layout');
    const node2 = makeNodeProtoype('layout');

    renderer.begin();

    renderer.beginElement(node1);
    renderer.setAttribute('text', 'hello');

    renderer.beginElement(node2);
    renderer.setAttribute('width', 42);
    renderer.endElement();

    renderer.endElement();

    renderer.end();

    expect(output.requests.length).toBe(1);
    output.clear();

    renderer.begin();

    renderer.beginElement(node1);
    renderer.setAttribute('text', 'hello');

    renderer.beginElement(node2);
    renderer.setAttribute('width', 84);
    renderer.endElement();

    renderer.endElement();

    renderer.end();

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 2,
            name: 'width',
            value: 84,
          },
        ],
      },
    ]);
  });

  it('doesnt rerender when attribute stay the same', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const node1 = makeNodeProtoype('layout');
    const node2 = makeNodeProtoype('layout');

    renderer.begin();

    renderer.beginElement(node1);
    renderer.setAttribute('text', 'hello');

    renderer.beginElement(node2);
    renderer.setAttribute('width', 42);
    renderer.endElement();

    renderer.endElement();

    renderer.end();

    expect(output.requests.length).toBe(1);
    output.clear();

    renderer.begin();

    renderer.beginElement(node1);
    renderer.setAttribute('text', 'hello');

    renderer.beginElement(node2);
    renderer.setAttribute('width', 42);
    renderer.endElement();

    renderer.endElement();

    renderer.end();

    expect(output.requests).toEqual([]);
  });

  class IndexesTestComponent {
    private node1 = makeNodeProtoype('layout');
    private renderer: Renderer;

    node2 = makeNodeProtoype('view');
    node3 = makeNodeProtoype('label');
    node4 = makeNodeProtoype('layout');

    constructor(renderer: Renderer) {
      this.renderer = renderer;
    }

    render(nodes: NodePrototype[]) {
      this.renderer.begin();

      this.renderer.beginElement(this.node1);

      for (const node of nodes) {
        this.renderer.beginElement(node);
        this.renderer.endElement();
      }

      this.renderer.endElement();

      this.renderer.end();
    }
  }

  it('remove element at start', () => {
    const output = new RendererTestDelegate();

    const component = new IndexesTestComponent(makeRenderer(output));

    component.render([component.node2, component.node3, component.node4]);

    expect(output.requests.length).toBe(1);
    output.clear();

    component.render([component.node3, component.node4]);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.destroyElement,
            id: 2,
          },
        ],
      },
    ]);
  });

  it('remove element at end', () => {
    const output = new RendererTestDelegate();

    const component = new IndexesTestComponent(makeRenderer(output));

    component.render([component.node2, component.node3, component.node4]);

    expect(output.requests.length).toBe(1);
    output.clear();

    component.render([component.node2, component.node3]);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.destroyElement,
            id: 4,
          },
        ],
      },
    ]);
  });

  it('remove element at center', () => {
    const output = new RendererTestDelegate();

    const component = new IndexesTestComponent(makeRenderer(output));

    component.render([component.node2, component.node3, component.node4]);

    expect(output.requests.length).toBe(1);
    output.clear();

    component.render([component.node2, component.node4]);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.destroyElement,
            id: 3,
          },
        ],
      },
    ]);
  });

  it('can move element from start to end', () => {
    const output = new RendererTestDelegate();

    const component = new IndexesTestComponent(makeRenderer(output));

    component.render([component.node2, component.node3, component.node4]);

    expect(output.requests.length).toBe(1);
    output.clear();

    component.render([component.node3, component.node4, component.node2]);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 3,
            parentId: 1,
            parentIndex: 0,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 4,
            parentId: 1,
            parentIndex: 1,
          },
        ],
      },
    ]);
  });

  it('can move element at end', () => {
    const output = new RendererTestDelegate();
    const component = new IndexesTestComponent(makeRenderer(output));

    component.render([component.node2, component.node3, component.node4]);

    expect(output.requests.length).toBe(1);
    output.clear();

    component.render([component.node2, component.node4, component.node3]);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 4,
            parentId: 1,
            parentIndex: 1,
          },
        ],
      },
    ]);
  });

  it('can move element at center', () => {
    const output = new RendererTestDelegate();
    const component = new IndexesTestComponent(makeRenderer(output));

    component.render([component.node2, component.node3, component.node4]);

    expect(output.requests.length).toBe(1);
    output.clear();

    component.render([component.node3, component.node2, component.node4]);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 3,
            parentId: 1,
            parentIndex: 0,
          },
        ],
      },
    ]);
  });

  it('can move and delete elements at the same time', () => {
    const output = new RendererTestDelegate();
    const component = new IndexesTestComponent(makeRenderer(output));

    component.render([component.node2, component.node3, component.node4]);

    expect(output.requests.length).toBe(1);
    output.clear();

    component.render([component.node4, component.node3]);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.destroyElement,
            id: 2,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 4,
            parentId: 1,
            parentIndex: 0,
          },
        ],
      },
    ]);
  });

  it('can move and insert at the same time', () => {
    const output = new RendererTestDelegate();
    const component = new IndexesTestComponent(makeRenderer(output));

    const node5 = makeNodeProtoype('layout');
    const node6 = makeNodeProtoype('layout');

    component.render([component.node2, component.node3, component.node4, node5]);

    expect(output.requests.length).toBe(1);
    output.clear();

    // We insert node6 at index 1, and swap node4 and node3

    component.render([component.node2, node6, component.node4, component.node3, node5]);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 6,
            className: 'layout',
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 6,
            parentId: 1,
            parentIndex: 1,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 4,
            parentId: 1,
            parentIndex: 2,
          },
        ],
      },
    ]);
  });

  it('can move and insert and delete at the same time', () => {
    const output = new RendererTestDelegate();
    const component = new IndexesTestComponent(makeRenderer(output));

    const node5 = makeNodeProtoype('layout');
    const node6 = makeNodeProtoype('layout');

    component.render([component.node2, component.node3, component.node4, node5]);

    expect(output.requests.length).toBe(1);
    output.clear();

    // We delete node2, insert node6 at index 0, and swap node4 and node3

    component.render([node6, component.node4, component.node3, node5]);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 6,
            className: 'layout',
          },
          {
            type: RawRenderRequestEntryType.destroyElement,
            id: 2,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 6,
            parentId: 1,
            parentIndex: 0,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 4,
            parentId: 1,
            parentIndex: 1,
          },
        ],
      },
    ]);
  });

  it('can move and delete at the same time', () => {
    const output = new RendererTestDelegate();
    const component = new IndexesTestComponent(makeRenderer(output));

    const node5 = makeNodeProtoype('layout');
    const node6 = makeNodeProtoype('layout');
    const node7 = makeNodeProtoype('layout');

    component.render([component.node2, component.node3, component.node4, node5, node6, node7]);

    expect(output.requests.length).toBe(1);
    output.clear();

    // We delete node2, delete node6 and move node5

    component.render([node5, component.node3, component.node4, node7]);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.destroyElement,
            id: 2,
          },
          {
            type: RawRenderRequestEntryType.destroyElement,
            id: 6,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 5,
            parentId: 1,
            parentIndex: 0,
          },
        ],
      },
    ]);
  });

  it('can insert at front', () => {
    const output = new RendererTestDelegate();
    const component = new IndexesTestComponent(makeRenderer(output));

    component.render([component.node2, component.node3]);

    expect(output.requests.length).toBe(1);
    output.clear();

    component.render([component.node4, component.node2, component.node3]);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 4,
            className: 'layout',
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 4,
            parentId: 1,
            parentIndex: 0,
          },
        ],
      },
    ]);
  });

  it('can insert at back', () => {
    const output = new RendererTestDelegate();
    const component = new IndexesTestComponent(makeRenderer(output));

    component.render([component.node2, component.node3]);

    expect(output.requests.length).toBe(1);
    output.clear();

    component.render([component.node2, component.node3, component.node4]);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 4,
            className: 'layout',
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 4,
            parentId: 1,
            parentIndex: 2,
          },
        ],
      },
    ]);
  });

  it('can render component', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const node1 = makeNodeProtoype('layout');
    const node2 = makeNodeProtoype('view');

    class RootComponent extends TestComponent {
      onRender(): void {
        super.onRender();

        renderer.beginElement(node1);

        renderer.beginElement(node2);
        renderer.endElement();

        renderer.beginElement(node2);
        renderer.endElement();

        renderer.endElement();
      }
    }

    const prototypeRootComponent = makeComponentPrototype();

    renderer.begin();

    renderer.beginComponent(RootComponent, prototypeRootComponent);
    renderer.endComponent();

    renderer.end();

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 1,
            className: 'layout',
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 2,
            className: 'view',
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 3,
            className: 'view',
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 2,
            parentId: 1,
            parentIndex: 0,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 3,
            parentId: 1,
            parentIndex: 1,
          },
          {
            type: RawRenderRequestEntryType.setRootElement,
            id: 1,
          },
        ],
      },
    ]);

    expect((renderer.getRootComponent() as RootComponent).onCreateCount).toBe(1);
  });

  it('can render component incrementally', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const root = makeNodeProtoype('root');
    const siblingBefore = makeNodeProtoype('before');
    const siblingAfter = makeNodeProtoype('after');

    const node1 = makeNodeProtoype('layout');
    const node2 = makeNodeProtoype('view');

    class RootComponent extends TestComponent {
      includeSecondNode = false;

      onRender(): void {
        super.onRender();

        renderer.beginElement(node1);

        renderer.beginElement(node2);
        renderer.endElement();

        if (this.includeSecondNode) {
          renderer.beginElement(node2);
          renderer.endElement();
        }

        renderer.endElement();
      }
    }

    const prototypeRootComponent = makeComponentPrototype();

    renderer.begin();

    renderer.beginElement(root);

    renderer.beginElement(siblingBefore);
    renderer.endElement();

    renderer.beginComponent(RootComponent, prototypeRootComponent);
    renderer.endComponent();

    renderer.beginElement(siblingAfter);
    renderer.endElement();

    renderer.endElement();

    renderer.end();

    expect(output.requests.length).toBe(1);
    output.clear();

    expect(renderer.getRootComponent()).toBeTruthy();

    const component = renderer.getRootComponent() as RootComponent;

    component.includeSecondNode = true;

    renderer.renderComponent(component, undefined);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 6,
            className: 'view',
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 6,
            parentId: 3,
            parentIndex: 1,
          },
        ],
      },
    ]);

    expect(renderer.getRootComponent()).toBe(component);
  });

  it('detect root component change', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    class Component1 extends TestComponent {}
    class Component2 extends TestComponent {}

    const prototypeComponent1 = makeComponentPrototype();
    const prototypeComponent2 = makeComponentPrototype();

    renderer.begin();

    renderer.beginComponent(Component1, prototypeComponent1);
    renderer.endComponent();

    renderer.end();

    expect(renderer.getRootComponent()).toBeTruthy();
    expect(renderer.getRootComponent()!.constructor).toBe(Component1);

    const component = renderer.getRootComponent() as Component1;

    renderer.begin();

    renderer.beginComponent(Component2, prototypeComponent2);
    renderer.endComponent();

    renderer.end();

    expect(renderer.getRootComponent()).toBeTruthy();
    expect(renderer.getRootComponent()!.constructor).toBe(Component2);
    expect(component.onDestroyCount).toBe(1);
    expect((renderer.getRootComponent() as TestComponent).onCreateCount).toBe(1);
  });

  it('can render child component', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const child1 = makeNodeProtoype('layout');
    const child2 = makeNodeProtoype('label');

    class ChildComponent extends TestComponent {
      onRender() {
        renderer.beginElement(child1);

        renderer.beginElement(child2);
        renderer.endElement();

        renderer.endElement();

        renderer.beginElement(child1);
        renderer.endElement();
      }
    }

    const prototypeChildComponent = makeComponentPrototype();

    const root1 = makeNodeProtoype('view');

    class RootComponent extends TestComponent {
      onRender() {
        renderer.beginElement(root1);

        renderer.beginComponent(ChildComponent, prototypeChildComponent);
        renderer.endComponent();

        renderer.endElement();
      }
    }

    const prototypeRootComponent = makeComponentPrototype();

    renderer.begin();

    renderer.beginComponent(RootComponent, prototypeRootComponent);
    renderer.endComponent();

    renderer.end();

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 1,
            className: 'view',
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 2,
            className: 'layout',
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 3,
            className: 'label',
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 3,
            parentId: 2,
            parentIndex: 0,
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 4,
            className: 'layout',
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 2,
            parentId: 1,
            parentIndex: 0,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 4,
            parentId: 1,
            parentIndex: 1,
          },
          {
            type: RawRenderRequestEntryType.setRootElement,
            id: 1,
          },
        ],
      },
    ]);
  });

  it('can pass view model to child component', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const child1 = makeNodeProtoype('label');

    class ChildComponent extends TestComponent {
      onRender() {
        renderer.beginElement(child1);

        renderer.setAttribute('text', this.viewModel.text);

        renderer.endElement();
      }
    }

    const prototypeChildComponent = makeComponentPrototype();

    const root1 = makeNodeProtoype('view');

    class RootComponent extends TestComponent {
      onRender() {
        renderer.beginElement(root1);

        renderer.beginComponent(ChildComponent, prototypeChildComponent);
        renderer.setViewModelProperty('text', 'Hello world');
        renderer.endComponent();

        renderer.endElement();
      }
    }

    const prototypeRootComponent = makeComponentPrototype();

    renderer.begin();

    renderer.beginComponent(RootComponent, prototypeRootComponent);
    renderer.endComponent();

    renderer.end();

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 1,
            className: 'view',
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 2,
            className: 'label',
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 2,
            name: 'text',
            value: 'Hello world',
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 2,
            parentId: 1,
            parentIndex: 0,
          },
          {
            type: RawRenderRequestEntryType.setRootElement,
            id: 1,
          },
        ],
      },
    ]);
  });

  it('rerender on view model update', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const label = makeNodeProtoype('label');

    class RootComponent extends TestComponent {
      onRender() {
        renderer.beginElement(label);

        renderer.setAttribute('text', this.viewModel.text);

        renderer.endElement();
      }
    }

    const prototypeRootComponent = makeComponentPrototype();

    renderer.begin();

    renderer.beginComponent(RootComponent, prototypeRootComponent);
    renderer.setViewModelProperty('text', 'First render');
    renderer.endComponent();

    renderer.end();

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 1,
            className: 'label',
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 1,
            name: 'text',
            value: 'First render',
          },
          {
            type: RawRenderRequestEntryType.setRootElement,
            id: 1,
          },
        ],
      },
    ]);

    output.clear();

    renderer.begin();

    renderer.beginComponent(RootComponent, prototypeRootComponent);
    renderer.setViewModelProperty('text', 'Second render');
    renderer.endComponent();

    renderer.end();

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 1,
            name: 'text',
            value: 'Second render',
          },
        ],
      },
    ]);
  });

  it('doesnt rerender when view model stay the same', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const label = makeNodeProtoype('label');

    class RootComponent extends TestComponent {
      onRender() {
        super.onRender();
        renderer.beginElement(label);

        renderer.setAttribute('text', this.viewModel.text);

        renderer.endElement();
      }
    }

    const prototypeRootComponent = makeComponentPrototype();

    function renderFunc() {
      renderer.begin();

      renderer.beginComponent(RootComponent, prototypeRootComponent);
      renderer.setViewModelProperty('text', 'First render');
      renderer.endComponent();

      renderer.end();
    }

    renderFunc();

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 1,
            className: 'label',
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 1,
            name: 'text',
            value: 'First render',
          },
          {
            type: RawRenderRequestEntryType.setRootElement,
            id: 1,
          },
        ],
      },
    ]);

    output.clear();

    expect((renderer.getRootComponent() as RootComponent).onRenderCount).toBe(1);

    renderFunc();

    expect(output.requests).toEqual([]);

    expect((renderer.getRootComponent() as RootComponent).onRenderCount).toBe(1);
  });

  it('destroys component when parent element is destroyed', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const root = makeNodeProtoype('root');
    const parent = makeNodeProtoype('parent');
    const child = makeNodeProtoype('child');

    class ChildComponent extends TestComponent {}

    const prototypeChildComponent = makeComponentPrototype();

    class Component extends TestComponent {
      onRender() {
        super.onRender();

        renderer.beginElement(child);

        renderer.beginComponent(ChildComponent, prototypeChildComponent);
        renderer.endComponent();

        renderer.endElement();
      }
    }

    const prototypeComponent = makeComponentPrototype();

    renderer.begin();

    renderer.beginElement(root);

    renderer.beginElement(parent);

    renderer.beginComponent(Component, prototypeComponent);
    renderer.endComponent();

    renderer.endElement();

    renderer.endElement();

    renderer.end();

    expect(renderer.getRootComponent()).toBeTruthy();
    const rootComponent = renderer.getRootComponent() as Component;
    expect(rootComponent.onDestroyCount).toBe(0);
    const childComponent = renderer.getComponentChildren(rootComponent)[0] as Component;
    expect(childComponent).toBeTruthy();
    expect(childComponent.onDestroyCount).toBe(0);

    renderer.begin();

    renderer.beginElement(root);

    renderer.endElement();

    renderer.end();

    expect(renderer.getRootComponent()).toBeFalsy();
    expect(rootComponent.onDestroyCount).toBe(1);
    expect(childComponent.onDestroyCount).toBe(1);
  });

  it('can find components', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const root = makeNodeProtoype('root');
    const container = makeNodeProtoype('container');
    const childContainer = makeNodeProtoype('childContainer');

    class Child1 extends TestComponent {}

    const prototypeChild1 = makeComponentPrototype();

    class NestedChild1 extends TestComponent {}
    class NestedChild2 extends TestComponent {}

    const prototypeNestedChild1 = makeComponentPrototype();
    const prototypeNestedChild2 = makeComponentPrototype();

    class Child2 extends TestComponent {
      onRender() {
        renderer.beginComponent(NestedChild1, prototypeNestedChild1);
        renderer.endComponent();
        renderer.beginElement(childContainer);
        renderer.beginComponent(NestedChild2, prototypeNestedChild2);
        renderer.endComponent();
        renderer.endElement();
      }
    }

    const prototypeChild2 = makeComponentPrototype();

    class Root extends TestComponent {
      onRender() {
        renderer.beginElement(root);

        renderer.beginComponent(Child1, prototypeChild1);
        renderer.endComponent();

        renderer.beginElement(container);

        renderer.beginComponent(Child2, prototypeChild2);
        renderer.endComponent();

        renderer.endElement();

        renderer.endElement();
      }
    }

    const prototypeRoot = makeComponentPrototype();

    renderer.begin();
    renderer.beginComponent(Root, prototypeRoot);
    renderer.endComponent();
    renderer.end();

    const rootComponent = renderer.getRootComponent();
    expect(rootComponent instanceof Root).toBeTruthy();

    const children = renderer.getComponentChildren(rootComponent!);
    expect(children.length).toBe(2);
    expect(children[0] instanceof Child1).toBeTruthy();
    expect(children[1] instanceof Child2).toBeTruthy();

    const nestedChildren1 = renderer.getComponentChildren(children[0]);
    expect(nestedChildren1.length).toBe(0);

    const nestedChildren2 = renderer.getComponentChildren(children[1]);
    expect(nestedChildren2.length).toBe(2);
    expect(nestedChildren2[0] instanceof NestedChild1).toBeTruthy();
    expect(nestedChildren2[1] instanceof NestedChild2).toBeTruthy();

    expect(renderer.getComponentParent(rootComponent!)).toBeFalsy();
    expect(renderer.getComponentParent(children[0])).toBe(rootComponent);
    expect(renderer.getComponentParent(children[1])).toBe(rootComponent);
    expect(renderer.getComponentParent(nestedChildren2[0])).toBe(children[1]);
    expect(renderer.getComponentParent(nestedChildren2[1])).toBe(children[1]);
  });

  it('can incrementally rerender components with complex hierarchies', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const child1Label = makeNodeProtoype('label1');

    class Child1 extends TestComponent {
      onRender() {
        super.onRender();

        renderer.beginElement(child1Label);
        renderer.setAttribute('text', this.viewModel.text);
        renderer.endElement();
      }
    }

    const prototypeChild1 = makeComponentPrototype();

    const child2Label = makeNodeProtoype('label2');

    class Child2 extends TestComponent {
      onRender() {
        super.onRender();

        renderer.beginElement(child2Label);
        renderer.setAttribute('text', this.viewModel.text);
        renderer.endElement();
      }
    }

    const prototypeChild2 = makeComponentPrototype();

    const before = makeNodeProtoype('before');
    const after = makeNodeProtoype('after');

    class RootComponent extends TestComponent {
      onRender() {
        super.onRender();

        renderer.beginElement(before);
        renderer.endElement();

        renderer.beginComponent(Child1, prototypeChild1);
        renderer.setViewModelProperty('text', this.viewModel.child1Text);
        renderer.endComponent();

        if (this.viewModel.child2Text) {
          renderer.beginComponent(Child2, prototypeChild2);
          renderer.setViewModelProperty('text', this.viewModel.child2Text);
          renderer.endComponent();
        }

        renderer.beginElement(after);
        renderer.endElement();
      }
    }

    const prototypeRootComponent = makeComponentPrototype();

    const root = makeNodeProtoype('root');

    function renderFunc(child1Text: string, child2Text: string | undefined) {
      renderer.begin();

      renderer.beginElement(root);

      renderer.beginComponent(RootComponent, prototypeRootComponent);
      renderer.setViewModelProperty('child1Text', child1Text);
      renderer.setViewModelProperty('child2Text', child2Text);
      renderer.endComponent();

      renderer.endElement();

      renderer.end();
    }

    renderFunc('text1', 'text2');

    expect(output.requests.length).toBe(1);
    output.clear();

    const rootComponent = renderer.getRootComponent()!;
    expect(rootComponent).toBeTruthy();

    const children = renderer.getComponentChildren(rootComponent);
    expect(children.length).toBe(2);

    const child1 = children[0] as TestComponent;
    const child2 = children[1] as TestComponent;

    function checkCallbacks(
      component: IComponent,
      expectedOnCreateCount: number,
      expectedOnRenderCount: number,
      expectedOnDestroyCount: number,
      expectedOnViewModelUpdateCount: number,
    ) {
      const typedComponent = component as TestComponent;
      expect(typedComponent.onCreateCount).toBe(expectedOnCreateCount, `onCreate should be ${expectedOnCreateCount}`);
      expect(typedComponent.onRenderCount).toBe(expectedOnRenderCount, `onRender should be ${expectedOnRenderCount}`);
      expect(typedComponent.onDestroyCount).toBe(
        expectedOnDestroyCount,
        `onDestroy should be ${expectedOnDestroyCount}`,
      );
      expect(typedComponent.onViewModelUpdateCount).toBe(
        expectedOnViewModelUpdateCount,
        `onViewModelUpdate should be ${expectedOnViewModelUpdateCount}`,
      );
    }

    checkCallbacks(rootComponent, 1, 1, 0, 1);
    checkCallbacks(child1, 1, 1, 0, 1);
    checkCallbacks(child2, 1, 1, 0, 1);

    // Don't change anything

    renderFunc('text1', 'text2');

    expect(output.requests).toEqual([]);

    checkCallbacks(rootComponent, 1, 1, 0, 1);
    checkCallbacks(child1, 1, 1, 0, 1);
    checkCallbacks(child2, 1, 1, 0, 1);

    // Change text of child1

    renderFunc('new text', 'text2');

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            name: 'text',
            value: 'new text',
            id: 3,
          },
        ],
      },
    ]);

    output.clear();

    checkCallbacks(rootComponent, 1, 2, 0, 2);
    checkCallbacks(child1, 1, 2, 0, 2);
    checkCallbacks(child2, 1, 1, 0, 1);

    // Change text of child2

    renderFunc('new text', 'new text 2');

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            name: 'text',
            value: 'new text 2',
            id: 4,
          },
        ],
      },
    ]);

    output.clear();

    checkCallbacks(rootComponent, 1, 3, 0, 3);
    checkCallbacks(child1, 1, 2, 0, 2);
    checkCallbacks(child2, 1, 2, 0, 2);

    // Render child1 independently

    renderer.renderComponent(child1, undefined);

    expect(output.requests).toEqual([]);

    checkCallbacks(rootComponent, 1, 3, 0, 3);
    checkCallbacks(child1, 1, 3, 0, 2);
    checkCallbacks(child2, 1, 2, 0, 2);

    // Render child2 independently

    renderer.renderComponent(child2, undefined);

    expect(output.requests).toEqual([]);

    checkCallbacks(rootComponent, 1, 3, 0, 3);
    checkCallbacks(child1, 1, 3, 0, 2);
    checkCallbacks(child2, 1, 3, 0, 2);

    // Update child2 independently

    renderer.renderComponent(child2, { text: 'updated' });

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            name: 'text',
            value: 'updated',
            id: 4,
          },
        ],
      },
    ]);
    output.clear();

    checkCallbacks(rootComponent, 1, 3, 0, 3);
    checkCallbacks(child1, 1, 3, 0, 2);
    checkCallbacks(child2, 1, 4, 0, 3);

    // Remove child 2

    renderFunc('new text', undefined);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.destroyElement,
            id: 4,
          },
        ],
      },
    ]);

    checkCallbacks(rootComponent, 1, 4, 0, 4);
    checkCallbacks(child1, 1, 3, 0, 2);
    checkCallbacks(child2, 1, 4, 1, 3);

    const childrenAfter = renderer.getComponentChildren(rootComponent);
    expect(childrenAfter.length).toBe(1);
    expect(child2.onDestroyCount).toBe(1);
  });

  it('can render slots', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const root = makeNodeProtoype('root');
    const headerContainer = makeNodeProtoype('headerContainer');
    const bodyContainer = makeNodeProtoype('bodyContainer');

    class SlotComponent extends TestComponent {
      onRender() {
        renderer.beginElement(headerContainer);
        renderer.renderNamedSlot('header', this);
        renderer.endElement();

        renderer.beginElement(bodyContainer);
        renderer.renderNamedSlot('body', this);
        renderer.endElement();
      }
    }

    const prototypeSlotComponent = makeComponentPrototype();

    const header = makeNodeProtoype('header');
    const body = makeNodeProtoype('body');

    renderer.begin();

    renderer.beginElement(root);

    renderer.beginComponent(SlotComponent, prototypeSlotComponent);
    renderer.setNamedSlot('header', () => {
      renderer.beginElement(header);
      renderer.endElement();
    });
    renderer.setNamedSlot('body', () => {
      renderer.beginElement(body);
      renderer.endElement();
    });
    renderer.endComponent();

    renderer.endElement();

    renderer.end();

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 1,
            className: 'root',
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 2,
            className: 'headerContainer',
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 3,
            className: 'header',
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 3,
            parentId: 2,
            parentIndex: 0,
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 4,
            className: 'bodyContainer',
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 5,
            className: 'body',
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 5,
            parentId: 4,
            parentIndex: 0,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 2,
            parentId: 1,
            parentIndex: 0,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 4,
            parentId: 1,
            parentIndex: 1,
          },
          {
            type: RawRenderRequestEntryType.setRootElement,
            id: 1,
          },
        ],
      },
    ]);
  });

  it('can avoid re-render slotted components', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const slotRoot = makeNodeProtoype('slotRoot');
    const slotBefore = makeNodeProtoype('slotBefore');
    const slotAfter = makeNodeProtoype('slotAfter');

    class ChildOfSlot extends TestComponent {}

    const prototypeChildOfSlot = makeComponentPrototype();

    class SlottedComponent extends TestComponent {
      onRender() {
        super.onRender();

        renderer.beginElement(slotRoot);

        renderer.beginElement(slotBefore);
        renderer.endElement();

        if (this.viewModel.renderSlot) {
          renderer.beginComponent(ChildOfSlot, prototypeChildOfSlot);
          renderer.endComponent();

          renderer.renderNamedSlot('default', this);
        }

        renderer.beginComponent(ChildOfSlot, prototypeChildOfSlot);
        renderer.endComponent();

        renderer.beginElement(slotAfter);
        renderer.endElement();

        renderer.endElement();
      }
    }

    const prototypeSlottedComponent = makeComponentPrototype();

    const root = makeNodeProtoype('root');
    const elements = makeNodeProtoype('injected');

    class InjectedComponent extends TestComponent {}

    const prototypeInjectedComponent = makeComponentPrototype();

    class RootComponent extends TestComponent {
      onRender() {
        super.onRender();

        renderer.beginElement(root);
        renderer.beginComponent(SlottedComponent, prototypeSlottedComponent);
        renderer.setViewModelProperty('renderSlot', this.viewModel.renderSlot);
        renderer.setNamedSlot('default', () => {
          for (const item of this.viewModel.elements) {
            renderer.beginElement(elements);
            renderer.setAttribute('value', item);
            renderer.endElement();
          }

          renderer.beginComponent(InjectedComponent, prototypeInjectedComponent);
          renderer.endComponent();
        });
        renderer.endComponent();
        renderer.endElement();
      }
    }

    const prototypeRootComponent = makeComponentPrototype();

    function renderFunction(viewModel: string[], renderSlot: boolean) {
      renderer.begin();
      renderer.beginComponent(RootComponent, prototypeRootComponent);
      renderer.setViewModelProperty('elements', viewModel);
      renderer.setViewModelProperty('renderSlot', renderSlot);
      renderer.endComponent();
      renderer.end();
    }

    renderFunction(['hello', 'world'], true);

    const rootComponent = renderer.getRootComponent() as TestComponent;
    const componentChildren = renderer.getComponentChildren(rootComponent!);
    expect(componentChildren.length).toBe(1);
    const childComponent = componentChildren[0] as TestComponent;

    expect(renderer.getComponentChildren(childComponent).length).toBe(3);

    expect(rootComponent.onRenderCount).toBe(1);
    expect(childComponent.onRenderCount).toBe(1);

    output.clear();

    // Don't change anything
    renderFunction(['hello', 'world'], true);

    expect(rootComponent.onRenderCount).toBe(2);
    expect(childComponent.onRenderCount).toBe(1);
    expect(output.requests).toEqual([]);
    expect(renderer.getComponentChildren(childComponent).length).toBe(3);

    // Insert items

    renderFunction(['hello', 'world', 'new'], true);

    // We should have successfully inserted in the slots without making the whole slot component rerender.
    expect(rootComponent.onRenderCount).toBe(3);
    expect(childComponent.onRenderCount).toBe(1);
    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 7,
            className: 'injected',
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 7,
            name: 'value',
            value: 'new',
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 7,
            parentId: 2,
            parentIndex: 3,
          },
        ],
      },
    ]);
    expect(renderer.getComponentChildren(childComponent).length).toBe(3);

    output.clear();

    // Re render with the same view model, child should not be rerendered

    renderFunction(['hello', 'world', 'new'], true);
    expect(rootComponent.onRenderCount).toBe(4);
    expect(childComponent.onRenderCount).toBe(1);
    expect(renderer.getComponentChildren(childComponent).length).toBe(3);

    expect(output.requests).toEqual([]);

    // Make the component not render the slot

    renderFunction(['hello', 'world', 'new'], false);
    expect(rootComponent.onRenderCount).toBe(5);
    expect(childComponent.onRenderCount).toBe(2);
    expect(renderer.getComponentChildren(childComponent).length).toBe(1);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.destroyElement,
            id: 4,
          },
          {
            type: RawRenderRequestEntryType.destroyElement,
            id: 5,
          },
          {
            type: RawRenderRequestEntryType.destroyElement,
            id: 7,
          },
        ],
      },
    ]);
    output.clear();

    // Do incremental renders, the slot should not be rendered

    renderFunction(['hello', 'world', 'new'], false);
    expect(rootComponent.onRenderCount).toBe(6);
    expect(childComponent.onRenderCount).toBe(2);
    expect(renderer.getComponentChildren(childComponent).length).toBe(1);
    expect(output.requests).toEqual([]);
  });

  it('supports slot in slot', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);
    const root = makeNodeProtoype('root');
    const childElement = makeNodeProtoype('sibling');
    const injectedElement = makeNodeProtoype('nested');
    const parentPrototype = makeComponentPrototype();
    const childPrototype = makeComponentPrototype();
    const nestedChildPrototype = makeComponentPrototype();

    class NestedChild extends TestComponent {
      onRender() {
        renderer.renderNamedSlot('default', this);
      }
    }

    class Child extends TestComponent {
      onRender() {
        renderer.beginElement(childElement);
        renderer.endElement();

        renderer.beginComponent(NestedChild, nestedChildPrototype);
        renderer.setNamedSlot('default', () => {
          renderer.renderNamedSlot('default', this);
        });
        renderer.endComponent();
      }
    }

    class Parent extends TestComponent {
      renderInjected = false;

      onRender() {
        renderer.beginElement(root);
        renderer.beginComponent(Child, childPrototype);
        renderer.setNamedSlot('default', () => {
          if (this.renderInjected) {
            renderer.beginElement(injectedElement);
            renderer.endElement();
          }
        });
        renderer.endComponent();
        renderer.endElement();
      }
    }

    renderer.renderRoot(() => {
      renderer.beginComponent(Parent, parentPrototype);
      renderer.endComponent();
    });

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 1,
            className: 'root',
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 2,
            className: 'sibling',
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 2,
            parentId: 1,
            parentIndex: 0,
          },
          {
            type: RawRenderRequestEntryType.setRootElement,
            id: 1,
          },
        ],
      },
    ]);
    output.clear();

    const parent = renderer.getRootComponent()! as Parent;
    const child = renderer.getComponentChildren(parent)[0]!;

    expect(parent).toBeTruthy();
    expect(child).toBeTruthy();

    const nestedChild = renderer.getComponentChildren(child)[0]!;

    expect(nestedChild).toBeTruthy();

    parent.renderInjected = true;
    renderer.renderComponent(parent, undefined);

    // This should have added nested
    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 3,
            className: 'nested',
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 3,
            parentId: 1,
            parentIndex: 1,
          },
        ],
      },
    ]);
    output.clear();

    // // We should be able to re-render Parent
    renderer.renderComponent(parent, undefined);

    expect(output.requests).toEqual([]);

    renderer.renderComponent(child, undefined);

    expect(output.requests).toEqual([]);

    renderer.renderComponent(nestedChild, undefined);

    expect(output.requests).toEqual([]);
  });

  it('can store rendered slots without waiting on injected slots from parent', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);
    const root = makeNodeProtoype('root');

    class SlotComponent extends TestComponent {
      onRender() {
        super.onRender();
        renderer.renderNamedSlot('default', this);
      }
    }

    const prototypeSlotComponent = makeComponentPrototype();

    function render(injectSlot: boolean) {
      renderer.begin();

      renderer.beginComponent(SlotComponent, prototypeSlotComponent);

      if (injectSlot) {
        renderer.setNamedSlot('default', () => {
          renderer.beginElement(root);
          renderer.endElement();
        });
      }

      renderer.endComponent();

      renderer.end();
    }

    render(false);

    expect(output.requests).toEqual([]);
    const slotComponent = renderer.getRootComponent() as SlotComponent;
    expect(slotComponent.onRenderCount).toBe(1);

    render(true);

    expect(output.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.createElement, id: 1, className: 'root' },
          { type: RawRenderRequestEntryType.setRootElement, id: 1 },
        ],
      },
    ]);
    expect(slotComponent.onRenderCount).toBe(2);
  });

  it('can collect elements in slot', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const slotRoot = makeNodeProtoype('slotRoot');
    const slotBefore = makeNodeProtoype('slotBefore');
    const slotAfter = makeNodeProtoype('slotAfter');

    class SomeComponent extends TestComponent {}

    const prototypeSomeComponent = makeComponentPrototype();

    class SlottedComponent extends TestComponent {
      slotRef = new ElementRef();

      onRender() {
        super.onRender();

        renderer.beginElement(slotRoot);

        renderer.beginElement(slotBefore);
        renderer.endElement();

        renderer.renderNamedSlot('default', this, this.slotRef);

        renderer.beginComponent(SomeComponent, prototypeSomeComponent);
        renderer.endComponent();

        renderer.beginElement(slotAfter);
        renderer.endElement();

        renderer.endElement();
      }
    }

    const prototypeSlottedComponent = makeComponentPrototype();

    const root = makeNodeProtoype('root');
    const node = makeNodeProtoype('child');

    function renderFunc(items: string[]) {
      renderer.begin();

      renderer.beginElement(root);
      renderer.beginComponent(SlottedComponent, prototypeSlottedComponent);
      renderer.setNamedSlot('default', () => {
        for (const item of items) {
          renderer.beginElement(node, item);
          renderer.setAttribute('value', item);
          renderer.endElement();
        }
      });
      renderer.endComponent();
      renderer.endElement();

      renderer.end();
    }

    renderFunc(['hello', 'world']);

    const componentInstance = renderer.getRootComponent() as SlottedComponent;
    expect(componentInstance).toBeTruthy();
    expect(componentInstance.onRenderCount).toBe(1);

    let allElements = componentInstance.slotRef.all();
    expect(allElements.length).toEqual(2);
    expect(allElements[0].getAttribute('value')).toBe('hello');
    expect(allElements[1].getAttribute('value')).toBe('world');

    renderFunc(['hello']);
    expect(componentInstance.onRenderCount).toBe(1);

    allElements = componentInstance.slotRef.all();
    expect(allElements.length).toBe(1);
    expect(allElements[0].getAttribute('value')).toBe('hello');

    renderFunc(['world', 'hello', 'nice']);
    expect(componentInstance.onRenderCount).toBe(1);
    allElements = componentInstance.slotRef.all();
    expect(allElements.length).toBe(3);
    expect(allElements[0].getAttribute('value')).toBe('world');
    expect(allElements[1].getAttribute('value')).toBe('hello');
    expect(allElements[2].getAttribute('value')).toBe('nice');

    // Render the same items to check whether the renderer is able to avoid collecting
    // the elements when not necessary

    renderFunc(['world', 'hello', 'nice']);
    expect(componentInstance.onRenderCount).toBe(1);

    const newElements = componentInstance.slotRef.all();
    // We should get exactly the same instance
    expect(allElements).toEqual(newElements);

    renderFunc(['world', 'hello']);
    const newElementsModified = componentInstance.slotRef.all();
    // In case of modifications instance should be different
    expect(allElements).not.toBe(newElementsModified);
  });

  it('supports slot rendered through children view model property', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);
    const componentPrototype = makeComponentPrototype();
    const root = makeNodeProtoype('root');
    const nested = makeNodeProtoype('nested');

    class SlottedComponent extends TestComponent {
      onRender(): void {
        super.onRender();

        renderer.beginElement(root);

        if (this.viewModel.children) {
          this.viewModel.children();
        }

        renderer.endElement();
      }
    }

    function render(includeChild: boolean, nestedValue: string | undefined) {
      renderer.begin();

      renderer.beginComponent(SlottedComponent, componentPrototype);

      if (includeChild) {
        renderer.setUnnamedSlot(() => {
          renderer.beginElement(nested);
          renderer.setAttributeString('value', nestedValue);
          renderer.endElement();
        });
      } else {
        renderer.setUnnamedSlot(undefined);
      }

      renderer.endComponent();

      renderer.end();
    }

    render(false, undefined);

    const componentInstance = renderer.getRootComponent() as SlottedComponent;
    expect(componentInstance).toBeTruthy();
    expect(componentInstance.onRenderCount).toBe(1);

    expect(output.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.createElement, id: 1, className: 'root' },
          { type: RawRenderRequestEntryType.setRootElement, id: 1 },
        ],
      },
    ]);
    output.clear();

    // Now passing a slot, it should trigger a re-render

    render(true, undefined);

    expect(componentInstance.onRenderCount).toBe(2);

    expect(output.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.createElement, id: 2, className: 'nested' },
          { type: RawRenderRequestEntryType.moveElementToParent, id: 2, parentId: 1, parentIndex: 0 },
        ],
      },
    ]);
    output.clear();

    // Passing a slot again, it should bypass rendering

    render(true, 'Hello World');

    expect(componentInstance.onRenderCount).toBe(2);

    expect(output.requests).toEqual([
      {
        entries: [{ type: RawRenderRequestEntryType.setElementAttribute, id: 2, name: 'value', value: 'Hello World' }],
      },
    ]);
    output.clear();

    // When removing the slot, it should trigger a re-render

    render(false, undefined);

    expect(componentInstance.onRenderCount).toBe(3);

    expect(output.requests).toEqual([
      {
        entries: [{ type: RawRenderRequestEntryType.destroyElement, id: 2 }],
      },
    ]);
    output.clear();
  });

  it('remembers last given arguments to slot function when bypassing rendering', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);
    const componentPrototype = makeComponentPrototype();
    const root = makeNodeProtoype('root');
    const nested = makeNodeProtoype('nested');

    class SlottedComponent extends TestComponent {
      onRender(): void {
        super.onRender();

        renderer.beginElement(root);

        this.viewModel.children(`My value is ${this.viewModel.value}`);

        renderer.endElement();
      }
    }

    function render(viewModelValue: string, prefix: string) {
      renderer.begin();

      renderer.beginComponent(SlottedComponent, componentPrototype);

      renderer.setViewModelProperty('value', viewModelValue);
      renderer.setUnnamedSlot(value => {
        renderer.beginElement(nested);
        renderer.setAttributeString('value', prefix + value);
        renderer.endElement();
      });

      renderer.endComponent();

      renderer.end();
    }

    render('42', 'initial: ');

    const componentInstance = renderer.getRootComponent() as SlottedComponent;
    expect(componentInstance).toBeTruthy();
    expect(componentInstance.onRenderCount).toBe(1);

    expect(output.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.createElement, id: 1, className: 'root' },
          { type: RawRenderRequestEntryType.createElement, id: 2, className: 'nested' },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 2,
            name: 'value',
            value: 'initial: My value is 42',
          },
          { type: RawRenderRequestEntryType.moveElementToParent, id: 2, parentId: 1, parentIndex: 0 },
          { type: RawRenderRequestEntryType.setRootElement, id: 1 },
        ],
      },
    ]);
    output.clear();

    // Render with the same view model, but different prefix.
    // It should bypass rendering, but give back the previous arguments passed in by the component
    render('42', 'next: ');

    expect(componentInstance.onRenderCount).toBe(1);
    expect(output.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.setElementAttribute, id: 2, name: 'value', value: 'next: My value is 42' },
        ],
      },
    ]);
    output.clear();

    // When passing a different view model property, it should re-render

    render('43', 'last: ');

    expect(componentInstance.onRenderCount).toBe(2);
    expect(output.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.setElementAttribute, id: 2, name: 'value', value: 'last: My value is 43' },
        ],
      },
    ]);
    output.clear();
  });

  it('can pass children as a regular view model property', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);
    const componentPrototype = makeComponentPrototype();
    const root = makeNodeProtoype('root');
    const nested = makeNodeProtoype('nested');

    class SlottedComponent extends TestComponent {
      onRender(): void {
        super.onRender();

        renderer.beginElement(root);

        this.viewModel.children();

        renderer.endElement();
      }
    }

    function render(nestedValue: string) {
      renderer.begin();

      renderer.beginComponent(SlottedComponent, componentPrototype);

      renderer.setViewModelProperty('children', () => {
        renderer.beginElement(nested);
        renderer.setAttributeString('value', nestedValue);
        renderer.endElement();
      });

      renderer.endComponent();

      renderer.end();
    }

    render('42');

    const componentInstance = renderer.getRootComponent() as SlottedComponent;
    expect(componentInstance).toBeTruthy();
    expect(componentInstance.onRenderCount).toBe(1);

    expect(output.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.createElement, id: 1, className: 'root' },
          { type: RawRenderRequestEntryType.createElement, id: 2, className: 'nested' },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 2,
            name: 'value',
            value: '42',
          },
          { type: RawRenderRequestEntryType.moveElementToParent, id: 2, parentId: 1, parentIndex: 0 },
          { type: RawRenderRequestEntryType.setRootElement, id: 1 },
        ],
      },
    ]);
    output.clear();

    // Because we are not using the slot API, passing a new function to the children
    // property should diff and cause a re-render

    render('42');

    expect(componentInstance.onRenderCount).toBe(2);
    expect(output.requests).toEqual([]);
  });

  it('supports mixing slot types', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);
    const componentPrototype = makeComponentPrototype();
    const root = makeNodeProtoype('root');
    const first = makeNodeProtoype('first');
    const second = makeNodeProtoype('second');

    class SlottedComponent extends TestComponent {
      onRender(): void {
        super.onRender();

        renderer.beginElement(root);

        renderer.renderUnnamedSlot(this);

        renderer.endElement();
      }
    }

    function render(renderSlot: () => void) {
      renderer.begin();
      renderer.beginComponent(SlottedComponent, componentPrototype);

      renderSlot();
      renderer.endComponent();
      renderer.end();
    }

    render(() => {});

    expect(output.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.createElement, id: 1, className: 'root' },
          { type: RawRenderRequestEntryType.setRootElement, id: 1 },
        ],
      },
    ]);

    output.clear();

    render(() => {
      renderer.setUnnamedSlot(() => {
        renderer.beginElement(first);
        renderer.endElement();
      });
    });

    expect(output.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.createElement, id: 2, className: 'first' },
          { type: RawRenderRequestEntryType.moveElementToParent, id: 2, parentId: 1, parentIndex: 0 },
        ],
      },
    ]);
    output.clear();

    render(() => {
      renderer.setNamedSlot('hello', () => {});
    });

    expect(output.requests).toEqual([]);

    render(() => {
      renderer.setNamedSlot('default', () => {
        renderer.beginElement(second);
        renderer.endElement();
      });
    });

    expect(output.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.createElement, id: 3, className: 'second' },
          { type: RawRenderRequestEntryType.destroyElement, id: 2 },
          { type: RawRenderRequestEntryType.moveElementToParent, id: 3, parentId: 1, parentIndex: 0 },
        ],
      },
    ]);
    output.clear();

    render(() => {
      renderer.setViewModelProperty('children', undefined);
    });

    expect(output.requests).toEqual([
      {
        entries: [{ type: RawRenderRequestEntryType.destroyElement, id: 3 }],
      },
    ]);
    output.clear();

    expect(() => {
      render(() => {
        renderer.setViewModelProperty('children', {
          default: () => {
            renderer.beginElement(first);
            renderer.endElement();
          },
        });
      });
    }).toThrowError(
      "Cannot render slot 'unnamed' in component SlottedComponent:  found an existing slot value that isn't a slot function or a named slot. Found slot value inside the 'children' property is of type: object",
    );

    expect(output.requests).toEqual([]);

    expect(() => {
      render(() => {
        renderer.setViewModelProperty('children', () => {
          renderer.beginElement(second);
          renderer.endElement();
        });
      });
    }).toThrowError(
      "Cannot render slot 'unnamed' in component SlottedComponent:  found an existing slot value that isn't a slot function or a named slot. Found slot value inside the 'children' property is of type: function",
    );

    expect(output.requests).toEqual([]);

    render(() => {
      renderer.setNamedSlot('default', () => {
        renderer.beginElement(first);
        renderer.endElement();
      });
    });

    expect(output.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.createElement, id: 4, className: 'first' },
          { type: RawRenderRequestEntryType.moveElementToParent, id: 4, parentId: 1, parentIndex: 0 },
        ],
      },
    ]);
    output.clear();

    render(() => {
      renderer.setUnnamedSlot(() => {
        renderer.beginElement(second);
        renderer.endElement();
      });
    });

    expect(output.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.createElement, id: 5, className: 'second' },
          { type: RawRenderRequestEntryType.destroyElement, id: 4 },
          { type: RawRenderRequestEntryType.moveElementToParent, id: 5, parentId: 1, parentIndex: 0 },
        ],
      },
    ]);
    output.clear();
  });

  it('renders unnamed slot when rendering default slot', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);
    const componentPrototype = makeComponentPrototype();
    const root = makeNodeProtoype('root');
    const child = makeNodeProtoype('child');

    class SlottedComponent extends TestComponent {
      onRender(): void {
        super.onRender();

        renderer.beginElement(root);

        renderer.renderNamedSlot(this.viewModel.slotName, this);

        renderer.endElement();
      }
    }

    function render(slotName: string) {
      renderer.begin();
      renderer.beginComponent(SlottedComponent, componentPrototype);

      renderer.setViewModelProperty('slotName', slotName);
      renderer.setUnnamedSlot(() => {
        renderer.beginElement(child);
        renderer.endElement();
      });

      renderer.endComponent();
      renderer.end();
    }

    render('doesMotExist');

    expect(output.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.createElement, id: 1, className: 'root' },
          { type: RawRenderRequestEntryType.setRootElement, id: 1 },
        ],
      },
    ]);
    output.clear();

    render('stillNotExist');

    expect(output.requests).toEqual([]);

    render('default');

    expect(output.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.createElement, id: 2, className: 'child' },
          { type: RawRenderRequestEntryType.moveElementToParent, id: 2, parentId: 1, parentIndex: 0 },
        ],
      },
    ]);
    output.clear();

    render('default');

    expect(output.requests).toEqual([]);

    render('doesNotExist');

    expect(output.requests).toEqual([{ entries: [{ type: RawRenderRequestEntryType.destroyElement, id: 2 }] }]);
  });

  it('receives named slots as children object', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);
    const componentPrototype = makeComponentPrototype();
    const root = makeNodeProtoype('root');
    const header = makeNodeProtoype('header');
    const footer = makeNodeProtoype('footer');

    class SlottedComponent extends TestComponent {
      headerRef = new ElementRef();
      footerRef = new ElementRef();

      onRender(): void {
        super.onRender();

        renderer.beginElement(root);

        if (this.viewModel.children?.header) {
          renderer.beginElement(header);
          renderer.setAttributeRef(this.headerRef);
          this.viewModel.children.header(this.headerRef);
          renderer.endElement();
        }

        if (this.viewModel.children?.footer) {
          renderer.beginElement(footer);
          renderer.setAttributeRef(this.footerRef);
          this.viewModel.children.footer(this.footerRef);
          renderer.endElement();
        }

        renderer.endElement();
      }
    }

    function render(headerValue: string | undefined, footerValue: string | undefined) {
      renderer.begin();

      renderer.beginComponent(SlottedComponent, componentPrototype);

      if (headerValue) {
        renderer.setNamedSlot('header', (ref: ElementRef) => {
          ref.setAttribute('value', headerValue);
        });
      } else {
        renderer.setNamedSlot('header', undefined);
      }

      if (footerValue) {
        renderer.setNamedSlot('footer', (ref: ElementRef) => {
          ref.setAttribute('value', footerValue);
        });
      } else {
        renderer.setNamedSlot('footer', undefined);
      }

      renderer.endComponent();

      renderer.end();
    }

    render(undefined, undefined);

    const componentInstance = renderer.getRootComponent() as SlottedComponent;
    expect(componentInstance).toBeTruthy();
    expect(componentInstance.onRenderCount).toBe(1);

    expect(output.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.createElement, id: 1, className: 'root' },
          { type: RawRenderRequestEntryType.setRootElement, id: 1 },
        ],
      },
    ]);
    output.clear();

    render('Hello', undefined);

    // Component should have re-rendered because it got a new named slot
    expect(componentInstance.onRenderCount).toBe(2);

    expect(output.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.createElement, id: 2, className: 'header' },
          { type: RawRenderRequestEntryType.setElementAttribute, id: 2, name: 'value', value: 'Hello' },
          { type: RawRenderRequestEntryType.moveElementToParent, id: 2, parentId: 1, parentIndex: 0 },
        ],
      },
    ]);
    output.clear();

    // When rendering with the same number of named slots, but different value
    // we should bypass rendering
    render('World', undefined);

    expect(componentInstance.onRenderCount).toBe(2);

    expect(output.requests).toEqual([
      {
        entries: [{ type: RawRenderRequestEntryType.setElementAttribute, id: 2, name: 'value', value: 'World' }],
      },
    ]);
    output.clear();

    render('World', 'Footing');

    expect(componentInstance.onRenderCount).toBe(3);

    expect(output.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.createElement, id: 3, className: 'footer' },
          { type: RawRenderRequestEntryType.setElementAttribute, id: 3, name: 'value', value: 'Footing' },
          { type: RawRenderRequestEntryType.moveElementToParent, id: 3, parentId: 1, parentIndex: 1 },
        ],
      },
    ]);
    output.clear();

    render('Bypass', 'Rendering');

    expect(componentInstance.onRenderCount).toBe(3);

    expect(output.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.setElementAttribute, id: 2, name: 'value', value: 'Bypass' },
          { type: RawRenderRequestEntryType.setElementAttribute, id: 3, name: 'value', value: 'Rendering' },
        ],
      },
    ]);
    output.clear();

    // When removing a slot, it should re-render the component

    render('Bypass', undefined);

    expect(componentInstance.onRenderCount).toBe(4);

    expect(output.requests).toEqual([
      {
        entries: [{ type: RawRenderRequestEntryType.destroyElement, id: 3 }],
      },
    ]);
    output.clear();

    render(undefined, undefined);

    expect(componentInstance.onRenderCount).toBe(5);

    expect(output.requests).toEqual([
      {
        entries: [{ type: RawRenderRequestEntryType.destroyElement, id: 2 }],
      },
    ]);
    output.clear();
  });

  it('can render components with key', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const root = makeNodeProtoype('root');
    const label = makeNodeProtoype('label');

    class ChildComponent extends TestComponent {
      onRender() {
        renderer.beginElement(label);
        renderer.setAttribute('value', this.viewModel.text);
        renderer.endElement();
      }
    }

    const prototypeChildComponent = makeComponentPrototype();

    function renderFunction(items: string[]) {
      renderer.begin();
      renderer.beginElement(root);

      for (const item of items) {
        renderer.beginComponent(ChildComponent, prototypeChildComponent, item);
        renderer.setViewModelProperty('text', item);
        renderer.endComponent();
      }

      renderer.endElement();
      renderer.end();
    }

    renderFunction(['hello', 'world']);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 1,
            className: 'root',
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 2,
            className: 'label',
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 2,
            name: 'value',
            value: 'hello',
          },
          {
            type: RawRenderRequestEntryType.createElement,
            id: 3,
            className: 'label',
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 3,
            name: 'value',
            value: 'world',
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 2,
            parentId: 1,
            parentIndex: 0,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 3,
            parentId: 1,
            parentIndex: 1,
          },
          {
            type: RawRenderRequestEntryType.setRootElement,
            id: 1,
          },
        ],
      },
    ]);
    output.clear();

    renderFunction(['world', 'hello']);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 3,
            parentId: 1,
            parentIndex: 0,
          },
        ],
      },
    ]);
    output.clear();

    renderFunction(['nice', 'world', 'hello']);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 4,
            className: 'label',
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 4,
            name: 'value',
            value: 'nice',
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 4,
            parentId: 1,
            parentIndex: 0,
          },
        ],
      },
    ]);
    output.clear();

    renderFunction(['nice', 'world']);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.destroyElement,
            id: 2,
          },
        ],
      },
    ]);
    output.clear();

    renderFunction(['nice', 'world', 'great']);

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 5,
            className: 'label',
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 5,
            name: 'value',
            value: 'great',
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 5,
            parentId: 1,
            parentIndex: 2,
          },
        ],
      },
    ]);
    output.clear();
  });

  it('creates empty view models when no properties are passed', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    class RootComponent extends TestComponent {}

    const prototypeRootComponent = makeComponentPrototype();

    function renderFunc() {
      renderer.begin();
      renderer.beginComponent(RootComponent, prototypeRootComponent);
      renderer.endComponent();
      renderer.end();
    }

    renderFunc();

    const rootComponent = renderer.getRootComponent() as RootComponent;
    expect(rootComponent.viewModel).toBeTruthy();
  });

  class RefTestComponent extends TestComponent {
    rootRef = new ElementRef();
    labelRef = new ElementRef();

    private root = makeNodeProtoype('root');
    private label = makeNodeProtoype('label');

    onRender() {
      const renderer = this.renderer as Renderer;

      renderer.beginElement(this.root);
      renderer.setAttribute('ref', this.rootRef);

      for (const item of this.viewModel.items) {
        renderer.beginElement(this.label);
        renderer.setAttribute('$ref', this.labelRef);
        renderer.setAttribute('value', item);
        renderer.endElement();
      }

      renderer.beginElement(this.label, 'last');
      renderer.setAttribute('ref', this.labelRef);
      renderer.setAttribute('value', 'always last');
      renderer.endElement();

      renderer.endElement();
    }

    static prototypeRefTestComponent = makeComponentPrototype();

    static doRender(renderer: Renderer, items: string[]): RefTestComponent {
      renderer.begin();
      renderer.beginComponent(RefTestComponent, RefTestComponent.prototypeRefTestComponent, undefined);
      renderer.setViewModelProperties({ items });
      renderer.endComponent();
      renderer.end();

      return renderer.getRootComponent() as RefTestComponent;
    }
  }

  it('can handle refs', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const component = RefTestComponent.doRender(renderer, []);

    const elements = component.labelRef.all();

    expect(elements.length).toBe(1);
    expect(elements[0].getAttribute('value')).toBe('always last');

    RefTestComponent.doRender(renderer, ['hello', 'world']);

    const elementsAfter = component.labelRef.all();

    expect(elementsAfter.length).toBe(3);
    expect(elementsAfter[0].getAttribute('value')).toBe('hello');
    expect(elementsAfter[1].getAttribute('value')).toBe('world');
    expect(elementsAfter[2].getAttribute('value')).toBe('always last');

    // Instance should be re-used
    expect(elements[0]).toBe(elementsAfter[2]);

    RefTestComponent.doRender(renderer, ['hello']);

    const elementsLast = component.labelRef.all();
    expect(elementsLast.length).toBe(2);
    expect(elementsLast[0]).toBe(elementsAfter[0]);
    expect(elementsLast[1]).toBe(elementsAfter[2]);
  });

  class ChildComponent extends TestComponent {
    private child = makeNodeProtoype('label');
    onRender() {
      const renderer = this.renderer as Renderer;

      renderer.beginElement(this.child);
      renderer.endElement();
    }
  }

  const prototypeChildComponent = makeComponentPrototype();

  interface ComponentRefTestComponentViewModel {
    shouldRenderSingleChild: boolean;
    shouldRenderMultipleChildren: boolean;
  }

  class ComponentRefTestComponent extends TestComponent {
    singleChildRef = new ComponentRef<ChildComponent>();

    multipleChildrenRef = new ComponentRef<ChildComponent>();

    private root = makeNodeProtoype('root');

    onRender() {
      const renderer = this.renderer as Renderer;

      renderer.beginElement(this.root);

      if (this.viewModel.shouldRenderSingleChild) {
        renderer.beginComponent(ChildComponent, prototypeChildComponent, undefined, this.singleChildRef);
        renderer.setViewModelProperties({ someKey: 'someVal' });
        renderer.endComponent();
      }

      if (this.viewModel.shouldRenderMultipleChildren) {
        ['one', 'two', 'three'].forEach(childValue => {
          renderer.beginComponent(ChildComponent, prototypeChildComponent, undefined, this.multipleChildrenRef);
          renderer.setViewModelProperties({ someKey: childValue });
          renderer.endComponent();
        });
      }

      renderer.endElement();
    }

    static prototypeComponentRefTestComponent = makeComponentPrototype();

    static doRender(renderer: Renderer, viewModel: ComponentRefTestComponentViewModel): ComponentRefTestComponent {
      renderer.begin();
      renderer.beginComponent(
        ComponentRefTestComponent,
        ComponentRefTestComponent.prototypeComponentRefTestComponent,
        undefined,
      );
      renderer.setViewModelProperties(viewModel as unknown as PropertyList);
      renderer.endComponent();
      renderer.end();

      return renderer.getRootComponent() as ComponentRefTestComponent;
    }
  }

  it('can handle single() component refs', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const rootComponent = ComponentRefTestComponent.doRender(renderer, {
      shouldRenderSingleChild: true,
      shouldRenderMultipleChildren: false,
    });
    const childComponent = rootComponent.singleChildRef.single();
    // Can get the child component via the component ref
    expect(childComponent).toBeDefined();
    expect(childComponent instanceof ChildComponent).toBeTruthy();
    expect(childComponent?.viewModel).toEqual({ someKey: 'someVal' });

    // Can't get the child component via the component ref if the child component was destroyed
    const rootComponent2 = ComponentRefTestComponent.doRender(renderer, {
      shouldRenderSingleChild: false,
      shouldRenderMultipleChildren: false,
    });
    expect(rootComponent2).toEqual(rootComponent);
    const childComponent2 = rootComponent2.singleChildRef.single();
    expect(childComponent2).toBeUndefined();
  });

  it('can handle all() component refs', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const rootComponent = ComponentRefTestComponent.doRender(renderer, {
      shouldRenderSingleChild: false,
      shouldRenderMultipleChildren: true,
    });
    const children = rootComponent.multipleChildrenRef.all();
    // Can get the child components via the component ref
    expect(children).toBeDefined();
    expect(children.length).toBe(3);
    const firstChild = children[0];
    expect(firstChild).toBeDefined();
    expect(firstChild instanceof ChildComponent).toBeTruthy();
    expect(firstChild.viewModel).toEqual({ someKey: 'one' });

    // Can't get the child components via the component ref if the child components were all destroyed
    const rootComponent2 = ComponentRefTestComponent.doRender(renderer, {
      shouldRenderSingleChild: false,
      shouldRenderMultipleChildren: false,
    });
    expect(rootComponent2).toEqual(rootComponent);
    const childComponent2 = rootComponent2.singleChildRef.all();
    expect(childComponent2).toEqual([]);
  });

  it('can update elements outside of render', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const component = RefTestComponent.doRender(renderer, []);
    output.clear();

    const elements = component.labelRef.all();

    expect(elements.length).toBe(1);

    elements[0].setAttribute('value', 'updated');

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 2,
            name: 'value',
            value: 'updated',
          },
        ],
      },
    ]);

    RefTestComponent.doRender(renderer, ['hello', 'world']);
    output.clear();

    const elementsAfter = component.labelRef.all();
    expect(elementsAfter.length).toBe(3);

    renderer.batchUpdates(() => {
      elementsAfter[0].setAttribute('value', 'goodbye');
      elementsAfter[1].setAttribute('value', 'world');
      elementsAfter[2].setAttribute('value', 'updated again');
    });

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 3,
            name: 'value',
            value: 'goodbye',
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 2,
            name: 'value',
            value: 'updated again',
          },
        ],
      },
    ]);
  });

  it('can render components independently in same request', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const root = makeNodeProtoype('root');
    const leaf = makeNodeProtoype('leaf');
    class MyComponent extends TestComponent {
      onRender() {
        super.onRender();

        renderer.beginElement(leaf);
        if (this.viewModel) {
          renderer.setAttribute('value', this.viewModel.value);
        }
        renderer.endElement();
      }
    }

    const prototypeMyComponent = makeComponentPrototype();

    renderer.begin();

    renderer.beginElement(root);
    renderer.beginComponent(MyComponent, prototypeMyComponent);
    renderer.endComponent();
    renderer.beginComponent(MyComponent, prototypeMyComponent);
    renderer.endComponent();
    renderer.endElement();

    renderer.end();

    const components = renderer.getRootComponents() as MyComponent[];
    expect(components.length).toBe(2);

    expect(components[0].onRenderCount).toBe(1);
    expect(components[1].onRenderCount).toBe(1);

    expect(output.requests.length).toBe(1);
    output.clear();

    renderer.batchUpdates(() => {
      renderer.renderComponent(components[0], { value: 'hello' });
      renderer.renderComponent(components[1], { value: 'world' });
    });

    expect(output.requests.length).toBe(1);
    expect(components[0].onRenderCount).toBe(2);
    expect(components[1].onRenderCount).toBe(2);
  });

  it('can call render callbacks', () => {
    const renderer = makeRenderer(new RendererTestDelegate());

    const node = makeNodeProtoype('root');
    function renderFunc() {
      renderer.beginElement(node);
      renderer.endElement();
    }

    let firstRenderCompleteCount = 0;
    renderer.onRenderComplete(() => {
      firstRenderCompleteCount++;
    });

    expect(firstRenderCompleteCount).toBe(1);

    let secondRenderCompleteCount = 0;

    renderer.begin();
    renderFunc();
    renderer.onRenderComplete(() => {
      secondRenderCompleteCount++;
    });

    expect(secondRenderCompleteCount).toBe(0);

    renderer.end();

    expect(secondRenderCompleteCount).toBe(1);
    expect(firstRenderCompleteCount).toBe(1);
  });

  it('can enqueue draw callbacks without executing them immediately', () => {
    const delegate = new RendererTestDelegate();
    const renderer = makeRenderer(delegate);

    let nextDrawCount = 0;
    let observedHookTimeMs = 0;
    renderer.onNextDraw(() => {
      nextDrawCount++;
    });
    renderer.onNextDraw((hookTimeMs) => {
      observedHookTimeMs = hookTimeMs;
    });

    expect(nextDrawCount).toBe(0);
    expect(observedHookTimeMs).toBe(0);
    expect(delegate.nextDrawCallbacks.length).toBe(2);

    delegate.nextDrawCallbacks[0](123.5);
    delegate.nextDrawCallbacks[1](123.5);
    expect(nextDrawCount).toBe(1);
    expect(observedHookTimeMs).toBe(123.5);
  });

  it('can enqueue draw callbacks while rendering', () => {
    const delegate = new RendererTestDelegate();
    const renderer = makeRenderer(delegate);
    const node = makeNodeProtoype('root');

    renderer.begin();
    renderer.beginElement(node);
    renderer.endElement();

    let nextDrawCount = 0;
    renderer.onNextDraw(() => {
      nextDrawCount++;
    });

    expect(nextDrawCount).toBe(0);
    expect(delegate.nextDrawCallbacks.length).toBe(1);

    renderer.end();

    expect(delegate.requests.length).toBe(1);
    expect(nextDrawCount).toBe(0);
  });

  it('doesnt send destroy element request for whole tree', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    const root = makeNodeProtoype('root');
    const childContainer = makeNodeProtoype('container');
    const child = makeNodeProtoype('child');

    function renderFunc(renderContainer: boolean) {
      renderer.begin();
      renderer.beginElement(root);
      if (renderContainer) {
        renderer.beginElement(childContainer);
        renderer.beginElement(child);
        renderer.endElement();
        renderer.endElement();
      }
      renderer.endElement();
      renderer.end();
    }

    renderFunc(true);
    expect(output.requests.length).toBe(1);
    output.clear();

    renderFunc(false);
    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.destroyElement,
            id: 2,
          },
        ],
      },
    ]);
  });

  it('child can conditionally render slots of parent', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    class Child extends Component {
      loading = true;

      onRender() {
        if (this.loading) {
          renderer.renderNamedSlot('loading', this);
        } else {
          renderer.renderNamedSlot('default', this);
        }
      }
    }

    const prototypeChild = makeComponentPrototype();

    const loading = makeNodeProtoype('loading');
    const body = makeNodeProtoype('body');

    class Wrapper extends Component {
      onRender() {
        renderer.beginComponent(Child, prototypeChild);
        renderer.setNamedSlot('loading', () => {
          renderer.renderNamedSlot('loading', this);
        });
        renderer.setNamedSlot('default', () => {
          renderer.renderNamedSlot('default', this);
        });
        renderer.endComponent();
      }
    }

    const prototypeWrapper = makeComponentPrototype();

    class Parent extends Component {
      onRender() {
        renderer.beginComponent(Wrapper, prototypeWrapper);
        renderer.setNamedSlot('loading', () => {
          renderer.beginElement(loading);
          renderer.endElement();
        });
        renderer.setNamedSlot('default', () => {
          renderer.beginElement(body);
          renderer.endElement();
        });
        renderer.endComponent();
      }
    }

    const prototypeParent = makeComponentPrototype();

    renderer.begin();
    renderer.beginComponent(Parent, prototypeParent);
    renderer.endComponent();
    renderer.end();

    let rootElement = renderer.getRootElement();
    expect(rootElement?.viewClass).toBe('loading');

    const rootComponent = renderer.getRootComponent()! as Parent;
    const wrapperComponent = renderer.getComponentChildren(rootComponent)[0];
    const childComponent = renderer.getComponentChildren(wrapperComponent)[0] as Child;

    childComponent.loading = false;
    childComponent.scheduleRender();

    rootElement = renderer.getRootElement();
    expect(rootElement?.viewClass).toBe('body');

    rootComponent.scheduleRender();

    rootElement = renderer.getRootElement();
    expect(rootElement?.viewClass).toBe('body');
  });

  it('supports user provided slot key', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);
    const componentPrototype = makeComponentPrototype();
    const root = makeNodeProtoype('root');
    const label = makeNodeProtoype('label');

    interface ViewModel {
      slotKey: string;
    }

    class SlottedComponent extends Component<ViewModel> {
      onRender(): void {
        renderer.beginElement(root);
        renderer.renderUnnamedSlot(this, undefined, this.viewModel.slotKey);
        renderer.endElement();
      }
    }

    function render(slotKey: string, slotFn: () => void) {
      renderer.begin();
      renderer.beginComponent(SlottedComponent, componentPrototype);

      renderer.setUnnamedSlot(slotFn);
      renderer.setViewModelProperty('slotKey', slotKey);

      renderer.endComponent();
      renderer.end();
    }

    render('beforeKey', () => {});

    output.clear();

    render('beforeKey', () => {
      renderer.beginElement(label);
      renderer.endElement();
    });

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 2,
            className: 'label',
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 2,
            parentId: 1,
            parentIndex: 0,
          },
        ],
      },
    ]);
    output.clear();

    // Render with the same key
    render('beforeKey', () => {
      renderer.beginElement(label);
      renderer.endElement();
    });

    expect(output.requests).toEqual([]);

    // Render the same content with a different key,
    // it should teardown the whole subtree and create it again

    render('afterKey', () => {
      renderer.beginElement(label);
      renderer.endElement();
    });

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 3,
            className: 'label',
          },
          {
            type: RawRenderRequestEntryType.destroyElement,
            id: 2,
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 3,
            parentId: 1,
            parentIndex: 0,
          },
        ],
      },
    ]);
    output.clear();

    render('afterKey', () => {
      renderer.beginElement(label);
      renderer.endElement();
    });
    expect(output.requests).toEqual([]);
  });

  it('generates slot key again when not passing a previously passed key', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);
    const componentPrototype = makeComponentPrototype();
    const root = makeNodeProtoype('root');

    interface ViewModel {
      slotKey: string;
    }

    class SlottedComponent extends Component<ViewModel> {
      onRender(): void {
        renderer.renderUnnamedSlot(this, undefined, this.viewModel.slotKey);
      }
    }

    function render(slotKey: string | undefined, slotFn: () => void) {
      renderer.begin();
      renderer.beginComponent(SlottedComponent, componentPrototype);

      renderer.setUnnamedSlot(slotFn);
      renderer.setViewModelProperty('slotKey', slotKey);

      renderer.endComponent();
      renderer.end();
    }

    render('myKey', () => {
      renderer.beginElement(root);
      renderer.endElement();
    });

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 1,
            className: 'root',
          },
          {
            type: RawRenderRequestEntryType.setRootElement,
            id: 1,
          },
        ],
      },
    ]);
    output.clear();

    render(undefined, () => {
      renderer.beginElement(root);
      renderer.endElement();
    });

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 2,
            className: 'root',
          },
          {
            type: RawRenderRequestEntryType.destroyElement,
            id: 1,
          },
          {
            type: RawRenderRequestEntryType.setRootElement,
            id: 2,
          },
        ],
      },
    ]);
    output.clear();

    render(undefined, () => {
      renderer.beginElement(root);
      renderer.endElement();
    });

    expect(output.requests).toEqual([]);

    render('myKey', () => {
      renderer.beginElement(root);
      renderer.endElement();
    });

    expect(output.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.createElement,
            id: 3,
            className: 'root',
          },
          {
            type: RawRenderRequestEntryType.destroyElement,
            id: 2,
          },
          {
            type: RawRenderRequestEntryType.setRootElement,
            id: 3,
          },
        ],
      },
    ]);
  });

  it('throws when two different components with same key are rendered', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output, undefined, true);

    class Component1 extends Component {}
    class Component2 extends Component {}

    const prototypeComponent1 = makeComponentPrototype();
    const prototypeComponent2 = makeComponentPrototype();

    const root = makeNodeProtoype('root');
    function renderFunc() {
      renderer.begin();

      renderer.beginElement(root);

      renderer.beginComponent(Component1, prototypeComponent1, 'same-key');
      renderer.endComponent();
      renderer.beginComponent(Component2, prototypeComponent2, 'same-key');
      renderer.endComponent();

      renderer.endElement();

      renderer.end();
    }

    Object.defineProperty(Component2, 'name', { value: Component1.name });

    expect(() => {
      renderFunc();
    }).toThrowError();
  });

  it('throws when rendering disallowed root element', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output, ['root'], true);

    const nodePrototype1 = makeNodeProtoype('root');
    const nodePrototype2 = makeNodeProtoype('view');

    renderer.begin();
    renderer.beginElement(nodePrototype1);
    renderer.endElement();
    renderer.end();

    expect(output.requests).toEqual([
      {
        entries: [
          { className: 'root', type: RawRenderRequestEntryType.createElement, id: 1 },
          { type: RawRenderRequestEntryType.setRootElement, id: 1 },
        ],
      },
    ]);

    output.clear();

    // Now render with a disallowed root element type

    expect(() => {
      renderer.begin();
      renderer.beginElement(nodePrototype2);
      renderer.endElement();
      renderer.end();
    }).toThrowError('Root element must be one of: <root> (resolved root element is <view>)');
  });

  it('can notify visibility changes', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output, undefined, true);

    let elementVisible: boolean | undefined;

    const root = makeNodeProtoype('root');
    const child = makeNodeProtoype('child');
    const ref = new ElementRef();

    function renderFunc() {
      renderer.begin();
      renderer.beginElement(root);
      renderer.beginElement(child);
      renderer.setAttribute('ref', ref);
      renderer.setAttribute('onVisibilityChanged', (visible: boolean) => {
        elementVisible = visible;
      });
      renderer.endElement();
      renderer.endElement();

      renderer.end();
    }

    renderFunc();

    expect(elementVisible).toBeUndefined();

    const element = ref.single()!;
    // This should have been set automatically
    expect(element.getAttribute('observeVisibility')).toBe(true);

    output.notifyElementVisibilityChanged(element.id, true);
    expect(elementVisible).toBe(true);

    output.notifyElementVisibilityChanged(element.id, false);
    expect(elementVisible).toBe(false);
  });

  it('propgates error when an exception is thrown', () => {
    const errorLabel = makeNodeProtoype('error');
    class ErrorComponent extends TestComponent {
      onRender() {
        const renderer = this.renderer as Renderer;

        renderer.beginElement(errorLabel);

        const rendererError = this.viewModel.error as RendererError;
        renderer.setAttribute('value', rendererError.sourceError.message);
        renderer.endElement();
      }
    }

    const prototypeErrorComponent = makeComponentPrototype();

    const rootElement = makeNodeProtoype('root');
    class ChildComponent extends TestComponent {
      onRender() {
        const renderer = this.renderer as Renderer;

        renderer.beginElement(rootElement);
        if (this.viewModel && this.viewModel.throwError) {
          throw Error('Bad Error');
        }
        renderer.endElement();
      }
    }

    const prototypeChildComponent = makeComponentPrototype();

    class RootComponent extends TestComponent {
      onRender() {
        const renderer = this.renderer as Renderer;

        if (this.viewModel.error) {
          renderer.beginComponent(ErrorComponent, prototypeErrorComponent);
          renderer.setViewModelProperty('error', this.viewModel.error);
          renderer.endComponent();
        } else {
          renderer.beginComponent(ChildComponent, prototypeChildComponent);
          renderer.setViewModelProperty('throwError', this.viewModel.throwError);
          renderer.endComponent();
        }
      }

      onError(error: Error) {
        this.renderer.renderComponent(this, { error: error });
      }
    }

    const prototypeRootComponent = makeComponentPrototype();

    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output, undefined, true);

    renderer.renderRootComponent(RootComponent, prototypeRootComponent, { throwError: false }, undefined);

    let rootComponent = renderer.getRootComponent()!;
    expect(rootComponent instanceof RootComponent).toBeTruthy();
    expect(renderer.getComponentChildren(rootComponent).length).toEqual(1);
    expect(renderer.getComponentChildren(rootComponent)[0] instanceof ChildComponent).toBeTruthy();

    output.clear();

    // Now rendering with an error
    renderer.renderRootComponent(RootComponent, prototypeRootComponent, { throwError: true }, undefined);
    rootComponent = renderer.getRootComponent()!;
    expect(rootComponent instanceof RootComponent).toBeTruthy();
    expect(renderer.getComponentChildren(rootComponent).length).toEqual(1);
    expect(renderer.getComponentChildren(rootComponent)[0] instanceof ErrorComponent).toBeTruthy();

    expect(output.requests).toEqual([
      {
        entries: [
          { className: 'error', type: RawRenderRequestEntryType.createElement, id: 2 },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            name: 'value',
            value: 'Bad Error',
            id: 2,
          },
          { type: RawRenderRequestEntryType.destroyElement, id: 1 },
          { type: RawRenderRequestEntryType.setRootElement, id: 2 },
        ],
      },
    ]);

    output.clear();
  });

  it('provides the failed node on exception during render', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output, undefined, true);

    const containerElement = makeNodeProtoype('layout');

    class CrashComponent extends TestComponent {
      onRender() {
        renderer.beginElement(containerElement);
        throw new Error('Bad error!');
      }
    }

    const prototypeCrashComponent = makeComponentPrototype();

    const rootElement = makeNodeProtoype('root');
    class RootComponent extends TestComponent {
      errors: Error[] = [];

      onRender() {
        renderer.beginElement(rootElement);
        renderer.setAttribute('hello', 'world');
        renderer.beginComponent(CrashComponent, prototypeCrashComponent);
        renderer.endComponent();
        renderer.endElement();
      }

      onError(error: Error) {
        this.errors.push(error);
      }
    }

    const prototypeRootComponent = makeComponentPrototype();

    renderer.renderRootComponent(RootComponent, prototypeRootComponent, undefined, undefined);

    const rootComponent = renderer.getRootComponent()! as RootComponent;
    expect(rootComponent instanceof RootComponent).toBeTruthy();

    expect(rootComponent.errors.length).toBe(1);
    expect(rootComponent.errors[0] instanceof RendererError).toBeTruthy();

    const lastRenderedNode = (rootComponent.errors[0] as RendererError).lastRenderedNode!;

    expect(lastRenderedNode).toBeTruthy();

    // The returned node should be CrashComponent
    expect(getNodeTag(lastRenderedNode)).toEqual('CrashComponent');

    let root = lastRenderedNode;
    while (root.parent) {
      root = root.parent;
    }

    expect(sanitize(toXML(root, true))).toEqual(
      sanitize(`
      <RootComponent>
        <root hello="world">
          <CrashComponent>
            <layout/>
          </CrashComponent>
        </root>
      </RootComponent>
    `),
    );
  });

  it('passes component context to child', () => {
    const renderer = makeRenderer(new RendererTestDelegate());

    class ChildComponent extends TestComponent {}

    const prototypeChildComponent = makeComponentPrototype();

    class RootComponent extends TestComponent {
      onRender() {
        renderer.beginComponent(ChildComponent, prototypeChildComponent);
        renderer.endComponent();
      }
    }

    const prototypeRootComponent = makeComponentPrototype();

    const myContext = { hello: 'world' };

    renderer.begin();
    renderer.beginComponent(RootComponent, prototypeRootComponent);
    renderer.setComponentContext(myContext);
    renderer.endComponent();
    renderer.end();

    const rootComponent = renderer.getRootComponent()!;
    expect(rootComponent instanceof RootComponent).toBeTruthy();

    const childComponent = renderer.getComponentChildren(rootComponent)[0];
    expect(childComponent instanceof ChildComponent).toBeTruthy();

    expect(rootComponent.context).toBe(myContext);
    expect(childComponent.context).toBe(myContext);
  });

  it('can render components in batch updates', () => {
    const delegate = new RendererTestDelegate();
    const renderer = makeRenderer(delegate);
    const label = makeNodeProtoype('label');
    const view = makeNodeProtoype('view');

    class MyComponent extends TestComponent {
      onRender() {
        if (this.viewModel.useLabel) {
          renderer.beginElement(label);
          renderer.endElement();
        } else {
          renderer.beginElement(view);
          renderer.endElement();
        }
      }
    }

    const prototypeMyComponent = makeComponentPrototype();

    const ref = new ComponentRef();

    renderer.renderRoot(() => {
      renderer.beginComponent(MyComponent, prototypeMyComponent, undefined, ref);
      renderer.endComponent();
    });

    delegate.clear();

    const component = ref.single()!;

    renderer.batchUpdates(() => {
      renderer.renderComponent(component, { useLabel: true });
      renderer.renderComponent(component, { useLabel: false });
    });

    // We should have the create and destroy for the first renderComponent call,
    // and the create and destroy for the second renderComponent call.
    expect(delegate.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.createElement, className: 'label', id: 2 },
          { type: RawRenderRequestEntryType.destroyElement, id: 1 },
          { type: RawRenderRequestEntryType.setRootElement, id: 2 },

          { type: RawRenderRequestEntryType.createElement, className: 'view', id: 3 },
          { type: RawRenderRequestEntryType.destroyElement, id: 2 },
          { type: RawRenderRequestEntryType.setRootElement, id: 3 },
        ],
      },
    ]);
  });

  it('can defer render components when renderer is busy', () => {
    const delegate = new RendererTestDelegate();
    const renderer = makeRenderer(delegate);

    const root = makeNodeProtoype('root');
    const label = makeNodeProtoype('label');

    const childRef = new ComponentRef<Child>();

    class Child extends TestComponent {
      labelText = 'Hello';
      onRenderCallCount = 0;

      onRender() {
        this.onRenderCallCount += 1;

        renderer.beginElement(label);
        renderer.setAttributeString('value', this.labelText);
        renderer.endElement();
      }
    }

    const prototypeChild = makeComponentPrototype();

    class Parent extends TestComponent {
      onRender() {
        renderer.beginElement(root);
        renderer.beginComponent(Child, prototypeChild, undefined, childRef);
        renderer.endComponent();
        renderer.endElement();
      }
    }

    const prototypeParent = makeComponentPrototype();

    renderer.renderRoot(() => {
      renderer.beginComponent(Parent, prototypeParent);
      renderer.endComponent();
    });

    expect(delegate.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.createElement, id: 1, className: 'root' },
          { type: RawRenderRequestEntryType.createElement, id: 2, className: 'label' },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 2,
            name: 'value',
            value: 'Hello',
          },
          {
            type: RawRenderRequestEntryType.moveElementToParent,
            id: 2,
            parentId: 1,
            parentIndex: 0,
          },
          { type: RawRenderRequestEntryType.setRootElement, id: 1 },
        ],
      },
    ]);

    delegate.clear();

    const childComponent = childRef.single()!;
    expect(childComponent.onRenderCallCount).toBe(1);

    // Now start a root render on which we will also ask the Child to re-render before it completes

    renderer.renderRoot(() => {
      renderer.beginComponent(Parent, prototypeParent);
      renderer.endComponent();

      childComponent.labelText = 'World';
      renderer.renderComponent(childComponent, undefined);
      // Our onRenderCallCount should still be 1, since the rendering should have been deferred
      expect(childComponent.onRenderCallCount).toBe(1);
    });

    // Our onRenderCallCount should now be 2, as the component should have been re-rendered at the end of renderRoot()
    expect(childComponent.onRenderCallCount).toBe(2);

    expect(delegate.requests).toEqual([
      {
        entries: [
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 2,
            name: 'value',
            value: 'World',
          },
        ],
      },
    ]);
  });

  it('supports animating attribute', () => {
    const delegate = new RendererTestDelegate();
    const renderer = makeRenderer(delegate);
    const ref = new ElementRef();

    const node = makeNodeProtoype('view');

    renderer.begin();
    renderer.beginElement(node);
    renderer.setAttributeRef(ref);
    renderer.setAttributeNumber('opacity', 0);
    renderer.endElement();
    renderer.end();

    delegate.clear();

    renderer.animate({ duration: 0.3 }, () => {
      ref.setAttribute('opacity', 1);
    });

    expect(delegate.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.beginAnimations, options: { duration: 0.3 } },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 1,
            name: 'opacity',
            value: 1.0,
          },
          { type: RawRenderRequestEntryType.endAnimations },
        ],
      },
    ]);
  });

  it('supports nested animations', () => {
    const delegate = new RendererTestDelegate();
    const renderer = makeRenderer(delegate);
    const ref = new ElementRef();

    const node = makeNodeProtoype('view');

    renderer.begin();
    renderer.beginElement(node);
    renderer.setAttributeRef(ref);
    renderer.setAttributeNumber('opacity', 0);
    renderer.endElement();
    renderer.end();

    delegate.clear();

    renderer.animate({ duration: 0.3 }, () => {
      ref.setAttribute('opacity', 1);

      renderer.animate({ duration: 0.5 }, () => {
        ref.setAttribute('width', 100);
      });
    });

    expect(delegate.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.beginAnimations, options: { duration: 0.3 } },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 1,
            name: 'opacity',
            value: 1.0,
          },
          { type: RawRenderRequestEntryType.beginAnimations, options: { duration: 0.5 } },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 1,
            name: 'width',
            value: 100.0,
          },
          { type: RawRenderRequestEntryType.endAnimations },
          { type: RawRenderRequestEntryType.endAnimations },
        ],
      },
    ]);
  });

  it('supports animating component', () => {
    const delegate = new RendererTestDelegate();
    const renderer = makeRenderer(delegate);
    const root = makeNodeProtoype('root');

    class AnimatableComponent extends TestComponent {
      onRender() {
        renderer.beginElement(root);
        renderer.setAttributeNumber('opacity', this.viewModel.opacity ?? 0);
        renderer.endElement();
      }
    }

    const prototypeAnimatableComponent = makeComponentPrototype();

    const ref = new ComponentRef<AnimatableComponent>();

    renderer.begin();
    renderer.beginComponent(AnimatableComponent, prototypeAnimatableComponent, undefined, ref);
    renderer.endComponent();
    renderer.end();

    delegate.clear();

    renderer.animate({ duration: 0.5 }, () => {
      renderer.renderComponent(ref.single()!, { opacity: 1 });
    });

    expect(delegate.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.beginAnimations, options: { duration: 0.5 } },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 1,
            name: 'opacity',
            value: 1.0,
          },
          { type: RawRenderRequestEntryType.endAnimations },
        ],
      },
    ]);
  });

  it('supports animating multiple components', () => {
    const delegate = new RendererTestDelegate();
    const renderer = makeRenderer(delegate);
    const element = makeNodeProtoype('view');
    const root = makeNodeProtoype('root');

    class AnimatableComponent extends TestComponent {
      onRender() {
        renderer.beginElement(element);
        renderer.setAttributeNumber('opacity', this.viewModel.opacity ?? 0);
        renderer.endElement();
      }
    }

    const prototypeAnimatableComponent = makeComponentPrototype();

    const ref = new ComponentRef<AnimatableComponent>();

    renderer.begin();
    renderer.beginElement(root);
    renderer.beginComponent(AnimatableComponent, prototypeAnimatableComponent, undefined, ref);
    renderer.endComponent();
    renderer.beginComponent(AnimatableComponent, prototypeAnimatableComponent, undefined, ref);
    renderer.endComponent();
    renderer.endElement();
    renderer.end();

    delegate.clear();

    const components = ref.all();

    expect(components.length).toBe(2);

    renderer.animate({ duration: 0.5 }, () => {
      renderer.renderComponent(components[0], { opacity: 0.5 });
      renderer.renderComponent(components[1], { opacity: 1.0 });
    });

    expect(delegate.requests).toEqual([
      {
        entries: [
          { type: RawRenderRequestEntryType.beginAnimations, options: { duration: 0.5 } },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 2,
            name: 'opacity',
            value: 0.5,
          },
          {
            type: RawRenderRequestEntryType.setElementAttribute,
            id: 3,
            name: 'opacity',
            value: 1.0,
          },
          { type: RawRenderRequestEntryType.endAnimations },
        ],
      },
    ]);
  });

  it('uses different components for different prototypes', () => {
    const renderer = makeRenderer(new RendererTestDelegate());
    const root = makeNodeProtoype('root');

    const alives: DummyComponent[] = [];

    class DummyComponent extends TestComponent {
      onCreate() {
        alives.push(this);
      }
      onDestroy() {
        const index = alives.indexOf(this);
        if (index > -1) {
          alives.splice(index, 1);
        }
      }
      onRender() {}
    }

    const prototypeDummyComponent1 = makeComponentPrototype();
    const prototypeDummyComponent2 = makeComponentPrototype();

    renderer.begin();
    renderer.beginElement(root);
    renderer.beginComponent(DummyComponent, prototypeDummyComponent1);
    renderer.endComponent();
    renderer.endElement();
    renderer.end();

    expect(alives.length).toBe(1);
    const alive1 = alives[0];

    renderer.begin();
    renderer.beginElement(root);
    renderer.beginComponent(DummyComponent, prototypeDummyComponent2);
    renderer.endComponent();
    renderer.endElement();
    renderer.end();

    expect(alives.length).toBe(1);
    const alive2 = alives[0];

    expect(alive1 !== alive2).toBe(true);
  });

  it('re-using component prototype re-use components', () => {
    const renderer = makeRenderer(new RendererTestDelegate());
    const root = makeNodeProtoype('root');

    const alives: DummyComponent[] = [];

    class DummyComponent extends TestComponent {
      onCreate() {
        alives.push(this);
      }
      onDestroy() {
        const index = alives.indexOf(this);
        if (index > -1) {
          alives.splice(index, 1);
        }
      }
      onRender() {}
    }

    const prototypeDummyComponent = makeComponentPrototype();

    renderer.begin();
    renderer.beginElement(root);
    renderer.beginComponent(DummyComponent, prototypeDummyComponent);
    renderer.endComponent();
    renderer.endElement();
    renderer.end();

    expect(alives.length).toBe(1);
    const alive1 = alives[0];

    renderer.begin();
    renderer.beginElement(root);
    renderer.beginComponent(DummyComponent, prototypeDummyComponent);
    renderer.endComponent();
    renderer.endElement();
    renderer.end();

    expect(alives.length).toBe(1);
    const alive2 = alives[0];

    expect(alive1 === alive2).toBe(true);
  });

  it('different component prototype (same component) cannot share the same key', () => {
    const renderer = makeRenderer(new RendererTestDelegate());
    const root = makeNodeProtoype('root');

    class DummyComponent extends TestComponent {}

    const prototypeDummyComponent1 = makeComponentPrototype();
    const prototypeDummyComponent2 = makeComponentPrototype();

    function renderFunc() {
      renderer.begin();
      renderer.beginElement(root);
      renderer.beginComponent(DummyComponent, prototypeDummyComponent1, 'same-key');
      renderer.endComponent();
      renderer.beginComponent(DummyComponent, prototypeDummyComponent2, 'same-key');
      renderer.endComponent();
      renderer.endElement();
      renderer.end();
    }

    expect(() => {
      renderFunc();
    }).toThrowError(
      "Duplicate Component 'DummyComponent' and 'DummyComponent' for resolved key 'same-key'. This can happen when two different components are added in the same parent with the same key, You can fix this issue by combining into a single <Component/> tag with conditional attributes instead.",
    );
  });

  it('should be possible to use multiple instance with same component prototype in the same render', () => {
    const renderer = makeRenderer(new RendererTestDelegate());
    const root = makeNodeProtoype('root');

    let createCount = 0;

    class DummyComponent extends TestComponent {
      onCreate() {
        createCount++;
      }
      onRender() {}
    }

    const prototypeDummyComponent = makeComponentPrototype();

    renderer.begin();
    renderer.beginElement(root);
    renderer.beginComponent(DummyComponent, prototypeDummyComponent);
    renderer.endComponent();
    renderer.beginComponent(DummyComponent, prototypeDummyComponent);
    renderer.endComponent();
    renderer.beginComponent(DummyComponent, prototypeDummyComponent);
    renderer.endComponent();
    renderer.endElement();
    renderer.end();

    expect(createCount).toBe(3);
  });

  it('should be possible to use multiple instance of the same component with different component prototype in the same render', () => {
    const renderer = makeRenderer(new RendererTestDelegate());
    const root = makeNodeProtoype('root');

    let createCount = 0;

    class DummyComponent extends TestComponent {
      onCreate() {
        createCount++;
      }
      onRender() {}
    }

    const prototypeDummyComponent1 = makeComponentPrototype();
    const prototypeDummyComponent2 = makeComponentPrototype();

    renderer.begin();
    renderer.beginElement(root);
    renderer.beginComponent(DummyComponent, prototypeDummyComponent1);
    renderer.endComponent();
    renderer.beginComponent(DummyComponent, prototypeDummyComponent1);
    renderer.endComponent();
    renderer.beginComponent(DummyComponent, prototypeDummyComponent2);
    renderer.endComponent();
    renderer.beginComponent(DummyComponent, prototypeDummyComponent2);
    renderer.endComponent();
    renderer.endElement();
    renderer.end();

    expect(createCount).toBe(4);
  });

  it('doesnt rerender when a callback changes', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);
    const parent = makeComponentPrototype();
    const root = makeNodeProtoype('root');

    class Component extends TestComponent {
      onRender() {
        super.onRender();

        renderer.beginElement(root);
        renderer.setAttribute('onTap', this.viewModel.callback);
        renderer.endElement();
      }
    }

    function render(fn: ((...params: any) => any) | undefined) {
      renderer.renderRoot(() => {
        renderer.beginComponent(Component, parent);
        renderer.setViewModelProperty('callback', fn ? createReusableCallback(fn) : undefined);
        renderer.endComponent();
      });
    }

    let current = 42;
    const callback = (increment: number) => {
      const oldValue = current;
      current += increment;
      return oldValue;
    };

    render(callback);

    expect(output.requests.length).toBe(1);

    expect(output.requests[0]?.entries[1]?.value instanceof Function).toBeTruthy();

    const attributeFn = output.requests[0].entries[1].value as (...params: any[]) => any;
    output.clear();

    const component = renderer.getRootComponent()! as TestComponent;

    expect(component.onRenderCount).toBe(1);

    // The submitted callback for the element should have been wrapped
    expect(attributeFn).not.toEqual(callback);

    // Calling the attribute callback should call back the original callback
    let result = attributeFn(1);
    expect(current).toBe(43);
    expect(result).toBe(42);

    // Re-rendering with the same callback
    render(callback);

    // Nothing should have changed
    expect(component.onRenderCount).toBe(1);
    expect(output.requests).toEqual([]);

    result = attributeFn(1);
    expect(current).toBe(44);
    expect(result).toBe(43);

    // Re-rendering with a different callback
    render(() => 'Nice');

    // The component should not have re-rendered
    expect(component.onRenderCount).toBe(1);
    expect(output.requests).toEqual([]);

    // But calling the attribute function should now call the new callback
    expect(attributeFn()).toBe('Nice');

    // When passing undefined
    render(undefined);

    // We should expect a rerender
    expect(component.onRenderCount).toBe(2);
    expect(output.requests).toEqual([
      {
        entries: [{ type: RawRenderRequestEntryType.setElementAttribute, id: 1, name: 'onTap', value: undefined }],
      },
    ]);
  });

  it('re-render when callback changes with a different owner', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);
    const parent = makeComponentPrototype();
    const child = makeComponentPrototype();

    class ChildComponent extends TestComponent {}

    class ParentComponent extends TestComponent {
      onRender() {
        super.onRender();

        renderer.beginComponent(ChildComponent, child);
        renderer.setViewModelProperty('callback', this.viewModel.callback);
        renderer.endComponent();
      }
    }

    function render(fn: () => void) {
      renderer.renderRoot(() => {
        renderer.beginComponent(ParentComponent, parent);
        renderer.setViewModelProperty('callback', fn);
        renderer.endComponent();
      });
    }

    const fn = createReusableCallback(() => {
      return 42;
    });

    render(fn);

    const parentComponent = renderer.getRootComponent()! as ParentComponent;
    const childComponent = renderer.getComponentChildren(parentComponent)[0] as ChildComponent;

    expect(parentComponent.onRenderCount).toBe(1);
    expect(childComponent.onRenderCount).toBe(1);
    expect(parentComponent.viewModel.callback).toBe(fn);
    expect(childComponent.viewModel.callback).toBe(fn);

    expect(fn()).toBe(42);

    const fn2 = createReusableCallback(() => {
      return 1337;
    });
    // Assign the callback with a different owner
    tryReuseCallback({}, undefined, fn2);

    render(fn2);

    expect(parentComponent.onRenderCount).toBe(2);
    expect(childComponent.onRenderCount).toBe(2);
    expect(parentComponent.viewModel.callback).toBe(fn2);
    expect(childComponent.viewModel.callback).toBe(fn2);
  });

  it('can call component disposable', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);

    let lambdaDisposeCallCount = 0;
    let unsubscribeCallCount = 0;

    class RootComponent extends TestComponent {
      onCreate(): void {
        super.onCreate();

        this.renderer.registerComponentDisposable(this, () => {
          lambdaDisposeCallCount++;
        });

        this.renderer.registerComponentDisposable(this, {
          unsubscribe: () => {
            unsubscribeCallCount++;
          },
        });
      }
    }

    const prototypeRootComponent = makeComponentPrototype();

    renderer.begin();
    renderer.beginComponent(RootComponent, prototypeRootComponent);
    renderer.endComponent();
    renderer.end();

    expect((renderer.getRootComponent() as RootComponent).onCreateCount).toBe(1);
    expect(lambdaDisposeCallCount).toBe(0);
    expect(unsubscribeCallCount).toBe(0);

    renderer.begin();
    renderer.end();

    expect(renderer.getRootComponent()).toBeUndefined();
    expect(lambdaDisposeCallCount).toBe(1);
    expect(unsubscribeCallCount).toBe(1);
  });

  it('notifies event listener', () => {
    const output = new RendererTestDelegate();
    const renderer = makeRenderer(output);
    const recorder = new RendererEventRecorder();
    renderer.setEventListener(recorder);

    const childComponentPrototype = new ComponentPrototype('__child');
    const rootComponentPrototype = new ComponentPrototype('__root');

    class ChildComponent extends TestComponent {
      onRender(): void {}
    }

    class RootComponent extends TestComponent {
      onRender(): void {
        for (const name of this.viewModel.names) {
          renderer.beginComponent(ChildComponent, childComponentPrototype);
          renderer.setViewModelProperty('name', name);
          renderer.endComponent();
        }
      }
    }

    const renderFn = (names: string[]) => {
      renderer.begin();
      renderer.beginComponent(RootComponent, rootComponentPrototype);
      renderer.setViewModelProperty('names', names);
      renderer.endComponent();
      renderer.end();
    };

    const usernames = ['Hello', 'World'];
    renderFn(usernames);

    expect(recorder.toString().trim()).toEqual(
      `
Begin render
  Begin RootComponent (key: __root)
    Begin ChildComponent (key: __child)
    End ChildComponent
    Begin ChildComponent (key: __child2)
    End ChildComponent
  End RootComponent
End render
    `.trim(),
    );
    recorder.clear();

    renderFn(usernames);
    expect(recorder.toString().trim()).toEqual(
      `
Begin render
  Begin RootComponent (key: __root)
    Bypass render
  End RootComponent
End render
    `.trim(),
    );
    recorder.clear();

    renderFn(['Goodbye', 'World']);

    expect(recorder.toString().trim()).toEqual(
      `
Begin render
  Begin RootComponent (key: __root)
    ViewModel property 'names' changed
    Begin ChildComponent (key: __child)
      ViewModel property 'name' changed
    End ChildComponent
    Begin ChildComponent (key: __child2)
      Bypass render
    End ChildComponent
  End RootComponent
End render
    `.trim(),
    );
    recorder.clear();
  });
});
