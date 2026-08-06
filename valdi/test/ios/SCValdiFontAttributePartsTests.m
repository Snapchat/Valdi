//
//  SCValdiFontAttributePartsTests.m
//  Valdi
//
//  Guards the composite font-attribute part lists against duplicate or reordered entries.
//
//  `fontAttributesWithCompositeValue:` and `fontAttributesWithGrowableCompositeValue:` read the
//  incoming value array by hardcoded index, so the part list and those readers have to agree
//  positionally. Nothing enforced that, and they silently disagreed: `numberOfLines` appeared twice in
//  `valdiFontAttributes` — once from `valdiFontAttributesGrowable` and once appended manually — which
//  pushed `textOverflow` to index 9 while the reader still read index 8. `textOverflow` therefore
//  resolved to an Int, `ObjectAs(..., NSString)` returned nil, and iOS text truncation stopped working
//  with no error anywhere.
//
//  The arity check in the reader could not catch it, because the part count and the value count both
//  grew to 10 together. Only the positions were wrong.
//
//  These assertions are about list shape, not rendering, so they are cheap and run without a host app
//  — unlike the pixel coverage that would otherwise be needed to notice truncation breaking.
//

#import "valdi/ios/Text/NSAttributedString+Valdi.h"
#import "valdi_core/SCNValdiCoreCompositeAttributePart.h"

#import <XCTest/XCTest.h>

// The attribute-name constants are file-private statics in NSAttributedString+Valdi.m, so match on the
// wire name. That is the name the composite value is keyed by anyway, which is what matters here.
static NSString *const kValdiTextOverflowAttributeName = @"textOverflow";

@interface SCValdiFontAttributePartsTests : XCTestCase
@end

@implementation SCValdiFontAttributePartsTests

- (NSArray<NSString *> *)_attributeNames:(NSArray<SCNValdiCoreCompositeAttributePart *> *)parts
{
    NSMutableArray<NSString *> *names = [NSMutableArray arrayWithCapacity:parts.count];
    for (SCNValdiCoreCompositeAttributePart *part in parts) {
        [names addObject:part.attribute];
    }
    return names;
}

- (void)testFontAttributePartsContainNoDuplicates
{
    NSArray<NSString *> *names = [self _attributeNames:[NSAttributedString valdiFontAttributes]];
    NSCountedSet *counted = [NSCountedSet setWithArray:names];

    NSMutableArray<NSString *> *duplicates = [NSMutableArray array];
    for (NSString *name in counted) {
        if ([counted countForObject:name] > 1) {
            [duplicates addObject:name];
        }
    }

    XCTAssertEqualObjects(duplicates,
                          @[],
                          @"valdiFontAttributes must not repeat an attribute. A duplicate shifts every "
                          @"later part by one while the hardcoded index reads in "
                          @"fontAttributesWithCompositeValue: stay put, so an attribute silently "
                          @"resolves to the wrong value. Full order: %@",
                          names);
}

- (void)testGrowableFontAttributePartsContainNoDuplicates
{
    NSArray<NSString *> *names = [self _attributeNames:[NSAttributedString valdiFontAttributesGrowable]];
    XCTAssertEqual([NSSet setWithArray:names].count,
                   names.count,
                   @"valdiFontAttributesGrowable must not repeat an attribute. Order: %@",
                   names);
}

- (void)testGrowablePartsArePrefixOfFullParts
{
    NSArray<NSString *> *growable = [self _attributeNames:[NSAttributedString valdiFontAttributesGrowable]];
    NSArray<NSString *> *full = [self _attributeNames:[NSAttributedString valdiFontAttributes]];

    // Return rather than fall through: XCTAssert does not stop execution, and subarrayWithRange: would
    // raise NSRangeException on a short array, which reads as a crash rather than a failed assertion.
    if (growable.count > full.count) {
        XCTFail(@"valdiFontAttributesGrowable (%lu) cannot be longer than valdiFontAttributes (%lu)",
                (unsigned long)growable.count,
                (unsigned long)full.count);
        return;
    }

    // valdiFontAttributes is built by copying the growable list and appending, so the shared prefix
    // must line up index-for-index. Both readers rely on that: they use identical indices for every
    // attribute the two lists have in common.
    XCTAssertEqualObjects([full subarrayWithRange:NSMakeRange(0, growable.count)],
                          growable,
                          @"valdiFontAttributes must begin with valdiFontAttributesGrowable, in order");
}

- (void)testTextOverflowIsTheLastFullPart
{
    NSArray<NSString *> *full = [self _attributeNames:[NSAttributedString valdiFontAttributes]];

    // fontAttributesWithCompositeValue: reads textOverflow at the index one past the growable list,
    // i.e. the final entry. Pinning it here means adding a part before it fails loudly rather than
    // quietly making textOverflow unreadable.
    XCTAssertEqualObjects(full.lastObject,
                          kValdiTextOverflowAttributeName,
                          @"textOverflow must remain the final part. Order: %@",
                          full);
}

@end
