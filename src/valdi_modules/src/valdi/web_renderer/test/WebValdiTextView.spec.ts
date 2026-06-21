import 'jasmine/src/jasmine';
import { WebValdiTextView } from '../src/views/WebValdiTextView';

function installDomStubs() {
  const textarea = {
    value: '',
    placeholder: '',
    disabled: false,
    selectionStart: 0,
    selectionEnd: 0,
    style: {} as Record<string, string>,
    addEventListener: () => {},
    removeEventListener: () => {},
    setAttribute: () => {},
    blur: () => {},
    focus: () => {},
    select: () => {},
    setSelectionRange: () => {},
  };

  (globalThis as any).document = {
    activeElement: null,
    createElement: () => textarea,
    addEventListener: () => {},
    removeEventListener: () => {},
  };

  (globalThis as any).IntersectionObserver = function () {
    return { observe: () => {}, unobserve: () => {}, disconnect: () => {} };
  };

  return textarea;
}

function uninstallDomStubs() {
  delete (globalThis as any).document;
  delete (globalThis as any).IntersectionObserver;
}

describe('WebValdiTextView textDecoration', () => {
  afterEach(() => uninstallDomStubs());

  it('maps Valdi decoration values to CSS text decoration styles', () => {
    const textarea = installDomStubs();
    const textView = new WebValdiTextView(1);

    textView.changeAttribute('textDecoration', 'underline');
    expect(textarea.style.textDecorationLine).toBe('underline');
    expect(textarea.style.textDecorationStyle).toBe('');

    textView.changeAttribute('textDecoration', 'dashed-underline');
    expect(textarea.style.textDecorationLine).toBe('underline');
    expect(textarea.style.textDecorationStyle).toBe('dashed');

    textView.changeAttribute('textDecoration', 'dotted-underline');
    expect(textarea.style.textDecorationLine).toBe('underline');
    expect(textarea.style.textDecorationStyle).toBe('dotted');

    textView.changeAttribute('textDecoration', 'strikethrough');
    expect(textarea.style.textDecorationLine).toBe('line-through');
    expect(textarea.style.textDecorationStyle).toBe('');

    textView.changeAttribute('textDecoration', 'none');
    expect(textarea.style.textDecorationLine).toBe('none');
    expect(textarea.style.textDecorationStyle).toBe('');
  });
});
