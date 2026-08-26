import type { AttributeApplier, AttributeApplierContext } from '../core/ElementClass';
import { ElementClass } from '../core/ElementClass';
import { assignStyles, createBaseElement, setApplierCleanup } from './ElementClassSupport';
import { ViewElementClass } from './ViewElementClass';

interface BrowserWebViewController {
  attachWebView(frame: HTMLIFrameElement): void;
  detachWebView(frame: HTMLIFrameElement): void;
}

function controllerAttributeApplier(): AttributeApplier {
  return {
    apply(element: HTMLElement, value: unknown, _attributeName: string, context: AttributeApplierContext): void {
      const frame = element.querySelector('iframe');
      const controller = value as Partial<BrowserWebViewController> | undefined;
      if (
        frame === null ||
        typeof controller?.attachWebView !== 'function' ||
        typeof controller.detachWebView !== 'function'
      ) {
        console.warn('The Valdi browser WebView requires an iframe-backed browser controller.');
        setApplierCleanup(context, 'webview:controller', undefined);
        return;
      }
      const browserController = controller as BrowserWebViewController;
      browserController.attachWebView(frame);
      setApplierCleanup(context, 'webview:controller', () => browserController.detachWebView(frame));
    },
    reset(_element: HTMLElement, _attributeName: string, context: AttributeApplierContext): void {
      setApplierCleanup(context, 'webview:controller', undefined);
    },
  };
}

export class WebViewElementClass extends ElementClass {
  constructor(viewElementClass: ViewElementClass) {
    super(
      'webview',
      { ...viewElementClass.attributeAppliers, controller: controllerAttributeApplier() },
      viewElementClass.compositeAttributes,
    );
  }

  protected onCreateElement(): HTMLElement {
    const container = createBaseElement('div');
    container.style.pointerEvents = 'auto';
    const frame = document.createElement('iframe');
    frame.title = 'Valdi embedded application';
    frame.setAttribute('sandbox', 'allow-scripts allow-forms');
    assignStyles(frame, { border: '0', flexGrow: '1', height: '100%', minHeight: '0', minWidth: '0', width: '100%' });
    container.appendChild(frame);
    return container;
  }
}
