//
//  SCValdiSharedLogger.h
//  valdi-ios
//
//  Created by Simon Corsin on 4/1/19.
//

#import "valdi_core/SCMacros.h"
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, SCValdiLoggerLevel) {
    SCValdiLoggerLevelVerbose,
    SCValdiLoggerLevelDebug,
    SCValdiLoggerLevelInfo,
    SCValdiLoggerLevelWarn,
    SCValdiLoggerLevelError,
};

@protocol SCValdiLogger <NSObject>

- (BOOL)isLogEnabledForLevel:(SCValdiLoggerLevel)level;

- (void)outputLog:(NSString*)log forLevel:(SCValdiLoggerLevel)level;

@end

SC_EXTERN_C_BEGIN

extern __nullable id<SCValdiLogger> SCValdiGetSharedLogger();
extern void SCValdiSetSharedLogger(__nullable id<SCValdiLogger> logger);

SC_EXTERN_C_END

NS_ASSUME_NONNULL_END
