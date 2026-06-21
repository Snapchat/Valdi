//
//  SCValdiCustomUnderlineStyle.m
//  Valdi
//

#import "valdi/ios/Text/SCValdiCustomUnderlineStyle.h"

#include <math.h>

static NSString *const SCValdiCustomUnderlineStyleErrorDomain = @"SCValdiCustomUnderlineStyleErrorDomain";

static void SCValdiCustomUnderlineStyleSetError(NSError **error, NSString *message)
{
    if (error) {
        *error = [NSError errorWithDomain:SCValdiCustomUnderlineStyleErrorDomain
                                     code:0
                                 userInfo:@{NSLocalizedDescriptionKey: message}];
    }
}

static BOOL SCValdiCustomUnderlineStyleScanNumber(NSScanner *scanner, double *value, NSError **error)
{
    if (![scanner scanDouble:value]) {
        SCValdiCustomUnderlineStyleSetError(error, @"Invalid customUnderlineStyle number");
        return NO;
    }

    if (!isfinite(*value)) {
        SCValdiCustomUnderlineStyleSetError(error, @"customUnderlineStyle values must be finite numbers");
        return NO;
    }

    return YES;
}

@implementation SCValdiCustomUnderlineStyle

- (instancetype)initWithHeight:(CGFloat)height
                       onWidth:(CGFloat)onWidth
                      offWidth:(CGFloat)offWidth
                        offset:(CGFloat)offset
{
    self = [super init];
    if (self) {
        _height = height;
        _onWidth = onWidth;
        _offWidth = offWidth;
        _offset = offset;
    }
    return self;
}

- (BOOL)isPatterned
{
    return _onWidth > 0 && _offWidth > 0;
}

+ (instancetype)styleWithString:(NSString *)styleString error:(NSError **)error
{
    if (error) {
        *error = nil;
    }

    NSScanner *scanner = [NSScanner scannerWithString:styleString];
    scanner.charactersToBeSkipped = [NSCharacterSet whitespaceAndNewlineCharacterSet];

    double height = 0;
    double onWidth = 0;
    double offWidth = 0;
    double offset = 0;
    if (!SCValdiCustomUnderlineStyleScanNumber(scanner, &height, error)
        || !SCValdiCustomUnderlineStyleScanNumber(scanner, &onWidth, error)
        || !SCValdiCustomUnderlineStyleScanNumber(scanner, &offWidth, error)
        || !SCValdiCustomUnderlineStyleScanNumber(scanner, &offset, error)) {
        return nil;
    }

    if (!scanner.isAtEnd) {
        SCValdiCustomUnderlineStyleSetError(
            error, @"customUnderlineStyle must contain exactly four numbers: height onWidth offWidth offset");
        return nil;
    }

    if (height <= 0) {
        SCValdiCustomUnderlineStyleSetError(error, @"customUnderlineStyle height must be positive");
        return nil;
    }

    BOOL solid = onWidth == 0 && offWidth == 0;
    BOOL patterned = onWidth > 0 && offWidth > 0;
    if (!solid && !patterned) {
        SCValdiCustomUnderlineStyleSetError(
            error, @"customUnderlineStyle onWidth and offWidth must both be positive, or both be 0");
        return nil;
    }

    return [[self alloc] initWithHeight:height onWidth:onWidth offWidth:offWidth offset:offset];
}

@end
