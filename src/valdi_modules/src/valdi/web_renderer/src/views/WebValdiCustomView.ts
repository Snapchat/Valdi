import { getWebViewClassFactory } from '../WebViewClassRegistry';
import { WebValdiView } from './WebValdiView';

/**
 * Web implementation of <custom-view>.
 * Resolves webClass via WebViewClassRegistry and runs the registered factory.
 * If no factory is registered, shows a placeholder.
 */
export class WebValdiCustomView extends WebValdiView {
  public override type = 'custom-view';

  private _webClassApplied = false;
  private _fallbackScheduled = false;

  override changeAttribute(attributeName: string, attributeValue: unknown): void {
    // WARNING: console.log can be replaced during Init in the web runtime.
    // Also, custom-view attributes arrive in batches, so this method sees intermediate states.
    // If you need debugging here, use __valdiLogToConsole__ and account for partial updates.
    if (attributeName === 'androidClass' || attributeName === 'iosClass' || attributeName === 'macosClass') {
      // Native-only class selectors are expected on custom-view, but ignored on web.
      return;
    }
    if (attributeName === 'webClass') {
      if (attributeValue && typeof attributeValue === 'string' && !this._webClassApplied) {
        this._webClassApplied = true;
        const factory = getWebViewClassFactory(attributeValue);
        if (factory) {
          this.htmlElement.replaceChildren();
          factory(this.htmlElement);
        } else {
          this.appendPlaceholder(attributeValue);
        }
        if (!this.htmlElement.style.height) {
          this.htmlElement.style.minHeight = '80px';
        }
      }
      return;
    }
    if (!this._webClassApplied && !this._fallbackScheduled) {
      this._fallbackScheduled = true;
      // Delay so webClass can arrive in a later attribute batch.
      setTimeout(() => {
        if (!this._webClassApplied && this.htmlElement.childNodes.length === 0) {
          this.appendPlaceholder('(no webClass)');
          if (!this.htmlElement.style.minHeight) {
            this.htmlElement.style.minHeight = '80px';
          }
        }
      }, 150);
    }
    super.changeAttribute(attributeName, attributeValue);
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
