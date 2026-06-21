import 'jasmine/src/jasmine';
import { renderAttributedText } from '../src/utils/parseAttributedText';
import { WebValdiTextView } from '../src/views/WebValdiTextView';

const enum AttributedTextEntryType {
  Content = 1,
  Pop,
  PushFont,
  PushTextDecoration,
  PushColor,
  PushOnTap,
  PushOnLayout,
  PushOutlineColor,
  PushOutlineWidth,
  PushOuterOutlineColor,
  PushOuterOutlineWidth,
  InlineImage,
  PushAnimationTransform,
  PushBackgroundColor,
  PushBackgroundPadding,
  PushBackgroundBorderRadius,
}

function installDomStubs() {
  const elements: any[] = [];

  function createElement(tagName: string) {
    const children: any[] = [];
    const style = {} as Record<string, string>;
    style.setProperty = ((name: string, value: string) => {
      style[name] = value;
    }) as any;
    style.removeProperty = ((name: string) => {
      delete style[name];
    }) as any;
    const element: any = {
      tagName: tagName.toUpperCase(),
      nodeType: 1,
      value: '',
      placeholder: '',
      disabled: false,
      selectionStart: 0,
      selectionEnd: 0,
      style,
      children,
      childNodes: { item: (i: number) => children[i], length: children.length },
      appendChild(child: any) {
        children.push(child);
        this.firstChild = children[0];
        return child;
      },
      replaceChildren(...newChildren: any[]) {
        children.length = 0;
        children.push(...newChildren);
        this.firstChild = children[0];
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
        visit(this);
        return matches;
      },
      addEventListener: () => {},
      removeEventListener: () => {},
      setAttribute: () => {},
      removeAttribute: () => {},
      getBoundingClientRect: () => ({ left: 0, top: 0, width: 10, height: 10 }),
      getRootNode: () => (globalThis as any).document,
      contains: () => false,
      blur: () => {},
      focus: () => {},
      select: () => {},
      setSelectionRange(start: number, end: number) {
        this.selectionStart = start;
        this.selectionEnd = end;
      },
      remove: () => {},
    };
    elements.push(element);
    return element;
  }

  (globalThis as any).document = {
    activeElement: null,
    createElement,
    createTextNode: (text: string) => ({ nodeType: 3, textContent: text, childNodes: { length: 0, item: () => null } }),
    addEventListener: () => {},
    removeEventListener: () => {},
  };

  (globalThis as any).window = { setTimeout, clearTimeout };
  (globalThis as any).IntersectionObserver = function () {
    return { observe: () => {}, unobserve: () => {}, disconnect: () => {} };
  };
  (globalThis as any).requestAnimationFrame = (cb: Function) => cb();

  return { elements };
}

function uninstallDomStubs() {
  delete (globalThis as any).document;
  delete (globalThis as any).window;
  delete (globalThis as any).IntersectionObserver;
  delete (globalThis as any).requestAnimationFrame;
}

describe('WebValdiTextView textDecoration', () => {
  afterEach(() => uninstallDomStubs());

  it('maps Valdi decoration values to CSS text decoration styles', () => {
    installDomStubs();
    const textView = new WebValdiTextView(1);

    textView.changeAttribute('textDecoration', 'underline');
    expect(textView.htmlElement.style.textDecorationLine).toBe('underline');
    expect(textView.htmlElement.style.textDecorationStyle).toBe('');

    textView.changeAttribute('textDecoration', 'dashed-underline');
    expect(textView.htmlElement.style.textDecorationLine).toBe('underline');
    expect(textView.htmlElement.style.textDecorationStyle).toBe('dashed');

    textView.changeAttribute('textDecoration', 'dotted-underline');
    expect(textView.htmlElement.style.textDecorationLine).toBe('underline');
    expect(textView.htmlElement.style.textDecorationStyle).toBe('dotted');

    textView.changeAttribute('textDecoration', 'strikethrough');
    expect(textView.htmlElement.style.textDecorationLine).toBe('line-through');
    expect(textView.htmlElement.style.textDecorationStyle).toBe('');

    textView.changeAttribute('textDecoration', 'none');
    expect(textView.htmlElement.style.textDecorationLine).toBe('none');
    expect(textView.htmlElement.style.textDecorationStyle).toBe('');
  });

  it('renders attributed text with background styling', () => {
    installDomStubs();
    const textView = new WebValdiTextView(1);

    textView.changeAttribute('value', [
      AttributedTextEntryType.PushBackgroundColor,
      '#ffeeaa',
      AttributedTextEntryType.PushBackgroundPadding,
      { top: 2, right: 4, bottom: 6, left: 8 },
      AttributedTextEntryType.PushBackgroundBorderRadius,
      5,
      AttributedTextEntryType.Content,
      'styled',
    ]);

    const container = textView.htmlElement.children[0] as any;
    const span = container.children[0] as any;
    expect(span.textContent).toBe('styled');
    expect(span.style.backgroundColor).toBe('#ffeeaa');
    expect(span.style.padding).toBe('2px 4px 6px 8px');
    expect(span.style.borderRadius).toBe('5px');
  });

  it('applies text background effects around plain text', () => {
    installDomStubs();
    const textView = new WebValdiTextView(1);

    textView.changeAttribute('value', 'plain');
    textView.changeAttribute('backgroundEffectColor', '#123456');
    textView.changeAttribute('backgroundEffectBorderRadius', 9);
    textView.changeAttribute('backgroundEffectPadding', 8);

    const wrapper = textView.htmlElement.children[0] as any;
    const span = wrapper.children[0] as any;
    expect(span.textContent).toBe('plain');
    expect(span.style.backgroundColor).toBe('#123456');
    expect(span.style.borderRadius).toBe('9px');
    expect(span.style.padding).toBe('4px 8px');
  });
});

describe('renderAttributedText', () => {
  afterEach(() => uninstallDomStubs());

  it('renders inline images and system font descriptors', () => {
    installDomStubs();

    const container = renderAttributedText([
      AttributedTextEntryType.PushFont,
      'system-bold 18',
      AttributedTextEntryType.InlineImage,
      { attachmentId: 'img', width: 12, height: 7, imageData: new Uint8Array([1, 2, 3]) },
    ]);

    const span = container.children[0] as any;
    const image = span.children[0] as any;
    expect(span.style.fontFamily).toContain('Segoe UI');
    expect(span.style.fontWeight).toBe('700');
    expect(image.tagName).toBe('IMG');
    expect(image.style.width).toBe('12px');
    expect(image.style.height).toBe('7px');
    expect(image.src).toContain('data:image/png;base64,');
  });
});
