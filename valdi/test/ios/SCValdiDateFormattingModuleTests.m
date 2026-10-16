#import <XCTest/XCTest.h>

#import "valdi/ios/NativeModules/SCValdiDateFormattingModule.h"

// 2021-04-20 17:33:00 UTC
static const double SCValdiTestEpochMillis = 1618939980000.0;

@interface SCValdiDateFormattingModuleTests : XCTestCase
@end

@implementation SCValdiDateFormattingModuleTests

- (SCValdiDateFormattingModule *)moduleWithLocale:(NSString *)identifier
{
    return [[SCValdiDateFormattingModule alloc] initWithLocale:[NSLocale localeWithLocaleIdentifier:identifier]
                                                     isEnabled:^BOOL {
                                                         return YES;
                                                     }];
}

- (void)testModulePathMatchesJSLoader
{
    XCTAssertEqualObjects([[self moduleWithLocale:@"en_US"] getModulePath], @"DateFormatting");
}

- (void)testLoadModuleExportsFormatDateWhenEnabled
{
    NSDictionary *module = (NSDictionary *)[[self moduleWithLocale:@"en_US"] loadModule];
    XCTAssertNotNil(module[@"formatDate"]);
}

// JS treats a missing formatDate as "no native formatter" and keeps its own fallback.
- (void)testLoadModuleExportsNothingWhenDisabled
{
    SCValdiDateFormattingModule *module = [[SCValdiDateFormattingModule alloc] initWithIsEnabled:^BOOL {
        return NO;
    }];
    XCTAssertEqualObjects((NSDictionary *)[module loadModule], @{});
}

- (void)testTimeUses12HourClockForUSLocale
{
    NSString *formatted = [[self moduleWithLocale:@"en_US"] formatEpochMillis:SCValdiTestEpochMillis
                                                                       format:@"TIME"
                                                                 timeZoneName:@"UTC"];
    XCTAssertTrue([formatted containsString:@"5:33"], @"%@", formatted);
    XCTAssertTrue([formatted containsString:@"PM"], @"%@", formatted);
}

// The iOS 24-Hour Time toggle surfaces to NSDateFormatter as the locale's hour cycle.
- (void)testTimeHonors24HourCycleOverride
{
    NSString *formatted = [[self moduleWithLocale:@"en_US@hours=h23"] formatEpochMillis:SCValdiTestEpochMillis
                                                                                 format:@"TIME"
                                                                           timeZoneName:@"UTC"];
    XCTAssertEqualObjects(formatted, @"17:33");
}

- (void)testTimeUses24HourClockForFrenchLocale
{
    NSString *formatted = [[self moduleWithLocale:@"fr_FR"] formatEpochMillis:SCValdiTestEpochMillis
                                                                       format:@"TIME"
                                                                 timeZoneName:@"UTC"];
    XCTAssertEqualObjects(formatted, @"17:33");
}

- (void)testTimeZoneIsApplied
{
    NSString *formatted = [[self moduleWithLocale:@"fr_FR"] formatEpochMillis:SCValdiTestEpochMillis
                                                                       format:@"TIME"
                                                                 timeZoneName:@"Asia/Tokyo"];
    XCTAssertEqualObjects(formatted, @"02:33");
}

- (void)testDateFormatsForUSLocale
{
    SCValdiDateFormattingModule *module = [self moduleWithLocale:@"en_US"];
    NSDictionary<NSString *, NSString *> *expected = @{
        @"SHORT" : @"4/20/21",
        @"SHORT_NO_YEAR" : @"4/20",
        @"MEDIUM" : @"Apr 20, 2021",
        @"MEDIUM_NO_YEAR" : @"Apr 20",
        @"LONG" : @"April 20, 2021",
        @"LONG_NO_YEAR" : @"April 20",
        @"LONG_NO_DAY" : @"April 2021",
        @"DAY_OF_WEEK" : @"Tuesday",
        @"DAY_OF_WEEK_SHORT" : @"Tue",
    };
    [expected enumerateKeysAndObjectsUsingBlock:^(NSString *format, NSString *value, BOOL *stop) {
        XCTAssertEqualObjects([module formatEpochMillis:SCValdiTestEpochMillis format:format timeZoneName:@"UTC"],
                              value,
                              @"%@",
                              format);
    }];
}

- (void)testUnknownFormatFallsBackToMedium
{
    NSString *formatted = [[self moduleWithLocale:@"en_US"] formatEpochMillis:SCValdiTestEpochMillis
                                                                       format:@"NOT_A_FORMAT"
                                                                 timeZoneName:@"UTC"];
    XCTAssertEqualObjects(formatted, @"Apr 20, 2021");
}

@end
