#import <XCTest/XCTest.h>
#import "valdi_core/SCValdiBigInt.h"
#import "valdi_core/SCValdiObjCConversionUtils.h"
#include "valdi_core/cpp/Utils/BigInt.hpp"

@interface SCValdiBigIntTests : XCTestCase
@end

@implementation SCValdiBigIntTests

- (void)testExactConversionsAndNativeRoundTrip
{
    SCValdiBigInt *small = [[SCValdiBigInt alloc] initWithInt64:INT64_MIN];
    XCTAssertTrue(small.toInt64.success);
    XCTAssertEqual(INT64_MIN, small.toInt64.value);
    XCTAssertEqualObjects(small.toString, @"-9223372036854775808");
    NSError *error = nil;
    SCValdiBigInt *large = [[SCValdiBigInt alloc] initWithString:@"+00018446744073709551616" error:&error];
    XCTAssertNotNil(large);
    XCTAssertNil(error);
    XCTAssertFalse(large.toInt64.success);
    XCTAssertEqual(0, large.toInt64.value);
    XCTAssertEqualObjects(large.toString, @"18446744073709551616");
    auto value = ValdiIOS::ValueFromNSObject(large);
    XCTAssertTrue(value.isBigInt());
    SCValdiBigInt *copy = ValdiIOS::NSObjectFromValue(value);
    XCTAssertTrue([large bigInt] == [copy bigInt]);
    XCTAssertNil([[SCValdiBigInt alloc] initWithString:@"1.5" error:&error]);
    XCTAssertNotNil(error);
    XCTAssertNil([[SCValdiBigInt alloc] initWithString:@"1.5" error:nil]);
}

- (void)testEqualityAndHashUseTheNumericValue
{
    SCValdiBigInt *small = [[SCValdiBigInt alloc] initWithInt64:42];
    SCValdiBigInt *sameSmall = [[SCValdiBigInt alloc] initWithString:@"+0042" error:nil];
    XCTAssertEqualObjects(small, sameSmall);
    XCTAssertEqual(small.hash, sameSmall.hash);
    XCTAssertNotEqualObjects(small, @42);

    SCValdiBigInt *large = [[SCValdiBigInt alloc] initWithString:@"18446744073709551616" error:nil];
    SCValdiBigInt *sameLarge = [[SCValdiBigInt alloc] initWithString:@"+0018446744073709551616" error:nil];
    SCValdiBigInt *negative = [[SCValdiBigInt alloc] initWithString:@"-18446744073709551616" error:nil];
    XCTAssertEqualObjects(large, sameLarge);
    XCTAssertEqual(large.hash, sameLarge.hash);
    XCTAssertNotEqualObjects(large, negative);
    NSSet *values = [NSSet setWithObjects:small, large, nil];
    XCTAssertTrue([values containsObject:sameLarge]);
}

- (void)testBridgeRetainsNativeValue
{
    auto native = Valdi::BigInt::make(INT64_MAX);
    const auto initialRetainCount = native->retainCount();
    @autoreleasepool {
        SCValdiBigInt *wrapper = [[SCValdiBigInt alloc] initWithBigInt:Valdi::unsafeBridgeCast(native.get())];
        XCTAssertEqual(initialRetainCount + 1, native->retainCount());
        XCTAssertEqual(Valdi::unsafeBridgeCast(native.get()), wrapper.bigInt);
        XCTAssertTrue(wrapper.toInt64.success);
        XCTAssertEqual(INT64_MAX, wrapper.toInt64.value);
    }
    XCTAssertEqual(initialRetainCount, native->retainCount());
}

@end
