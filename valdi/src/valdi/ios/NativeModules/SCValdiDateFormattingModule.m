//
//  SCValdiDateFormattingModule.m
//  Valdi
//

#import "valdi/ios/NativeModules/SCValdiDateFormattingModule.h"
#import "valdi/ios/NativeModules/SCValdiBridgeModuleUtils.h"

static NSString *const SCValdiDateFormattingDefaultFormat = @"MEDIUM";

@implementation SCValdiDateFormattingModule {
    NSLocale *_locale;
    BOOL (^_isEnabled)(void);
    NSMutableDictionary<NSString *, NSDateFormatter *> *_formatters;
}

- (instancetype)initWithIsEnabled:(BOOL (^)(void))isEnabled
{
    return [self initWithLocale:[NSLocale autoupdatingCurrentLocale] isEnabled:isEnabled];
}

- (instancetype)initWithLocale:(NSLocale *)locale isEnabled:(BOOL (^)(void))isEnabled
{
    self = [super init];
    if (self) {
        _locale = locale;
        _isEnabled = [isEnabled copy];
        _formatters = [NSMutableDictionary dictionary];
        // Template-based formats are resolved when the formatter is configured, so a locale or 24-hour change
        // requires rebuilding them.
        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(_currentLocaleDidChange)
                                                   name:NSCurrentLocaleDidChangeNotification
                                                 object:nil];
    }
    return self;
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)_currentLocaleDidChange
{
    @synchronized(self) {
        [_formatters removeAllObjects];
    }
}

static NSDateFormatter *SCValdiMakeDateFormatter(NSString *format, NSLocale *locale)
{
    NSDateFormatter *formatter = [NSDateFormatter new];
    formatter.locale = locale;

    if ([format isEqualToString:@"SHORT"]) {
        // EN: 4/20/21 -> FR: 20/04/2021
        formatter.dateStyle = NSDateFormatterShortStyle;
    } else if ([format isEqualToString:@"SHORT_NO_YEAR"]) {
        // EN: 4/20 -> FR: 20/04
        [formatter setLocalizedDateFormatFromTemplate:@"Md"];
    } else if ([format isEqualToString:@"MEDIUM_NO_YEAR"]) {
        // EN: Apr 20 -> FR: 20 avr.
        [formatter setLocalizedDateFormatFromTemplate:@"MMMd"];
    } else if ([format isEqualToString:@"LONG"]) {
        // EN: April 20, 2021 -> FR: 20 avril 2021
        formatter.dateStyle = NSDateFormatterLongStyle;
    } else if ([format isEqualToString:@"LONG_NO_YEAR"]) {
        // EN: April 20 -> FR: 20 avril
        [formatter setLocalizedDateFormatFromTemplate:@"MMMMd"];
    } else if ([format isEqualToString:@"LONG_NO_DAY"]) {
        // EN: April 2021 -> FR: avril 2021
        [formatter setLocalizedDateFormatFromTemplate:@"MMMMy"];
    } else if ([format isEqualToString:@"DAY_OF_WEEK"]) {
        // EN: Tuesday -> FR: mardi
        [formatter setLocalizedDateFormatFromTemplate:@"EEEE"];
    } else if ([format isEqualToString:@"DAY_OF_WEEK_SHORT"]) {
        // EN: Tue -> FR: mar.
        [formatter setLocalizedDateFormatFromTemplate:@"EEE"];
    } else if ([format isEqualToString:@"TIME"]) {
        // EN: 5:33 PM -> FR: 17:33, and 17:33 in EN when 24-Hour Time is on
        formatter.timeStyle = NSDateFormatterShortStyle;
    } else {
        // MEDIUM and unknown formats. EN: Apr 20, 2021 -> FR: 20 avr. 2021
        formatter.dateStyle = NSDateFormatterMediumStyle;
    }
    return formatter;
}

- (NSString *)formatEpochMillis:(double)epochMillis format:(NSString *)format timeZoneName:(NSString *)timeZoneName
{
    NSTimeZone *timeZone = timeZoneName ? [NSTimeZone timeZoneWithName:timeZoneName] : nil;
    NSDate *date = [NSDate dateWithTimeIntervalSince1970:epochMillis / (double)MSEC_PER_SEC];
    NSTimeZone *targetTimeZone = timeZone ?: [NSTimeZone localTimeZone];
    // Formatters are shared across JS threads and their timeZone is mutated per call.
    @synchronized(self) {
        NSDateFormatter *formatter = _formatters[format];
        if (!formatter) {
            formatter = SCValdiMakeDateFormatter(format, _locale);
            _formatters[format] = formatter;
        }
        // Setting timeZone resets the formatter's cached ICU state even when the zone is unchanged.
        if (![formatter.timeZone isEqual:targetTimeZone]) {
            formatter.timeZone = targetTimeZone;
        }
        return [formatter stringFromDate:date] ?: @"";
    }
}

- (void)_formatDate:(SCValdiMarshaller *)marshaller
{
    double epochMillis = SCValdiMarshallerGetDouble(marshaller, 0);
    NSString *format = SCValdiMarshallerIsString(marshaller, 1) ? SCValdiMarshallerGetString(marshaller, 1)
                                                                 : SCValdiDateFormattingDefaultFormat;
    NSString *timeZoneName = SCValdiMarshallerIsString(marshaller, 2) ? SCValdiMarshallerGetString(marshaller, 2) : nil;
    SCValdiMarshallerPushString(marshaller, [self formatEpochMillis:epochMillis format:format timeZoneName:timeZoneName]);
}

- (NSString *)getModulePath
{
    return @"DateFormatting";
}

- (NSObject *)loadModule
{
    if (!_isEnabled()) {
        return @{};
    }
    return @{
        @"formatDate" : BRIDGE_METHOD(_formatDate),
    };
}

@end
