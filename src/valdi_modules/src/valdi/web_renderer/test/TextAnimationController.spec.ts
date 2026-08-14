import 'jasmine/src/jasmine';
import type { AttributeApplierContext } from '../src/core/ElementClass';
import { easeOutTextAnimationProgress } from '../src/utils/TextAnimationController';
import {
  registerTextAnimationGroup,
  registerTextAnimationParticipant,
  unregisterTextAnimationGroup,
  unregisterTextAnimationParticipant,
} from '../src/utils/TextAnimationRegistry';
import {
  markTextAnimationAttachmentSpan,
  NormalizedTextAnimationTransform,
  setTextAnimationTransform,
} from '../src/utils/TextAnimationTypes';

type FakeStyle = Record<string, string> & {
  setProperty(name: string, value: string): void;
  removeProperty(name: string): void;
};

type FakeElement = {
  tagName: string;
  style: FakeStyle;
  childNodes: { readonly length: number; item(index: number): FakeElement | null };
  parentElement: FakeElement | null;
  textContent: string;
  appendChild(child: FakeElement): void;
  replaceChildren(...newChildren: FakeElement[]): void;
};

class FakeAttributeApplierContext implements AttributeApplierContext {
  readonly id: number;
  private readonly state = new Map<string, unknown>();
  private readonly cleanups: Array<() => void> = [];

  constructor(id: number) {
    this.id = id;
  }

  getState<T>(key: string): T | undefined {
    return this.state.get(key) as T | undefined;
  }

  setState(key: string, value: unknown): void {
    this.state.set(key, value);
  }

  getViewAttributeElement(): HTMLElement {
    throw new Error('View attribute element is not available in text animation tests');
  }

  resolveColor(value: string): string {
    return value;
  }

  setColorPalette(_colorPaletteName: string | undefined): void {}

  addCleanup(callback: () => void): void {
    this.cleanups.push(callback);
  }

  enqueuePostLayoutCallback(_callback: () => void): void {}

  getLayoutObserver(): undefined {
    return undefined;
  }

  setLayoutObserver(_attributeName: string): void {}

  requestLayoutPass(): void {}

  getChildHtmlElement(_index: number): HTMLElement | undefined {
    return undefined;
  }

  setOnLayoutCallback(): void {}

  onAttributeUpdatedExternally(_attributeName: string, _attributeValue: unknown): void {}

  emitCurrentViewCreate(_callback: Function): void {}

  emitCurrentViewChange(): void {}

  isAnimationEnabled(): boolean {
    return true;
  }

  setAnimationsEnabled(): void {}

  runCleanups(): void {
    for (let i = 0; i < this.cleanups.length; i++) {
      this.cleanups[i]();
    }
    this.cleanups.length = 0;
  }
}

function makeStyle(): FakeStyle {
  const style = {} as FakeStyle;
  style.setProperty = (name: string, value: string) => {
    style[name] = value;
  };
  style.removeProperty = (name: string) => {
    delete style[name];
  };
  return style;
}

function makeFakeElement(tagName: string, textContent: string): FakeElement {
  const children: FakeElement[] = [];
  return {
    tagName: tagName.toUpperCase(),
    style: makeStyle(),
    childNodes: {
      get length(): number {
        return children.length;
      },
      item(index: number): FakeElement | null {
        return children[index] ?? null;
      },
    },
    parentElement: null,
    textContent,
    appendChild(child: FakeElement): void {
      child.parentElement = this;
      children.push(child);
    },
    replaceChildren(...newChildren: FakeElement[]): void {
      for (let i = 0; i < children.length; i++) {
        children[i].parentElement = null;
      }
      children.length = 0;
      for (let i = 0; i < newChildren.length; i++) {
        newChildren[i].parentElement = this;
        children.push(newChildren[i]);
      }
    },
  };
}

function asHtmlElement(element: FakeElement): HTMLElement {
  return element as unknown as HTMLElement;
}

