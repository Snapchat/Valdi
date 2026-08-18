import 'jasmine/src/jasmine';
import { AttributesApplier, AttributeSetResult } from '../src/attributes/AttributesApplier';
import { AttributesBinder } from '../src/attributes/AttributesBinder';
import { AttributeApplierContext, ElementClass } from '../src/core/ElementClass';

function createContext(): AttributeApplierContext {
  const state = new Map<string, unknown>();
  const viewAttributeElement = createElement();
  return {
    id: 1,
    getAssetTracker(): undefined {
      return undefined;
    },
    getState<T>(key: string): T | undefined {
      return state.get(key) as T | undefined;
    },
    setState(key: string, value: unknown): void {
      state.set(key, value);
    },
    getViewAttributeElement(): HTMLElement {
      return viewAttributeElement;
    },
    resolveColor(value: string): string {
      return value === 'primary' ? '#123456' : value;
    },
    setColorPalette(): void {},
    addCleanup(): void {},
    enqueuePostLayoutCallback(): void {},
    getLayoutObserver(): undefined {
      return undefined;
    },
    setLayoutObserver(_attributeName: string): void {},
    requestLayoutPass(): void {},
    setOnLayoutCallback(): void {},
    getChildHtmlElement(): HTMLElement | undefined {
      return undefined;
    },
    onAttributeUpdatedExternally(): void {},
    emitCurrentViewCreate(): void {},
    emitCurrentViewChange(): void {},
    isAnimationEnabled(): boolean {
      return true;
    },
    setAnimationsEnabled(): void {},
  };
}

function createElement(): HTMLElement {
  const attributes = new Map<string, string>();
  return {
    style: {},
    getAttribute(name: string): string | null {
      return attributes.get(name) ?? null;
    },
    removeAttribute(name: string): void {
      attributes.delete(name);
    },
    setAttribute(name: string, value: string): void {
      attributes.set(name, value);
    },
  } as HTMLElement;
}

describe('AttributesBinder', () => {
  it('forwards attribute names to apply and reset callbacks', () => {
    const binder = new AttributesBinder<HTMLElement>();
    const element = createElement();
    const context = createContext();
    let appliedAttributeName: string | undefined;
    let resetAttributeName: string | undefined;
    binder.bindStringAttribute(
      'value',
      (_target, _value, _context, attributeName) => {
        appliedAttributeName = attributeName;
      },
      (_target, _context, attributeName) => {
        resetAttributeName = attributeName;
      },
    );

    binder.attributeAppliers.value.apply(element, 'text', 'value', context);
    binder.attributeAppliers.value.reset(element, 'value', context);

    expect(appliedAttributeName).toBe('value');
    expect(resetAttributeName).toBe('value');
  });

  it('binds number attributes with parsing and reset handling', () => {
    const binder = new AttributesBinder<HTMLElement>();
    const element = createElement();
    const context = createContext();
    binder.bindNumberAttribute(
      'opacity',
      (target, value) => {
        target.style.opacity = String(value);
      },
      target => {
        target.style.opacity = '';
      },
    );

    binder.attributeAppliers.opacity.apply(element, 0.5, 'opacity', context);
    expect(element.style.opacity).toBe('0.5');

    binder.attributeAppliers.opacity.reset(element, 'opacity', context);
    expect(element.style.opacity).toBe('');
    expect(() => binder.attributeAppliers.opacity.apply(element, '0.5', 'opacity', context)).toThrow();
  });

  it('binds enum attributes with validation', () => {
    const binder = new AttributesBinder<HTMLElement>();
    const element = createElement();
    const context = createContext();
    binder.bindEnumAttribute(
      'display',
      ['flex'] as const,
      (target, value) => {
        target.style.display = value;
      },
      target => {
        target.style.display = 'flex';
      },
    );

    expect(() => binder.attributeAppliers.display.apply(element, 'grid', 'display', context)).toThrow();
    expect(() => binder.attributeAppliers.display.apply(element, 'block', 'display', context)).toThrow();
  });

  it('binds function attributes with validation', () => {
    const binder = new AttributesBinder<HTMLElement>();
    const element = createElement();
    const context = createContext();
    let called = false;
    binder.bindFunctionAttribute(
      'onTap',
      (_target, callback) => {
        callback();
      },
      () => {},
    );

    binder.attributeAppliers.onTap.apply(
      element,
      () => {
        called = true;
      },
      'onTap',
      context,
    );

    expect(called).toBeTrue();
    expect(() => binder.attributeAppliers.onTap.apply(element, 1, 'onTap', context)).toThrow();
  });

  it('binds direct DOM attributes', () => {
    const binder = new AttributesBinder<HTMLElement>();
    const element = createElement();
    const context = createContext();
    binder.bindDirectAttribute('accessibilityLabel', 'aria-label');

    binder.attributeAppliers.accessibilityLabel.apply(element, 'Label', 'accessibilityLabel', context);
    expect(element.getAttribute('aria-label')).toBe('Label');

    binder.attributeAppliers.accessibilityLabel.reset(element, 'accessibilityLabel', context);
    expect(element.getAttribute('aria-label')).toBeNull();
  });

  it('binds ARIA boolean attributes using DOM boolean text values', () => {
    const binder = new AttributesBinder<HTMLElement>();
    const element = createElement();
    const context = createContext();
    binder.bindAriaBooleanAttribute('accessibilityHidden', 'aria-hidden');

    binder.attributeAppliers.accessibilityHidden.apply(element, 1, 'accessibilityHidden', context);
    expect(element.getAttribute('aria-hidden')).toBe('true');

    binder.attributeAppliers.accessibilityHidden.apply(element, 0, 'accessibilityHidden', context);
    expect(element.getAttribute('aria-hidden')).toBe('false');

    binder.attributeAppliers.accessibilityHidden.reset(element, 'accessibilityHidden', context);
    expect(element.getAttribute('aria-hidden')).toBeNull();
  });

  it('marks color style attributes as color dependent', () => {
    const binder = new AttributesBinder<HTMLElement>();
    const element = createElement();
    const context = createContext();
    binder.bindColorStyleAttribute('backgroundColor', 'backgroundColor', '');

    expect(binder.attributeAppliers.backgroundColor.colorDependent).toBeTrue();
    binder.attributeAppliers.backgroundColor.apply(element, 'primary', 'backgroundColor', context);
    expect(element.style.backgroundColor).toBe('#123456');
  });

  it('marks layout dependent attributes explicitly', () => {
    const binder = new AttributesBinder<HTMLElement>();
    binder.bindNoOpAttribute('paintOnly');
    binder.bindNoOpAttribute('width', true);

    expect(binder.attributeAppliers.paintOnly.layoutDependent).toBeUndefined();
    expect(binder.attributeAppliers.width.layoutDependent).toBeTrue();
  });
});

