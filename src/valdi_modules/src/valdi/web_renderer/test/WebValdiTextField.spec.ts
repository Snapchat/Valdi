import 'jasmine/src/jasmine';
import { WebValdiTextField } from '../src/views/WebValdiTextField';

function installDomStubs() {
  // WebValdiTextField defines `class ValdiInput extends HTMLElement` at
  // module scope — the class declaration evaluates on first import, so
  // HTMLElement has to exist before `new WebValdiTextField()` triggers
  // the lazy module load.
  (globalThis as any).HTMLElement = class {};
  (globalThis as any).customElements = { get: () => undefined, define: () => {} };

  (globalThis as any).document = {
    createElement: (tag: string) => {
      const style: Record<string, string> = {};
      const attrs = new Map<string, string>();
      const el: any = {
        tagName: tag.toUpperCase(),
        style,
        children: [],
        getAttribute: (name: string) => (attrs.has(name) ? attrs.get(name)! : null),
        setAttribute: (name: string, value: string) => { attrs.set(name, String(value)); },
        removeAttribute: (name: string) => { attrs.delete(name); },
        hasAttribute: (name: string) => attrs.has(name),
        addEventListener: () => {},
        removeEventListener: () => {},
        appendChild: () => {},
        getBoundingClientRect: () => ({ width: 0, height: 0, x: 0, y: 0, left: 0, top: 0 }),
        getRootNode: () => (globalThis as any).document,
        contains: () => false,
        setAttributeDelegate: () => {},
      };
      style.setProperty = (() => {}) as any;
      style.removeProperty = (() => {}) as any;
      return el;
    },
    dir: 'ltr',
    body: { contains: () => false },
    activeElement: null,
    addEventListener: () => {},
    removeEventListener: () => {},
  };
  (globalThis as any).window = { devicePixelRatio: 1, setTimeout, clearTimeout };
  (globalThis as any).MutationObserver = function () {
    return { observe: () => {}, disconnect: () => {} };
  };
  (globalThis as any).ResizeObserver = function () {
    return { observe: () => {}, unobserve: () => {}, disconnect: () => {} };
  };
  (globalThis as any).IntersectionObserver = function () {
    return { observe: () => {}, unobserve: () => {}, disconnect: () => {} };
  };
  (globalThis as any).requestAnimationFrame = (cb: Function) => cb();
  (globalThis as any).performance = { now: () => 0 };
}

function uninstallDomStubs() {
  delete (globalThis as any).HTMLElement;
  delete (globalThis as any).customElements;
  delete (globalThis as any).document;
  delete (globalThis as any).window;
  delete (globalThis as any).MutationObserver;
  delete (globalThis as any).ResizeObserver;
  delete (globalThis as any).IntersectionObserver;
  delete (globalThis as any).requestAnimationFrame;
  delete (globalThis as any).performance;
}

function attrSnapshot(el: HTMLElement, names: string[]): Record<string, string | null> {
  const out: Record<string, string | null> = {};
  for (const name of names) out[name] = el.getAttribute(name);
  return out;
}

const TRAIT_ATTRS = ['type', 'inputmode', 'pattern', 'autocomplete', 'autocorrect', 'spellcheck'];

describe('WebValdiTextField – contentType', () => {
  afterEach(() => uninstallDomStubs());

  const cases: { contentType: string; expected: Record<string, string | null> }[] = [
    {
      contentType: 'default',
      expected: { type: 'text', inputmode: null, pattern: null, autocomplete: null, autocorrect: null, spellcheck: null },
    },
    {
      contentType: 'phoneNumber',
      expected: { type: 'tel', inputmode: null, pattern: null, autocomplete: null, autocorrect: null, spellcheck: null },
    },
    {
      contentType: 'email',
      expected: { type: 'email', inputmode: null, pattern: null, autocomplete: null, autocorrect: null, spellcheck: null },
    },
    {
      contentType: 'password',
      expected: { type: 'password', inputmode: null, pattern: null, autocomplete: null, autocorrect: null, spellcheck: null },
    },
    {
      contentType: 'passwordNumber',
      expected: { type: 'password', inputmode: 'numeric', pattern: '[0-9]*', autocomplete: null, autocorrect: null, spellcheck: null },
    },
    {
      contentType: 'passwordVisible',
      expected: { type: 'text', inputmode: null, pattern: null, autocomplete: 'off', autocorrect: null, spellcheck: null },
    },
    {
      contentType: 'url',
      expected: { type: 'url', inputmode: null, pattern: null, autocomplete: null, autocorrect: null, spellcheck: null },
    },
    {
      contentType: 'number',
      expected: { type: 'text', inputmode: 'numeric', pattern: '[0-9]*', autocomplete: null, autocorrect: null, spellcheck: null },
    },
    {
      contentType: 'numberDecimal',
      expected: { type: 'text', inputmode: 'decimal', pattern: null, autocomplete: null, autocorrect: null, spellcheck: null },
    },
    {
      contentType: 'numberDecimalSigned',
      expected: { type: 'text', inputmode: 'text', pattern: '-?[0-9]*[.,]?[0-9]*', autocomplete: null, autocorrect: null, spellcheck: null },
    },
    {
      contentType: 'noSuggestions',
      expected: { type: 'text', inputmode: null, pattern: null, autocomplete: 'off', autocorrect: 'off', spellcheck: 'false' },
    },
  ];

  for (const { contentType, expected } of cases) {
    it(`maps "${contentType}" to the expected input attributes`, () => {
      installDomStubs();
      const tf = new WebValdiTextField(1);
      tf.changeAttribute('contentType', contentType);
      expect(attrSnapshot(tf.htmlElement, TRAIT_ATTRS)).toEqual(expected);
    });
  }

  it('falls back to the default mapping for an unknown contentType', () => {
    installDomStubs();
    const tf = new WebValdiTextField(1);
    tf.changeAttribute('contentType', 'totallyMadeUp');
    expect(tf.htmlElement.getAttribute('type')).toBe('text');
    expect(tf.htmlElement.getAttribute('inputmode')).toBeNull();
  });

  it('clears stale secondary attrs when switching contentType', () => {
    installDomStubs();
    const tf = new WebValdiTextField(1);
    tf.changeAttribute('contentType', 'noSuggestions');
    expect(tf.htmlElement.getAttribute('autocomplete')).toBe('off');
    expect(tf.htmlElement.getAttribute('autocorrect')).toBe('off');
    expect(tf.htmlElement.getAttribute('spellcheck')).toBe('false');

    tf.changeAttribute('contentType', 'email');
    expect(tf.htmlElement.getAttribute('type')).toBe('email');
    expect(tf.htmlElement.getAttribute('autocomplete')).toBeNull();
    expect(tf.htmlElement.getAttribute('autocorrect')).toBeNull();
    expect(tf.htmlElement.getAttribute('spellcheck')).toBeNull();
  });

  it('does not touch attributes that are already at the target value', () => {
    installDomStubs();
    const tf = new WebValdiTextField(1);
    tf.changeAttribute('contentType', 'password');

    const setSpy = spyOn(tf.htmlElement, 'setAttribute').and.callThrough();
    const removeSpy = spyOn(tf.htmlElement, 'removeAttribute').and.callThrough();

    tf.changeAttribute('contentType', 'password');

    expect(setSpy).not.toHaveBeenCalled();
    expect(removeSpy).not.toHaveBeenCalled();
  });
});
