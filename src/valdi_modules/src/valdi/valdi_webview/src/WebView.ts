import { createNativeController, IWebViewController } from './WebViewNative';

export {
  IWebViewController,
  IWebViewControllerState,
  IWebViewLoadRequest,
  IWebViewListener,
  WebViewJavaScriptResult,
} from './WebViewNative';

export class WebView {
  static createController(): IWebViewController {
    return createNativeController();
  }
}
