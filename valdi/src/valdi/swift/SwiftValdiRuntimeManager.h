
#import "valdi_core/SCValdiRuntimeManagerProtocol.h"

@interface SwiftValdiRuntimeManager : NSObject

- (nonnull id<SCValdiRuntimeManagerProtocol>)createRuntimeManager;

- (nullable id<SCValdiRuntimeProtocol>)createRuntime;

@end
