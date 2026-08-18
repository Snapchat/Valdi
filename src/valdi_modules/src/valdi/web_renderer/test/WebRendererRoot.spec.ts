import 'jasmine/src/jasmine';
import {
  createIsolatedWebRendererRoot,
  registerWebRendererLayoutRoot,
  setWebRendererLayoutDirection,
} from '../src/WebRendererRoot';

class FakeShadowRoot {
  children: FakeElement[] = [];

  replaceChildren(...children: FakeElement[]): void {
    this.children = children;
  }
}

interface FakeElement {
  textContent?: string;
  style: Record<string, string>;
}

function makeFakeElement(): FakeElement {
  return { style: {} };
}

describe('WebRendererRoot', () => {
  const previousDocument = globalThis.document;
  const previousShadowRoot = globalThis.ShadowRoot;

  beforeEach(() => {
    (globalThis as unknown as { ShadowRoot: typeof FakeShadowRoot }).ShadowRoot = FakeShadowRoot;
    (globalThis as unknown as { document: { createElement(): FakeElement } }).document = {
      createElement: makeFakeElement,
    };
  });

  afterEach(() => {
    (globalThis as unknown as { document: Document }).document = previousDocument;
    (globalThis as unknown as { ShadowRoot: typeof ShadowRoot }).ShadowRoot = previousShadowRoot;
  });

  it('mounts the renderer in an open shadow root with isolated inherited styles', () => {
    const shadowRoot = new FakeShadowRoot();
    let attachedMode: ShadowRootMode | undefined;
    const host = {
      shadowRoot: null,
      attachShadow(init: ShadowRootInit) {
        attachedMode = init.mode;
        return shadowRoot;
      },
    };

    const root = createIsolatedWebRendererRoot(host as unknown as HTMLElement) as unknown as FakeElement;

    expect(attachedMode).toBe('open');
    expect(shadowRoot.children.length).toBe(2);
    expect(shadowRoot.children[0].textContent).toContain('box-sizing: border-box');
    expect(shadowRoot.children[0].textContent).toContain('content: attr(placeholder)');
    expect(shadowRoot.children[1]).toBe(root);
    expect(root.style.all).toBe('initial');
    expect(root.style.direction).toBeUndefined();
    expect(root.style.display).toBe('block');
    expect(root.style.fontFamily).toBe('inherit');
    expect(root.style.MozOsxFontSmoothing).toBe('inherit');
    expect(root.style.webkitFontSmoothing).toBe('inherit');
    expect(root.style.height).toBe('100%');
    expect(root.style.width).toBe('100%');
  });

  it('uses a supplied shadow root without creating another one', () => {
    const shadowRoot = new FakeShadowRoot();

    const root = createIsolatedWebRendererRoot(shadowRoot as unknown as ShadowRoot) as unknown as FakeElement;

    expect(shadowRoot.children[1]).toBe(root);
  });

  it('applies runtime layout direction only to its matching renderer context', () => {
    const firstRoot = makeFakeElement();
    const secondRoot = makeFakeElement();
    const firstRegistration = registerWebRendererLayoutRoot('first', firstRoot as unknown as HTMLElement);
    const secondRegistration = registerWebRendererLayoutRoot('second', secondRoot as unknown as HTMLElement);

    setWebRendererLayoutDirection('first', true);

    expect(firstRoot.style.direction).toBe('rtl');
    expect(secondRoot.style.direction).toBeUndefined();

    firstRegistration.dispose();
    secondRegistration.dispose();
  });
});
