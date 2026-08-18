import { getActiveElement } from '../src/elements/ElementClassSupport';

class FakeShadowRoot {
  constructor(readonly activeElement: Element | null) {}
}

describe('ElementClassSupport', () => {
  const previousShadowRoot = globalThis.ShadowRoot;

  beforeEach(() => {
    (globalThis as unknown as { ShadowRoot: typeof FakeShadowRoot }).ShadowRoot = FakeShadowRoot;
  });

  afterEach(() => {
    (globalThis as unknown as { ShadowRoot: typeof ShadowRoot }).ShadowRoot = previousShadowRoot;
  });

  it('reads the active element from the containing shadow root', () => {
    const activeElement = {} as Element;
    const root = new FakeShadowRoot(activeElement);
    const element = { getRootNode: () => root } as unknown as HTMLElement;

    expect(getActiveElement(element)).toBe(activeElement);
  });
});
