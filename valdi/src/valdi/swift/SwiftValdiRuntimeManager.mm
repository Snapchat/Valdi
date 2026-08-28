#import "SwiftValdiRuntimeManager.h"
#import "valdi/ios/SCValdiRuntimeManager.h"
#import "valdi_core/SCValdiRuntimeManagerProtocol.h"

@interface SwiftValdiRuntimeManager ()
@property (nonatomic, strong) SCValdiRuntimeManager* runtimeManager;
@end

@implementation SwiftValdiRuntimeManager

- (id<SCValdiRuntimeManagerProtocol>)createRuntimeManager {
    if (!self.runtimeManager) {
        self.runtimeManager = [SCValdiRuntimeManager new];
        [self.runtimeManager updateConfiguration:^(SCValdiConfiguration* configuration) {
            // Preserve Snap's default-on development-host behavior; direct hosts can opt out through configuration.
            configuration.enableDebuggerService = YES;
        }];
    }
    return self.runtimeManager;
}

- (id<SCValdiRuntimeProtocol>)createRuntime {
    return self.runtimeManager.mainRuntime;
}

@end
