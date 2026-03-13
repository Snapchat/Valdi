#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol SCValdiExceptionReporter <NSObject>
/**
 * Reports a non-fatal error.
 *
 * @param errorCode The error code associated with the error.
 * @param message The error message.
 * @param module The module where the error occurred. Can be nil if not applicable or module is unknown.
 * @param stackTrace The stack trace of the error. Can be nil if stack trace could not be obtained. See comment above
 * for format.
 */
- (void)reportNonFatalWithErrorCode:(NSInteger)errorCode
                            message:(NSString*)message
                             module:(nullable NSString*)module
                         stackTrace:(nullable NSString*)stackTrace;

/**
 * Reports a crash.
 *
 * @param message The crash message.
 * @param module The module where the crash occurred. Can be nil if not applicable or module is unknown.
 * @param stackTrace The stack trace of the crash. Can be nil if stack trace could not be obtained. See comment above
 * for format.
 * @param isANR Indicates if the crash is an Application Not Responding (ANR) error.
 */
- (void)reportCrashWithMessage:(NSString*)message
                        module:(nullable NSString*)module
                    stackTrace:(nullable NSString*)stackTrace
                         isANR:(BOOL)isANR;
@end

NS_ASSUME_NONNULL_END
