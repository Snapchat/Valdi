import 'jasmine/src/jasmine';

import type { IRenderedElement } from '../src/IRenderedElement';
import type { IRendererDelegate } from '../src/IRendererDelegate';
import type { RendererObserver } from '../src/IRenderer';
import { NodePrototype } from '../src/NodePrototype';
import { Renderer } from '../src/Renderer';
import type { ValdiRuntime } from '../src/ValdiRuntime';
import type { DebugSettingsSnapshot } from '../src/debugging/DebugSettings';
import { NativeAppearanceDebugSettings, NativeLayoutDirection } from '../src/debugging/NativeAppearanceDebugSettings';

declare const runtime: ValdiRuntime;

interface NativeAppearanceTestGlobal {
  __VALDI_DEBUG_SETTINGS__?: {
    getSnapshot(): DebugSettingsSnapshot;
    resetValue(groupId: string, settingId: string): DebugSettingsSnapshot;
    setValue(groupId: string, settingId: string, value: unknown): DebugSettingsSnapshot;
  };
}

interface NativeAppearanceTestRoot {
  readonly attributes: Record<string, unknown>;
  readonly element: IRenderedElement;
  readonly setDirections: unknown[];
}

interface NativeAppearanceTestRenderer {
  readonly observers: RendererObserver[];
  readonly renderedDirectionElements: Set<IRenderedElement>;
  readonly renderCompletions: (() => void)[];
  renderer: Renderer;
  readonly roots: NativeAppearanceTestRoot[];
}

function makeNativeAppearanceTestRoot(direction: unknown): NativeAppearanceTestRoot {
  const attributes: Record<string, unknown> = { direction };
  const setDirections: unknown[] = [];
  const element = {
    getAttribute: (name: string): unknown => attributes[name],
    setAttribute: (name: string, value: unknown): boolean => {
      if (attributes[name] === value) {
        return false;
      }
      attributes[name] = value;
      if (name === 'direction') {
        setDirections.push(value);
      }
      return true;
    },
  } as unknown as IRenderedElement;
  return { attributes, element, setDirections };
}

function makeNativeAppearanceTestRenderer(directions: readonly unknown[]): NativeAppearanceTestRenderer {
  const state: NativeAppearanceTestRenderer = {
    observers: [],
    renderedDirectionElements: new Set<IRenderedElement>(),
    renderCompletions: [],
    renderer: undefined as unknown as Renderer,
    roots: directions.map(makeNativeAppearanceTestRoot),
  };
  state.renderer = {
    addObserver: (observer: RendererObserver): void => {
      state.observers.push(observer);
    },
    getRootElements: (): IRenderedElement[] => state.roots.map(root => root.element),
    onRenderComplete: (callback: () => void): void => {
      state.renderCompletions.push(callback);
    },
    removeObserver: (observer: RendererObserver): void => {
      const index = state.observers.indexOf(observer);
      if (index >= 0) {
        state.observers.splice(index, 1);
      }
    },
    wasElementDirectionSetDuringCurrentRender: (element: IRenderedElement): boolean => {
      return state.renderedDirectionElements.delete(element);
    },
  } as unknown as Renderer;
  return state;
}

function completeNativeAppearanceTestRender(renderer: NativeAppearanceTestRenderer): void {
  const completions = renderer.renderCompletions.splice(0);
  completions.forEach(completion => completion());
}

function makeNoopRendererDelegate(): IRendererDelegate {
  return new Proxy(
    {},
    {
      get: () => () => {},
    },
  ) as IRendererDelegate;
}

