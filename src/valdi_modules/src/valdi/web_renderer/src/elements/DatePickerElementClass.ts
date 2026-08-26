import { ElementClass } from '../core/ElementClass';
import { createBaseElement } from './ElementClassSupport';
import { ViewElementClass } from './ViewElementClass';

export class DatePickerElementClass extends ElementClass {
  constructor(viewElementClass: ViewElementClass) {
    // TODO: Implement date picker input behavior and platform-specific attributes.
    super('datepicker', viewElementClass.attributeAppliers, viewElementClass.compositeAttributes);
  }

  protected onCreateElement(): HTMLElement {
    return createBaseElement('div');
  }
}
