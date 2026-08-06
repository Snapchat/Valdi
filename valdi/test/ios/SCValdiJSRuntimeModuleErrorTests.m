//
//  SCValdiJSRuntimeModuleErrorTests.m
//  ios_tests
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "valdi/ios/SCValdiRuntimeManager.h"
#import "valdi_core/SCValdiError.h"
#import "valdi_core/SCValdiJSRuntime.h"
#import "valdi_core/SCValdiMarshaller.h"

/// Pins the two halves of the SCValdiJSRuntime module-resolution contract:
/// pushModuleAthPath:inMarshaller: raises for Objective-C callers, while
/// pushModuleAtPath:reportingErrorOnMarshaller: never raises so that Swift callers (which cannot
/// catch NSException) can observe the failure through the marshaller instead.
@interface SCValdiJSRuntimeModuleErrorTests: XCTestCase

@property (strong, nonatomic) SCValdiRuntimeManager *runtimeManager;

@end

@implementation SCValdiJSRuntimeModuleErrorTests

static NSString *const kUnresolvableModulePath = @"__no_such_bundle__/__no_such_module__";

- (void)setUp
{
    self.runtimeManager = [SCValdiRuntimeManager new];
    self.continueAfterFailure = NO;
}

- (void)tearDown
{
    self.runtimeManager = nil;
}

/// Runs the block on the JS thread, which is where generated bridge-function resolution happens.
- (void)withJSRuntime:(void (^)(id<SCValdiJSRuntime> jsRuntime))block
{
    id<SCValdiRuntimeProtocol> runtime = self.runtimeManager.mainRuntime;
    XCTAssertNotNil(runtime);

    id<SCValdiJSRuntime> jsRuntime = [runtime jsRuntime];
    XCTAssertNotNil(jsRuntime);

    XCTestExpectation *expectation = [self expectationWithDescription:@"JS thread block ran"];
    [jsRuntime dispatchInJsThread:^{
        block(jsRuntime);
        [expectation fulfill];
    }];
    [self waitForExpectations:@[expectation] timeout:10.0];
}

- (void)testReportingVariantLeavesErrorOnMarshallerInsteadOfRaising
{
    [self withJSRuntime:^(id<SCValdiJSRuntime> jsRuntime) {
        SCValdiMarshallerScoped(marshaller, {
            @try {
                (void)[jsRuntime pushModuleAtPath:kUnresolvableModulePath reportingErrorOnMarshaller:marshaller];
            } @catch (NSException *exception) {
                XCTFail(@"pushModuleAtPath:reportingErrorOnMarshaller: must not raise, got %@: %@",
                        exception.name, exception.reason);
                return;
            }

            // The error must still be pending on the marshaller: this is the only channel a Swift
            // caller has, and SCValdiMarshallerCheck consumes it, so it must not have run yet.
            @try {
                SCValdiMarshallerCheck(marshaller);
                XCTFail(@"Expected an unresolvable module path to leave an error on the marshaller");
            } @catch (SCValdiError *error) {
                XCTAssertNotNil(error.reason);
            }
        })
    }];
}

- (void)testLegacyVariantStillRaisesForObjCCallers
{
    [self withJSRuntime:^(id<SCValdiJSRuntime> jsRuntime) {
        SCValdiMarshallerScoped(marshaller, {
            @try {
                (void)[jsRuntime pushModuleAthPath:kUnresolvableModulePath inMarshaller:marshaller];
                XCTFail(@"Expected pushModuleAthPath:inMarshaller: to raise for an unresolvable module path");
            } @catch (SCValdiError *error) {
                XCTAssertNotNil(error.reason);
            }
        })
    }];
}

- (void)testReportingVariantSucceedsForResolvableModule
{
    [self withJSRuntime:^(id<SCValdiJSRuntime> jsRuntime) {
        SCValdiMarshallerScoped(marshaller, {
            NSInteger index = [jsRuntime pushModuleAtPath:@"valdi_test/src/FunctionTest"
                               reportingErrorOnMarshaller:marshaller];
            @try {
                SCValdiMarshallerCheck(marshaller);
            } @catch (SCValdiError *error) {
                XCTFail(@"Expected a resolvable module path to leave no error, got %@", error.reason);
                return;
            }
            XCTAssertGreaterThanOrEqual(index, 0);
        })
    }];
}

@end