function asHtmlSpanElement(element: FakeElement): HTMLSpanElement {
  return element as unknown as HTMLSpanElement;
}

function makeTransform(overrides: Partial<NormalizedTextAnimationTransform>): NormalizedTextAnimationTransform {
  return {
    translationX: 0,
    translationY: 0,
    scale: 1,
    opacity: 0,
    duration: 1,
    timeOffsetBetweenParts: 0,
    groupIndex: 0,
    partIndexInGroup: 0,
    ...overrides,
  };
}

function appendAnimatedSpan(
  container: FakeElement,
  text: string,
  transform: NormalizedTextAnimationTransform,
): FakeElement {
  const span = makeFakeElement('span', text);
  setTextAnimationTransform(asHtmlSpanElement(span), transform);
  container.appendChild(span);
  return span;
}

function installAnimationDomStubs(): {
  flushFrame(time: number): void;
  pendingFrameCount(): number;
  uninstall(): void;
} {
  const previousDocument = (globalThis as { document?: unknown }).document;
  const previousRequestAnimationFrame = (globalThis as { requestAnimationFrame?: unknown }).requestAnimationFrame;
  const previousCancelAnimationFrame = (globalThis as { cancelAnimationFrame?: unknown }).cancelAnimationFrame;
  const previousPerformance = (globalThis as { performance?: unknown }).performance;
  const frameCallbacks = new Map<number, (time: number) => void>();
  let frameTime = 0;
  let nextFrameHandle = 1;

  (globalThis as { document?: unknown }).document = {
    createElement(tagName: string): FakeElement {
      return makeFakeElement(tagName, '');
    },
  };
  (globalThis as { requestAnimationFrame?: (callback: (time: number) => void) => number }).requestAnimationFrame =
    callback => {
      const handle = nextFrameHandle++;
      frameCallbacks.set(handle, callback);
      return handle;
    };
  (globalThis as { cancelAnimationFrame?: (handle: number) => void }).cancelAnimationFrame = handle => {
    frameCallbacks.delete(handle);
  };
  (globalThis as { performance?: unknown }).performance = {
    now(): number {
      return frameTime;
    },
  };

  return {
    flushFrame(time: number): void {
      frameTime = time;
      const callbacks = Array.from(frameCallbacks.values());
      frameCallbacks.clear();
      for (let i = 0; i < callbacks.length; i++) {
        callbacks[i](time);
      }
    },
    pendingFrameCount(): number {
      return frameCallbacks.size;
    },
    uninstall(): void {
      if (previousDocument === undefined) {
        delete (globalThis as { document?: unknown }).document;
      } else {
        (globalThis as { document?: unknown }).document = previousDocument;
      }
      if (previousRequestAnimationFrame === undefined) {
        delete (globalThis as { requestAnimationFrame?: unknown }).requestAnimationFrame;
      } else {
        (globalThis as { requestAnimationFrame?: unknown }).requestAnimationFrame = previousRequestAnimationFrame;
      }
      if (previousCancelAnimationFrame === undefined) {
        delete (globalThis as { cancelAnimationFrame?: unknown }).cancelAnimationFrame;
      } else {
        (globalThis as { cancelAnimationFrame?: unknown }).cancelAnimationFrame = previousCancelAnimationFrame;
      }
      if (previousPerformance === undefined) {
        delete (globalThis as { performance?: unknown }).performance;
      } else {
        (globalThis as { performance?: unknown }).performance = previousPerformance;
      }
    },
  };
}

