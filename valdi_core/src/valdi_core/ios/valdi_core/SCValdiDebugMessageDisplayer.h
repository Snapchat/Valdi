#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol SCValdiDebugMessageDisplayer <NSObject>

- (void)displayMessage:(NSString*)message;

@end

NS_ASSUME_NONNULL_END
