//
//  SCValdiApplicationTerminationTests.mm
//  ios_tests
//
//  Bridge crossings racing iOS process termination. UIApplicationWillTerminateNotification ->
//  -[SCValdiRuntimeManager _applicationWillTerminate] -> RuntimeManager::applicationWillTerminate ->
//  partialTeardown() tears the JS runtime down inline on the main thread while background work
//  (Swift concurrency tasks, queue performers, queued JS-thread callbacks) is still resolving and
//  invoking generated bridge functions. None of those crossings may raise across the bridge, and work
//  already running on the JS thread must still get real values.
//
//  Lives in its own test target: the notification reaches every live SCValdiRuntimeManager, and the
//  shutdown flag it sets is process-global.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import <SCCValdiTest/SCCValdiTest.h>

#import "valdi/ios/SCValdiRuntimeManager.h"
#import "valdi_core/SCValdiBridgeFunction.h"
#import "valdi_core/SCValdiJSRuntime.h"

#include "valdi/runtime/JavaScript/JavaScriptRuntime.hpp"
#include "valdi/runtime/Runtime.hpp"
#include "valdi/runtime/RuntimeManager.hpp"
#include "valdi/runtime/Utils/ShutdownUtils.hpp"

#include <sched.h>

@interface SCValdiApplicationTerminationTests : XCTestCase
@end

@implementation SCValdiApplicationTerminationTests {
    SCValdiRuntimeManager *_manager;
    id<SCValdiJSRuntime> _jsRuntime;
}

- (void)setUp
{
    self.continueAfterFailure = NO;
    _manager = [SCValdiRuntimeManager new];
    id<SCValdiRuntimeProtocol> runtime = _manager.mainRuntime;
    XCTAssertNotNil(runtime);
    _jsRuntime = [runtime jsRuntime];
    XCTAssertNotNil(_jsRuntime);
}

- (void)tearDown
{
    // Production exits right after termination and never releases a terminated runtime manager (its JS
    // context stays alive), so keep it instead of deallocating it. Clear the process-global shutdown flag
    // so the next test can load modules.
    static NSMutableArray<SCValdiRuntimeManager *> *terminatedManagers;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        terminatedManagers = [NSMutableArray array];
    });
    [terminatedManagers addObject:_manager];
    Valdi::setApplicationShuttingDown(false);
    _jsRuntime = nil;
    _manager = nil;
}

- (Valdi::JavaScriptRuntime *)javaScriptRuntime
{
    auto *cppManager = static_cast<Valdi::RuntimeManager *>(_manager.cppInstance);
    XCTAssertTrue(cppManager != nullptr);
    auto runtimes = cppManager->getAllRuntimes();
    XCTAssertEqual(runtimes.size(), 1u);
    return runtimes[0]->getJavaScriptRuntime();
}

- (void)postWillTerminate
{
    [[NSNotificationCenter defaultCenter] postNotificationName:UIApplicationWillTerminateNotification object:nil];
}

- (void)runInBackground:(NSString *)description block:(dispatch_block_t)block
{
    XCTestExpectation *expectation = [self expectationWithDescription:description];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        block();
        [expectation fulfill];
    });
    [self waitForExpectations:@[expectation] timeout:10.0];
}

// Background resolution and invocation after termination report or
// degrade instead of raising an SCValdiError below a Swift frame.
- (void)testBackgroundBridgeCrossingsAfterWillTerminateDoNotRaise
{
    id<SCValdiJSRuntime> jsRuntime = _jsRuntime;

    __block SCCValdiTestGetTestString *resolvedBeforeTermination = nil;
    __block NSString *liveResult = nil;
    [self runInBackground:@"live resolution"
                    block:^{
                        resolvedBeforeTermination = [SCCValdiTestGetTestString functionWithJSRuntime:jsRuntime];
                        liveResult = [resolvedBeforeTermination getTestString];
                    }];
    XCTAssertEqualObjects(liveResult, @"ok");

    [self postWillTerminate];

    __block NSException *raised = nil;
    __block SCCValdiTestGetTestString *degraded = nil;
    __block SCCValdiTestGetTestString *safeResolved = nil;
    __block NSError *safeResolveError = nil;
    __block NSString *degradedResult = @"sentinel";
    __block NSString *resolvedBeforeTerminationResult = @"sentinel";
    [self runInBackground:@"crossings after termination"
                    block:^{
                        @try {
                            degraded = [SCCValdiTestGetTestString functionWithJSRuntime:jsRuntime];
                            degradedResult = [degraded getTestString];
                            NSError *error = nil;
                            safeResolved = [SCCValdiTestGetTestString resolveFunctionWithJSRuntime:jsRuntime
                                                                                             error:&error];
                            safeResolveError = error;
                            resolvedBeforeTerminationResult = [resolvedBeforeTermination getTestString];
                        } @catch (NSException *exception) {
                            raised = exception;
                        }
                    }];

    XCTAssertNil(raised, @"No bridge crossing may raise after termination, got %@: %@", raised.name, raised.reason);
    XCTAssertNotNil(degraded, @"The raising resolver must degrade to a no-op function");
    XCTAssertNil(safeResolved);
    XCTAssertNotNil(safeResolveError, @"The non-raising resolver must report the teardown as an NSError");
    // Both invocations yield a null in the non-null return slot. This is the Type-B value nonnull Swift
    // callers trap on; the boundary-sanitize fix should make these empty strings.
    XCTAssertNil(degradedResult);
    XCTAssertNil(resolvedBeforeTerminationResult);
}

