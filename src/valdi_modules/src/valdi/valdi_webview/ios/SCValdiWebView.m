#import "SCValdiWebView.h"

#import "valdi/ios/Categories/UIView+Valdi.h"
#import "valdi/ios/SCValdiAttributesBinder.h"

@implementation SCValdiWebView {
    id<SCValdiNativeWebViewController> _controller;
}

- (BOOL)willEnqueueIntoValdiPool
{
    return NO;
}

- (void)setValdiWebViewController:(nullable id<SCValdiNativeWebViewController>)controller
{
    if (_controller == controller) {
        return;
    }

    [_controller.webView removeFromSuperview];

    _controller = controller;

    UIView *webView = _controller.webView;
    if (!webView) {
        return;
    }

    [webView removeFromSuperview];
    webView.frame = self.bounds;
    [self addSubview:webView];
}

- (void)layoutSubviews
{
    [super layoutSubviews];
    _controller.webView.frame = self.bounds;
}

+ (void)bindAttributes:(id<SCValdiAttributesBinderProtocol>)attributesBinder
{
    [attributesBinder bindAttribute:@"controller"
        invalidateLayoutOnChange:NO
        withUntypedBlock:^BOOL(SCValdiWebView *view, id attributeValue, id<SCValdiAnimatorProtocol> animator) {
            if (![attributeValue conformsToProtocol:@protocol(SCValdiNativeWebViewController)]) {
                [view setValdiWebViewController:nil];
                return NO;
            }
            [view setValdiWebViewController:attributeValue];
            return YES;
        }
        resetBlock:^(SCValdiWebView *view, id<SCValdiAnimatorProtocol> animator) {
            [view setValdiWebViewController:nil];
        }];
}

@end
