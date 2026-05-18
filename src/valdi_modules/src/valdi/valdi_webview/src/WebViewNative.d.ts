/**
 * @ExportModule
 */

/**
 * @ExportModel({
 *   ios: 'SCValdiWebViewLoadRequest',
 *   android: 'com.snap.valdi.modules.webview.WebViewLoadRequest'
 * })
 */
export interface IWebViewLoadRequest {
  /**
   * Load the web view with a URL.
   */
  url?: string;

  /**
   * Load the web view with the specified HTML.
   * Takes priority over url when both are set.
   */
  html?: string;
}

/**
 * @ExportModel({
 *   ios: 'SCValdiWebViewJavaScriptResult',
 *   android: 'com.snap.valdi.modules.webview.WebViewJavaScriptResult'
 * })
 */
export interface WebViewJavaScriptResult {
  value?: string;
  errorMessage?: string;
}

/**
 * @ExportModel({
 *   ios: 'SCValdiWebViewControllerState',
 *   android: 'com.snap.valdi.modules.webview.WebViewControllerState'
 * })
 */
export interface IWebViewControllerState {
  canGoBack: boolean;
  canGoForward: boolean;
  loading: boolean;
}

/**
 * @ExportProxy({
 *   ios: 'SCValdiWebViewListener',
 *   android: 'com.snap.valdi.modules.webview.IWebViewListener'
 * })
 */
export interface IWebViewListener {
  onMessage(message: string): void;
  onLoadFailed(errorMessage: string): void;
  onLoadCompleted(): void;
}

/**
 * @ExportProxy({
 *   ios: 'SCValdiWebViewController',
 *   android: 'com.snap.valdi.modules.webview.IWebViewController'
 * })
 */
export interface IWebViewController {
  load(request: IWebViewLoadRequest): void;
  reload(): void;
  stopLoading(): void;
  getState(): Promise<IWebViewControllerState>;
  goBack(): void;
  goForward(): void;
  evaluateJavaScript(script: string, callback?: (result: WebViewJavaScriptResult) => void): void;
  setListener(listener?: IWebViewListener): void;
  dispose(): void;
}

export function createNativeController(): IWebViewController;
