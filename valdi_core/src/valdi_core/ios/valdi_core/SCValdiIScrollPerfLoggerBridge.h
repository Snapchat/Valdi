#import <Foundation/Foundation.h>

/**
 * Represents a configured instance of the platform-specific
 * attributed scroll view performance logger.
 * Normally provided by a SCValdiIScrollPerfLoggerBridgeFactory.
 *
 * @see: SCValdiIScrollPerfLoggerBridgeFactory
 */
@protocol SCValdiIScrollPerfLoggerBridge <NSObject>

- (void)resume;
- (void)pauseAndCancelLogging:(BOOL)cancelLogging;

@end