describe('NativeAppearanceDebugSettings', () => {
  let previousDebugEnabled: boolean;
  let settings: NativeAppearanceDebugSettings;

  beforeEach(() => {
    previousDebugEnabled = runtime.isDebugEnabled;
    runtime.isDebugEnabled = true;
    settings = new NativeAppearanceDebugSettings();
  });

  afterEach(() => {
    settings.dispose();
    runtime.isDebugEnabled = previousDebugEnabled;
  });

  it('publishes an explicit system/LTR/RTL setting and restores each native root default', () => {
    const android = makeNativeAppearanceTestRenderer(['inherit']);
    const ios = makeNativeAppearanceTestRenderer([undefined]);
    settings.addRenderer(android.renderer);
    settings.addRenderer(ios.renderer);
    const bridge = (globalThis as NativeAppearanceTestGlobal).__VALDI_DEBUG_SETTINGS__;

    expect(bridge?.getSnapshot().groups).toContain(
      jasmine.objectContaining({
        categoryId: 'appearance',
        id: 'valdi.appearance',
        label: 'Appearance',
        settings: [
          jasmine.objectContaining({
            defaultValue: NativeLayoutDirection.System,
            id: 'layout-direction',
            kind: 'select',
            label: 'Layout direction',
            options: [
              { label: 'System', value: NativeLayoutDirection.System },
              { label: 'Left to right', value: NativeLayoutDirection.LeftToRight },
              { label: 'Right to left', value: NativeLayoutDirection.RightToLeft },
            ],
            value: NativeLayoutDirection.System,
          }),
        ],
      }),
    );

    bridge?.setValue('valdi.appearance', 'layout-direction', NativeLayoutDirection.RightToLeft);
    expect(android.roots[0]?.attributes['direction']).toBe('rtl');
    expect(ios.roots[0]?.attributes['direction']).toBe('rtl');

    bridge?.setValue('valdi.appearance', 'layout-direction', NativeLayoutDirection.LeftToRight);
    expect(android.roots[0]?.attributes['direction']).toBe('ltr');
    expect(ios.roots[0]?.attributes['direction']).toBe('ltr');

    bridge?.resetValue('valdi.appearance', 'layout-direction');
    expect(android.roots[0]?.attributes['direction']).toBe('inherit');
    expect(ios.roots[0]?.attributes['direction']).toBeUndefined();
  });

  it('tracks application direction changes on newly rendered and rerendered roots', () => {
    const bridge = (globalThis as NativeAppearanceTestGlobal).__VALDI_DEBUG_SETTINGS__;
    bridge?.setValue('valdi.appearance', 'layout-direction', NativeLayoutDirection.RightToLeft);
    const pendingRenderer = makeNativeAppearanceTestRenderer([]);
    settings.addRenderer(pendingRenderer.renderer);

    const root = makeNativeAppearanceTestRoot('inherit');
    pendingRenderer.roots.push(root);
    pendingRenderer.observers[0]?.onRootElementWillEndRender?.();
    completeNativeAppearanceTestRender(pendingRenderer);
    expect(root.attributes['direction']).toBe('rtl');

    root.attributes['direction'] = 'application';
    pendingRenderer.renderedDirectionElements.add(root.element);
    pendingRenderer.observers[0]?.onRootElementWillEndRender?.();
    completeNativeAppearanceTestRender(pendingRenderer);
    expect(root.attributes['direction']).toBe('rtl');

    bridge?.resetValue('valdi.appearance', 'layout-direction');
    expect(root.attributes['direction']).toBe('application');
  });

  it('updates and restores every top-level root immediately', () => {
    const renderer = makeNativeAppearanceTestRenderer(['first', 'second']);
    settings.addRenderer(renderer.renderer);
    const bridge = (globalThis as NativeAppearanceTestGlobal).__VALDI_DEBUG_SETTINGS__;

    bridge?.setValue('valdi.appearance', 'layout-direction', NativeLayoutDirection.LeftToRight);
    expect(renderer.roots.map(root => root.attributes['direction'])).toEqual(['ltr', 'ltr']);

    bridge?.resetValue('valdi.appearance', 'layout-direction');
    expect(renderer.roots.map(root => root.attributes['direction'])).toEqual(['first', 'second']);
  });

  it('restores a direction rerendered to the same value as the active override', () => {
    const renderer = new Renderer('appearance-test', undefined, makeNoopRendererDelegate());
    const node = new NodePrototype('view', 'view');
    settings.addRenderer(renderer);
    const renderDirection = (direction: string): void => {
      renderer.begin();
      renderer.beginElement(node);
      renderer.setAttributeString('direction', direction);
      renderer.endElement();
      renderer.end();
    };

    renderDirection('inherit');
    const bridge = (globalThis as NativeAppearanceTestGlobal).__VALDI_DEBUG_SETTINGS__;
    bridge?.setValue('valdi.appearance', 'layout-direction', NativeLayoutDirection.RightToLeft);
    expect(renderer.getRootElements()[0]?.getAttribute('direction')).toBe('rtl');

    renderDirection('rtl');
    expect(renderer.getRootElements()[0]?.getAttribute('direction')).toBe('rtl');

    bridge?.resetValue('valdi.appearance', 'layout-direction');
    expect(renderer.getRootElements()[0]?.getAttribute('direction')).toBe('rtl');
  });

  it('prunes replaced roots while preserving the replacement root default', () => {
    const renderer = makeNativeAppearanceTestRenderer(['original']);
    const originalRoot = renderer.roots[0]!;
    settings.addRenderer(renderer.renderer);
    const bridge = (globalThis as NativeAppearanceTestGlobal).__VALDI_DEBUG_SETTINGS__;
    bridge?.setValue('valdi.appearance', 'layout-direction', NativeLayoutDirection.RightToLeft);
    const originalRootSetCount = originalRoot.setDirections.length;

    const replacementRoot = makeNativeAppearanceTestRoot('replacement');
    renderer.roots.push(replacementRoot);
    renderer.observers[0]?.onRootElementWillEndRender?.();
    renderer.roots.splice(0, 2, replacementRoot);
    completeNativeAppearanceTestRender(renderer);
    expect(originalRoot.attributes['direction']).toBe('rtl');
    expect(originalRoot.setDirections.length).toBe(originalRootSetCount);
    expect(replacementRoot.attributes['direction']).toBe('rtl');

    bridge?.setValue('valdi.appearance', 'layout-direction', NativeLayoutDirection.LeftToRight);
    bridge?.resetValue('valdi.appearance', 'layout-direction');
    expect(originalRoot.attributes['direction']).toBe('rtl');
    expect(originalRoot.setDirections.length).toBe(originalRootSetCount);
    expect(replacementRoot.attributes['direction']).toBe('replacement');
  });

  it('replaces the prior observer when the same renderer is added twice', () => {
    const renderer = makeNativeAppearanceTestRenderer(['application']);
    const removeFirst = settings.addRenderer(renderer.renderer);
    const firstObserver = renderer.observers[0];

    const removeSecond = settings.addRenderer(renderer.renderer);
    expect(renderer.observers.length).toBe(1);
    expect(renderer.observers[0]).not.toBe(firstObserver);

    removeFirst();
    expect(renderer.observers.length).toBe(1);

    const bridge = (globalThis as NativeAppearanceTestGlobal).__VALDI_DEBUG_SETTINGS__;
    bridge?.setValue('valdi.appearance', 'layout-direction', NativeLayoutDirection.RightToLeft);
    expect(renderer.roots[0]?.attributes['direction']).toBe('rtl');

    removeSecond();
    expect(renderer.observers.length).toBe(0);
    expect(renderer.roots[0]?.attributes['direction']).toBe('application');
  });

  it('stops updating disposed roots and unregisters the framework setting', () => {
    const disposedRenderer = makeNativeAppearanceTestRenderer(['application']);
    const removeRenderer = settings.addRenderer(disposedRenderer.renderer);
    const bridge = (globalThis as NativeAppearanceTestGlobal).__VALDI_DEBUG_SETTINGS__;
    bridge?.setValue('valdi.appearance', 'layout-direction', NativeLayoutDirection.RightToLeft);

    removeRenderer();
    bridge?.setValue('valdi.appearance', 'layout-direction', NativeLayoutDirection.LeftToRight);
    expect(disposedRenderer.roots[0]?.attributes['direction']).toBe('application');
    expect(disposedRenderer.observers.length).toBe(0);

    settings.dispose();
    expect((globalThis as NativeAppearanceTestGlobal).__VALDI_DEBUG_SETTINGS__).toBeUndefined();
  });

  it('does not expose a mutable layout override when runtime debugging is disabled', () => {
    settings.dispose();
    runtime.isDebugEnabled = false;

    settings = new NativeAppearanceDebugSettings();

    expect((globalThis as NativeAppearanceTestGlobal).__VALDI_DEBUG_SETTINGS__).toBeUndefined();
  });
});
