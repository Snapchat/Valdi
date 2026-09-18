import 'jasmine/src/jasmine';
import { Style } from 'valdi_core/src/Style';
import { WebValdiLabel } from '../src/views/WebValdiLabel';
import { WebValdiLayout } from '../src/views/WebValdiLayout';
import { WebValdiScroll } from '../src/views/WebValdiScroll';
import { ValdiWebRendererDelegate } from '../src/ValdiWebRendererDelegate';

function installDomStubs() {
  const elements: HTMLElement[] = [];

  // WebValdiTextField defines `class ValdiInput extends HTMLElement` at module
  // scope, evaluated lazily on first import (e.g. via ValdiWebRendererDelegate ->
  // registerElements), so these must exist before that.
  (globalThis as any).HTMLElement = class {};
  (globalThis as any).customElements = { get: () => undefined, define: () => {} };

  (globalThis as any).document = {
    createElement: (tag: string) => {
      const style: Record<string, string> = {};
      const children: any[] = [];
      const classList = {
        _classes: new Set<string>(),
        add(c: string) { this._classes.add(c); },
        remove(c: string) { this._classes.delete(c); },
        contains(c: string) { return this._classes.has(c); },
      };
      const el: any = {
        tagName: tag.toUpperCase(),
        style,
        children,
        childNodes: { item: (i: number) => children[i] },
        classList,
        appendChild(child: any) { children.push(child); },
        insertBefore(child: any, ref: any) {
          const idx = ref ? children.indexOf(ref) : children.length;
          children.splice(idx >= 0 ? idx : children.length, 0, child);
        },
        remove() {},
        replaceChildren() { children.length = 0; },
        getAttribute: () => null,
        setAttribute: () => {},
        removeAttribute: () => {},
        setProperty: () => {},
        addEventListener: jasmine.createSpy('addEventListener'),
        removeEventListener: jasmine.createSpy('removeEventListener'),
        getBoundingClientRect: () => ({ width: 0, height: 0, x: 0, y: 0, left: 0, top: 0 }),
        getRootNode: () => (globalThis as any).document,
        contains: () => false,
        scrollLeft: 0,
        scrollTop: 0,
        scrollWidth: 100,
        scrollHeight: 200,
        clientWidth: 100,
        clientHeight: 100,
        scrollTo: jasmine.createSpy('scrollTo'),
      };
      // style.setProperty support
      style.setProperty = jasmine.createSpy('setProperty') as any;
      style.removeProperty = jasmine.createSpy('removeProperty') as any;
      elements.push(el);
      return el;
    },
    dir: 'ltr',
    body: { contains: () => false },
    activeElement: null,
  };

  (globalThis as any).window = { devicePixelRatio: 1, setTimeout, clearTimeout };
  (globalThis as any).IntersectionObserver = function () {
    return { observe: () => {}, unobserve: () => {}, disconnect: () => {} };
  };
  (globalThis as any).MutationObserver = function (cb: Function) {
    return { observe: () => {}, disconnect: () => {}, _cb: cb };
  };
  (globalThis as any).ResizeObserver = function () {
    return { observe: () => {}, unobserve: () => {}, disconnect: () => {} };
  };
  (globalThis as any).requestAnimationFrame = (cb: Function) => cb();
  (globalThis as any).performance = { now: () => 0 };

  return { elements };
}

function uninstallDomStubs() {
  delete (globalThis as any).HTMLElement;
  delete (globalThis as any).customElements;
  delete (globalThis as any).document;
  delete (globalThis as any).window;
  delete (globalThis as any).IntersectionObserver;
  delete (globalThis as any).MutationObserver;
  delete (globalThis as any).ResizeObserver;
  delete (globalThis as any).requestAnimationFrame;
  delete (globalThis as any).performance;
}

describe('WebValdiLabel – numberOfLines', () => {
  afterEach(() => uninstallDomStubs());

  it('applies line clamp styles when set to a positive number', () => {
    installDomStubs();
    const label = new WebValdiLabel(1);
    label.changeAttribute('numberOfLines', 2);

    expect((label.htmlElement.style as any).display).toBe('-webkit-box');
    expect((label.htmlElement.style as any).WebkitLineClamp).toBe('2');
    expect((label.htmlElement.style as any).WebkitBoxOrient).toBe('vertical');
    expect((label.htmlElement.style as any).overflow).toBe('hidden');
  });

  it('enables unlimited multiline when set to 0', () => {
    installDomStubs();
    const label = new WebValdiLabel(1);
    label.changeAttribute('numberOfLines', 2);
    label.changeAttribute('numberOfLines', 0);

    expect((label.htmlElement.style as any).display).toBe('inline');
    expect((label.htmlElement.style as any).WebkitLineClamp).toBe('');
    expect((label.htmlElement.style as any).overflow).toBe('visible');
    expect((label.htmlElement.style as any).whiteSpace).toBe('pre-wrap');
    expect((label.htmlElement.style as any).wordWrap).toBe('break-word');
  });

  it('restores the single-line default when numberOfLines is cleared', () => {
    installDomStubs();
    const label = new WebValdiLabel(1);
    label.changeAttribute('numberOfLines', 2);
    label.changeAttribute('numberOfLines', undefined);

    expect((label.htmlElement.style as any).display).toBe('inline');
    expect((label.htmlElement.style as any).WebkitLineClamp).toBe('');
    expect((label.htmlElement.style as any).overflow).toBe('visible');
    expect((label.htmlElement.style as any).whiteSpace).toBe('nowrap');
  });

  it('does not throw when numberOfLines is undefined', () => {
    installDomStubs();
    const label = new WebValdiLabel(1);
    expect(() => label.changeAttribute('numberOfLines', undefined)).not.toThrow();
  });
});

