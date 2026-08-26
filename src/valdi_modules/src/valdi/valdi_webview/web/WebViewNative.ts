import type {
  IWebViewController,
  IWebViewControllerState,
  IWebViewListener,
  IWebViewLoadRequest,
  WebViewJavaScriptResult,
} from '../src/WebViewNative';

const MAX_BROWSER_WEBVIEW_MESSAGE_LENGTH = 512_000;

/** Browser controller deliberately preserves the iframe's opaque sandbox origin. */
class BrowserWebViewController implements IWebViewController {
  private frame: HTMLIFrameElement | undefined;
  private listener: IWebViewListener | undefined;
  private loading = false;
  private request: IWebViewLoadRequest | undefined;

  private readonly handleFrameLoad = (): void => {
    this.loading = false;
    this.listener?.onLoadCompleted();
  };

  private readonly handleFrameError = (): void => {
    this.loading = false;
    this.listener?.onLoadFailed('The sandboxed browser WebView could not load its content.');
  };

  private readonly handleWindowMessage = (event: MessageEvent): void => {
    const frameWindow = this.frame?.contentWindow;
    if (frameWindow === undefined || frameWindow === null || event.source !== frameWindow) {
      return;
    }
    let message: string | undefined;
    try {
      message = typeof event.data === 'string' ? event.data : JSON.stringify(event.data);
    } catch (error) {
      console.warn(`Ignored an unserializable sandboxed WebView message: ${String(error)}`);
      return;
    }
    if (typeof message !== 'string' || message.length > MAX_BROWSER_WEBVIEW_MESSAGE_LENGTH) {
      console.warn('Ignored an invalid or oversized sandboxed WebView message.');
      return;
    }
    this.listener?.onMessage(message);
  };

  attachWebView(frame: HTMLIFrameElement): void {
    if (this.frame === frame) {
      return;
    }
    if (this.frame !== undefined) {
      this.detachWebView(this.frame);
    }
    this.frame = frame;
    frame.addEventListener('load', this.handleFrameLoad);
    frame.addEventListener('error', this.handleFrameError);
    window.addEventListener('message', this.handleWindowMessage);
    if (this.request !== undefined) {
      this.applyRequest(this.request);
    }
  }

  detachWebView(frame: HTMLIFrameElement): void {
    if (this.frame !== frame) {
      return;
    }
    frame.removeEventListener('load', this.handleFrameLoad);
    frame.removeEventListener('error', this.handleFrameError);
    window.removeEventListener('message', this.handleWindowMessage);
    this.frame = undefined;
  }

  load(request: IWebViewLoadRequest): void {
    this.request = request;
    this.loading = true;
    if (this.frame !== undefined) {
      this.applyRequest(request);
    }
  }

  reload(): void {
    if (this.request !== undefined) {
      this.load(this.request);
    }
  }

  stopLoading(): void {
    this.loading = false;
    if (this.frame !== undefined) {
      this.frame.removeAttribute('srcdoc');
      this.frame.src = 'about:blank';
    }
  }

  getState(): Promise<IWebViewControllerState> {
    return Promise.resolve({ canGoBack: false, canGoForward: false, loading: this.loading });
  }

  goBack(): void {
    console.warn('Sandboxed browser WebViews do not expose cross-origin navigation history.');
  }

  goForward(): void {
    console.warn('Sandboxed browser WebViews do not expose cross-origin navigation history.');
  }

  evaluateJavaScript(_script: string, callback?: (result: WebViewJavaScriptResult) => void): void {
    const errorMessage = 'JavaScript injection is unavailable for opaque-origin sandboxed browser WebViews.';
    console.warn(errorMessage);
    callback?.({ errorMessage });
  }

  setListener(listener?: IWebViewListener): void {
    this.listener = listener;
  }

  dispose(): void {
    if (this.frame !== undefined) {
      this.detachWebView(this.frame);
    }
    this.listener = undefined;
    this.request = undefined;
  }

  private applyRequest(request: IWebViewLoadRequest): void {
    const frame = this.frame;
    if (frame === undefined) {
      return;
    }
    if (request.html !== undefined) {
      frame.removeAttribute('src');
      frame.srcdoc = request.html;
      return;
    }
    if (request.url !== undefined) {
      frame.removeAttribute('srcdoc');
      frame.src = request.url;
      return;
    }
    this.loading = false;
    this.listener?.onLoadFailed('The browser WebView request must contain HTML or a URL.');
  }
}

export function createNativeController(): IWebViewController {
  return new BrowserWebViewController();
}
