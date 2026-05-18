#import <UIKit/UIKit.h>

@protocol SCValdiNativeWebViewController <NSObject>

@property (nonatomic, readonly, nullable) UIView* webView;

@end

@interface SCValdiWebView : UIView

@end
