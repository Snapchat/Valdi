//
//  SCValdiJSRuntimeModuleErrorTests.m
//  ios_tests
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import <SCCValdiTest/SCCValdiTest.h>

#import "valdi/ios/SCValdiRuntimeManager.h"
#import "valdi_core/SCValdiBridgeFunction.h"
#import "valdi_core/SCValdiError.h"
#import "valdi_core/SCValdiJSRuntime.h"
#import "valdi_core/SCValdiMarshaller.h"

/// Pins the two halves of the SCValdiJSRuntime module-resolution contract:
/// pushModuleAthPath:inMarshaller: raises for Objective-C callers, while
/// pushModuleAtPath:reportingErrorOnMarshaller: never raises so that Swift callers (which cannot
/// catch NSException) can observe the failure through the marshaller instead.
///
/// Also pins the SCValdiBridgeFunction resolveFunctionWithJSRuntime:error: contract: resolution
/// failures (unresolvable module, torn-down runtime) surface as an NSError, never as a raised
/// SCValdiError, so generated bridge-function classes can be resolved safely from Swift.
@interface SCValdiJSRuntimeModuleErrorTests: XCTestCase

@property (strong, nonatomic) SCValdiRuntimeManager *runtimeManager;

@end

static NSString *const kUnresolvableModulePath = @"__no_such_bundle__/__no_such_module__";

/// A generated bridge function (inheriting SCCValdiTestMakeTestObject's marshallable descriptor)
/// whose module path can never resolve.
@interface SCValdiTestUnresolvableBridgeFunction : SCCValdiTestMakeTestObject
@end

@implementation SCValdiTestUnresolvableBridgeFunction

+ (NSString *)modulePath
{
    return kUnresolvableModulePath;
}

@end

@implementation SCValdiJSRuntimeModuleErrorTests

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
    }
                        attribution:@"test.moduleError.withJSRuntime"];
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

- (void)testSafeResolverSucceedsForResolvableFunction
{
    [self withJSRuntime:^(id<SCValdiJSRuntime> jsRuntime) {
        NSError *error = nil;
        SCCValdiTestMakeTestObject *function = [SCCValdiTestMakeTestObject resolveFunctionWithJSRuntime:jsRuntime
                                                                                                  error:&error];
        XCTAssertNotNil(function);
        XCTAssertNil(error);
    }];
}

- (void)testSafeResolverReturnsErrorForUnresolvableModuleInsteadOfRaising
{
    [self withJSRuntime:^(id<SCValdiJSRuntime> jsRuntime) {
        NSError *error = nil;
        SCValdiTestUnresolvableBridgeFunction *function = nil;
        @try {
            function = [SCValdiTestUnresolvableBridgeFunction resolveFunctionWithJSRuntime:jsRuntime error:&error];
        } @catch (NSException *exception) {
            XCTFail(@"resolveFunctionWithJSRuntime:error: must not raise, got %@: %@", exception.name,
                    exception.reason);
            return;
        }
        XCTAssertNil(function);
        XCTAssertNotNil(error);
        XCTAssertEqualObjects(error.domain, SCValdiBridgeFunctionErrorDomain);
    }];
}

