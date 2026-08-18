import type { ElementFrame } from 'valdi_tsx/src/Geometry';
import type { AnimationInterpolator } from '../attributes/AttributeAnimation';
import type { IViewNodeAssetTracker } from 'valdi_core/src/IViewNodeAssetTracker';

export type MakeAnimationInterpolator<TElement extends HTMLElement = HTMLElement> = (
  element: TElement,
  from: unknown,
  to: unknown,
  context: AttributeApplierContext,
) => AnimationInterpolator | undefined;

export interface ElementLayoutObserver {
  onMeasure?(element: HTMLElement): void;
  onSizeChanged?(width: number, height: number): void;
  onCommit?(element: HTMLElement): void;
}

export interface AttributeUpdatedExternallyDelegate {
  onAttributeUpdatedExternally(elementId: number, attributeName: string, attributeValue: unknown): void;
}

export interface AttributeApplierContext {
  readonly id: number;
  getAssetTracker(): IViewNodeAssetTracker | undefined;
  getState<T>(key: string): T | undefined;
  setState(key: string, value: unknown): void;
  getViewAttributeElement(): HTMLElement;
  resolveColor(value: string): string;
  setColorPalette(colorPaletteName: string | undefined): void;
  addCleanup(callback: () => void): void;
  enqueuePostLayoutCallback(callback: () => void): void;
  getLayoutObserver(attributeName: string): ElementLayoutObserver | undefined;
  setLayoutObserver(attributeName: string, observer: ElementLayoutObserver | undefined): void;
  requestLayoutPass(): void;
  getChildHtmlElement(index: number): HTMLElement | undefined;
  setOnLayoutCallback(callback: ((frame: ElementFrame) => void) | undefined): void;
  onAttributeUpdatedExternally(attributeName: string, attributeValue: unknown): void;
  emitCurrentViewCreate(callback: Function): void;
  emitCurrentViewChange(): void;
  isAnimationEnabled(): boolean;
  setAnimationsEnabled(enabled: boolean): void;
}

export interface LayoutAnimationTranslationCorrection {
  readonly x: number;
  readonly y: number;
}

export interface LayoutAnimationSizeApplier {
  apply(scaleX: number, scaleY: number): LayoutAnimationTranslationCorrection;
  reset(): void;
}

export interface AttributeApplier<TElement extends HTMLElement = HTMLElement> {
  apply(element: TElement, value: unknown, attributeName: string, context: AttributeApplierContext): void;
  reset(element: TElement, attributeName: string, context: AttributeApplierContext): void;
  makeAnimationInterpolator?: MakeAnimationInterpolator<TElement>;
  animationMinimumVisibleChange?: number;
  colorDependent?: boolean;
  layoutDependent?: boolean;
}

export interface CompositeAttributePart<TElement extends HTMLElement = HTMLElement> {
  name: string;
  optional: boolean;
  colorDependent?: boolean;
  layoutDependent?: boolean;
  parse?: (element: TElement, value: unknown, attributeName: string, context: AttributeApplierContext) => unknown;
}

export interface CompositeAttribute<TElement extends HTMLElement = HTMLElement> {
  name: string;
  parts: ReadonlyArray<CompositeAttributePart<TElement>>;
  apply(
    element: TElement,
    values: ReadonlyArray<unknown>,
    attributeName: string,
    context: AttributeApplierContext,
  ): void;
  reset(element: TElement, attributeName: string, context: AttributeApplierContext): void;
  makeAnimationInterpolator?: MakeAnimationInterpolator<TElement>;
  animationMinimumVisibleChange?: number;
  colorDependent?: boolean;
  layoutDependent?: boolean;
}

export interface UnknownAttributeApplier<TElement extends HTMLElement = HTMLElement> {
  apply(element: TElement, value: unknown, attributeName: string, context: AttributeApplierContext): void;
  reset(element: TElement, attributeName: string, context: AttributeApplierContext): void;
  layoutDependent?: boolean;
}

export interface ElementAttribute<TElement extends HTMLElement = HTMLElement> {
  applier: AttributeApplier<TElement> | undefined;
  composite: CompositeAttribute<TElement> | undefined;
  isCompositePart: boolean;
}

export abstract class ElementClass<TElement extends HTMLElement = HTMLElement> {
  readonly elementAttributes: Readonly<Record<string, ElementAttribute<TElement> | undefined>>;

  protected constructor(
    readonly className: string,
    readonly attributeAppliers: Readonly<Record<string, AttributeApplier<TElement>>>,
    readonly compositeAttributes: Readonly<Record<string, CompositeAttribute<TElement>>> = {},
    readonly unknownAttributeApplier?: UnknownAttributeApplier<TElement>,
  ) {
    const elementAttributes: Record<string, ElementAttribute<TElement> | undefined> = {};
    Object.keys(attributeAppliers).forEach(name => {
      const applier = attributeAppliers[name];
      if (applier.layoutDependent && applier.makeAnimationInterpolator) {
        throw new Error(`Layout-dependent attribute '${name}' cannot be animated`);
      }
      elementAttributes[name] = { applier, composite: undefined, isCompositePart: false };
    });
    Object.keys(compositeAttributes).forEach(name => {
      const composite = compositeAttributes[name];
      if (!composite.colorDependent && composite.parts.some(part => part.colorDependent)) {
        (composite as { colorDependent?: boolean }).colorDependent = true;
      }
      if (!composite.layoutDependent && composite.parts.some(part => part.layoutDependent)) {
        (composite as { layoutDependent?: boolean }).layoutDependent = true;
      }
      if (composite.layoutDependent && composite.makeAnimationInterpolator) {
        throw new Error(`Layout-dependent composite attribute '${name}' cannot be animated`);
      }
      elementAttributes[composite.name] = { applier: undefined, composite, isCompositePart: false };
      composite.parts.forEach(part => {
        const elementAttribute = elementAttributes[part.name];
        if (elementAttribute) {
          elementAttribute.composite = composite;
          elementAttribute.isCompositePart = true;
        } else {
          elementAttributes[part.name] = { applier: undefined, composite, isCompositePart: true };
        }
      });
    });
    this.elementAttributes = elementAttributes;
  }

  private templateElement?: TElement;

  createElement(_id: number, _viewClass: string): TElement {
    let templateElement = this.templateElement;
    if (!templateElement) {
      templateElement = this.onCreateElement();
      this.templateElement = templateElement;
    }
    return templateElement.cloneNode(true) as TElement;
  }

  protected abstract onCreateElement(): TElement;

  getViewAttributeElement(element: TElement, _context: AttributeApplierContext): HTMLElement {
    return element;
  }

  makeLayoutAnimationSizeApplier(
    _element: TElement,
    _context: AttributeApplierContext,
    _finalWidth: number,
    _finalHeight: number,
  ): LayoutAnimationSizeApplier | undefined {
    return undefined;
  }

  destroy(_element: TElement): void {}
}

export type AnyElementClass = ElementClass<any>;
