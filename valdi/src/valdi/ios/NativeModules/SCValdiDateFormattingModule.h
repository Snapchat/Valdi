//
//  SCValdiDateFormattingModule.h
//  Valdi
//

#import "valdi_core/SCNValdiCoreModuleFactory.h"

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * Formats dates with NSDateFormatter so Valdi output follows the user's iOS settings (e.g. the 24-Hour Time toggle),
 * which the JS runtime's locale-only formatting ignores. iOS counterpart of ValdiDateFormattingModule on Android.
 */
@interface SCValdiDateFormattingModule : NSObject <SCNValdiCoreModuleFactory>

/**
 * Formats with the user's current locale and settings. `isEnabled` is read when JS first loads the module; when it
 * returns NO the module exports nothing, so JS keeps its own formatter.
 */
- (instancetype)initWithIsEnabled:(BOOL (^)(void))isEnabled;

- (instancetype)initWithLocale:(NSLocale*)locale isEnabled:(BOOL (^)(void))isEnabled NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

/**
 * Formats a date the way the JS `DateFormatting.formatDate` bridge does. Unknown formats fall back to MEDIUM, and a nil
 * or unknown time zone falls back to the local one.
 */
- (NSString*)formatEpochMillis:(double)epochMillis
                        format:(NSString*)format
                  timeZoneName:(nullable NSString*)timeZoneName;

@end

NS_ASSUME_NONNULL_END
