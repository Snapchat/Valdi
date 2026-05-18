#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

NS_ASSUME_NONNULL_BEGIN

typedef void (^SCValdiWKWebViewCallback)(WKWebView* _Nullable webView);

@interface SCValdiWKWebViewHolder : NSObject

- (instancetype)initWithNavigationDelegate:(id<WKNavigationDelegate>)navigationDelegate
                      scriptMessageHandler:(id<WKScriptMessageHandler>)scriptMessageHandler
                                bridgeName:(NSString*)bridgeName NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (UIView* _Nullable)getWebViewSync;
- (void)getWebViewOnMainThread:(SCValdiWKWebViewCallback)callback;
- (void)withExistingWebViewOnMainThread:(SCValdiWKWebViewCallback)callback;
- (void)setBridgeEnabled:(BOOL)bridgeEnabled;
- (void)dispose;

@end

NS_ASSUME_NONNULL_END
