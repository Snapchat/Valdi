#import "SCValdiWKWebViewHolder.h"

#import "valdi_core/SCValdiUnsetAndReleaseInMainThread.h"

static void SCValdiWKWebViewRunOnMainAsync(dispatch_block_t block)
{
    if (NSThread.isMainThread) {
        block();
        return;
    }
    dispatch_async(dispatch_get_main_queue(), block);
}

static void SCValdiWKWebViewRunOnMainSync(dispatch_block_t block)
{
    if (NSThread.isMainThread) {
        block();
        return;
    }
    dispatch_sync(dispatch_get_main_queue(), block);
}

@interface SCValdiWeakScriptMessageHandler : NSObject <WKScriptMessageHandler>
@property (nonatomic, weak) id<WKScriptMessageHandler> delegate;
@end

@implementation SCValdiWeakScriptMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message {
    [self.delegate userContentController:userContentController didReceiveScriptMessage:message];
}
@end

@implementation SCValdiWKWebViewHolder {
    WKWebView *_webView;
    __weak id<WKNavigationDelegate> _navigationDelegate;
    __weak id<WKScriptMessageHandler> _scriptMessageHandler;
    NSString *_bridgeName;
    BOOL _disposed;
    BOOL _bridgeEnabled;
    BOOL _bridgeInstalled;
}

- (instancetype)initWithNavigationDelegate:(id<WKNavigationDelegate>)navigationDelegate
                      scriptMessageHandler:(id<WKScriptMessageHandler>)scriptMessageHandler
                                bridgeName:(NSString *)bridgeName
{
    self = [super init];
    if (self) {
        _navigationDelegate = navigationDelegate;
        _scriptMessageHandler = scriptMessageHandler;
        _bridgeName = [bridgeName copy];
    }
    return self;
}

- (UIView *)getWebViewSync
{
    __block WKWebView *webView = nil;
    SCValdiWKWebViewRunOnMainSync(^{
        webView = [self _ensureWebView];
    });
    return webView;
}

- (void)getWebViewOnMainThread:(SCValdiWKWebViewCallback)callback
{
    SCValdiWKWebViewRunOnMainAsync(^{
        callback([self _ensureWebView]);
    });
}

- (void)withExistingWebViewOnMainThread:(SCValdiWKWebViewCallback)callback
{
    SCValdiWKWebViewRunOnMainAsync(^{
        callback(_webView);
    });
}

- (void)setBridgeEnabled:(BOOL)bridgeEnabled
{
    SCValdiWKWebViewRunOnMainAsync(^{
        _bridgeEnabled = bridgeEnabled;
        if (_webView) {
            [self _configureBridgeInWebView:_webView];
        }
    });
}

- (void)dispose
{
    SCValdiWKWebViewRunOnMainAsync(^{
        if (_disposed) return;
        _disposed = YES;
        [self _prepareWebViewForRelease];
        _webView = nil;
    });
}

#pragma mark - Private

- (WKWebView *)_ensureWebView
{
    if (_disposed) {
        return nil;
    }

    if (_webView) {
        return _webView;
    }

    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
    _webView = [[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration];
    [self _configureWebView:_webView];
    return _webView;
}

- (void)_configureWebView:(WKWebView *)webView
{
    webView.navigationDelegate = _navigationDelegate;
    [self _configureBridgeInWebView:webView];
}

- (void)_configureBridgeInWebView:(WKWebView *)webView
{
    WKUserContentController *controller = webView.configuration.userContentController;
    if (_bridgeInstalled) {
        [controller removeScriptMessageHandlerForName:_bridgeName];
        [controller removeAllUserScripts];
        _bridgeInstalled = NO;
    }

    id<WKScriptMessageHandler> scriptMessageHandler = _scriptMessageHandler;
    if (!_bridgeEnabled || !scriptMessageHandler) {
        return;
    }

    NSString *source = [NSString stringWithFormat:
        @"window.%@ = window.%@ || {};"
         "window.%@.postMessage = function(message) {"
         "  var stringMessage = typeof message === 'string' ? message : JSON.stringify(message);"
         "  window.webkit.messageHandlers.%@.postMessage(String(stringMessage));"
         "};",
        _bridgeName,
        _bridgeName,
        _bridgeName,
        _bridgeName];
    WKUserScript *script = [[WKUserScript alloc] initWithSource:source
                                                  injectionTime:WKUserScriptInjectionTimeAtDocumentStart
                                               forMainFrameOnly:NO];
    [controller addUserScript:script];
    SCValdiWeakScriptMessageHandler *proxy = [[SCValdiWeakScriptMessageHandler alloc] init];
    proxy.delegate = scriptMessageHandler;
    [controller addScriptMessageHandler:proxy name:_bridgeName];
    _bridgeInstalled = YES;
}

- (void)_prepareWebViewForRelease
{
    [_webView removeFromSuperview];
    [_webView stopLoading];
    if (_bridgeInstalled) {
        [_webView.configuration.userContentController removeScriptMessageHandlerForName:_bridgeName];
        [_webView.configuration.userContentController removeAllUserScripts];
        _bridgeInstalled = NO;
    }
    _webView.navigationDelegate = nil;
}

- (void)dealloc
{
    WKWebView *webView = _webView;
    BOOL bridgeInstalled = _bridgeInstalled;
    NSString *bridgeName = _bridgeName;
    if (webView) {
        SCValdiWKWebViewRunOnMainAsync(^{
            [webView removeFromSuperview];
            [webView stopLoading];
            if (bridgeInstalled) {
                [webView.configuration.userContentController removeScriptMessageHandlerForName:bridgeName];
                [webView.configuration.userContentController removeAllUserScripts];
            }
            webView.navigationDelegate = nil;
        });
    }
}

@end
