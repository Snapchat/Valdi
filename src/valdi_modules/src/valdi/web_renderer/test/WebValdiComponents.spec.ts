import 'jasmine/src/jasmine';
import { WebValdiLabel } from '../src/views/WebValdiLabel';
import { WebValdiScroll } from '../src/views/WebValdiScroll';

function installDomStubs() {
  const elements: HTMLElement[] = [];

  (globalThis as any).document = {
    createElement: (tag: string) => {
      const style: Record<string, string> = {};
      const children: any[] = [];
      const classList = {
        _classes: new Set<string>(),
        add(c: string) {
          this._classes.add(c);
        },
        remove(c: string) {
          this._classes.delete(c);
        },
        contains(c: string) {
          return this._classes.has(c);
        },
      };
      const el: any = {
        tagName: tag.toUpperCase(),
        style,
        children,
        childNodes: { item: (i: number) => children[i] },
        classList,
        appendChild(child: any) {
          children.push(child);
        },
        insertBefore(child: any, ref: any) {
          const idx = ref ? children.indexOf(ref) : children.length;
          children.splice(idx >= 0 ? idx : children.length, 0, child);
        },
        querySelectorAll(selector: string) {
          const matches: any[] = [];
          const visit = (node: any) => {
            if (selector === 'span' && node.tagName === 'SPAN') {
              matches.push(node);
            }
            for (const child of node.children ?? []) {
              visit(child);
            }
          };
          visit(el);
          return matches;
        },
        remove() {},
        replaceChildren(...newChildren: any[]) {
          children.length = 0;
          children.push(...newChildren);
        },
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

  it('resets line clamp when set to 0', () => {
    installDomStubs();
    const label = new WebValdiLabel(1);
    label.changeAttribute('numberOfLines', 2);
    label.changeAttribute('numberOfLines', 0);

    expect((label.htmlElement.style as any).display).toBe('inline');
    expect((label.htmlElement.style as any).WebkitLineClamp).toBe('');
    expect((label.htmlElement.style as any).overflow).toBe('visible');
  });

  it('does not throw when numberOfLines is undefined', () => {
    installDomStubs();
    const label = new WebValdiLabel(1);
    expect(() => label.changeAttribute('numberOfLines', undefined)).not.toThrow();
  });
});

describe('WebValdiLabel – text rendering parity', () => {
  afterEach(() => uninstallDomStubs());

  it('applies explicit lineHeight over lineHeightMultiple', () => {
    installDomStubs();
    const label = new WebValdiLabel(1);

    label.changeAttribute('lineHeightMultiple', 1.4);
    expect((label.htmlElement.style as any).lineHeight).toBe('1.4');

    label.changeAttribute('lineHeight', 23);
    expect((label.htmlElement.style as any).lineHeight).toBe('23px');

    label.changeAttribute('lineHeight', undefined);
    expect((label.htmlElement.style as any).lineHeight).toBe('1.4');
  });

  it('maps textAlign and textDecoration values to CSS', () => {
    installDomStubs();
    const label = new WebValdiLabel(1);

    label.changeAttribute('textAlign', 'justified');
    expect((label.htmlElement.style as any).textAlign).toBe('justify');

    label.changeAttribute('textDecoration', 'dotted-underline');
    expect((label.htmlElement.style as any).textDecorationLine).toBe('underline');
    expect((label.htmlElement.style as any).textDecorationStyle).toBe('dotted');
  });

  it('parses Valdi textShadow descriptors', () => {
    installDomStubs();
    const label = new WebValdiLabel(1);

    label.changeAttribute('textShadow', '#000000 3 0.5 4 5');

    expect((label.htmlElement.style as any).textShadow).toBe('4px 5px 3px rgba(0, 0, 0, 0.5)');
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
