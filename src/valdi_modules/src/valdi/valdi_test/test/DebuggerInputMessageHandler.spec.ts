import { IRenderedElement } from 'valdi_core/src/IRenderedElement';
import { IRenderedVirtualNode } from 'valdi_core/src/IRenderedVirtualNode';
import { IRenderer } from 'valdi_core/src/IRenderer';
import { DebuggerInputMessageHandler, DebuggerInputType } from 'valdi_core/src/debugging/DebuggerInputMessageHandler';
import 'jasmine/src/jasmine';

const DEBUGGER_INPUT_IDENTIFIER = 'ValdiDebuggerInput';

class TestElement {
  readonly viewClass = 'TestView';
  readonly key: string;
  readonly children: IRenderedElement[] = [];
  readonly frame = { x: 10, y: 20, width: 100, height: 40 };
  parent: IRenderedElement | undefined;
  parentIndex = 0;
  renderer: IRenderer = undefined!;
  emittingComponent = undefined;

  constructor(
    readonly id: number,
    readonly tag: string,
    private readonly attributes: { [name: string]: any },
  ) {
    this.key = `${tag}-${id}`;
  }

  getAttributeNames(): string[] {
    return Object.keys(this.attributes);
  }

  getAttribute(name: string): any {
    return this.attributes[name];
  }

  setAttribute(name: string, value: any): boolean {
    this.attributes[name] = value;
    return true;
  }

  setAttributes(attributes: { [name: string]: any }): boolean {
    Object.keys(attributes).forEach(name => {
      this.attributes[name] = attributes[name];
    });
    return true;
  }

  getVirtualNode(): IRenderedVirtualNode {
    throw new Error('Not implemented by debugger input test element.');
  }

  getNativeView(): Promise<undefined> {
    return Promise.resolve(undefined);
  }

  getNativeNode(): undefined {
    return undefined;
  }

  takeSnapshot(): Promise<undefined> {
    return Promise.resolve(undefined);
  }

  asRenderedElement(): IRenderedElement {
    return this as unknown as IRenderedElement;
  }
}

class ThrowingSetAttributesElement extends TestElement {
  setAttributes(_attributes: { [name: string]: any }): boolean {
    throw new Error('internal setAttributes failed');
  }
}

function makeNode(element: TestElement | undefined, children: IRenderedVirtualNode[]): IRenderedVirtualNode {
  return {
    key: element?.key ?? 'root',
    parent: undefined,
    element: element?.asRenderedElement(),
    component: undefined,
    children,
    parentIndex: 0,
    uniqueId: element?.key ?? 'root',
  } as IRenderedVirtualNode;
}

function makeHandler(root: IRenderedVirtualNode): DebuggerInputMessageHandler {
  const elementsById = new Map<number, IRenderedElement>();
  const pending = [root];
  while (pending.length) {
    const node = pending.pop()!;
    if (node.element) {
      elementsById.set(node.element.id, node.element);
    }
    pending.push(...node.children);
  }
  const renderer = {
    getRootVirtualNode: () => root,
    getElementForId: (elementId: number) => elementsById.get(elementId),
  } as unknown as IRenderer;
  return new DebuggerInputMessageHandler(contextId => (contextId === 'context-1' ? renderer : undefined));
}

async function send(
  handler: DebuggerInputMessageHandler,
  request: { [name: string]: any },
): Promise<{ [name: string]: any }> {
  const response = handler.messageReceived(DEBUGGER_INPUT_IDENTIFIER, request);
  if (!response) {
    throw new Error('Expected debugger input handler to accept the request.');
  }
  return await response;
}

function containsLoneSurrogate(value: string): boolean {
  for (let index = 0; index < value.length; index += 1) {
    const codeUnit = value.charCodeAt(index);
    if (codeUnit >= 0xd800 && codeUnit <= 0xdbff) {
      if (index + 1 >= value.length) {
        return true;
      }
      const nextCodeUnit = value.charCodeAt(index + 1);
      if (nextCodeUnit < 0xdc00 || nextCodeUnit > 0xdfff) {
        return true;
      }
      index += 1;
    } else if (codeUnit >= 0xdc00 && codeUnit <= 0xdfff) {
      return true;
    }
  }
  return false;
}

