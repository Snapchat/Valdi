import { AttributesBinder } from '../attributes/AttributesBinder';
import { ElementClass } from '../core/ElementClass';
import { AttributeApplierMap, createBaseElement } from './ElementClassSupport';
import {
  registerTextAnimationGroup,
  setTextAnimationGroupFlushDurationThreshold,
  setTextAnimationGroupFlushMultiplier,
  unregisterTextAnimationGroup,
} from '../utils/TextAnimationRegistry';
import { LayoutElementClass } from './LayoutElementClass';

function buildTextAnimationGroupAttributeAppliers(): AttributeApplierMap {
  const binder = new AttributesBinder<HTMLElement>();
  binder.bindNumberAttribute(
    'flushDurationThreshold',
    (element, value) => {
      setTextAnimationGroupFlushDurationThreshold(element, Math.max(value, 0));
    },
    element => {
      setTextAnimationGroupFlushDurationThreshold(element, undefined);
    },
  );
  binder.bindNumberAttribute(
    'flushMultiplier',
    (element, value) => {
      setTextAnimationGroupFlushMultiplier(element, Math.max(value, 0));
    },
    element => {
      setTextAnimationGroupFlushMultiplier(element, undefined);
    },
  );
  return binder.attributeAppliers;
}

export class TextAnimationGroupElementClass extends ElementClass {
  constructor(layoutElementClass: LayoutElementClass) {
    super(
      'textanimationgroup',
      {
        ...layoutElementClass.attributeAppliers,
        ...buildTextAnimationGroupAttributeAppliers(),
      },
      layoutElementClass.compositeAttributes,
    );
  }

  createElement(id: number, viewClass: string): HTMLElement {
    const element = super.createElement(id, viewClass);
    registerTextAnimationGroup(element);
    return element;
  }

  destroy(element: HTMLElement): void {
    unregisterTextAnimationGroup(element);
  }

  protected onCreateElement(): HTMLElement {
    return createBaseElement('div');
  }
}
