#import <XCTest/XCTest.h>
#import "valdi/ios/Text/SCValdiFontManager.h"
#import "valdi_core/SCValdiFontLoaderProtocol.h"

@interface CrashingFontLoader : NSObject <SCValdiFontLoaderProtocol>
@property (nonatomic, assign) BOOL wasCalled;
@end

@implementation CrashingFontLoader

- (UIFont *)loadFontWithName:(NSString *)fontName fontSize:(CGFloat)fontSize
{
    self.wasCalled = YES;
    NSAssert(NO, @"SCStandardCachedFont would SCAssert here for unknown font: %@", fontName);
    return nil;
}

- (UIFont *)loadFontWithName:(NSString *)fontName
                    fontSize:(CGFloat)fontSize
            legibilityWeight:(SCUILegibilityWeight)legibilityWeight
{
    self.wasCalled = YES;
    NSAssert(NO, @"SCStandardCachedFont would SCAssert here for unknown font: %@", fontName);
    return nil;
}

- (BOOL)shouldBypassContextForLegibilityWeight
{
    return YES;
}

@end

@interface TrackingFontLoader : NSObject <SCValdiFontLoaderProtocol>
@property (nonatomic, assign) BOOL wasCalled;
@end

@implementation TrackingFontLoader

- (UIFont *)loadFontWithName:(NSString *)fontName fontSize:(CGFloat)fontSize
{
    self.wasCalled = YES;
    return [UIFont fontWithName:fontName size:fontSize];
}

- (UIFont *)loadFontWithName:(NSString *)fontName
                    fontSize:(CGFloat)fontSize
            legibilityWeight:(SCUILegibilityWeight)legibilityWeight
{
    self.wasCalled = YES;
    return [UIFont fontWithName:fontName size:fontSize];
}

- (BOOL)shouldBypassContextForLegibilityWeight
{
    return YES;
}

@end

@interface NilReturningFontLoader : NSObject <SCValdiFontLoaderProtocol>
@property (nonatomic, assign) BOOL wasCalled;
@end

@implementation NilReturningFontLoader

- (UIFont *)loadFontWithName:(NSString *)fontName fontSize:(CGFloat)fontSize
{
    self.wasCalled = YES;
    return nil;
}

- (UIFont *)loadFontWithName:(NSString *)fontName
                    fontSize:(CGFloat)fontSize
            legibilityWeight:(SCUILegibilityWeight)legibilityWeight
{
    self.wasCalled = YES;
    return nil;
}

- (BOOL)shouldBypassContextForLegibilityWeight
{
    return YES;
}

@end

@interface SCValdiFontManagerTests : XCTestCase
@end

@implementation SCValdiFontManagerTests

- (void)testUnknownFontSkipsLoaderAndReturnsSystemFont
{
    SCValdiFontManager *fontManager = [SCValdiFontManager new];
    CrashingFontLoader *loader = [CrashingFontLoader new];
    [fontManager setFontLoader:loader];

    UIFont *font = [fontManager fontWithName:@"Montserrat-SemiBold"
                                    fontSize:12
                            legibilityWeight:SCUILegibilityWeightRegular];

    XCTAssertNotNil(font, @"Should return a font, not nil");
    XCTAssertFalse(loader.wasCalled, @"Loader should not be called for unknown font names");
    XCTAssertEqualWithAccuracy(font.pointSize, 12, 0.01);
}

- (void)testUnknownFontWithBoldLegibilitySkipsLoader
{
    SCValdiFontManager *fontManager = [SCValdiFontManager new];
    CrashingFontLoader *loader = [CrashingFontLoader new];
    [fontManager setFontLoader:loader];

    UIFont *font = [fontManager fontWithName:@"RobotoMono-Bold"
                                    fontSize:14
                            legibilityWeight:SCUILegibilityWeightBold];

    XCTAssertNotNil(font, @"Should return a font, not nil");
    XCTAssertFalse(loader.wasCalled, @"Loader should not be called for unknown font names");
}

- (void)testKnownSystemFontStillWorks
{
    SCValdiFontManager *fontManager = [SCValdiFontManager new];

    UIFont *font = [fontManager fontWithName:@"system"
                                    fontSize:16
                            legibilityWeight:SCUILegibilityWeightRegular];

    XCTAssertNotNil(font);
    XCTAssertEqualWithAccuracy(font.pointSize, 16, 0.01);
}

- (void)testSystemBoldFontStillWorks
{
    SCValdiFontManager *fontManager = [SCValdiFontManager new];

    UIFont *font = [fontManager fontWithName:@"system-bold"
                                    fontSize:18
                            legibilityWeight:SCUILegibilityWeightRegular];

    XCTAssertNotNil(font);
    XCTAssertEqualWithAccuracy(font.pointSize, 18, 0.01);
}

- (void)testKnownFontCallsLoader
{
    SCValdiFontManager *fontManager = [SCValdiFontManager new];
    TrackingFontLoader *loader = [TrackingFontLoader new];
    [fontManager setFontLoader:loader];

    UIFont *font = [fontManager fontWithName:@"Helvetica"
                                    fontSize:12
                            legibilityWeight:SCUILegibilityWeightRegular];

    XCTAssertNotNil(font);
    XCTAssertTrue(loader.wasCalled, @"Loader should be called for known font names");
}

- (void)testKnownFontFallsBackWhenLoaderReturnsNil
{
    SCValdiFontManager *fontManager = [SCValdiFontManager new];
    NilReturningFontLoader *loader = [NilReturningFontLoader new];
    [fontManager setFontLoader:loader];

    UIFont *font = [fontManager fontWithName:@"Helvetica"
                                    fontSize:14
                            legibilityWeight:SCUILegibilityWeightRegular];

    XCTAssertNotNil(font, @"Should fall back to UIFont when loader returns nil");
    XCTAssertTrue(loader.wasCalled);
    XCTAssertEqualWithAccuracy(font.pointSize, 14, 0.01);
}

@end