describe('TextAnimationController', () => {
  let domStubs: ReturnType<typeof installAnimationDomStubs>;

  beforeEach(() => {
    domStubs = installAnimationDomStubs();
  });

  afterEach(() => {
    domStubs.uninstall();
  });

  it('exposes cubic ease-out progress clamped to the animation range', () => {
    expect(easeOutTextAnimationProgress(-1)).toBe(0);
    expect(easeOutTextAnimationProgress(0)).toBe(0);
    expect(easeOutTextAnimationProgress(0.5)).toBeCloseTo(0.875, 5);
    expect(easeOutTextAnimationProgress(1)).toBe(1);
    expect(easeOutTextAnimationProgress(2)).toBe(1);
  });

  it('animates whole parts and restores the original inline style on completion', () => {
    const owner = makeFakeElement('label', '');
    const container = makeFakeElement('span', '');
    const span = appendAnimatedSpan(
      container,
      'hello',
      makeTransform({
        opacity: 0.2,
        scale: 0.5,
        translationX: 5,
        translationY: 10,
      }),
    );
    const context = new FakeAttributeApplierContext(1);
    owner.appendChild(container);
    span.style.display = 'inline';
    span.style.opacity = '0.9';
    span.style.transform = 'rotate(1deg)';

    registerTextAnimationParticipant(asHtmlElement(owner), asHtmlElement(container), context);

    expect(span.style.display).toBe('inline-block');
    expect(span.style.opacity).toBe('0.2');
    expect(span.style.transform).toBe('translateX(5px) translateY(10px) scale(0.5)');

    domStubs.flushFrame(500);

    expect(Number(span.style.opacity)).toBeCloseTo(0.9, 5);
    expect(span.style.transform).toBe('translateX(0.625px) translateY(1.25px) scale(0.9375)');

    domStubs.flushFrame(1000);

    expect(span.style.display).toBe('inline');
    expect(span.style.opacity).toBe('0.9');
    expect(span.style.transform).toBe('rotate(1deg)');
    unregisterTextAnimationParticipant(context);
  });

  it('splits partPattern matches, leaves unmatched text unanimated, and applies part delays', () => {
    const owner = makeFakeElement('label', '');
    const container = makeFakeElement('span', '');
    const partSpan = appendAnimatedSpan(
      container,
      'hi all',
      makeTransform({
        partPattern: '\\S+',
        timeOffsetBetweenParts: 0.1,
      }),
    );
    const context = new FakeAttributeApplierContext(2);
    owner.appendChild(container);

    registerTextAnimationParticipant(asHtmlElement(owner), asHtmlElement(container), context);

    const firstAnimated = partSpan.childNodes.item(0)!;
    const unmatched = partSpan.childNodes.item(1)!;
    const secondAnimated = partSpan.childNodes.item(2)!;
    expect(partSpan.childNodes.length).toBe(3);
    expect(firstAnimated.textContent).toBe('hi');
    expect(unmatched.textContent).toBe(' ');
    expect(secondAnimated.textContent).toBe('all');
    expect(unmatched.style.opacity).toBeUndefined();

    domStubs.flushFrame(50);

    expect(Number(firstAnimated.style.opacity)).toBeGreaterThan(0);
    expect(secondAnimated.style.opacity).toBe('0');
    unregisterTextAnimationParticipant(context);
  });

  it('logs invalid partPattern values and leaves the text unanimated', () => {
    const errorSpy = spyOn(console, 'error');
    const owner = makeFakeElement('label', '');
    const container = makeFakeElement('span', '');
    const span = appendAnimatedSpan(container, 'invalid', makeTransform({ partPattern: '[' }));
    const context = new FakeAttributeApplierContext(3);
    owner.appendChild(container);

    registerTextAnimationParticipant(asHtmlElement(owner), asHtmlElement(container), context);

    expect(errorSpy).toHaveBeenCalled();
    expect(String(errorSpy.calls.mostRecent().args[0])).toContain('Invalid text animation partPattern');
    expect(span.childNodes.length).toBe(0);
    expect(span.style.opacity).toBeUndefined();
    expect(domStubs.pendingFrameCount()).toBe(0);
  });

  it('treats inline attachments as one animated unit even when partPattern is present', () => {
    const owner = makeFakeElement('label', '');
    const container = makeFakeElement('span', '');
    const attachmentSpan = appendAnimatedSpan(container, '', makeTransform({ partPattern: '.' }));
    const image = makeFakeElement('img', '');
    attachmentSpan.appendChild(image);
    markTextAnimationAttachmentSpan(asHtmlSpanElement(attachmentSpan));
    const context = new FakeAttributeApplierContext(4);
    owner.appendChild(container);

    registerTextAnimationParticipant(asHtmlElement(owner), asHtmlElement(container), context);

    expect(attachmentSpan.childNodes.length).toBe(1);
    expect(attachmentSpan.childNodes.item(0)).toBe(image);
    expect(attachmentSpan.style.opacity).toBe('0');
    unregisterTextAnimationParticipant(context);
  });

  it('unregisters participants by restoring styles and cancelling pending frames', () => {
    const owner = makeFakeElement('label', '');
    const container = makeFakeElement('span', '');
    const span = appendAnimatedSpan(container, 'cleanup', makeTransform({ translationY: 6 }));
    const context = new FakeAttributeApplierContext(5);
    owner.appendChild(container);

    registerTextAnimationParticipant(asHtmlElement(owner), asHtmlElement(container), context);
    expect(domStubs.pendingFrameCount()).toBe(1);
    expect(span.style.transform).toBe('translateY(6px)');

    unregisterTextAnimationParticipant(context);

    expect(domStubs.pendingFrameCount()).toBe(0);
    expect(span.style.transform).toBe('');
    domStubs.flushFrame(100);
    expect(span.style.transform).toBe('');
  });

  it('coordinates grouped participants in DOM order while isolating nested groups', () => {
    const group = makeFakeElement('textanimationgroup', '');
    const nestedGroup = makeFakeElement('textanimationgroup', '');
    const firstOwner = makeFakeElement('label', '');
    const nestedOwner = makeFakeElement('label', '');
    const lastOwner = makeFakeElement('label', '');
    const firstContainer = makeFakeElement('span', '');
    const nestedContainer = makeFakeElement('span', '');
    const lastContainer = makeFakeElement('span', '');
    const firstSpan = appendAnimatedSpan(firstContainer, 'first', makeTransform({ timeOffsetBetweenParts: 0.1 }));
    const nestedSpan = appendAnimatedSpan(nestedContainer, 'nested', makeTransform({ timeOffsetBetweenParts: 0.1 }));
    const lastSpan = appendAnimatedSpan(lastContainer, 'last', makeTransform({ timeOffsetBetweenParts: 0.1 }));
    const firstContext = new FakeAttributeApplierContext(6);
    const nestedContext = new FakeAttributeApplierContext(7);
    const lastContext = new FakeAttributeApplierContext(8);

    firstOwner.appendChild(firstContainer);
    nestedOwner.appendChild(nestedContainer);
    lastOwner.appendChild(lastContainer);
    nestedGroup.appendChild(nestedOwner);
    group.appendChild(firstOwner);
    group.appendChild(nestedGroup);
    group.appendChild(lastOwner);

    registerTextAnimationGroup(asHtmlElement(group));
    registerTextAnimationGroup(asHtmlElement(nestedGroup));
    registerTextAnimationParticipant(asHtmlElement(firstOwner), asHtmlElement(firstContainer), firstContext);
    registerTextAnimationParticipant(asHtmlElement(nestedOwner), asHtmlElement(nestedContainer), nestedContext);
    registerTextAnimationParticipant(asHtmlElement(lastOwner), asHtmlElement(lastContainer), lastContext);

    domStubs.flushFrame(0);
    domStubs.flushFrame(150);

    expect(Number(firstSpan.style.opacity)).toBeGreaterThan(0);
    expect(Number(nestedSpan.style.opacity)).toBeGreaterThan(0);
    expect(Number(lastSpan.style.opacity)).toBeGreaterThan(0);

    unregisterTextAnimationParticipant(firstContext);
    unregisterTextAnimationParticipant(nestedContext);
    unregisterTextAnimationParticipant(lastContext);
    unregisterTextAnimationGroup(asHtmlElement(nestedGroup));
    unregisterTextAnimationGroup(asHtmlElement(group));
  });

  it('starts independent keyed timelines together within a group', () => {
    const group = makeFakeElement('textanimationgroup', '');
    const firstOwner = makeFakeElement('label', '');
    const secondOwner = makeFakeElement('label', '');
    const firstContainer = makeFakeElement('span', '');
    const secondContainer = makeFakeElement('span', '');
    const firstSpan = appendAnimatedSpan(
      firstContainer,
      'first',
      makeTransform({ key: 'first-timeline', timeOffsetBetweenParts: 0.1 }),
    );
    const secondSpan = appendAnimatedSpan(
      secondContainer,
      'second',
      makeTransform({ key: 'second-timeline', timeOffsetBetweenParts: 0.1 }),
    );
    const firstContext = new FakeAttributeApplierContext(9);
    const secondContext = new FakeAttributeApplierContext(10);

    firstOwner.appendChild(firstContainer);
    secondOwner.appendChild(secondContainer);
    group.appendChild(firstOwner);
    group.appendChild(secondOwner);

    registerTextAnimationGroup(asHtmlElement(group));
    registerTextAnimationParticipant(asHtmlElement(firstOwner), asHtmlElement(firstContainer), firstContext);
    registerTextAnimationParticipant(asHtmlElement(secondOwner), asHtmlElement(secondContainer), secondContext);

    domStubs.flushFrame(0);
    domStubs.flushFrame(50);

    expect(Number(firstSpan.style.opacity)).toBeGreaterThan(0);
    expect(Number(secondSpan.style.opacity)).toBeGreaterThan(0);

    unregisterTextAnimationParticipant(firstContext);
    unregisterTextAnimationParticipant(secondContext);
    unregisterTextAnimationGroup(asHtmlElement(group));
  });

  it('compresses grouped pending segment delays after the flush threshold', () => {
    const group = makeFakeElement('textanimationgroup', '');
    const owner = makeFakeElement('label', '');
    const container = makeFakeElement('span', '');
    const partSpan = appendAnimatedSpan(
      container,
      'one two three',
      makeTransform({
        duration: 1,
        key: 'flush-demo',
        partPattern: '\\S+',
        timeOffsetBetweenParts: 1,
      }),
    );
    const context = new FakeAttributeApplierContext(11);

    owner.appendChild(container);
    group.appendChild(owner);

    const groupController = registerTextAnimationGroup(asHtmlElement(group));
    groupController.setFlushDurationThreshold(0.3);
    registerTextAnimationParticipant(asHtmlElement(owner), asHtmlElement(container), context);

    domStubs.flushFrame(0);
    domStubs.flushFrame(1100);

    const thirdAnimated = partSpan.childNodes.item(4)!;
    expect(Number(thirdAnimated.style.opacity)).toBeGreaterThan(0);

    unregisterTextAnimationParticipant(context);
    unregisterTextAnimationGroup(asHtmlElement(group));
  });

  it('uses custom grouped flush multiplier values', () => {
    const group = makeFakeElement('textanimationgroup', '');
    const owner = makeFakeElement('label', '');
    const container = makeFakeElement('span', '');
    const partSpan = appendAnimatedSpan(
      container,
      'one two three',
      makeTransform({
        duration: 1,
        key: 'flush-demo',
        partPattern: '\\S+',
        timeOffsetBetweenParts: 1,
      }),
    );
    const context = new FakeAttributeApplierContext(12);

    owner.appendChild(container);
    group.appendChild(owner);

    const groupController = registerTextAnimationGroup(asHtmlElement(group));
    groupController.setFlushDurationThreshold(0.3);
    groupController.setFlushMultiplier(0);
    registerTextAnimationParticipant(asHtmlElement(owner), asHtmlElement(container), context);

    domStubs.flushFrame(0);
    domStubs.flushFrame(1100);

    const thirdAnimated = partSpan.childNodes.item(4)!;
    expect(thirdAnimated.style.opacity).toBe('0');

    unregisterTextAnimationParticipant(context);
    unregisterTextAnimationGroup(asHtmlElement(group));
  });
});
