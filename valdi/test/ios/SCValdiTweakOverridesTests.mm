//
//  SCValdiTweakOverridesTests.mm
//  ios_tests
//
//  Pins SCValdiConfiguration.tweakOverrides end to end: values set on the configuration reach the
//  runtime's tweak getters, whether they are NSNumber, BOOL or NSString.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "valdi/ios/SCValdiRuntime+Private.h"
#import "valdi/ios/SCValdiRuntimeManager.h"
#import "valdi_core/SCValdiConfiguration.h"

#include "valdi/runtime/Resources/ResourceManager.hpp"
#include "valdi/runtime/Runtime.hpp"

@interface SCValdiTweakOverridesTests : XCTestCase
@end

@implementation SCValdiTweakOverridesTests

- (BOOL)deferredGCEnabledWithOverrides:(NSDictionary<NSString *, id> *)overrides
{
    SCValdiRuntimeManager *manager = [SCValdiRuntimeManager new];
    [manager updateConfiguration:^(SCValdiConfiguration *configuration) {
        configuration.tweakOverrides = overrides;
    }];
    SCValdiRuntime *runtime = (SCValdiRuntime *)manager.mainRuntime;
    XCTAssertNotNil(runtime);
    return runtime.cppInstance->getResourceManager().enableDeferredGC();
}

- (void)testNoOverridesLeavesTheDefault
{
    XCTAssertFalse([self deferredGCEnabledWithOverrides:nil]);
}

- (void)testBoolOverrideReachesTheRuntime
{
    XCTAssertTrue([self deferredGCEnabledWithOverrides:@{@"VALDI_ENABLE_DEFERRED_GC" : @YES}]);
    XCTAssertFalse([self deferredGCEnabledWithOverrides:@{@"VALDI_ENABLE_DEFERRED_GC" : @NO}]);
}

- (void)testStringOverrideReachesTheRuntime
{
    XCTAssertTrue([self deferredGCEnabledWithOverrides:@{@"VALDI_ENABLE_DEFERRED_GC" : @"true"}]);
    XCTAssertFalse([self deferredGCEnabledWithOverrides:@{@"VALDI_ENABLE_DEFERRED_GC" : @"0"}]);
}

- (void)testNumericOverrideIsReadAsABool
{
    XCTAssertTrue([self deferredGCEnabledWithOverrides:@{@"VALDI_ENABLE_DEFERRED_GC" : @1}]);
}

- (void)testUnrelatedKeysDoNotEnableTheTweak
{
    XCTAssertFalse([self deferredGCEnabledWithOverrides:@{@"VALDI_SOMETHING_ELSE" : @YES}]);
}

@end
