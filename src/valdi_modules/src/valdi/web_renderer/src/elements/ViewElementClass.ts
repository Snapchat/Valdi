import type {
  AttributeApplierContext,
  CompositeAttribute,
  LayoutAnimationSizeApplier,
  LayoutAnimationTranslationCorrection,
} from '../core/ElementClass';
import { assignStyles, type AttributeApplierMap } from './ElementClassSupport';
import { LayoutElementClass } from './LayoutElementClass';
import { buildViewAttributeAppliers, viewCompositeAttributes } from './ViewElementAttributes';
import { getViewPaintElement, setViewPaintElement } from './ViewElementState';

const ZERO_TRANSLATION_CORRECTION: LayoutAnimationTranslationCorrection = { x: 0, y: 0 };

class PaintElementLayoutAnimationSizeApplier implements LayoutAnimationSizeApplier {
  private readonly originalScale: string;

  constructor(private readonly element: HTMLElement) {
    this.originalScale = element.style.getPropertyValue('scale');
  }

  apply(scaleX: number, scaleY: number): LayoutAnimationTranslationCorrection {
    this.element.style.setProperty('scale', `${scaleX} ${scaleY}`);
    return ZERO_TRANSLATION_CORRECTION;
  }

  reset(): void {
    if (this.originalScale) {
      this.element.style.setProperty('scale', this.originalScale);
    } else {
      this.element.style.removeProperty('scale');
    }
  }
}

export class ViewElementClass extends LayoutElementClass {
  private paintElementTemplate: HTMLElement | undefined;

  constructor(
    className: string,
    additionalAttributeAppliers: AttributeApplierMap,
    additionalCompositeAttributes: Readonly<Record<string, CompositeAttribute>>,
  ) {
    super(
      className,
      { ...buildViewAttributeAppliers(), ...additionalAttributeAppliers },
      { ...viewCompositeAttributes, ...additionalCompositeAttributes },
    );
  }

  override getViewAttributeElement(element: HTMLElement, context: AttributeApplierContext): HTMLElement {
    const existing = getViewPaintElement(context);
    if (existing) {
      return existing;
    }

    const paintElement = this.getPaintElementTemplate().cloneNode(false) as HTMLElement;
    element.style.isolation = 'isolate';
    element.insertBefore(paintElement, element.childNodes.item(0));
    setViewPaintElement(context, paintElement);
    return paintElement;
  }

  private getPaintElementTemplate(): HTMLElement {
    if (!this.paintElementTemplate) {
      const paintElement = document.createElement('div');
      paintElement.setAttribute('aria-hidden', 'true');
      assignStyles(paintElement, {
        inset: '0',
        pointerEvents: 'none',
        position: 'absolute',
        transformOrigin: '0 0',
        zIndex: '-1',
      });
      this.paintElementTemplate = paintElement;
    }
    return this.paintElementTemplate;
  }

  override makeLayoutAnimationSizeApplier(
    _element: HTMLElement,
    context: AttributeApplierContext,
    _finalWidth: number,
    _finalHeight: number,
  ): LayoutAnimationSizeApplier | undefined {
    const paintElement = getViewPaintElement(context);
    return paintElement ? new PaintElementLayoutAnimationSizeApplier(paintElement) : undefined;
  }
}
