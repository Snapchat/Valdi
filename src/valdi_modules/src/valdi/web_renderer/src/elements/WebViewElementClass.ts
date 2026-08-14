import { ElementClass } from '../core/ElementClass';
import { createBaseElement } from './ElementClassSupport';
import { ViewElementClass } from './ViewElementClass';

export class WebViewElementClass extends ElementClass {
  constructor(viewElementClass: ViewElementClass) {
    // TODO: Implement webview controller and iframe/content behavior.
    super('webview', viewElementClass.attributeAppliers, viewElementClass.compositeAttributes);
  }

  protected onCreateElement(): HTMLElement {
    return createBaseElement('div');
  }
}