describe('WebValdiScroll – pagingEnabled', () => {
  afterEach(() => uninstallDomStubs());

  it('sets scroll-snap-type when enabled', () => {
    installDomStubs();
    const scroll = new WebValdiScroll(1);
    scroll.changeAttribute('pagingEnabled', true);

    expect((scroll.htmlElement.style as any).scrollSnapType).toContain('mandatory');
  });

  it('clears scroll-snap-type when disabled', () => {
    installDomStubs();
    const scroll = new WebValdiScroll(1);
    scroll.changeAttribute('pagingEnabled', true);
    scroll.changeAttribute('pagingEnabled', false);

    expect((scroll.htmlElement.style as any).scrollSnapType).toBe('');
  });

  it('sets scroll-snap-align on existing children', () => {
    installDomStubs();
    const scroll = new WebValdiScroll(1);
    // Simulate children
    const child1 = { style: {} } as any;
    const child2 = { style: {} } as any;
    (scroll.htmlElement.children as any).push(child1, child2);

    scroll.changeAttribute('pagingEnabled', true);

    expect(child1.style.scrollSnapAlign).toBe('start');
    expect(child2.style.scrollSnapAlign).toBe('start');
  });

  it('clears scroll-snap-align on children when disabled', () => {
    installDomStubs();
    const scroll = new WebValdiScroll(1);
    const child = { style: { scrollSnapAlign: 'start' } } as any;
    (scroll.htmlElement.children as any).push(child);

    scroll.changeAttribute('pagingEnabled', true);
    scroll.changeAttribute('pagingEnabled', false);

    expect(child.style.scrollSnapAlign).toBe('');
  });
});

describe('WebValdiLayout – style object expansion', () => {
  afterEach(() => uninstallDomStubs());

  it('expands a Style object passed as the style attribute into per-key styles', () => {
    installDomStubs();
    const layout = new WebValdiLayout(1);
    layout.changeAttribute('style', new Style({ width: 74, height: 74, borderRadius: 4 }));

    expect(String((layout.htmlElement.style as any).width)).toMatch(/^74(px)?$/);
    expect(String((layout.htmlElement.style as any).height)).toMatch(/^74(px)?$/);
    expect(String((layout.htmlElement.style as any).borderRadius)).toMatch(/^4(px)?$/);
  });

  it('leaves non-Style style values on the generic path', () => {
    installDomStubs();
    const layout = new WebValdiLayout(1);
    expect(() => layout.changeAttribute('style', 'not-a-style')).not.toThrow();
  });
});

describe('WebValdiLabel – single-line default', () => {
  afterEach(() => uninstallDomStubs());

  it('defaults to nowrap so short labels stay on one line', () => {
    installDomStubs();
    const label = new WebValdiLabel(1);
    expect((label.htmlElement.style as any).whiteSpace).toBe('nowrap');
  });

  it('enables wrapping while numberOfLines is set and restores nowrap on reset', () => {
    installDomStubs();
    const label = new WebValdiLabel(1);
    label.changeAttribute('numberOfLines', 2);
    expect((label.htmlElement.style as any).whiteSpace).toBe('pre-wrap');
    expect((label.htmlElement.style as any).wordWrap).toBe('break-word');

    label.changeAttribute('numberOfLines', undefined);
    expect((label.htmlElement.style as any).whiteSpace).toBe('nowrap');
  });
});

describe('web renderer – cross-page node id isolation (Valdi#115)', () => {
  afterEach(() => uninstallDomStubs());

  it('keeps each page\'s views separate when element ids collide across renderers', () => {
    installDomStubs();
    const doc = (globalThis as any).document;

    // Page A: its own renderer/delegate. Its first element gets id 1 (per-renderer
    // id counters all start at 1), a label.
    const pageA = new ValdiWebRendererDelegate(doc.createElement('div'));
    pageA.onElementCreated(1, 'label');

    // Page B is pushed on top: a separate renderer/delegate whose id counter also
    // starts at 1, so its first element (a layout) reuses id 1.
    const pageB = new ValdiWebRendererDelegate(doc.createElement('div'));
    pageB.onElementCreated(1, 'layout');

    // Back on Page A, a re-render sets a label-only attribute on its id-1 view.
    // Bug #115: id 1 resolves to Page B's WebValdiLayout in the shared global map,
    // which has no `value` case and throws "Attribute value is not valid".
    expect(() => pageA.onElementAttributeChangeString(1, 'value', 'hello')).not.toThrow();
  });
});
