#import <AppKit/AppKit.h>

@class SCValdiMacOSAttributesBinder;

@interface SCValdiWebView : NSView

+ (void)bindAttributes:(SCValdiMacOSAttributesBinder*)attributesBinder;

@end
