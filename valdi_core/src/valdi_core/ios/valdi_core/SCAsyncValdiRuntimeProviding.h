
#import <Foundation/Foundation.h>
#import <valdi_core/SCValdiJSRuntime.h>
#import <valdi_core/SCValdiRuntimeProtocol.h>

NS_ASSUME_NONNULL_BEGIN

/// Protocol for asynchronous access to the Valdi runtime.
/// Use this instead of synchronous runtime access to avoid blocking the main thread during startup.
@protocol SCAsyncValdiRuntimeProviding <NSObject>

- (void)getRuntime:(void (^)(id<SCValdiRuntimeProtocol>))completion
    NS_SWIFT_NAME(getRuntime(completion:));

- (void)getJSRuntime:(void (^)(id<SCValdiJSRuntime> _Nullable))completion
    NS_SWIFT_NAME(getJSRuntime(completion:));

/**
 * Gets the JS runtime and attributes the JS-thread dispatch to the provided callsite.
 */
- (void)getJSRuntimeWithAttribution:(NSString *)attribution
                         completion:(void (^)(id<SCValdiJSRuntime> _Nullable))completion
    NS_SWIFT_NAME(getJSRuntime(attribution:completion:));

@end

NS_ASSUME_NONNULL_END
