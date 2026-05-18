#import "SCValdiWebViewControllerImpl.h"

#import <WebKit/WebKit.h>

#import "SCValdiWebView.h"
#import "SCValdiWKWebViewHolder.h"
#import "valdi_core/SCValdiResolvablePromise.h"

static NSString *const SCValdiWebViewBridgeName = @"Valdi";

@interface SCValdiWebViewControllerImpl () <SCValdiNativeWebViewController, WKNavigationDelegate, WKScriptMessageHandler>
@end

@implementation SCValdiWebViewControllerImpl {
    SCValdiWKWebViewHolder *_webViewHolder;
    id<SCValdiWebViewListener> _listener;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        _webViewHolder = [[SCValdiWKWebViewHolder alloc] initWithNavigationDelegate:self
                                                               scriptMessageHandler:self
                                                                         bridgeName:SCValdiWebViewBridgeName];
    }
    return self;
}

- (UIView *)webView
{
    return [_webViewHolder getWebViewSync];
}

- (void)loadWithRequest:(SCValdiWebViewLoadRequest *)request
{
    [_webViewHolder getWebViewOnMainThread:^(WKWebView *webView) {
        if (!webView) {
            return;
        }
        [self _loadRequest:request inWebView:webView];
    }];
}

- (void)reload
{
    [_webViewHolder getWebViewOnMainThread:^(WKWebView *webView) {
        [webView reload];
    }];
}

- (void)stopLoading
{
    [_webViewHolder getWebViewOnMainThread:^(WKWebView *webView) {
        [webView stopLoading];
    }];
}

- (SCValdiPromise<SCValdiWebViewControllerState *> *)getState
{
    SCValdiResolvablePromise<SCValdiWebViewControllerState *> *promise = [SCValdiResolvablePromise new];
    [_webViewHolder getWebViewOnMainThread:^(WKWebView *webView) {
        SCValdiWebViewControllerState *state = [SCValdiWebViewControllerState new];
        state.canGoBack = webView.canGoBack;
        state.canGoForward = webView.canGoForward;
        state.loading = webView.loading;
        [promise fulfillWithSuccessValue:state];
    }];
    return promise;
}

- (void)goBack
{
    [_webViewHolder getWebViewOnMainThread:^(WKWebView *webView) {
        [webView goBack];
    }];
}

- (void)goForward
{
    [_webViewHolder getWebViewOnMainThread:^(WKWebView *webView) {
        [webView goForward];
    }];
}

- (void)evaluateJavaScriptWithScript:(NSString *)script callback:(SCValdiWebViewControllerEvaluateJavaScriptCallbackBlock)callback
{
    [_webViewHolder getWebViewOnMainThread:^(WKWebView *webView) {
        if (!webView) {
            if (callback) {
                SCValdiWebViewJavaScriptResult *result = [SCValdiWebViewJavaScriptResult new];
                result.errorMessage = @"WebView is disposed";
                callback(result);
            }
            return;
        }

        [webView evaluateJavaScript:script completionHandler:^(id value, NSError *error) {
            if (!callback) {
                return;
            }
            SCValdiWebViewJavaScriptResult *result = [SCValdiWebViewJavaScriptResult new];
            if (error) {
                result.errorMessage = error.localizedDescription;
            } else if (value && value != NSNull.null) {
                NSData *data = [NSJSONSerialization dataWithJSONObject:@[value] options:0 error:nil];
                if (data) {
                    NSString *json = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
                    if (json.length >= 2) {
                        result.value = [json substringWithRange:NSMakeRange(1, json.length - 2)];
                    }
                } else {
                    result.value = [value description];
                }
            }
            callback(result);
        }];
    }];
}

- (void)setListenerWithListener:(id<SCValdiWebViewListener>)listener
{
    [self _setLockedListener:listener];
    [_webViewHolder setBridgeEnabled:listener != nil];
}

- (void)dispose
{
    [self _setLockedListener:nil];
    [_webViewHolder dispose];
}

#pragma mark - WKNavigationDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    [[self _lockedListener] onLoadCompleted];
}

- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    [[self _lockedListener] onLoadFailedWithErrorMessage:error.localizedDescription];
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    [[self _lockedListener] onLoadFailedWithErrorMessage:error.localizedDescription];
}

#pragma mark - WKScriptMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController
      didReceiveScriptMessage:(WKScriptMessage *)message
{
    if (![message.name isEqualToString:SCValdiWebViewBridgeName]) {
        return;
    }

    NSString *body = [message.body isKindOfClass:NSString.class] ? message.body : [message.body description];
    [[self _lockedListener] onMessageWithMessage:body ?: @""];
}

#pragma mark - Private

- (id<SCValdiWebViewListener>)_lockedListener
{
    @synchronized (self) {
        return _listener;
    }
}

- (void)_setLockedListener:(id<SCValdiWebViewListener>)listener
{
    @synchronized (self) {
        _listener = listener;
    }
}

- (void)_loadRequest:(SCValdiWebViewLoadRequest *)request inWebView:(WKWebView *)webView
{
    if (!request) {
        return;
    }

    if (request.html != nil) {
        [webView loadHTMLString:request.html baseURL:nil];
        return;
    }

    if (request.url.length == 0) {
        return;
    }

    NSURL *url = [NSURL URLWithString:request.url];
    if (!url) {
        [[self _lockedListener] onLoadFailedWithErrorMessage:[NSString stringWithFormat:@"Invalid URL: %@", request.url]];
        return;
    }

    [webView loadRequest:[NSURLRequest requestWithURL:url]];
}

- (void)dealloc
{
    [self dispose];
}

@end