describe('AttributesApplier', () => {
  class TestElementClass extends ElementClass {
    constructor() {
      const binder = new AttributesBinder<HTMLElement>();
      binder.bindNoOpAttribute('paintOnly');
      binder.bindNoOpAttribute('width', true);
      super('test', binder.attributeAppliers);
    }

    protected onCreateElement(): HTMLElement {
      return createElement();
    }
  }

  it('reports layout invalidation only for changed layout dependent attributes', () => {
    const applier = new AttributesApplier(1, new TestElementClass());

    expect(applier.setAttribute('paintOnly', 1)).toBe(AttributeSetResult.Changed);
    expect(applier.setAttribute('width', 10)).toBe(AttributeSetResult.ChangedAndInvalidatesLayout);
    expect(applier.setAttribute('width', 10)).toBe(AttributeSetResult.Unchanged);
  });

  it('does not invalidate layout when a style value loses to direct attribute precedence', () => {
    const applier = new AttributesApplier(1, new TestElementClass());

    expect(applier.setAttribute('width', 10)).toBe(AttributeSetResult.ChangedAndInvalidatesLayout);
    expect(applier.setAttribute('style', { attributes: { width: 20 } })).toBe(AttributeSetResult.Unchanged);
  });

  it('synchronizes external values without applying them back to the element', () => {
    const appliedValues: unknown[] = [];
    const binder = new AttributesBinder<HTMLElement>();
    binder.bindStringAttribute(
      'value',
      (_element, value) => {
        appliedValues.push(value);
      },
      () => {},
    );
    class ExternalValueElementClass extends ElementClass {
      constructor() {
        super('external-value', binder.attributeAppliers);
      }

      protected onCreateElement(): HTMLElement {
        return createElement();
      }
    }
    const applier = new AttributesApplier(1, new ExternalValueElementClass());
    const element = createElement();
    const context = createContext();

    applier.setAttribute('value', 'rendered');
    applier.flush(element, context, undefined);
    applier.updateAttributeWithoutApply('value', 'edited in the DOM');
    applier.flush(element, context, undefined);

    expect(applier.getAttribute('value')).toBe('edited in the DOM');
    expect(appliedValues).toEqual(['rendered']);

    expect(applier.setAttribute('value', 'cleared by the renderer')).toBe(AttributeSetResult.Changed);
    applier.flush(element, context, undefined);
    expect(appliedValues).toEqual(['rendered', 'cleared by the renderer']);
  });
});
