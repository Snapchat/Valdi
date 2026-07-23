import { NodePrototype } from 'valdi_core/src/NodePrototype';
import { remember } from 'valdi_core/src/Remember';
import { Renderer } from 'valdi_core/src/Renderer';
import 'jasmine/src/jasmine';
import { RendererTestDelegate } from './RendererTestDelegate';

interface TestVirtualNode {
  rememberState?: unknown;
  children?: {
    children: TestVirtualNode[];
  };
}

interface RememberedValue {
  id: number;
  itemKey?: string;
}

function makeRenderer(): Renderer {
  return new Renderer('', undefined, new RendererTestDelegate());
}

function makeNodePrototype(viewClass: string): NodePrototype {
  return new NodePrototype(viewClass, viewClass);
}

function hasRememberStorage(node: TestVirtualNode): boolean {
  if (Object.prototype.hasOwnProperty.call(node, 'rememberState')) {
    return true;
  }

  const children = node.children?.children;
  if (!children) {
    return false;
  }

  for (const child of children) {
    if (hasRememberStorage(child)) {
      return true;
    }
  }

  return false;
}

describe('remember', () => {
  it('does not allocate remember storage when unused', () => {
    const renderer = makeRenderer();
    const rootPrototype = makeNodePrototype('view');

    renderer.begin();
    renderer.beginElement(rootPrototype);
    renderer.endElement();
    renderer.end();

    expect(hasRememberStorage((renderer as any).nodeTree)).toBe(false);
  });

  it('persists values with no keys across renders of the same VirtualNode', () => {
    const renderer = makeRenderer();
    const rootPrototype = makeNodePrototype('view');
    let factoryCalls = 0;
    let renderedValue: RememberedValue | undefined;

    function render() {
      renderer.begin();
      renderer.beginElement(rootPrototype);
      renderedValue = remember(() => ({ id: ++factoryCalls }));
      renderer.endElement();
      renderer.end();
    }

    render();
    const firstValue = renderedValue;
    render();

    expect(renderedValue).toBe(firstValue);
    expect(factoryCalls).toBe(1);
  });

  it('compares keys with Object.is', () => {
    const renderer = makeRenderer();
    const rootPrototype = makeNodePrototype('view');
    let factoryCalls = 0;
    let renderedValue = 0;

    function render(key: unknown) {
      renderer.begin();
      renderer.beginElement(rootPrototype);
      renderedValue = remember(() => ++factoryCalls, key);
      renderer.endElement();
      renderer.end();
    }

    render(NaN);
    expect(renderedValue).toBe(1);

    render(NaN);
    expect(renderedValue).toBe(1);
    expect(factoryCalls).toBe(1);

    render(0);
    expect(renderedValue).toBe(2);

    render(-0);
    expect(renderedValue).toBe(3);
    expect(factoryCalls).toBe(3);
  });

  it('keeps multiple calls in the same VirtualNode independent', () => {
    const renderer = makeRenderer();
    const rootPrototype = makeNodePrototype('view');
    let firstFactoryCalls = 0;
    let secondFactoryCalls = 0;
    let firstValue: RememberedValue | undefined;
    let secondValue: RememberedValue | undefined;

    function render() {
      renderer.begin();
      renderer.beginElement(rootPrototype);
      firstValue = remember(() => ({ id: ++firstFactoryCalls }), 'same-key');
      secondValue = remember(() => ({ id: ++secondFactoryCalls }), 'same-key');
      renderer.endElement();
      renderer.end();
    }

    render();
    const initialFirstValue = firstValue;
    const initialSecondValue = secondValue;
    render();

    expect(firstValue).toBe(initialFirstValue);
    expect(secondValue).toBe(initialSecondValue);
    expect(firstValue).not.toBe(secondValue);
    expect(firstFactoryCalls).toBe(1);
    expect(secondFactoryCalls).toBe(1);
  });

  it('scopes remembered values to keyed VirtualNodes across sibling reorders', () => {
    const renderer = makeRenderer();
    const rootPrototype = makeNodePrototype('view');
    const itemPrototype = makeNodePrototype('label');
    let nextId = 0;

    function render(keys: string[]): { [key: string]: RememberedValue } {
      const values: { [key: string]: RememberedValue } = {};

      renderer.begin();
      renderer.beginElement(rootPrototype);
      for (const key of keys) {
        renderer.beginElement(itemPrototype, key);
        values[key] = remember(() => ({ id: ++nextId, itemKey: key }));
        renderer.endElement();
      }
      renderer.endElement();
      renderer.end();

      return values;
    }

    const firstRender = render(['one', 'two', 'three']);
    const secondRender = render(['three', 'one', 'two']);

    expect(secondRender.one).toBe(firstRender.one);
    expect(secondRender.two).toBe(firstRender.two);
    expect(secondRender.three).toBe(firstRender.three);
    expect(nextId).toBe(3);
  });

  it('forgets values when a VirtualNode is removed', () => {
    const renderer = makeRenderer();
    const rootPrototype = makeNodePrototype('view');
    const itemPrototype = makeNodePrototype('label');
    let factoryCalls = 0;

    function render(showItem: boolean): RememberedValue | undefined {
      let value: RememberedValue | undefined;

      renderer.begin();
      renderer.beginElement(rootPrototype);
      if (showItem) {
        renderer.beginElement(itemPrototype, 'item');
        value = remember(() => ({ id: ++factoryCalls }));
        renderer.endElement();
      }
      renderer.endElement();
      renderer.end();

      return value;
    }

    const firstValue = render(true);
    render(false);
    const secondValue = render(true);

    expect(firstValue).toBeDefined();
    expect(secondValue).toBeDefined();
    expect(secondValue).not.toBe(firstValue);
    expect(factoryCalls).toBe(2);
  });

  it('throws when called outside a render', () => {
    let error: unknown;
    try {
      remember(() => 1);
    } catch (err: unknown) {
      error = err;
    }

    expect(String(error)).toContain('Cannot call this outside of a onRender callback');
  });
});