// VALDI_ENABLE_RESOLUTION_TEARDOWN_DEGRADE off (its kill switch): resolution after termination raises the
// original SCValdiError again. Pins that the flag is a real lever at termination, not only after dealloc.
- (void)testBackgroundResolutionAfterWillTerminateRaisesWithDegradeOff
{
    id<SCValdiJSRuntime> jsRuntime = _jsRuntime;
    [self postWillTerminate];
    [self javaScriptRuntime]->setResolutionTeardownDegradeEnabled(false);

    __block BOOL raised = NO;
    [self runInBackground:@"resolution after termination, degrade off"
                    block:^{
                        @try {
                            (void)[SCCValdiTestGetTestString functionWithJSRuntime:jsRuntime];
                        } @catch (NSException *exception) {
                            raised = YES;
                        }
                    }];
    [self javaScriptRuntime]->setResolutionTeardownDegradeEnabled(true);
    XCTAssertTrue(raised, @"With the degrade off, resolution after termination must raise");
}

// Work already running on the JS thread when termination starts (e.g. a native callback waiting on the
// JS queue) still resolves and invokes against the live context and gets the real value.
- (void)testJsThreadWorkInFlightAtWillTerminateGetsRealValues
{
    XCTAssertEqualObjects([self resultOfJsThreadWorkInFlightAtWillTerminateClearingRunning:NO cooperative:YES], @"ok");
}

// VALDI_USE_COOPERATIVE_TERMINATION off (aggressive): disposed work is skipped, so the same in-flight work
// gets nil again. Pins what flipping that flag costs at termination.
- (void)testJsThreadWorkInFlightAtWillTerminateDegradesWithoutCooperativeTermination
{
    XCTAssertNil([self resultOfJsThreadWorkInFlightAtWillTerminateClearingRunning:NO cooperative:NO]);
}

// Same window with _running cleared at queue teardown, as teardownOnJsThread did before flush-then-dispose:
// the in-flight resolution is skipped, degrades, and hands back nil. Pins that the
// test above discriminates.
- (void)testJsThreadWorkInFlightAtWillTerminateDegradesWhenTeardownClearsRunning
{
    XCTAssertNil([self resultOfJsThreadWorkInFlightAtWillTerminateClearingRunning:YES cooperative:YES]);
}

- (NSString *)resultOfJsThreadWorkInFlightAtWillTerminateClearingRunning:(BOOL)clearRunning
                                                               cooperative:(BOOL)cooperative
{
    id<SCValdiJSRuntime> jsRuntime = _jsRuntime;
    Valdi::JavaScriptRuntime *javaScriptRuntime = [self javaScriptRuntime];
    javaScriptRuntime->setCooperativeTermination(cooperative ? true : false);

    // Termination runs on the main thread and blocks it until the in-flight task finishes, so the task is
    // released from a background queue once the runtime reports disposed.
    dispatch_semaphore_t terminationStarted = dispatch_semaphore_create(0);

    XCTestExpectation *finished = [self expectationWithDescription:@"in-flight JS-thread work finished"];
    __block NSString *result = @"sentinel";
    __block NSException *raised = nil;
    dispatch_semaphore_t running = dispatch_semaphore_create(0);
    dispatch_block_t inFlightWork = ^{
        dispatch_semaphore_signal(running);
        dispatch_semaphore_wait(terminationStarted, DISPATCH_TIME_FOREVER);
        @try {
            result = [[SCCValdiTestGetTestString functionWithJSRuntime:jsRuntime] getTestString];
        } @catch (NSException *exception) {
            raised = exception;
        }
        [finished fulfill];
    };
    javaScriptRuntime->dispatchOnJsThreadAsync(STRING_LITERAL("test.termination"),
                                               [inFlightWork](auto & /*jsEntry*/) { inFlightWork(); });

    // Termination drops work that has not started, so the task must be running before it begins. It
    // runs on the main thread, as in production, and returns once the in-flight task finishes.
    dispatch_semaphore_wait(running, DISPATCH_TIME_FOREVER);
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        while (!javaScriptRuntime->isDisposed()) {
            sched_yield();
        }
        if (clearRunning) {
            javaScriptRuntime->setRunningForTesting(false);
        }
        dispatch_semaphore_signal(terminationStarted);
    });
    [self postWillTerminate];
    [self waitForExpectations:@[finished] timeout:10.0];

    javaScriptRuntime->setCooperativeTermination(true);
    XCTAssertNil(raised, @"In-flight JS-thread work must not raise during termination, got %@: %@", raised.name,
                 raised.reason);
    return result;
}

@end
