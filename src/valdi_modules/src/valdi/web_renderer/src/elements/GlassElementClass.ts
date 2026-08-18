import { AttributesBinder } from '../attributes/AttributesBinder';
import { createBorderRadiusAttributeApplier } from '../attributes/BorderRadiusAttribute';
import { ElementClass } from '../core/ElementClass';
import { assignStyles, createBaseElement } from './ElementClassSupport';
import { ViewElementClass } from './ViewElementClass';

const DEFAULT_GLASS_BACKGROUND = 'rgba(246, 246, 246, 0.34)';

export class GlassElementClass extends ElementClass {
  constructor(viewElementClass: ViewElementClass) {
    const binder = new AttributesBinder<HTMLElement>();
    binder.bindAttribute('borderRadius', createBorderRadiusAttributeApplier(true));
    binder.bindAttribute('glassCornerRadius', createBorderRadiusAttributeApplier(true));
    binder.bindAttribute('glassStyle', {
      apply() {},
      reset() {},
    });
    binder.bindAttribute('glassInteractive', {
      apply() {},
      reset() {},
    });
    binder.bindAttribute('glassAppearance', {
      apply() {},
      reset() {},
    });
    binder.bindColorAttribute(
      'glassTintColor',
      (element, value) => {
        element.style.backgroundColor = value;
      },
      element => {
        element.style.backgroundColor = DEFAULT_GLASS_BACKGROUND;
      },
    );
    super(
      'glass',
      { ...viewElementClass.attributeAppliers, ...binder.attributeAppliers },
      viewElementClass.compositeAttributes,
    );
  }

  protected onCreateElement(): HTMLElement {
    const element = createBaseElement('div');
    assignStyles(element, {
      backdropFilter: 'blur(18px) saturate(110%)',
      backgroundColor: DEFAULT_GLASS_BACKGROUND,
      overflow: 'hidden',
      WebkitBackdropFilter: 'blur(18px) saturate(110%)',
    });
    return element;
  }
}
