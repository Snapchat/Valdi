import 'jasmine';
import { execSync } from 'child_process';
import * as fs from 'fs';
import path from 'path';
import * as vm from 'vm';

interface Deferred<Value> {
  promise: Promise<Value>;
  resolve: (value: Value) => void;
}

function createDeferred<Value>(): Deferred<Value> {
  let resolvePromise!: (value: Value) => void;
  const promise = new Promise<Value>(resolve => {
    resolvePromise = resolve;
  });
  return { promise, resolve: resolvePromise };
}

/**
 * Verifies that all directories required at runtime are included in the
 * published npm package. This prevents regressions like
 * https://github.com/Snapchat/Valdi/issues/93 where `valdi bootstrap` failed
 * because template files were missing from the tarball.
 */
describe('npm package contents', () => {
  let cliRoot: string;
  let packedFiles: string;

  beforeAll(() => {
    // The CLI test suite is emitted as CommonJS.
    // eslint-disable-next-line unicorn/prefer-module
    cliRoot = path.join(__dirname, '../..');
    packedFiles = execSync('npm pack --dry-run 2>&1', {
      cwd: cliRoot,
      encoding: 'utf8',
    });
  });

  it('includes .metadata templates for bootstrap config', () => {
    expect(packedFiles).toContain('.metadata/config.yaml.template');
    expect(packedFiles).toContain('.metadata/MODULE.bazel.template');
  });

  it('includes .bootstrap templates for project scaffolding', () => {
    expect(packedFiles).toContain('.bootstrap/');
  });

  it('includes dist output', () => {
    expect(packedFiles).toContain('dist/');
  });

  it('does not publish compiled Jasmine specifications', () => {
    expect(packedFiles).not.toMatch(/dist\/.*\.spec\.js/);
  });

  it('includes bundled-skills', () => {
    expect(packedFiles).toContain('bundled-skills/');
  });

  it('includes debugger assets', () => {
    const debuggerRoot = path.join(cliRoot, 'debugger');
    const indexHtml = fs.readFileSync(path.join(debuggerRoot, 'index.html'), 'utf8');
    const stylesheetPaths: string[] = [];
    for (const match of indexHtml.matchAll(/<link\s+[^>]*href=["']\.\/(.+?)["']/g)) {
      const stylesheetPath = match[1];
      if (stylesheetPath !== undefined) stylesheetPaths.push(stylesheetPath);
    }
    const scriptPaths: string[] = [];
    for (const match of indexHtml.matchAll(/<script\s+src=["']\.\/(.+?)["']/g)) {
      const scriptPath = match[1];
      if (scriptPath !== undefined) scriptPaths.push(scriptPath);
    }

    expect(packedFiles).toContain('debugger/index.html');
    expect(stylesheetPaths.length).toBeGreaterThan(0);
    expect(scriptPaths.length).toBeGreaterThan(0);
    for (const assetPath of [...stylesheetPaths, ...scriptPaths]) {
      expect(packedFiles).toContain(`debugger/${assetPath}`);
    }

    const orderedBundle = scriptPaths
      .map(scriptPath => fs.readFileSync(path.join(debuggerRoot, scriptPath), 'utf8'))
      .join('\n');
    expect(() => new vm.Script(orderedBundle, { filename: 'valdi-debugger.js' })).not.toThrow();
    for (const deferredSurface of [
      'renderDataProviders',
      'renderNetwork',
      'capturePerformanceTrace',
      'renderWebPreviewFrame',
    ]) {
      expect(orderedBundle).not.toContain(deferredSurface);
    }
    expect(orderedBundle).toContain('dispatchDebuggerInput');
    expect(orderedBundle).toContain("apiPost('/api/input'");
  });

  it('does not let projected trees auto-load remote media', () => {
    const treeModelSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-tree-model.js'), 'utf8');
    const previewSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-preview-html.js'), 'utf8');
    const policyResult = new vm.Script(
      `${treeModelSource}\n${previewSource}\n[
        previewSafeMediaSource('https://example.com/image.png'),
        previewSafeMediaSource('http://127.0.0.1:8080/private'),
        previewSafeMediaSource('data:image/png;base64,AA=='),
        previewSafeMediaSource('blob:http://127.0.0.1:8765/id')
      ];`,
      { filename: 'debugger-preview-html.js' },
    ).runInNewContext({}) as string[];

    expect(policyResult).toEqual(['', '', 'data:image/png;base64,AA==', 'blob:http://127.0.0.1:8765/id']);
    expect(previewSource).not.toContain('frame.src = source');
  });

  it('projects effective ancestor, accessibility, and touch-disabled state', () => {
    const treeModelSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-tree-model.js'), 'utf8');
    const previewSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-preview-html.js'), 'utf8');
    const disabledStates = new vm.Script(
      `${treeModelSource}\n${previewSource}
      [
        isHtmlPreviewEffectivelyDisabled({ attributes: {} }, true),
        isHtmlPreviewEffectivelyDisabled({ attributes: { enabled: false } }, false),
        isHtmlPreviewEffectivelyDisabled({ attributes: { accessibilityStateDisabled: true } }, false),
        isHtmlPreviewEffectivelyDisabled({ attributes: { touchEnabled: false } }, false),
        isHtmlPreviewEffectivelyDisabled({ attributes: { enabled: true, touchEnabled: true } }, false),
      ];`,
      { filename: 'debugger-preview-html.js' },
    ).runInNewContext({
      getNodeAttributes: (node: { attributes: Record<string, unknown> }) => node.attributes,
    }) as boolean[];

    expect(disabledStates).toEqual([true, true, true, true, false]);
  });

  it('orders editable HTML preview focus and blur around text and key input', async () => {
    const treeModelSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-tree-model.js'), 'utf8');
    const previewSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-preview-html.js'), 'utf8');
    const bootstrapSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-bootstrap.js'), 'utf8');
    const dispatchedInputs: Array<{
      options: Record<string, unknown>;
      payload: Record<string, unknown>;
      target: Record<string, unknown>;
    }> = [];
    const timers = new Map<number, () => void>();
    let nextTimerId = 1;
    class FakeInputElement {
      readonly dataset = { previewElementId: '42', previewNodeId: 'node-42' };
      readonly isContentEditable = false;
      readonly selectionEnd = 5;
      readonly selectionStart = 5;
      readonly value = 'draft';

      closest(): FakeInputElement {
        return this;
      }

      blur(): void {}
    }
    class FakeTextAreaElement {}
    const editableTarget = new FakeInputElement();
    const enqueueInput = (
      target: Record<string, unknown>,
      payload: Record<string, unknown>,
      options: Record<string, unknown>,
    ): Promise<{ handled: boolean }> => {
      dispatchedInputs.push({ options, payload, target });
      return Promise.resolve({ handled: true });
    };
    const operation = new vm.Script(
      `${treeModelSource}\n${previewSource}
      (async () => {
        const target = Object.freeze({ port: 13_591, clientId: 'client-a', contextId: 'context-a' });
        associateHtmlPreviewElement(editableTarget, target);
        const event = {
          target: editableTarget,
          preventDefault() {},
          stopPropagation() {},
        };
        await dispatchHtmlPreviewFocusInput(event, true);
        await dispatchHtmlPreviewTapInput(event);
        dispatchHtmlPreviewTextInput(event);
        await dispatchHtmlPreviewKeyInput({ ...event, key: 'Enter' });
        await dispatchHtmlPreviewFocusInput(event, false);
      })();`,
      { filename: 'debugger-preview-html.js' },
    ).runInNewContext({
      HTMLInputElement: FakeInputElement,
      HTMLTextAreaElement: FakeTextAreaElement,
      debuggerInputTargetElementKey: (
        target: { port: number; clientId: string; contextId: string },
        elementId: number,
      ) => `${target.port}:${target.clientId}:${target.contextId}:${elementId}`,
      enqueueDebuggerInput: enqueueInput,
      document: { activeElement: editableTarget },
      editableTarget,
      elements: { htmlPreviewRoot: { contains: () => true } },
      findNode: () => ({ attributes: { closesWhenReturnKeyPressed: false }, tag: 'textfield' }),
      getNodeAttributes: (node: { attributes: Record<string, unknown> }) => node.attributes,
      reserveDebuggerInput: (target: Record<string, unknown>) => ({
        cancel: () => Promise.resolve(null),
        dispatch: (payload: Record<string, unknown>, options: Record<string, unknown>) =>
          enqueueInput(target, payload, options),
      }),
      selectPreviewNode: jasmine.createSpy('selectPreviewNode'),
      window: {
        clearTimeout: (timerId: number) => timers.delete(timerId),
        setTimeout: (callback: () => void) => {
          const timerId = nextTimerId;
          nextTimerId += 1;
          timers.set(timerId, callback);
          return timerId;
        },
      },
    }) as Promise<void>;

    await operation;

    expect(dispatchedInputs.map(input => input.payload['type'])).toEqual(['focus', 'text', 'key', 'focus']);
    expect(dispatchedInputs[0]?.payload['focused']).toBeTrue();
    expect(dispatchedInputs[3]?.payload['focused']).toBeFalse();
    expect(dispatchedInputs[0]?.options['refresh']).toBeFalse();
    expect(dispatchedInputs[3]?.options['refresh']).toBeTrue();
    expect(dispatchedInputs.every(input => input.target['contextId'] === 'context-a')).toBeTrue();
    expect(bootstrapSource).toContain("addEventListener('focusin'");
    expect(bootstrapSource).toContain("addEventListener('focusout'");
  });

  it('keeps the runtime key path authoritative for projected textarea Return', async () => {
    const treeModelSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-tree-model.js'), 'utf8');
    const previewSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-preview-html.js'), 'utf8');
    const dispatchedInputs: Array<Record<string, unknown>> = [];
    class FakeInputElement {}
    class FakeTextAreaElement {
      readonly dataset = { previewElementId: '42', previewNodeId: 'node-42' };
      readonly isContentEditable = false;
      selectionEnd = 1;
      selectionStart = 1;
      value = 'ab';

      blur(): void {}

      closest(): FakeTextAreaElement {
        return this;
      }

      setSelectionRange(selectionStart: number, selectionEnd: number): void {
        this.selectionStart = selectionStart;
        this.selectionEnd = selectionEnd;
      }
    }
    const textarea = new FakeTextAreaElement();
    const operation = new vm.Script(
      `${treeModelSource}\n${previewSource}
      (async () => {
        const target = Object.freeze({ port: 13_591, clientId: 'client-a', contextId: 'context-a' });
        associateHtmlPreviewElement(textarea, target);
        let browserInputEventCount = 0;
        const event = {
          target: textarea,
          key: 'Enter',
          defaultPrevented: false,
          preventDefault() { this.defaultPrevented = true; },
          stopPropagation() {},
        };
        const keyDispatch = dispatchHtmlPreviewKeyInput(event);
        const valueBeforeRuntimeResponse = textarea.value;

        // Model the browser's default phase after keydown: an unprevented textarea Return mutates
        // the DOM and emits input, which would otherwise enqueue a second debugger text operation.
        if (!event.defaultPrevented) {
          textarea.value = 'a\\nb';
          textarea.selectionStart = 2;
          textarea.selectionEnd = 2;
          browserInputEventCount += 1;
          dispatchHtmlPreviewTextInput({ target: textarea });
        }

        await keyDispatch;
        return {
          browserInputEventCount,
          defaultPrevented: event.defaultPrevented,
          selectionEnd: textarea.selectionEnd,
          selectionStart: textarea.selectionStart,
          value: textarea.value,
          valueBeforeRuntimeResponse,
        };
      })();`,
      { filename: 'debugger-preview-html.js' },
    ).runInNewContext({
      HTMLInputElement: FakeInputElement,
      HTMLTextAreaElement: FakeTextAreaElement,
      debuggerInputTargetElementKey: (
        target: { port: number; clientId: string; contextId: string },
        elementId: number,
      ) => `${target.port}:${target.clientId}:${target.contextId}:${elementId}`,
      document: { activeElement: textarea },
      elements: { htmlPreviewRoot: { contains: () => true } },
      enqueueDebuggerInput: (_target: Record<string, unknown>, payload: Record<string, unknown>) => {
        dispatchedInputs.push(payload);
        return Promise.resolve({
          action: 'onReturn',
          handled: true,
          selectionEnd: 2,
          selectionStart: 2,
          value: 'A\nB',
        });
      },
      findNode: () => ({ attributes: { closesWhenReturnKeyPressed: false }, tag: 'textview' }),
      getNodeAttributes: (node: { attributes: Record<string, unknown> }) => node.attributes,
      reserveDebuggerInput: () => ({
        cancel: () => Promise.resolve(null),
        dispatch: () => Promise.resolve(null),
      }),
      selectPreviewNode: jasmine.createSpy('selectPreviewNode'),
      textarea,
      window: {
        clearTimeout() {},
        setTimeout: () => 1,
      },
    }) as Promise<{
      browserInputEventCount: number;
      defaultPrevented: boolean;
      selectionEnd: number;
      selectionStart: number;
      value: string;
      valueBeforeRuntimeResponse: string;
    }>;

    expect(await operation).toEqual({
      browserInputEventCount: 0,
      defaultPrevented: true,
      selectionEnd: 2,
      selectionStart: 2,
      value: 'A\nB',
      valueBeforeRuntimeResponse: 'a\nb',
    });
    expect(dispatchedInputs).toEqual([
      jasmine.objectContaining({
        elementId: 42,
        key: 'Enter',
        selectionEnd: 1,
        selectionStart: 1,
        type: 'key',
      }),
    ]);
  });

  it('rejects delayed input reconciliation after ABA edits and refocus', async () => {
    const treeModelSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-tree-model.js'), 'utf8');
    const previewSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-preview-html.js'), 'utf8');
    const keyResponse = createDeferred<Record<string, unknown>>();
    const dispatchedTypes: string[] = [];
    const timers = new Map<number, () => void>();
    let nextTimerId = 1;
    class FakeInputElement {}
    class FakeTextAreaElement {
      readonly dataset = { previewElementId: '42', previewNodeId: 'node-42' };
      readonly isContentEditable = false;
      blurCount = 0;
      selectionEnd = 1;
      selectionStart = 1;
      value = 'ab';

      blur(): void {
        this.blurCount += 1;
      }

      closest(): FakeTextAreaElement {
        return this;
      }

      setSelectionRange(selectionStart: number, selectionEnd: number): void {
        this.selectionStart = selectionStart;
        this.selectionEnd = selectionEnd;
      }
    }
    const textarea = new FakeTextAreaElement();
    const operation = new vm.Script(
      `${treeModelSource}\n${previewSource}
      (async () => {
        const target = Object.freeze({ port: 13_591, clientId: 'client-a', contextId: 'context-a' });
        associateHtmlPreviewElement(textarea, target);
        const baseEvent = { target: textarea, preventDefault() {}, stopPropagation() {} };
        await dispatchHtmlPreviewFocusInput(baseEvent, true);
        const keyDispatch = dispatchHtmlPreviewKeyInput({ ...baseEvent, key: 'Enter' });

        textarea.value = 'edited-away';
        textarea.selectionStart = textarea.value.length;
        textarea.selectionEnd = textarea.value.length;
        dispatchHtmlPreviewTextInput(baseEvent);
        textarea.value = 'a\\nb';
        textarea.selectionStart = 2;
        textarea.selectionEnd = 2;
        dispatchHtmlPreviewTextInput(baseEvent);

        await dispatchHtmlPreviewFocusInput(baseEvent, false);
        await dispatchHtmlPreviewFocusInput(baseEvent, true);
        resolveKeyResponse();
        await keyDispatch;
        return { blurCount: textarea.blurCount, value: textarea.value };
      })();`,
      { filename: 'debugger-preview-html.js' },
    ).runInNewContext({
      HTMLInputElement: FakeInputElement,
      HTMLTextAreaElement: FakeTextAreaElement,
      debuggerInputTargetElementKey: (
        target: { port: number; clientId: string; contextId: string },
        elementId: number,
      ) => `${target.port}:${target.clientId}:${target.contextId}:${elementId}`,
      document: { activeElement: textarea },
      elements: { htmlPreviewRoot: { contains: () => true } },
      enqueueDebuggerInput: (
        _target: Record<string, unknown>,
        payload: Record<string, unknown>,
      ): Promise<Record<string, unknown>> => {
        dispatchedTypes.push(String(payload['type']));
        return payload['type'] === 'key' ? keyResponse.promise : Promise.resolve({ handled: true });
      },
      findNode: () => ({ attributes: { closesWhenReturnKeyPressed: true }, tag: 'textview' }),
      getNodeAttributes: (node: { attributes: Record<string, unknown> }) => node.attributes,
      reserveDebuggerInput: () => ({
        cancel: () => Promise.resolve(null),
        dispatch: (payload: Record<string, unknown>) => {
          dispatchedTypes.push(String(payload['type']));
          return Promise.resolve({
            handled: true,
            selectionEnd: payload['selectionEnd'],
            selectionStart: payload['selectionStart'],
            value: payload['text'],
          });
        },
      }),
      resolveKeyResponse: () => {
        keyResponse.resolve({
          action: 'onReturn',
          handled: true,
          selectionEnd: 2,
          selectionStart: 2,
          value: 'runtime-stale',
        });
      },
      selectPreviewNode: () => {},
      textarea,
      window: {
        clearTimeout: (timerId: number) => timers.delete(timerId),
        setTimeout: (callback: () => void) => {
          const timerId = nextTimerId;
          nextTimerId += 1;
          timers.set(timerId, callback);
          return timerId;
        },
      },
    }) as Promise<{ blurCount: number; value: string }>;
    expect(await operation).toEqual({ blurCount: 0, value: 'a\nb' });
    expect(dispatchedTypes).toEqual(['focus', 'key', 'text', 'focus', 'focus']);
  });

  it('cancels pending projected input when the preview incarnation changes', async () => {
    const treeModelSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-tree-model.js'), 'utf8');
    const previewSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-preview-html.js'), 'utf8');
    const cancelledReservations: number[] = [];
    const dispatchedInputs: Array<{ payload: Record<string, unknown>; reservationId: number }> = [];
    const timers = new Map<number, () => void>();
    let nextReservationId = 1;
    let nextTimerId = 1;
    let hasTree = true;
    class FakePreviewElement {
      readonly dataset: { previewElementId: string; previewNodeId: string };

      constructor(nodeId: string, elementId: number) {
        this.dataset = { previewElementId: String(elementId), previewNodeId: nodeId };
      }

      closest(): FakePreviewElement {
        return this;
      }
    }
    class FakeInputElement extends FakePreviewElement {
      readonly isContentEditable = false;
      selectionEnd: number;
      selectionStart: number;
      value: string;

      constructor(nodeId: string, elementId: number, value: string) {
        super(nodeId, elementId);
        this.value = value;
        this.selectionEnd = value.length;
        this.selectionStart = value.length;
      }

      setSelectionRange(selectionStart: number, selectionEnd: number): void {
        this.selectionStart = selectionStart;
        this.selectionEnd = selectionEnd;
      }
    }
    class FakeTextAreaElement extends FakePreviewElement {}
    const oldInput = new FakeInputElement('old-input', 42, 'old');
    const oldScroll = new FakePreviewElement('old-scroll', 42);
    const newInput = new FakeInputElement('new-input', 42, 'new');
    const nodes: Record<string, { attributes: Record<string, unknown>; elementId: number; tag: string }> = {
      'new-input': { attributes: {}, elementId: 42, tag: 'textfield' },
      'old-input': { attributes: {}, elementId: 42, tag: 'textfield' },
      'old-scroll': { attributes: {}, elementId: 42, tag: 'scroll' },
    };
    const target = Object.freeze({ port: 13_591, clientId: 'client-a', contextId: 'context-a' });
    const operation = new vm.Script(
      `${treeModelSource}\n${previewSource}
      (async () => {
        associateHtmlPreviewElement(oldInput, target);
        associateHtmlPreviewElement(oldScroll, target);
        dispatchHtmlPreviewTextInput({ target: oldInput });
        dispatchHtmlPreviewScrollInput({
          target: oldScroll,
          deltaX: 1,
          deltaY: 2,
          preventDefault() {},
          stopPropagation() {},
        });

        setHasTree(false);
        renderHtmlPreview();
        associateHtmlPreviewElement(newInput, target);
        dispatchHtmlPreviewTextInput({ target: newInput });
        runAllTimers();
        await settlePromises();
        return {
          incarnation: htmlPreviewIncarnation,
          pendingScrollCount: htmlPreviewScrollInputs.size,
          pendingTextCount: htmlPreviewTextInputs.size,
        };
      })();`,
      { filename: 'debugger-preview-html.js' },
    ).runInNewContext({
      HTMLInputElement: FakeInputElement,
      HTMLTextAreaElement: FakeTextAreaElement,
      debuggerInputTargetElementKey: (
        inputTarget: { port: number; clientId: string; contextId: string },
        elementId: number,
      ) => `${inputTarget.port}:${inputTarget.clientId}:${inputTarget.contextId}:${elementId}`,
      elements: {
        device: { style: { setProperty() {} } },
        htmlPreviewRoot: {
          classList: { toggle() {} },
          contains: () => true,
          replaceChildren() {},
        },
      },
      findNode: (nodeId: string) => nodes[nodeId],
      findNodeAtPoint: (
        _point: unknown,
        predicate: (node: { attributes: Record<string, unknown>; elementId: number; tag: string }) => boolean,
      ) => {
        const node = nodes['old-scroll'];
        return node !== undefined && predicate(node) ? node : undefined;
      },
      getElementIdForNode: (node: { elementId: number }) => node.elementId,
      getNodeAttributes: (node: { attributes: Record<string, unknown> }) => node.attributes,
      hasScrollState: () => false,
      hasSnapshotTree: () => hasTree,
      newInput,
      nodes,
      oldInput,
      oldScroll,
      pointFromScreenEvent: () => ({ x: 10, y: 20 }),
      reserveDebuggerInput: () => {
        const reservationId = nextReservationId;
        nextReservationId += 1;
        return {
          cancel: () => {
            cancelledReservations.push(reservationId);
            return Promise.resolve(null);
          },
          dispatch: (payload: Record<string, unknown>) => {
            dispatchedInputs.push({ payload, reservationId });
            return Promise.resolve({
              handled: true,
              selectionEnd: payload['selectionEnd'],
              selectionStart: payload['selectionStart'],
              value: payload['text'],
            });
          },
        };
      },
      runAllTimers: () => {
        while (timers.size > 0) {
          const callbacks = Array.from(timers.values());
          timers.clear();
          callbacks.forEach(callback => callback());
        }
      },
      selectPreviewNode: () => {},
      setHasTree: (value: boolean) => {
        hasTree = value;
      },
      settlePromises: async () => {
        for (let index = 0; index < 8; index += 1) await Promise.resolve();
      },
      state: { source: 'daemon' },
      target,
      window: {
        clearTimeout: (timerId: number) => timers.delete(timerId),
        setTimeout: (callback: () => void) => {
          const timerId = nextTimerId;
          nextTimerId += 1;
          timers.set(timerId, callback);
          return timerId;
        },
      },
    }) as Promise<{ incarnation: number; pendingScrollCount: number; pendingTextCount: number }>;

    expect(await operation).toEqual({ incarnation: 1, pendingScrollCount: 0, pendingTextCount: 0 });
    expect(cancelledReservations).toEqual([1, 2]);
    expect(dispatchedInputs).toEqual([
      {
        payload: jasmine.objectContaining({ elementId: 42, text: 'new', type: 'text' }),
        reservationId: 3,
      },
    ]);
  });

  it('invalidates released text and wheel input still queued when the preview incarnation changes', async () => {
    const treeModelSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-tree-model.js'), 'utf8');
    const modelSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-model.js'), 'utf8');
    const previewSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-preview-html.js'), 'utf8');
    const firstResponse = createDeferred<{ input: { handled: boolean } }>();
    interface FakeNode {
      attributes: Record<string, unknown>;
      elementId: number;
      id: string;
      tag: string;
    }
    interface FakeTimer {
      callback: () => void;
      dueAt: number;
    }
    const requests: Array<Record<string, unknown>> = [];
    const timers = new Map<number, FakeTimer>();
    let currentTime = 0;
    let nextTimerId = 1;
    let hasTree = true;
    const runAllTimers = (): void => {
      while (timers.size > 0) {
        const nextTimer = Array.from(timers.entries()).sort(
          ([firstId, first], [secondId, second]) => first.dueAt - second.dueAt || firstId - secondId,
        )[0];
        if (!nextTimer) throw new Error('Expected a scheduled timer.');
        const [timerId, timer] = nextTimer;
        timers.delete(timerId);
        currentTime = timer.dueAt;
        timer.callback();
      }
    };
    const settlePromises = async (): Promise<void> => {
      for (let index = 0; index < 12; index += 1) await Promise.resolve();
    };
    class FakePreviewElement {
      readonly dataset: { previewElementId: string; previewNodeId: string };

      constructor(nodeId: string, elementId: number) {
        this.dataset = { previewElementId: String(elementId), previewNodeId: nodeId };
      }

      closest(): FakePreviewElement {
        return this;
      }
    }
    class FakeInputElement extends FakePreviewElement {
      readonly isContentEditable = false;
      readonly selectionEnd: number;
      readonly selectionStart: number;
      readonly value: string;

      constructor(nodeId: string, value: string) {
        super(nodeId, 42);
        this.selectionEnd = value.length;
        this.selectionStart = value.length;
        this.value = value;
      }
    }
    class FakeTextAreaElement extends FakePreviewElement {}
    const oldInput = new FakeInputElement('old-input', 'old');
    const oldScroll = new FakePreviewElement('old-scroll', 43);
    const newInput = new FakeInputElement('new-input', 'new');
    const newScroll = new FakePreviewElement('new-scroll', 43);
    const nodes: Record<string, FakeNode> = {
      'new-input': { attributes: {}, elementId: 42, id: 'new-input', tag: 'textfield' },
      'new-scroll': { attributes: {}, elementId: 43, id: 'new-scroll', tag: 'scroll' },
      'old-input': { attributes: {}, elementId: 42, id: 'old-input', tag: 'textfield' },
      'old-scroll': { attributes: {}, elementId: 43, id: 'old-scroll', tag: 'scroll' },
    };
    const activeScrollNode = { current: nodes['old-scroll'] };
    const target = Object.freeze({ port: 13_591, clientId: 'client-a', contextId: 'context-a' });
    const state = {
      source: 'daemon',
      inputRefreshTimers: new Map(),
      snapshot: { target: { proxyPort: target.port, clientId: target.clientId, contextId: target.contextId } },
    };
    const vmContext = vm.createContext({
      HTMLInputElement: FakeInputElement,
      HTMLTextAreaElement: FakeTextAreaElement,
      activeScrollNode,
      addLog: () => {},
      apiPost: (_path: string, _params: Record<string, unknown>, payload: Record<string, unknown>) => {
        requests.push(payload);
        return payload['elementId'] === 99
          ? firstResponse.promise
          : Promise.resolve({ input: { handled: true, value: payload['text'] } });
      },
      document: { activeElement: null },
      elements: {
        device: { style: { setProperty() {} } },
        htmlPreviewRoot: {
          classList: { toggle() {} },
          contains: () => true,
          replaceChildren() {},
        },
      },
      hasSnapshotTree: () => hasTree,
      loadRealSnapshot: () => Promise.resolve(),
      newInput,
      newScroll,
      nodes,
      oldInput,
      oldScroll,
      runAllTimers,
      setHasTree: (value: boolean) => {
        hasTree = value;
      },
      state,
      target,
      window: {
        clearTimeout: (timerId: number) => timers.delete(timerId),
        setTimeout: (callback: () => void, delayMs: number) => {
          const timerId = nextTimerId;
          nextTimerId += 1;
          timers.set(timerId, { callback, dueAt: currentTime + delayMs });
          return timerId;
        },
      },
    });
    new vm.Script(
      `${treeModelSource}\n${modelSource}
      findNode = nodeId => nodes[nodeId] || null;
      findNodeAtPoint = (_point, predicate) => predicate(activeScrollNode.current) ? activeScrollNode.current : null;
      getElementIdForNode = node => node?.elementId ?? null;
      getNodeAttributes = node => node.attributes;
      pointFromScreenEvent = () => ({ x: 10, y: 20 });
      selectPreviewNode = () => {};
      ${previewSource}
      associateHtmlPreviewElement(oldInput, target);
      associateHtmlPreviewElement(oldScroll, target);`,
      { filename: 'debugger-input-bundle.js' },
    ).runInContext(vmContext);

    new vm.Script(
      `enqueueDebuggerInput(target, { type: 'focus', elementId: 99, focused: true }, { quiet: true, refresh: false });`,
    ).runInContext(vmContext);
    await settlePromises();
    expect(requests).toEqual([jasmine.objectContaining({ elementId: 99, type: 'focus' })]);

    new vm.Script(
      `dispatchHtmlPreviewTextInput({ target: oldInput });
      dispatchHtmlPreviewScrollInput({
        target: oldScroll,
        deltaX: 1,
        deltaY: 2,
        preventDefault() {},
        stopPropagation() {},
      });`,
    ).runInContext(vmContext);
    runAllTimers();
    await settlePromises();
    expect(requests.length).toBe(1);

    new vm.Script(
      `setHasTree(false);
      renderHtmlPreview();
      associateHtmlPreviewElement(newInput, target);
      associateHtmlPreviewElement(newScroll, target);
      activeScrollNode.current = nodes['new-scroll'];
      setHasTree(true);
      dispatchHtmlPreviewTextInput({ target: newInput });
      dispatchHtmlPreviewScrollInput({
        target: newScroll,
        deltaX: 3,
        deltaY: 4,
        preventDefault() {},
        stopPropagation() {},
      });`,
    ).runInContext(vmContext);
    runAllTimers();
    firstResponse.resolve({ input: { handled: true } });
    await (new vm.Script('debuggerInputDispatchTail').runInContext(vmContext) as Promise<unknown>);
    await settlePromises();

    expect(requests).toEqual([
      jasmine.objectContaining({ elementId: 99, type: 'focus' }),
      jasmine.objectContaining({ elementId: 42, text: 'new', type: 'text' }),
      jasmine.objectContaining({ deltaX: 3, deltaY: 4, elementId: 43, type: 'scroll' }),
    ]);
    expect(new vm.Script('htmlPreviewQueuedInputs.size').runInContext(vmContext)).toBe(0);
  });

  it('reserves debounced HTML input in event order across different timer deadlines and targets', async () => {
    const treeModelSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-tree-model.js'), 'utf8');
    const modelSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-model.js'), 'utf8');
    const previewSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-preview-html.js'), 'utf8');
    interface FakeNode {
      attributes: Record<string, unknown>;
      elementId: number;
      id: string;
      tag: string;
    }
    interface FakeTimer {
      callback: () => void;
      dueAt: number;
    }
    const requests: Array<{
      params: { port: number; clientId: string; contextId: string };
      payload: Record<string, unknown>;
    }> = [];
    const timers = new Map<number, FakeTimer>();
    let currentTime = 0;
    let nextTimerId = 1;
    const runNextTimer = (): void => {
      const nextTimer = Array.from(timers.entries()).sort(
        ([firstId, first], [secondId, second]) => first.dueAt - second.dueAt || firstId - secondId,
      )[0];
      if (!nextTimer) throw new Error('Expected a scheduled timer.');
      const [timerId, timer] = nextTimer;
      timers.delete(timerId);
      currentTime = timer.dueAt;
      timer.callback();
    };
    const settlePromises = async (): Promise<void> => {
      for (let index = 0; index < 12; index += 1) await Promise.resolve();
    };
    class FakePreviewElement {
      readonly dataset: { previewElementId: string; previewNodeId: string };

      constructor(nodeId: string, elementId: number) {
        this.dataset = { previewElementId: String(elementId), previewNodeId: nodeId };
      }

      closest(): FakePreviewElement {
        return this;
      }
    }
    class FakeInputElement extends FakePreviewElement {
      readonly isContentEditable = false;
      readonly selectionEnd = 5;
      readonly selectionStart = 5;
      readonly value = 'draft';

      blur(): void {}
    }
    class FakeTextAreaElement extends FakePreviewElement {}
    const editableElement = new FakeInputElement('input', 42);
    const scrollElementA = new FakePreviewElement('scroll-a', 43);
    const scrollElementB = new FakePreviewElement('scroll-b', 44);
    const tapElementB = new FakePreviewElement('tap-b', 45);
    const inputNode: FakeNode = { id: 'input', elementId: 42, tag: 'textfield', attributes: {} };
    const scrollNodeA: FakeNode = { id: 'scroll-a', elementId: 43, tag: 'scroll', attributes: {} };
    const scrollNodeB: FakeNode = { id: 'scroll-b', elementId: 44, tag: 'scroll', attributes: {} };
    const tapNodeB: FakeNode = { id: 'tap-b', elementId: 45, tag: 'view', attributes: {} };
    const nodes: Record<string, FakeNode> = {
      input: inputNode,
      'scroll-a': scrollNodeA,
      'scroll-b': scrollNodeB,
      'tap-b': tapNodeB,
    };
    const activeScrollNode = { current: scrollNodeA };
    const targetA = Object.freeze({ port: 13_591, clientId: 'client-a', contextId: 'context-a' });
    const targetB = Object.freeze({ port: 13_592, clientId: 'client-b', contextId: 'context-b' });
    const state = {
      source: 'daemon',
      inputRefreshTimers: new Map(),
      snapshot: { target: { proxyPort: targetA.port, clientId: targetA.clientId, contextId: targetA.contextId } },
    };
    const context = {
      HTMLInputElement: FakeInputElement,
      HTMLTextAreaElement: FakeTextAreaElement,
      activeScrollNode,
      addLog: () => {},
      apiPost: (
        _path: string,
        params: { port: number; clientId: string; contextId: string },
        payload: Record<string, unknown>,
      ) => {
        requests.push({ params, payload });
        return Promise.resolve({ input: { handled: true } });
      },
      document: { activeElement: null },
      editableElement,
      elements: { htmlPreviewRoot: { contains: () => true } },
      hasSnapshotTree: () => true,
      loadRealSnapshot: () => Promise.resolve(),
      nodes,
      scrollElementA,
      scrollElementB,
      state,
      tapElementB,
      targetA,
      targetB,
      testFindNode: (nodeId: string) => nodes[nodeId] || null,
      testFindNodeAtPoint: (_point: unknown, predicate: (candidate: FakeNode) => boolean) =>
        predicate(activeScrollNode.current) ? activeScrollNode.current : null,
      window: {
        clearTimeout: (timerId: number) => timers.delete(timerId),
        setTimeout: (callback: () => void, delayMs: number) => {
          const timerId = nextTimerId;
          nextTimerId += 1;
          timers.set(timerId, { callback, dueAt: currentTime + delayMs });
          return timerId;
        },
      },
    };
    const setup = new vm.Script(
      `${treeModelSource}\n${modelSource}
      findNode = testFindNode;
      findNodeAtPoint = testFindNodeAtPoint;
      getElementIdForNode = node => node?.elementId ?? null;
      getNodeAttributes = node => node.attributes;
      pointFromScreenEvent = () => ({ x: 10, y: 20 });
      selectPreviewNode = () => {};
      ${previewSource}
      associateHtmlPreviewElement(editableElement, targetA);
      associateHtmlPreviewElement(scrollElementA, targetA);
      associateHtmlPreviewElement(scrollElementB, targetB);
      associateHtmlPreviewElement(tapElementB, targetB);`,
      { filename: 'debugger-input-bundle.js' },
    );
    const vmContext = vm.createContext(context);
    setup.runInContext(vmContext);

    activeScrollNode.current = scrollNodeB;
    new vm.Script(
      `dispatchHtmlPreviewTextInput({ target: editableElement });
      dispatchHtmlPreviewScrollInput({
        target: scrollElementB,
        deltaX: 2,
        deltaY: 3,
        preventDefault() {},
        stopPropagation() {},
      });`,
    ).runInContext(vmContext);

    runNextTimer();
    await settlePromises();
    expect(requests).toEqual([]);
    runNextTimer();
    await settlePromises();
    expect(requests.map(request => request.payload['type'])).toEqual(['text', 'scroll']);
    expect(requests.map(request => request.params.contextId)).toEqual(['context-a', 'context-b']);

    activeScrollNode.current = scrollNodeA;
    const wheelBeforeTap = new vm.Script(
      `dispatchHtmlPreviewScrollInput({
        target: scrollElementA,
        deltaX: 0,
        deltaY: 4,
        preventDefault() {},
        stopPropagation() {},
      });
      dispatchHtmlPreviewTapInput({
        target: tapElementB,
        preventDefault() {},
        stopPropagation() {},
      });`,
    ).runInContext(vmContext) as Promise<void>;
    await settlePromises();
    expect(requests.map(request => request.payload['type'])).toEqual(['text', 'scroll']);
    runNextTimer();
    await wheelBeforeTap;
    expect(requests.map(request => request.payload['type'])).toEqual(['text', 'scroll', 'scroll', 'tap']);

    const lifecycle = new vm.Script(
      `(() => {
        const focus = dispatchHtmlPreviewFocusInput({ target: editableElement }, true);
        dispatchHtmlPreviewTextInput({ target: editableElement });
        const blur = dispatchHtmlPreviewFocusInput({ target: editableElement }, false);
        const tap = dispatchHtmlPreviewTapInput({
          target: tapElementB,
          preventDefault() {},
          stopPropagation() {},
        });
        return Promise.all([focus, blur, tap]);
      })();`,
    ).runInContext(vmContext) as Promise<unknown>;
    await lifecycle;
    expect(requests.slice(-4).map(request => request.payload['type'])).toEqual(['focus', 'text', 'focus', 'tap']);
    expect(requests.at(-4)?.payload['focused']).toBeTrue();
    expect(requests.at(-2)?.payload['focused']).toBeFalse();
  });

  it('serializes input across elements while preserving each immutable target', async () => {
    const modelSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-model.js'), 'utf8');
    const firstDispatch = createDeferred<{ input: { handled: boolean } }>();
    const requests: Array<{
      params: { port: number; clientId: string; contextId: string };
      payload: Record<string, unknown>;
    }> = [];
    const state = {
      source: 'daemon',
      inputRefreshTimers: new Map(),
      snapshot: {
        target: { proxyPort: 13_591, clientId: 'client-a', contextId: 'context-a' },
      },
    };
    let first = true;
    const operation = new vm.Script(
      `${modelSource}
      (() => {
        const targetA = captureDebuggerInputTarget();
        const focus = enqueueDebuggerInput(targetA, { type: 'focus', elementId: 1, focused: true }, { quiet: true, refresh: false });
        const text = enqueueDebuggerInput(targetA, { type: 'text', elementId: 1, text: 'draft' }, { quiet: true, refresh: false });
        const blur = enqueueDebuggerInput(targetA, { type: 'focus', elementId: 1, focused: false }, { quiet: true, refresh: false });
        state.snapshot.target = { proxyPort: 13592, clientId: 'client-b', contextId: 'context-b' };
        const targetB = captureDebuggerInputTarget();
        const tap = enqueueDebuggerInput(targetB, { type: 'tap', elementId: 2 }, { quiet: true, refresh: false });
        return Promise.all([focus, text, blur, tap]);
      })();`,
      { filename: 'debugger-model.js' },
    ).runInNewContext({
      state,
      getSelectedTargetParams: () => {
        const target = state.snapshot.target;
        return { port: target.proxyPort, clientId: target.clientId, contextId: target.contextId };
      },
      apiPost: (
        _path: string,
        params: { port: number; clientId: string; contextId: string },
        payload: Record<string, unknown>,
      ) => {
        requests.push({ params, payload });
        if (first) {
          first = false;
          return firstDispatch.promise;
        }
        return Promise.resolve({ input: { handled: true } });
      },
      addLog: () => {},
    }) as Promise<unknown>;

    for (let index = 0; index < 6; index += 1) await Promise.resolve();
    expect(requests.map(request => request.payload['type'])).toEqual(['focus']);
    firstDispatch.resolve({ input: { handled: true } });
    await operation;

    expect(requests.map(request => request.payload['type'])).toEqual(['focus', 'text', 'focus', 'tap']);
    expect(requests.slice(0, 3).map(request => request.params.contextId)).toEqual([
      'context-a',
      'context-a',
      'context-a',
    ]);
    expect(requests[3]?.params.contextId).toBe('context-b');
    expect(Object.isFrozen(requests[0]?.params)).toBeTrue();
    expect(Object.isFrozen(requests[3]?.params)).toBeTrue();
  });

  it('uses unambiguous debugger input queue keys', () => {
    const modelSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-model.js'), 'utf8');
    const keys = new vm.Script(
      `${modelSource}
      [
        debuggerInputTargetKey({ port: 1, clientId: 'a:b', contextId: 'c' }),
        debuggerInputTargetKey({ port: 1, clientId: 'a', contextId: 'b:c' }),
        debuggerInputTargetElementKey({ port: 1, clientId: 'a:b', contextId: 'c' }, 2),
        debuggerInputTargetElementKey({ port: 1, clientId: 'a', contextId: 'b:c' }, 2),
      ];`,
      { filename: 'debugger-model.js' },
    ).runInNewContext({}) as string[];

    expect(keys[0]).not.toBe(keys[1]);
    expect(keys[2]).not.toBe(keys[3]);
  });

  it('recovers an active CPU profile from the contexts response', async () => {
    const performanceSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-performance.js'), 'utf8');
    const bootstrapSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-bootstrap.js'), 'utf8');
    const performanceState = {
      profileActive: false,
      activeProfileContextId: null as string | null,
      lastProfile: null,
      profileContexts: [],
    };
    const profileContextSelect = {
      value: 'previous-context',
      innerHTML: '',
      disabled: false,
    };
    const profileStartButton = { disabled: false };
    const profileStopButton = { disabled: true };
    const context = {
      state: { performance: performanceState },
      elements: {
        profileStatusPill: { textContent: '', className: '' },
        profileStartButton,
        profileStopButton,
        profileCaptureButton: { disabled: false },
        profileExportButton: { disabled: false },
        profileSummary: { innerHTML: '' },
        profileContextSelect,
      },
      apiGet: () =>
        Promise.resolve({
          contexts: [
            { id: 'previous-context', title: 'Previous context' },
            { id: 'active-context', title: 'Active context' },
          ],
          active: {
            profiling: true,
            contextId: 'active-context',
            contextTitle: 'Active context',
          },
        }),
      addLog: () => {},
      escapeHtml: String,
    };

    await (new vm.Script(`${performanceSource}\nrefreshProfileContexts({ silent: true });`, {
      filename: 'debugger-performance.js',
    }).runInNewContext(context) as Promise<void>);

    expect(performanceState.profileActive).toBeTrue();
    expect(performanceState.activeProfileContextId).toBe('active-context');
    expect(profileContextSelect.innerHTML).toContain('value="active-context" selected');
    expect(profileContextSelect.disabled).toBeTrue();
    expect(profileStartButton.disabled).toBeTrue();
    expect(profileStopButton.disabled).toBeFalse();
    expect(bootstrapSource).toContain('void refreshProfileContexts({ silent: true });');
  });

  it('shows one-shot CPU capture as active until the profile completes', async () => {
    const performanceSource = fs.readFileSync(path.join(cliRoot, 'debugger', 'debugger-performance.js'), 'utf8');
    const completedProfile = {
      profile: { nodes: [] },
      summary: { sampleCount: 3, nodeCount: 0 },
    };
    const pendingCapture = createDeferred<typeof completedProfile>();
    const performanceState = {
      profileActive: false,
      activeProfileContextId: null as string | null,
      lastProfile: null as typeof completedProfile | null,
      profileContexts: [{ id: 'requested-context', title: 'Requested context' }],
    };
    const profileStatusPill = { textContent: '', className: '' };
    const profileContextSelect = {
      value: 'requested-context',
      innerHTML: '',
      disabled: false,
    };
    const profileStartButton = { disabled: false };
    const profileStopButton = { disabled: true };
    const profileCaptureButton = { disabled: false };
    const context = {
      state: { performance: performanceState },
      elements: {
        profileStatusPill,
        profileStartButton,
        profileStopButton,
        profileCaptureButton,
        profileExportButton: { disabled: true },
        profileSummary: { innerHTML: '' },
        profileContextSelect,
        profileDurationInput: { value: '5' },
      },
      apiPost: () => pendingCapture.promise,
      addLog: () => {},
      escapeHtml: String,
    };

    const captureOperation = new vm.Script(`${performanceSource}\ncaptureCpuProfile();`, {
      filename: 'debugger-performance.js',
    }).runInNewContext(context) as Promise<void>;

    expect(performanceState.profileActive).toBeTrue();
    expect(performanceState.activeProfileContextId).toBe('requested-context');
    expect(profileStatusPill.textContent).toBe('Recording');
    expect(profileContextSelect.disabled).toBeTrue();
    expect(profileStartButton.disabled).toBeTrue();
    expect(profileStopButton.disabled).toBeFalse();
    expect(profileCaptureButton.disabled).toBeTrue();

    pendingCapture.resolve(completedProfile);
    await captureOperation;

    expect(performanceState.profileActive).toBeFalse();
    expect(performanceState.activeProfileContextId).toBeNull();
    expect(performanceState.lastProfile).toBe(completedProfile);
    expect(profileStatusPill.textContent).toBe('Idle');
    expect(profileContextSelect.disabled).toBeFalse();
    expect(profileStartButton.disabled).toBeFalse();
    expect(profileStopButton.disabled).toBeTrue();
    expect(profileCaptureButton.disabled).toBeFalse();
  });
});
