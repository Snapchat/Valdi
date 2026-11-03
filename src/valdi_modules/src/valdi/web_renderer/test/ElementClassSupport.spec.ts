import { applyFontString, getActiveElement, SYSTEM_FONT_FAMILY } from '../src/elements/ElementClassSupport';

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

  describe('applyFontString', () => {
    function fakeElement(): HTMLElement {
      return { style: {} } as unknown as HTMLElement;
    }

    it('recovers the weight and installed family from a PostScript face name', () => {
      const element = fakeElement();
      applyFontString(element, 'AvenirNext-DemiBold 16', 'font');

      expect(element.style.fontFamily).toBe('AvenirNext-DemiBold, "Avenir Next", sans-serif');
      expect(element.style.fontWeight).toBe('600');
      expect(element.style.fontSize).toBe('16px');
    });

    it('accepts multi-word CSS families with a trailing weight', () => {
      const element = fakeElement();
      applyFontString(element, 'Times New Roman 16 bold', 'font');

      expect(element.style.fontFamily).toBe('"Times New Roman", sans-serif');
      expect(element.style.fontWeight).toBe('bold');
    });

    it('keeps system font descriptors on the system stack', () => {
      const element = fakeElement();
      applyFontString(element, 'system-bold 14', 'font');

      expect(element.style.fontFamily).toBe(SYSTEM_FONT_FAMILY);
      expect(element.style.fontWeight).toBe('600');
    });

    it('clears the weight left behind by a previous font', () => {
      const element = fakeElement();
      applyFontString(element, 'AvenirNext-Bold 16', 'font');
      applyFontString(element, 'Helvetica 12', 'font');

      expect(element.style.fontWeight).toBe('');
    });
  });
});