/// Regression test for the teardown-resolution shape: resolving a bridge function against a JS runtime
/// whose backing runtime has been torn down (logout) must report an NSError, not raise an
/// SCValdiError that would be uncatchable below a Swift frame.
- (void)testSafeResolverAfterRuntimeTeardownReturnsErrorInsteadOfRaising
{
    __block id<SCValdiJSRuntime> jsRuntime = nil;

    // Warm resolution so the wrapper caches the underlying runtime before teardown, matching the
    // production shape (a feature holding a jsRuntime across a logout).
    [self withJSRuntime:^(id<SCValdiJSRuntime> runtime) {
        jsRuntime = runtime;
        NSError *error = nil;
        XCTAssertNotNil([SCCValdiTestMakeTestObject resolveFunctionWithJSRuntime:runtime error:&error]);
        XCTAssertNil(error);
    }];

    // Dealloc drives RuntimeManager::fullTeardown(), which marks the runtime disposed synchronously.
    self.runtimeManager = nil;

    // valdi_test enables async_strict_mode, so resolution must happen off the main thread.
    XCTestExpectation *expectation = [self expectationWithDescription:@"resolution after teardown completed"];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSError *error = nil;
        SCCValdiTestMakeTestObject *function = nil;
        @try {
            function = [SCCValdiTestMakeTestObject resolveFunctionWithJSRuntime:jsRuntime error:&error];
        } @catch (NSException *exception) {
            XCTFail(@"resolveFunctionWithJSRuntime:error: must not raise after runtime teardown, got %@: %@",
                    exception.name, exception.reason);
            [expectation fulfill];
            return;
        }
        XCTAssertNil(function);
        XCTAssertNotNil(error);
        // Pins the C++ teardown stamping (JavaScriptRuntime::pushModuleToMarshaller): once the runtime
        // is disposed the module push is skipped and an explicit "runtime has been destroyed" error is
        // recorded. Without that stamp the marshaller is empty and resolution instead surfaces an
        // "undefined" conversion error, so this guards against silently regressing the teardown path.
        XCTAssertTrue([error.localizedDescription containsString:@"destroyed"],
                      @"expected the teardown-resolution error, got: %@", error.localizedDescription);
        [expectation fulfill];
    });
    [self waitForExpectations:@[expectation] timeout:10.0];
}

/// Companion to the safe-resolver teardown test, for the RAISING resolver (functionWithJSRuntime: /
/// Swift SCCFoo(jsRuntime:)) that the majority of call sites still use. When resolution races runtime
/// teardown it must NOT raise an SCValdiError (uncatchable below a Swift frame -> SIGABRT); it degrades
/// to a non-nil no-op function so a dying session unwinds quietly. Gated by the
/// VALDI_ENABLE_RESOLUTION_TEARDOWN_DEGRADE kill switch (default on).
- (void)testRaisingResolverAfterRuntimeTeardownDegradesInsteadOfRaising
{
    __block id<SCValdiJSRuntime> jsRuntime = nil;

    [self withJSRuntime:^(id<SCValdiJSRuntime> runtime) {
        jsRuntime = runtime;
        XCTAssertNotNil([SCCValdiTestMakeTestObject functionWithJSRuntime:runtime]);
    }];

    // Dealloc drives RuntimeManager::fullTeardown(), which marks the runtime disposed synchronously.
    self.runtimeManager = nil;

    // valdi_test enables async_strict_mode, so resolution must happen off the main thread.
    XCTestExpectation *expectation = [self expectationWithDescription:@"raising resolution after teardown completed"];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        SCCValdiTestMakeTestObject *function = nil;
        @try {
            function = [SCCValdiTestMakeTestObject functionWithJSRuntime:jsRuntime];
        } @catch (NSException *exception) {
            XCTFail(@"functionWithJSRuntime: must degrade, not raise, after runtime teardown, got %@: %@",
                    exception.name, exception.reason);
            [expectation fulfill];
            return;
        }
        // The raising resolver is imported into Swift as non-null, so the degrade must return an object
        // rather than nil (which would trap on first use).
        XCTAssertNotNil(function);
        [expectation fulfill];
    });
    [self waitForExpectations:@[expectation] timeout:10.0];
}

/// The degrade must trigger only on teardown, never mask a genuine resolution failure on a live
/// runtime: functionWithJSRuntime: for an unresolvable module still raises.
- (void)testRaisingResolverStillRaisesForUnresolvableModuleOnLiveRuntime
{
    [self withJSRuntime:^(id<SCValdiJSRuntime> jsRuntime) {
        @try {
            (void)[SCValdiTestUnresolvableBridgeFunction functionWithJSRuntime:jsRuntime];
            XCTFail(@"functionWithJSRuntime: must still raise for an unresolvable module on a live runtime");
        } @catch (SCValdiError *error) {
            XCTAssertNotNil(error.reason);
        }
    }];
}

@end
