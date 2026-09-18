import { getWebViewClassFactory, WebViewClassAttributeHandler } from '../WebViewClassRegistry';
import { WebValdiView } from './WebValdiView';

/**
 * Web implementation of <custom-view>.
 * Resolves webClass via WebViewClassRegistry and runs the registered factory.
 * The factory can optionally return an object with a changeAttribute method
 * to receive attribute updates.
 */
export class WebValdiCustomView extends WebValdiView {
  public override type = 'custom-view';

  private _webClassApplied = false;
  private _attributeHandler?: WebViewClassAttributeHandler;
  private _pendingAttributes: Array<[string, unknown]> = [];

  override changeAttribute(attributeName: string, attributeValue: unknown): void {
    if (attributeName === 'androidClass' || attributeName === 'iosClass' || attributeName === 'macosClass') {
      return;
    }
    if (attributeName === 'webClass') {
      if (attributeValue && typeof attributeValue === 'string' && !this._webClassApplied) {
        this._webClassApplied = true;
        const factory = getWebViewClassFactory(attributeValue);
        if (factory) {
          this.htmlElement.replaceChildren();
          const result = factory(this.htmlElement);
          if (result) {
            this._attributeHandler = result;
          }
          // Flush pending attributes
          for (const [name, value] of this._pendingAttributes) {
            this._attributeHandler?.changeAttribute(name, value);
          }
          this._pendingAttributes = [];
        } else {
          this.appendPlaceholder(attributeValue);
        }
        if (!this.htmlElement.style.height) {
          this.htmlElement.style.minHeight = '80px';
        }
      }
      return;
    }

    // Try known layout/view attributes first via super
    try {
      super.changeAttribute(attributeName, attributeValue);
    } catch {
      // Unknown attribute — forward to the custom view's attribute handler
      if (this._attributeHandler) {
        this._attributeHandler.changeAttribute(attributeName, attributeValue);
      } else if (!this._webClassApplied) {
        // webClass hasn't arrived yet; buffer the attribute
        this._pendingAttributes.push([attributeName, attributeValue]);
      }
    }
  }

  private appendPlaceholder(message: string): void {
    this.htmlElement.style.position = 'relative';
    const label = document.createElement('span');
    label.textContent = message;
    Object.assign(label.style, {
      position: 'absolute',
      inset: '0',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      pointerEvents: 'none',
      fontSize: '14px',
      color: 'inherit',
    });
    this.htmlElement.appendChild(label);
  }
}