describe('DebuggerInputMessageHandler', () => {
  it('reports its stable contract and supported operations', async () => {
    const handler = makeHandler(makeNode(undefined, []));

    const response = await send(handler, { type: DebuggerInputType.Capabilities });

    expect(response['handled']).toBeTrue();
    expect(response['contractVersion']).toBe(1);
    expect(response['supportedTypes']).toContain(DebuggerInputType.Query);
    expect(response['supportedTypes']).toContain(DebuggerInputType.Key);
  });

  it('queries elements by accessibilityId with typed automation metadata', async () => {
    const input = new TestElement(7, 'textfield', {
      accessibilityId: 'composer',
      accessibilityCategory: 'input',
      accessibilityLabel: 'Message',
      value: 'hello',
      focused: true,
      accessibilityStateSelected: true,
    });
    const handler = makeHandler(makeNode(undefined, [makeNode(input, [])]));

    const response = await send(handler, {
      type: DebuggerInputType.Query,
      contextId: 'context-1',
      selector: '#composer',
    });

    expect(response['handled']).toBeTrue();
    expect(response['elements']).toEqual([
      jasmine.objectContaining({
        elementId: 7,
        tag: 'textfield',
        accessibilityId: 'composer',
        accessibilityCategory: 'input',
        accessibilityLabel: 'Message',
        accessibilityValue: 'hello',
        selected: true,
        enabled: true,
        focused: true,
        actions: jasmine.arrayContaining([DebuggerInputType.Focus, DebuggerInputType.Text, DebuggerInputType.Key]),
      }),
    ]);
  });

  it('resolves numeric element IDs through the renderer without traversing the tree', async () => {
    const input = new TestElement(70, 'textfield', {
      accessibilityId: 'direct-input',
      value: 'direct',
    });
    const getElementForId = jasmine.createSpy('getElementForId').and.returnValue(input.asRenderedElement());
    const getRootVirtualNode = jasmine.createSpy('getRootVirtualNode').and.callFake(() => {
      throw new Error('numeric element lookup must not traverse the tree');
    });
    const renderer = { getElementForId, getRootVirtualNode } as unknown as IRenderer;
    const handler = new DebuggerInputMessageHandler(contextId => (contextId === 'context-1' ? renderer : undefined));

    const response = await send(handler, {
      type: DebuggerInputType.Query,
      contextId: 'context-1',
      selector: { elementId: 70, tag: 'textfield' },
    });

    expect(response['handled']).toBeTrue();
    expect(response['elements']).toEqual([jasmine.objectContaining({ elementId: 70, tag: 'textfield' })]);
    expect(getElementForId).toHaveBeenCalledOnceWith(70);
    expect(getRootVirtualNode).not.toHaveBeenCalled();
  });

  it('returns a structured failure for a cyclic direct-ID parent chain', async () => {
    const first = new TestElement(72, 'view', {});
    const second = new TestElement(73, 'view', {});
    first.parent = second.asRenderedElement();
    second.parent = first.asRenderedElement();
    const getRootVirtualNode = jasmine.createSpy('getRootVirtualNode').and.callFake(() => {
      throw new Error('numeric element lookup must not traverse the tree');
    });
    const renderer = {
      getElementForId: () => first.asRenderedElement(),
      getRootVirtualNode,
    } as unknown as IRenderer;
    const handler = new DebuggerInputMessageHandler(contextId => (contextId === 'context-1' ? renderer : undefined));

    const response = await send(handler, {
      type: DebuggerInputType.Query,
      contextId: 'context-1',
      elementId: first.id,
    });

    expect(response).toEqual(
      jasmine.objectContaining({
        handled: false,
        message: 'Debugger input element ancestry contains a cycle.',
      }),
    );
    expect(getRootVirtualNode).not.toHaveBeenCalled();
  });

  it('returns a structured failure for an over-deep direct-ID parent chain', async () => {
    const target = new TestElement(74, 'view', {});
    let child = target;
    for (let index = 0; index < 20001; index += 1) {
      const parent = new TestElement(1000 + index, 'view', {});
      child.parent = parent.asRenderedElement();
      child = parent;
    }
    const getRootVirtualNode = jasmine.createSpy('getRootVirtualNode').and.callFake(() => {
      throw new Error('numeric element lookup must not traverse the tree');
    });
    const renderer = {
      getElementForId: () => target.asRenderedElement(),
      getRootVirtualNode,
    } as unknown as IRenderer;
    const handler = new DebuggerInputMessageHandler(contextId => (contextId === 'context-1' ? renderer : undefined));

    const response = await send(handler, {
      type: DebuggerInputType.Query,
      contextId: 'context-1',
      elementId: target.id,
    });

    expect(response).toEqual(
      jasmine.objectContaining({
        handled: false,
        message: 'Debugger input traversal exceeds the 20000-node work limit.',
      }),
    );
    expect(getRootVirtualNode).not.toHaveBeenCalled();
  });

  it('queries a deeply nested tree without recursive traversal', async () => {
    const target = new TestElement(71, 'view', { accessibilityId: 'deep-target' });
    let root = makeNode(target, []);
    for (let index = 0; index < 12000; index += 1) {
      root = makeNode(undefined, [root]);
    }
    const handler = makeHandler(root);

    const response = await send(handler, {
      type: DebuggerInputType.Query,
      contextId: 'context-1',
      accessibilityId: 'deep-target',
    });

    expect(response['handled']).toBeTrue();
    expect(response['elements']).toEqual([jasmine.objectContaining({ elementId: 71 })]);
  });

  it('describes every element in a deep tree with linear parent work', async () => {
    const elementCount = 2000;
    const elements: TestElement[] = [];
    let parent: IRenderedElement | undefined;
    let parentReadCount = 0;
    for (let index = 0; index < elementCount; index += 1) {
      const element = new TestElement(10000 + index, 'view', {});
      const capturedParent = parent;
      Object.defineProperty(element, 'parent', {
        configurable: true,
        get: () => {
          parentReadCount += 1;
          return capturedParent;
        },
      });
      elements.push(element);
      parent = element.asRenderedElement();
    }
    let root = makeNode(elements[elementCount - 1], []);
    for (let index = elementCount - 2; index >= 0; index -= 1) {
      root = makeNode(elements[index], [root]);
    }
    const handler = makeHandler(root);

    const response = await send(handler, {
      type: DebuggerInputType.Query,
      contextId: 'context-1',
    });

    expect(response['handled']).toBeTrue();
    const descriptors = response['elements'] as Array<{ absoluteFrame: { x: number; y: number } }>;
    expect(descriptors.length).toBe(elementCount);
    expect(descriptors[elementCount - 1]!.absoluteFrame).toEqual(jasmine.objectContaining({ x: 20000, y: 40000 }));
    expect(parentReadCount).toBeLessThanOrEqual(elementCount + 1);
  });

  it('memoizes absolute frames with parent offsets and translations', async () => {
    const parent = new TestElement(76, 'view', {
      contentOffsetX: 2,
      contentOffsetY: 5,
      translationX: 3,
      translationY: 1,
    });
    const child = new TestElement(77, 'view', {
      accessibilityId: 'positioned-child',
      translationX: 4,
      translationY: 2,
    });
    child.parent = parent.asRenderedElement();
    const handler = makeHandler(makeNode(parent, [makeNode(child, [])]));

    const response = await send(handler, {
      type: DebuggerInputType.Query,
      contextId: 'context-1',
      accessibilityId: 'positioned-child',
    });

    expect(response['elements']).toEqual([
      jasmine.objectContaining({
        elementId: child.id,
        parentElementId: parent.id,
        absoluteFrame: jasmine.objectContaining({ x: 25, y: 38 }),
      }),
    ]);
  });

  it('shares one work budget between virtual nodes and off-tree parents', async () => {
    const target = new TestElement(75, 'view', { accessibilityId: 'globally-bounded' });
    let child = target;
    for (let index = 0; index < 10000; index += 1) {
      const parent = new TestElement(40000 + index, 'view', {});
      child.parent = parent.asRenderedElement();
      child = parent;
    }
    let root = makeNode(target, []);
    for (let index = 0; index < 11000; index += 1) {
      root = makeNode(undefined, [root]);
    }
    const handler = makeHandler(root);

    const response = await send(handler, {
      type: DebuggerInputType.Query,
      contextId: 'context-1',
      accessibilityId: 'globally-bounded',
    });

    expect(response).toEqual(
      jasmine.objectContaining({
        handled: false,
        message: 'Debugger input traversal exceeds the 20000-node work limit.',
      }),
    );
  });

  it('returns a structured failure when selector traversal exceeds its node budget', async () => {
    let root = makeNode(undefined, []);
    for (let index = 0; index < 20001; index += 1) {
      root = makeNode(undefined, [root]);
    }
    const handler = makeHandler(root);

    const response = await send(handler, {
      type: DebuggerInputType.Query,
      contextId: 'context-1',
      accessibilityId: 'missing',
    });

    expect(response['handled']).toBeFalse();
    expect(response['message']).toBe('Debugger input traversal exceeds the 20000-node work limit.');
  });

  it('rejects an over-wide tree before reading or queuing its children', async () => {
    let childReadCount = 0;
    const children = new Proxy([] as IRenderedVirtualNode[], {
      get(target, property): unknown {
        if (property === 'length') return 20001;
        childReadCount += 1;
        throw new Error(`unexpected child read: ${String(property)}`);
      },
    });
    const root = makeNode(undefined, children);
    const renderer = {
      getElementForId: () => undefined,
      getRootVirtualNode: () => root,
    } as unknown as IRenderer;
    const handler = new DebuggerInputMessageHandler(contextId => (contextId === 'context-1' ? renderer : undefined));

    const response = await send(handler, {
      type: DebuggerInputType.Query,
      contextId: 'context-1',
      accessibilityId: 'missing',
    });

    expect(response['handled']).toBeFalse();
    expect(response['message']).toBe('Debugger input traversal exceeds the 20000-node work limit.');
    expect(childReadCount).toBe(0);
  });

  it('rejects cyclic virtual trees without revisiting a node', async () => {
    const root = makeNode(undefined, []);
    root.children.push(root);
    const renderer = {
      getElementForId: () => undefined,
      getRootVirtualNode: () => root,
    } as unknown as IRenderer;
    const handler = new DebuggerInputMessageHandler(contextId => (contextId === 'context-1' ? renderer : undefined));

    const response = await send(handler, {
      type: DebuggerInputType.Query,
      contextId: 'context-1',
      accessibilityId: 'missing',
    });

    expect(response).toEqual(
      jasmine.objectContaining({
        handled: false,
        message: 'Debugger input render tree contains a cycle or repeated node.',
      }),
    );
  });

  it('dispatches a tap through the nearest rendered onTap callback', async () => {
    const onTap = jasmine.createSpy('onTap');
    const button = new TestElement(2, 'view', { accessibilityId: 'send', onTap });
    const label = new TestElement(3, 'label', { value: 'Send' });
    label.parent = button.asRenderedElement();
    button.children.push(label.asRenderedElement());
    const handler = makeHandler(makeNode(undefined, [makeNode(button, [makeNode(label, [])])]));

    const response = await send(handler, {
      type: DebuggerInputType.Tap,
      contextId: 'context-1',
      accessibilityId: 'send',
    });

    expect(response).toEqual(
      jasmine.objectContaining({
        handled: true,
        elementId: 2,
        action: 'onTap',
        actionElementId: 2,
      }),
    );
    expect(onTap).toHaveBeenCalledTimes(1);
    expect(onTap).toHaveBeenCalledWith(
      jasmine.objectContaining({
        pointerCount: 1,
        absoluteX: 60,
        absoluteY: 40,
      }),
    );
  });

  it('rejects taps when the target or an ancestor disables touch input', async () => {
    const targetOnTap = jasmine.createSpy('targetOnTap');
    const target = new TestElement(19, 'view', {
      accessibilityId: 'touch-disabled-target',
      onTap: targetOnTap,
      touchEnabled: false,
    });
    const ancestorOnTap = jasmine.createSpy('ancestorOnTap');
    const ancestor = new TestElement(20, 'view', {
      accessibilityId: 'touch-disabled-ancestor',
      onTap: ancestorOnTap,
      touchEnabled: false,
    });
    const descendant = new TestElement(21, 'label', { accessibilityId: 'touch-disabled-descendant' });
    descendant.parent = ancestor.asRenderedElement();
    ancestor.children.push(descendant.asRenderedElement());
    const handler = makeHandler(
      makeNode(undefined, [makeNode(target, []), makeNode(ancestor, [makeNode(descendant, [])])]),
    );

    const targetResponse = await send(handler, {
      type: DebuggerInputType.Tap,
      contextId: 'context-1',
      accessibilityId: 'touch-disabled-target',
    });
    const ancestorResponse = await send(handler, {
      type: DebuggerInputType.Tap,
      contextId: 'context-1',
      accessibilityId: 'touch-disabled-descendant',
    });

    expect(targetResponse).toEqual(
      jasmine.objectContaining({
        handled: false,
        elementId: 19,
        actionElementId: 19,
        message: 'Element 19 has touchEnabled=false.',
      }),
    );
    expect(ancestorResponse).toEqual(
      jasmine.objectContaining({
        handled: false,
        elementId: 21,
        actionElementId: 20,
        message: 'Element 20 has touchEnabled=false.',
      }),
    );
    expect(targetOnTap).not.toHaveBeenCalled();
    expect(ancestorOnTap).not.toHaveBeenCalled();
  });

  it('focuses and edits text inputs while preserving selection metadata', async () => {
    const onChange = jasmine.createSpy('onChange');
    const input = new TestElement(4, 'textfield', {
      accessibilityId: 'composer',
      value: '',
      onChange,
    });
    const handler = makeHandler(makeNode(undefined, [makeNode(input, [])]));

    const response = await send(handler, {
      type: DebuggerInputType.Text,
      contextId: 'context-1',
      selector: { accessibilityId: 'composer', tag: 'textfield' },
      text: 'draft',
      selectionStart: 5,
      selectionEnd: 5,
    });

    expect(response).toEqual(
      jasmine.objectContaining({
        handled: true,
        elementId: 4,
        value: 'draft',
        selectionStart: 5,
        selectionEnd: 5,
      }),
    );
    expect(input.getAttribute('focused')).toBeTrue();
    expect(input.getAttribute('value')).toBe('draft');
    expect(input.getAttribute('selection')).toEqual([5, 5]);
    expect(onChange).toHaveBeenCalledWith({
      text: 'draft',
      selectionStart: 5,
      selectionEnd: 5,
    });
  });

  it('dispatches return keys and relative scrolling', async () => {
    const onReturn = jasmine.createSpy('onReturn');
    const input = new TestElement(5, 'textfield', {
      accessibilityId: 'composer',
      value: 'send this',
      focused: true,
      onReturn,
    });
    const scroll = new TestElement(6, 'scroll', {
      accessibilityId: 'messages',
      contentOffsetX: 2,
      contentOffsetY: 10,
    });
    const handler = makeHandler(makeNode(undefined, [makeNode(input, []), makeNode(scroll, [])]));

    const keyResponse = await send(handler, {
      type: DebuggerInputType.Key,
      contextId: 'context-1',
      accessibilityId: 'composer',
      key: 'Enter',
    });
    const scrollResponse = await send(handler, {
      type: DebuggerInputType.Scroll,
      contextId: 'context-1',
      accessibilityId: 'messages',
      deltaX: 3,
      deltaY: 25,
    });

    expect(keyResponse['handled']).toBeTrue();
    expect(keyResponse['action']).toBe('onReturn');
    expect(onReturn).toHaveBeenCalledWith({
      text: 'send this',
      selectionStart: 9,
      selectionEnd: 9,
    });
    expect(input.getAttribute('focused')).toBeFalse();
    expect(scrollResponse).toEqual(
      jasmine.objectContaining({
        handled: true,
        actionElementId: 6,
        contentOffsetX: 5,
        contentOffsetY: 35,
      }),
    );
    expect(scroll.getAttribute('contentOffsetX')).toBe(5);
    expect(scroll.getAttribute('contentOffsetY')).toBe(35);
  });

  it('uses platform text input defaults when return-key closing is not configured', async () => {
    const fieldOnReturn = jasmine.createSpy('fieldOnReturn');
    const viewOnReturn = jasmine.createSpy('viewOnReturn');
    const field = new TestElement(22, 'textfield', {
      accessibilityId: 'default-field',
      focused: true,
      value: 'field',
      onReturn: fieldOnReturn,
    });
    const view = new TestElement(23, 'textview', {
      accessibilityId: 'default-view',
      focused: true,
      value: 'view',
      onReturn: viewOnReturn,
    });
    const handler = makeHandler(makeNode(undefined, [makeNode(field, []), makeNode(view, [])]));

    const fieldResponse = await send(handler, {
      type: DebuggerInputType.Key,
      contextId: 'context-1',
      accessibilityId: 'default-field',
      key: 'Enter',
    });
    const viewResponse = await send(handler, {
      type: DebuggerInputType.Key,
      contextId: 'context-1',
      accessibilityId: 'default-view',
      key: 'Enter',
    });

    expect(fieldResponse['handled']).toBeTrue();
    expect(viewResponse['handled']).toBeTrue();
    expect(field.getAttribute('focused')).toBeFalse();
    expect(view.getAttribute('focused')).toBeTrue();
    expect(view.getAttribute('value')).toBe('view\n');
    expect(fieldOnReturn).toHaveBeenCalledTimes(1);
    expect(viewOnReturn).toHaveBeenCalledTimes(1);
    expect(viewOnReturn).toHaveBeenCalledWith({ text: 'view\n', selectionStart: 5, selectionEnd: 5 });
  });

  it('matches multiline Return insertion, ignore-newline, close, and callback ordering', async () => {
    const callbackOrder: string[] = [];
    const multiline = new TestElement(72, 'textview', {
      accessibilityId: 'multiline-return',
      value: 'ab',
      selection: [1, 1],
      focused: true,
      closesWhenReturnKeyPressed: true,
      onWillChange: (event: { text: string }) => {
        callbackOrder.push(`will:${event.text}`);
      },
      onChange: (event: { text: string }) => {
        callbackOrder.push(`change:${event.text}`);
      },
      onReturn: (event: { text: string }) => {
        callbackOrder.push(`return:${event.text}:${String(multiline.getAttribute('focused'))}`);
      },
    });
    const ignoreNewlines = new TestElement(73, 'textview', {
      accessibilityId: 'ignore-return',
      value: 'unchanged',
      selection: [9, 9],
      focused: true,
      ignoreNewlines: true,
      closesWhenReturnKeyPressed: false,
    });
    const handler = makeHandler(makeNode(undefined, [makeNode(multiline, []), makeNode(ignoreNewlines, [])]));

    const multilineResponse = await send(handler, {
      type: DebuggerInputType.Key,
      contextId: 'context-1',
      elementId: 72,
      key: 'Enter',
    });
    const ignoredResponse = await send(handler, {
      type: DebuggerInputType.Key,
      contextId: 'context-1',
      elementId: 73,
      key: 'Return',
    });

    expect(multilineResponse).toEqual(
      jasmine.objectContaining({ handled: true, value: 'a\nb', selectionStart: 2, selectionEnd: 2 }),
    );
    expect(multiline.getAttribute('focused')).toBeFalse();
    expect(callbackOrder).toEqual(['will:a\nb', 'change:a\nb', 'return:a\nb:false']);
    expect(ignoredResponse).toEqual(
      jasmine.objectContaining({
        handled: true,
        action: 'return',
        value: 'unchanged',
        selectionStart: 9,
        selectionEnd: 9,
      }),
    );
    expect(ignoreNewlines.getAttribute('value')).toBe('unchanged');
    expect(ignoreNewlines.getAttribute('focused')).toBeTrue();
  });

  it('distinguishes backward and forward deletion at a collapsed caret', async () => {
    const onWillDelete = jasmine.createSpy('onWillDelete');
    const backspaceInput = new TestElement(10, 'textfield', {
      accessibilityId: 'backspace-input',
      value: 'abcd',
      selection: [2, 2],
      onWillDelete,
    });
    const deleteInput = new TestElement(11, 'textfield', {
      accessibilityId: 'delete-input',
      value: 'abcd',
      selection: [2, 2],
      onWillDelete,
    });
    const handler = makeHandler(makeNode(undefined, [makeNode(backspaceInput, []), makeNode(deleteInput, [])]));

    const backspaceResponse = await send(handler, {
      type: DebuggerInputType.Key,
      contextId: 'context-1',
      accessibilityId: 'backspace-input',
      key: 'Backspace',
    });
    const deleteResponse = await send(handler, {
      type: DebuggerInputType.Key,
      contextId: 'context-1',
      accessibilityId: 'delete-input',
      key: 'Delete',
    });

    expect(backspaceResponse).toEqual(
      jasmine.objectContaining({
        handled: true,
        value: 'acd',
        selectionStart: 1,
        selectionEnd: 1,
      }),
    );
    expect(deleteResponse).toEqual(
      jasmine.objectContaining({
        handled: true,
        value: 'abd',
        selectionStart: 2,
        selectionEnd: 2,
      }),
    );
    expect(onWillDelete).toHaveBeenCalledTimes(2);
  });

  it('inserts and deletes Unicode code points without splitting surrogate pairs', async () => {
    const insertInput = new TestElement(24, 'textfield', {
      accessibilityId: 'unicode-insert',
      value: 'ab',
      selection: [1, 1],
    });
    const backspaceInput = new TestElement(25, 'textfield', {
      accessibilityId: 'unicode-backspace',
      value: 'a😀b',
      selection: [3, 3],
    });
    const deleteInput = new TestElement(26, 'textfield', {
      accessibilityId: 'unicode-delete',
      value: 'a😀b',
      selection: [1, 1],
    });
    const splitSelectionInput = new TestElement(27, 'textfield', {
      accessibilityId: 'unicode-split-selection',
      value: 'a😀b',
      selection: [2, 3],
    });
    const handler = makeHandler(
      makeNode(undefined, [
        makeNode(insertInput, []),
        makeNode(backspaceInput, []),
        makeNode(deleteInput, []),
        makeNode(splitSelectionInput, []),
      ]),
    );

    const insertResponse = await send(handler, {
      type: DebuggerInputType.Key,
      contextId: 'context-1',
      accessibilityId: 'unicode-insert',
      key: '😀',
    });
    const backspaceResponse = await send(handler, {
      type: DebuggerInputType.Key,
      contextId: 'context-1',
      accessibilityId: 'unicode-backspace',
      key: 'Backspace',
    });
    const deleteResponse = await send(handler, {
      type: DebuggerInputType.Key,
      contextId: 'context-1',
      accessibilityId: 'unicode-delete',
      key: 'Delete',
    });
    const splitSelectionResponse = await send(handler, {
      type: DebuggerInputType.Key,
      contextId: 'context-1',
      accessibilityId: 'unicode-split-selection',
      key: 'Backspace',
    });

    expect(insertResponse).toEqual(
      jasmine.objectContaining({ handled: true, value: 'a😀b', selectionStart: 3, selectionEnd: 3 }),
    );
    expect(backspaceResponse).toEqual(
      jasmine.objectContaining({ handled: true, value: 'ab', selectionStart: 1, selectionEnd: 1 }),
    );
    expect(deleteResponse).toEqual(
      jasmine.objectContaining({ handled: true, value: 'ab', selectionStart: 1, selectionEnd: 1 }),
    );
    expect(splitSelectionResponse).toEqual(
      jasmine.objectContaining({ handled: true, value: 'ab', selectionStart: 1, selectionEnd: 1 }),
    );
    for (const input of [insertInput, backspaceInput, deleteInput, splitSelectionInput]) {
      expect(containsLoneSurrogate(input.getAttribute('value'))).toBeFalse();
    }
  });

  it('rejects lone-surrogate key payloads without mutating text', async () => {
    const input = new TestElement(28, 'textfield', {
      accessibilityId: 'invalid-unicode-key',
      value: 'safe',
      selection: [4, 4],
    });
    const handler = makeHandler(makeNode(undefined, [makeNode(input, [])]));

    const response = await send(handler, {
      type: DebuggerInputType.Key,
      contextId: 'context-1',
      accessibilityId: 'invalid-unicode-key',
      key: String.fromCharCode(0xd83d),
    });

    expect(response['handled']).toBeFalse();
    expect(input.getAttribute('value')).toBe('safe');
    expect(input.getAttribute('selection')).toEqual([4, 4]);
    expect(containsLoneSurrogate(input.getAttribute('value'))).toBeFalse();
  });

  it('inserts and deletes whole grapheme clusters', async () => {
    const input = new TestElement(74, 'textfield', {
      accessibilityId: 'grapheme-input',
      value: '',
      selection: [0, 0],
    });
    const handler = makeHandler(makeNode(undefined, [makeNode(input, [])]));
    const graphemes = ['e\u0301', '👍🏽', '🇺🇸', '✈️', '👨‍👩‍👧‍👦'];

    for (const grapheme of graphemes) {
      input.setAttributes({ value: `a${grapheme}b`, selection: [1 + grapheme.length, 1 + grapheme.length] });
      const backspaceResponse = await send(handler, {
        type: DebuggerInputType.Key,
        contextId: 'context-1',
        elementId: 74,
        key: 'Backspace',
      });
      expect(backspaceResponse).toEqual(
        jasmine.objectContaining({ handled: true, value: 'ab', selectionStart: 1, selectionEnd: 1 }),
      );

      input.setAttributes({ value: `a${grapheme}b`, selection: [1, 1] });
      const deleteResponse = await send(handler, {
        type: DebuggerInputType.Key,
        contextId: 'context-1',
        elementId: 74,
        key: 'Delete',
      });
      expect(deleteResponse).toEqual(
        jasmine.objectContaining({ handled: true, value: 'ab', selectionStart: 1, selectionEnd: 1 }),
      );

      input.setAttributes({ value: 'ab', selection: [1, 1] });
      const insertResponse = await send(handler, {
        type: DebuggerInputType.Key,
        contextId: 'context-1',
        elementId: 74,
        key: grapheme,
      });
      expect(insertResponse).toEqual(
        jasmine.objectContaining({
          handled: true,
          value: `a${grapheme}b`,
          selectionStart: 1 + grapheme.length,
          selectionEnd: 1 + grapheme.length,
        }),
      );
      expect(containsLoneSurrogate(input.getAttribute('value'))).toBeFalse();
    }
  });

  it('does not split a grapheme cluster after a large text prefix', async () => {
    const globalObject = globalThis as { Intl?: typeof Intl };
    const savedIntl = globalObject.Intl;
    const prefix = 'a'.repeat(1000001);
    const grapheme = '👨‍👩‍👧‍👦';
    const input = new TestElement(82, 'textfield', {
      accessibilityId: 'large-grapheme-input',
      value: `${prefix}${grapheme}b`,
      selection: [prefix.length + grapheme.length, prefix.length + grapheme.length],
    });
    const handler = makeHandler(makeNode(undefined, [makeNode(input, [])]));

    try {
      globalObject.Intl = undefined;
      const response = await send(handler, {
        type: DebuggerInputType.Key,
        contextId: 'context-1',
        elementId: 82,
        key: 'Backspace',
      });

      expect(response).toEqual(
        jasmine.objectContaining({
          handled: true,
          value: `${prefix}b`,
          selectionStart: prefix.length,
          selectionEnd: prefix.length,
        }),
      );
      expect(containsLoneSurrogate(input.getAttribute('value'))).toBeFalse();
    } finally {
      globalObject.Intl = savedIntl;
    }
  });

  it('rejects lone-surrogate full text and existing values before mutation', async () => {
    const input = new TestElement(75, 'textfield', {
      accessibilityId: 'unicode-validation',
      value: 'safe',
      selection: [4, 4],
    });
    const handler = makeHandler(makeNode(undefined, [makeNode(input, [])]));
    const invalidUnicode = String.fromCharCode(0xd83d);

    for (const request of [
      { type: DebuggerInputType.Text, elementId: 75, text: invalidUnicode },
      { type: DebuggerInputType.Text, elementId: 75, value: invalidUnicode },
    ]) {
      const response = await send(handler, { ...request, contextId: 'context-1' });
      expect(response['handled']).toBeFalse();
      expect(response['message']).toContain('must contain valid Unicode');
      expect(input.getAttribute('value')).toBe('safe');
    }

    input.setAttributes({ value: invalidUnicode, selection: [1, 1] });
    const response = await send(handler, {
      type: DebuggerInputType.Key,
      contextId: 'context-1',
      elementId: 75,
      key: 'x',
    });
    expect(response['handled']).toBeFalse();
    expect(response['message']).toBe('Element 75 contains invalid Unicode text.');
    expect(input.getAttribute('value')).toBe(invalidUnicode);
  });

  it('rejects disabled focus, text, and key actions without mutating the input', async () => {
    const onChange = jasmine.createSpy('onChange');
    const disabledByEnabled = new TestElement(12, 'textfield', {
      accessibilityId: 'disabled-by-enabled',
      enabled: false,
      focused: false,
      value: 'original',
      selection: [8, 8],
      onChange,
    });
    const disabledByAccessibility = new TestElement(13, 'textfield', {
      accessibilityId: 'disabled-by-accessibility',
      accessibilityStateDisabled: true,
      focused: false,
      value: 'original',
      selection: [8, 8],
      onChange,
    });
    const handler = makeHandler(
      makeNode(undefined, [makeNode(disabledByEnabled, []), makeNode(disabledByAccessibility, [])]),
    );

    const focusResponse = await send(handler, {
      type: DebuggerInputType.Focus,
      contextId: 'context-1',
      accessibilityId: 'disabled-by-enabled',
      focused: true,
    });
    const textResponse = await send(handler, {
      type: DebuggerInputType.Text,
      contextId: 'context-1',
      accessibilityId: 'disabled-by-accessibility',
      text: 'changed',
    });
    const keyResponse = await send(handler, {
      type: DebuggerInputType.Key,
      contextId: 'context-1',
      accessibilityId: 'disabled-by-accessibility',
      key: 'x',
    });

    expect(focusResponse).toEqual(
      jasmine.objectContaining({ handled: false, actionElementId: 12, message: 'Element 12 is disabled.' }),
    );
    expect(textResponse).toEqual(
      jasmine.objectContaining({ handled: false, actionElementId: 13, message: 'Element 13 is disabled.' }),
    );
    expect(keyResponse).toEqual(
      jasmine.objectContaining({ handled: false, actionElementId: 13, message: 'Element 13 is disabled.' }),
    );
    expect(disabledByEnabled.getAttribute('focused')).toBeFalse();
    expect(disabledByAccessibility.getAttribute('focused')).toBeFalse();
    expect(disabledByAccessibility.getAttribute('value')).toBe('original');
    expect(disabledByAccessibility.getAttribute('selection')).toEqual([8, 8]);
    expect(onChange).not.toHaveBeenCalled();
  });

  it('applies ancestor interaction gates to focus, text, key, and scroll and rejects read-only edits', async () => {
    const touchDisabledAncestor = new TestElement(76, 'view', { touchEnabled: false });
    const gatedInput = new TestElement(77, 'textfield', {
      accessibilityId: 'gated-input',
      value: 'safe',
      selection: [4, 4],
      focused: false,
    });
    gatedInput.parent = touchDisabledAncestor.asRenderedElement();
    touchDisabledAncestor.children.push(gatedInput.asRenderedElement());

    const disabledAncestor = new TestElement(78, 'view', { enabled: false });
    const gatedScroll = new TestElement(79, 'scroll', {
      accessibilityId: 'gated-scroll',
      contentOffsetX: 1,
      contentOffsetY: 2,
    });
    gatedScroll.parent = disabledAncestor.asRenderedElement();
    disabledAncestor.children.push(gatedScroll.asRenderedElement());

    const readOnlyInput = new TestElement(80, 'textfield', {
      accessibilityId: 'read-only-input',
      editable: false,
      value: 'read only',
      selection: [9, 9],
    });
    const handler = makeHandler(
      makeNode(undefined, [
        makeNode(touchDisabledAncestor, [makeNode(gatedInput, [])]),
        makeNode(disabledAncestor, [makeNode(gatedScroll, [])]),
        makeNode(readOnlyInput, []),
      ]),
    );

    for (const request of [
      { type: DebuggerInputType.Focus, elementId: 77, focused: true },
      { type: DebuggerInputType.Text, elementId: 77, text: 'changed' },
      { type: DebuggerInputType.Key, elementId: 77, key: 'x' },
    ]) {
      const response = await send(handler, { ...request, contextId: 'context-1' });
      expect(response['handled']).toBeFalse();
      expect(response['message']).toBe('Element 76 has touchEnabled=false.');
    }
    const scrollResponse = await send(handler, {
      type: DebuggerInputType.Scroll,
      contextId: 'context-1',
      elementId: 79,
      deltaY: 20,
    });
    expect(scrollResponse['handled']).toBeFalse();
    expect(scrollResponse['message']).toBe('Element 78 is disabled.');

    for (const request of [
      { type: DebuggerInputType.Text, elementId: 80, text: 'changed' },
      { type: DebuggerInputType.Key, elementId: 80, key: 'x' },
    ]) {
      const response = await send(handler, { ...request, contextId: 'context-1' });
      expect(response['handled']).toBeFalse();
      expect(response['message']).toBe('Element 80 is not editable.');
    }

    expect(gatedInput.getAttribute('focused')).toBeFalse();
    expect(gatedInput.getAttribute('value')).toBe('safe');
    expect(gatedScroll.getAttribute('contentOffsetY')).toBe(2);
    expect(readOnlyInput.getAttribute('value')).toBe('read only');
  });

  it('returns structured failures when app-owned input callbacks throw', async () => {
    const makeThrowingCallback = (name: string) => () => {
      throw new Error(`${name} failed`);
    };
    const button = new TestElement(81, 'view', { onTap: makeThrowingCallback('onTap') });
    const willChangeInput = new TestElement(82, 'textfield', {
      value: 'seed',
      selection: [4, 4],
      onWillChange: makeThrowingCallback('onWillChange'),
    });
    const changeInput = new TestElement(83, 'textfield', {
      value: 'seed',
      selection: [4, 4],
      onChange: makeThrowingCallback('onChange'),
    });
    const returnInput = new TestElement(84, 'textfield', {
      value: 'seed',
      selection: [4, 4],
      onReturn: makeThrowingCallback('onReturn'),
    });
    const deleteInput = new TestElement(85, 'textfield', {
      value: 'seed',
      selection: [4, 4],
      onWillDelete: makeThrowingCallback('onWillDelete'),
    });
    const handler = makeHandler(
      makeNode(undefined, [
        makeNode(button, []),
        makeNode(willChangeInput, []),
        makeNode(changeInput, []),
        makeNode(returnInput, []),
        makeNode(deleteInput, []),
      ]),
    );
    const cases = [
      { elementId: 81, callbackName: 'onTap', type: DebuggerInputType.Tap },
      { elementId: 82, callbackName: 'onWillChange', type: DebuggerInputType.Text, text: 'changed' },
      { elementId: 83, callbackName: 'onChange', type: DebuggerInputType.Text, text: 'changed' },
      { elementId: 84, callbackName: 'onReturn', type: DebuggerInputType.Key, key: 'Return' },
      { elementId: 85, callbackName: 'onWillDelete', type: DebuggerInputType.Key, key: 'Backspace' },
    ];

    for (const testCase of cases) {
      const { callbackName, ...request } = testCase;
      const response = await send(handler, { ...request, contextId: 'context-1' });
      expect(response).toEqual(
        jasmine.objectContaining({
          handled: false,
          elementId: testCase.elementId,
          actionElementId: testCase.elementId,
          message: `Element ${testCase.elementId} ${callbackName} callback failed: ${callbackName} failed`,
        }),
      );
    }
    expect(changeInput.getAttribute('value')).toBe('changed');
  });

  it('keeps internal input mutation failures loud', async () => {
    const input = new ThrowingSetAttributesElement(86, 'textfield', {
      value: 'seed',
      selection: [4, 4],
    });
    const handler = makeHandler(makeNode(undefined, [makeNode(input, [])]));

    await expectAsync(
      send(handler, {
        type: DebuggerInputType.Text,
        contextId: 'context-1',
        elementId: 86,
        text: 'changed',
      }),
    ).toBeRejectedWithError('internal setAttributes failed');
  });

  it('rejects missing contexts and callbacks with specific errors', async () => {
    const plainView = new TestElement(14, 'view', { accessibilityId: 'plain-view' });
    const inputWithoutReturn = new TestElement(15, 'textfield', {
      accessibilityId: 'input-without-return',
      focused: true,
      value: 'draft',
    });
    const handler = makeHandler(makeNode(undefined, [makeNode(plainView, []), makeNode(inputWithoutReturn, [])]));

    const missingContext = await send(handler, {
      type: DebuggerInputType.Query,
    });
    const unknownContext = await send(handler, {
      type: DebuggerInputType.Query,
      contextId: 'missing-context',
    });
    const missingTap = await send(handler, {
      type: DebuggerInputType.Tap,
      contextId: 'context-1',
      accessibilityId: 'plain-view',
    });
    const returnWithoutCallback = await send(handler, {
      type: DebuggerInputType.Key,
      contextId: 'context-1',
      accessibilityId: 'input-without-return',
      key: 'Enter',
    });

    expect(missingContext['handled']).toBeFalse();
    expect(missingContext['message']).toBe('A contextId is required.');
    expect(unknownContext['handled']).toBeFalse();
    expect(unknownContext['message']).toBe('No Valdi renderer found for context missing-context.');
    expect(missingTap['handled']).toBeFalse();
    expect(missingTap['message']).toBe('Element 14 and its ancestors do not expose onTap.');
    expect(returnWithoutCallback['handled']).toBeTrue();
    expect(returnWithoutCallback['action']).toBe('focused');
    expect(inputWithoutReturn.getAttribute('focused')).toBeFalse();
  });

  it('rejects unsupported operations and non-object requests as handled failures', async () => {
    const handler = makeHandler(makeNode(undefined, []));

    const unsupported = await send(handler, {
      type: 'drag',
      contextId: 'context-1',
    });
    const nonObjectPromise = handler.messageReceived(DEBUGGER_INPUT_IDENTIFIER, []);
    if (!nonObjectPromise) {
      throw new Error('Expected debugger input handler to accept the request.');
    }
    const nonObject = await nonObjectPromise;

    expect(unsupported['handled']).toBeFalse();
    expect(unsupported['message']).toBe("Unsupported debugger input type 'drag'.");
    expect(nonObject['handled']).toBeFalse();
    expect(nonObject['message']).toBe('Debugger input request must be an object.');
  });

  it('validates selectors and action payloads before mutation or callback dispatch', async () => {
    const onTap = jasmine.createSpy('onTap');
    const onChange = jasmine.createSpy('onChange');
    const button = new TestElement(16, 'view', { accessibilityId: 'button', onTap });
    const input = new TestElement(17, 'textfield', {
      accessibilityId: 'input',
      focused: false,
      value: 'original',
      selection: [8, 8],
      onChange,
    });
    const scroll = new TestElement(18, 'scroll', {
      accessibilityId: 'scroll',
      contentOffsetX: 1,
      contentOffsetY: 2,
    });
    const handler = makeHandler(makeNode(undefined, [makeNode(button, []), makeNode(input, []), makeNode(scroll, [])]));

    const malformedRequests: Array<{ request: { [name: string]: any }; message: string }> = [
      {
        request: { type: DebuggerInputType.Tap, contextId: 'context-1', elementId: '16' },
        message: 'elementId must be a finite integer.',
      },
      {
        request: { type: DebuggerInputType.Tap, contextId: 'context-1', elementId: Number.NaN },
        message: 'elementId must be a finite integer.',
      },
      {
        request: {
          type: DebuggerInputType.Query,
          contextId: 'context-1',
          selector: { accessibilityId: 'input', unknown: true },
        },
        message: "Unsupported selector field 'unknown'.",
      },
      {
        request: {
          type: DebuggerInputType.Query,
          contextId: 'context-1',
          selector: { zUnknown: true, aUnknown: true },
        },
        message: "Unsupported selector field 'aUnknown'.",
      },
      {
        request: { type: DebuggerInputType.Query, contextId: 'context-1', selector: [] },
        message: 'selector must be a string or an object.',
      },
      {
        request: { type: DebuggerInputType.Query, contextId: 'context-1', selector: { elementId: undefined } },
        message: 'selector object must include elementId, accessibilityId, or tag.',
      },
      {
        request: { type: DebuggerInputType.Query, contextId: 'context-1', accessibilityId: '' },
        message: 'accessibilityId must not be empty.',
      },
      {
        request: {
          type: DebuggerInputType.Query,
          contextId: 'context-1',
          elementId: 17,
          accessibilityId: 'input',
        },
        message: 'Use only one of elementId, accessibilityId, or selector.',
      },
      {
        request: {
          type: DebuggerInputType.Query,
          contextId: 'context-1',
          selector: { elementId: 17.5 },
        },
        message: 'selector.elementId must be a finite integer.',
      },
      {
        request: { type: DebuggerInputType.Tap, contextId: 'context-1', accessibilityId: 'button', x: Infinity },
        message: 'x must be a finite number.',
      },
      {
        request: {
          type: DebuggerInputType.Focus,
          contextId: 'context-1',
          accessibilityId: 'input',
          focused: 'yes',
        },
        message: 'focused must be a boolean.',
      },
      {
        request: { type: DebuggerInputType.Text, contextId: 'context-1', accessibilityId: 'input', text: 7 },
        message: 'text must be a string.',
      },
      {
        request: { type: DebuggerInputType.Text, contextId: 'context-1', accessibilityId: 'input' },
        message: 'Text input requires a string text or value.',
      },
      {
        request: {
          type: DebuggerInputType.Text,
          contextId: 'context-1',
          accessibilityId: 'input',
          text: 'first',
          value: 'second',
        },
        message: 'Use only one of text or value.',
      },
      {
        request: { type: DebuggerInputType.Tap, contextId: 'context-1', accessibilityId: 'button', text: 'ignored' },
        message: "Field 'text' is not supported for tap input.",
      },
      {
        request: { type: DebuggerInputType.Key, contextId: 'context-1', accessibilityId: 'input', key: 7 },
        message: 'key must be a string.',
      },
      {
        request: { type: DebuggerInputType.Key, contextId: 'context-1', accessibilityId: 'input', key: '' },
        message: 'key must not be empty.',
      },
      {
        request: {
          type: DebuggerInputType.Key,
          contextId: 'context-1',
          accessibilityId: 'input',
          key: 'x',
          selectionStart: Number.NaN,
        },
        message: 'selectionStart must be a finite integer.',
      },
      {
        request: {
          type: DebuggerInputType.Scroll,
          contextId: 'context-1',
          accessibilityId: 'scroll',
          deltaY: Infinity,
        },
        message: 'deltaY must be a finite number.',
      },
    ];

    for (const malformed of malformedRequests) {
      const response = await send(handler, malformed.request);
      expect(response['handled']).toBeFalse();
      expect(response['message']).toBe(malformed.message);
    }
    expect(onTap).not.toHaveBeenCalled();
    expect(onChange).not.toHaveBeenCalled();
    expect(input.getAttribute('focused')).toBeFalse();
    expect(input.getAttribute('value')).toBe('original');
    expect(input.getAttribute('selection')).toEqual([8, 8]);
    expect(scroll.getAttribute('contentOffsetX')).toBe(1);
    expect(scroll.getAttribute('contentOffsetY')).toBe(2);
  });

  it('rejects ambiguous accessibility identifiers', async () => {
    const first = new TestElement(8, 'view', { accessibilityId: 'duplicate' });
    const second = new TestElement(9, 'view', { accessibilityId: 'duplicate' });
    const handler = makeHandler(makeNode(undefined, [makeNode(first, []), makeNode(second, [])]));

    const response = await send(handler, {
      type: DebuggerInputType.Tap,
      contextId: 'context-1',
      accessibilityId: 'duplicate',
    });

    expect(response['handled']).toBeFalse();
    expect(response['message']).toContain('matched 2 elements');
    expect(response['elements'].length).toBe(2);
  });
});
