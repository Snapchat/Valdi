//
//  SCValdiBridgeInvocationErrorTests.m
//  ios_tests
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "valdi/ios/SCValdiRuntimeManager.h"
#import "valdi_core/SCValdiError.h"
#import "valdi_core/SCValdiJSRuntime.h"
#import "valdi_core/SCValdiSharedLogger.h"

#import <SCCValdiTest/SCCValdiTest.h>

/// A synchronous JS throw during a bridged invocation must not cross the bridge as an SCValdiError
/// NSException: Swift callers cannot catch it (the process aborts below the Swift frame) and
/// unguarded Objective-C callers abort too. The bridge trampoline must instead report the error and
/// return a type-safe default value. See ios/swift/README.md.
@interface SCValdiBridgeInvocationErrorTests: XCTestCase

@property (strong, nonatomic) SCValdiRuntimeManager *runtimeManager;

@end

/// Captures every error-level log so the test can assert the degraded invocation was reported
/// rather than silently swallowed.
@interface SCValdiCapturingLogger: NSObject <SCValdiLogger>
@property (strong, nonatomic) NSMutableArray<NSString *> *errorLogs;
@end

@implementation SCValdiCapturingLogger

- (instancetype)init
{
    self = [super init];
    if (self) {
        _errorLogs = [NSMutableArray array];
    }
    return self;
}

- (BOOL)isLogEnabledForLevel:(SCValdiLoggerLevel)level
{
    return YES;
}

- (void)outputLog:(NSString *)log forLevel:(SCValdiLoggerLevel)level
{
    if (level == SCValdiLoggerLevelError) {
        @synchronized(self.errorLogs) {
            [self.errorLogs addObject:log];
        }
    }
}

@end

@implementation SCValdiBridgeInvocationErrorTests

- (void)setUp
{
    self.runtimeManager = [SCValdiRuntimeManager new];
    self.continueAfterFailure = NO;
}

- (void)tearDown
{
    self.runtimeManager = nil;
}

/// Resolves the FunctionTest fixture's ITestObject off the main thread (async_strict_mode forbids
/// resolution on the main thread) and hands it to the block.
- (void)withTestObject:(void (^)(id<SCCValdiTestITestObject> testObject))block
{
    XCTestExpectation *expectation = [self expectationWithDescription:@"test object resolved"];
    id<SCValdiRuntimeProtocol> runtime = self.runtimeManager.mainRuntime;
    XCTAssertNotNil(runtime);

    [SCCValdiTestMakeTestObject invokeWithJSRuntimeProvider:^id<SCValdiJSRuntime> {
        return [runtime jsRuntime];
    } completionHandler:^(id<SCCValdiTestITestObject> testObject) {
        XCTAssertNotNil(testObject);
        block(testObject);
        [expectation fulfill];
    }];

    [self waitForExpectations:@[expectation] timeout:5.0];
}

- (void)testSynchronousThrowDuringInvocationIsReportedNotRaised
{
    SCValdiCapturingLogger *logger = [SCValdiCapturingLogger new];
    id<SCValdiLogger> previousLogger = SCValdiGetSharedLogger();
    SCValdiSetSharedLogger(logger);

    @try {
        [self withTestObject:^(id<SCCValdiTestITestObject> testObject) {
            __block double result = 123.0;
            @try {
                result = [testObject throwSynchronously];
            } @catch (NSException *exception) {
                XCTFail(@"A synchronous JS throw must not raise across the bridge, got %@: %@",
                        exception.name, exception.reason);
                return;
            }

            // The crossing degraded to the type-safe default for a non-nullable `number` return.
            XCTAssertEqual(result, 0.0);

            // The failure must still be reported (logged), not silently swallowed.
            @synchronized(logger.errorLogs) {
                BOOL reported = NO;
                for (NSString *log in logger.errorLogs) {
                    if ([log containsString:@"throwSynchronously"]) {
                        reported = YES;
                        break;
                    }
                }
                XCTAssertTrue(reported, @"Expected the degraded invocation to be logged, got %@",
                              logger.errorLogs);
            }
        }];
    } @finally {
        SCValdiSetSharedLogger(previousLogger);
    }
}

- (void)testNonThrowingInvocationStillReturnsValue
{
    [self withTestObject:^(id<SCCValdiTestITestObject> testObject) {
        // Control: the fix must not disturb the normal, non-throwing invocation path.
        XCTAssertEqual([testObject addWithValue:10.0], 10.0);
        XCTAssertEqual([testObject addWithValue:32.0], 42.0);
    }];
}

/// Invocation-teardown null-in-nonnull: after the runtime is torn down, the raising resolver
/// degrades to a no-op function; INVOKING it returns a null value in a `_Nonnull`-typed return slot
/// (here a nil NSString). This is the framework side of the production crash where a caller then
/// feeds that null to an API with a non-null precondition (e.g. +[NSURL fileURLWithPath:]) and
/// aborts, or a Swift caller dereferences it. The framework itself must NOT raise here — it returns
/// nil.
///
/// NOTE: the durable boundary-sanitize fix should replace this teardown null with a type-safe
/// non-null default (an empty string). When that lands, flip the assertion to
/// `XCTAssertEqualObjects(result, @"")`.
- (void)testStringInvocationAfterTeardownReturnsNullNotRaise
{
    // Isolated runtime so teardown can be forced without touching other tests.
    SCValdiRuntimeManager *manager = [SCValdiRuntimeManager new];
    id<SCValdiRuntimeProtocol> runtime = manager.mainRuntime;
    XCTAssertNotNil(runtime);
    id<SCValdiJSRuntime> jsRuntime = [runtime jsRuntime];
    XCTAssertNotNil(jsRuntime);
    runtime = nil;

    // Control: on the live runtime the string function resolves + invokes to its real value.
    // (off the main thread: async_strict_mode forbids resolution on the main thread.)
    XCTestExpectation *live = [self expectationWithDescription:@"live invocation"];
    __block NSString *liveResult = nil;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        SCCValdiTestGetTestString *fn = [SCCValdiTestGetTestString functionWithJSRuntime:jsRuntime];
        liveResult = [fn getTestString];
        [live fulfill];
    });
    [self waitForExpectations:@[live] timeout:5.0];
    XCTAssertEqualObjects(liveResult, @"ok");

    // Drop the last strong reference to the manager -> dealloc -> fullTeardown -> runtime disposed.
    // jsRuntime is retained separately so the disposed runtime stays addressable.
    manager = nil;

    // After teardown: resolution degrades to a no-op function and invoking it returns nil. It must
    // not raise an SCValdiError NSException.
    XCTestExpectation *afterTeardown = [self expectationWithDescription:@"invocation after teardown"];
    __block NSString *result = @"sentinel";
    __block BOOL raised = NO;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        @try {
            SCCValdiTestGetTestString *fn = [SCCValdiTestGetTestString functionWithJSRuntime:jsRuntime];
            result = [fn getTestString];
        } @catch (NSException *exception) {
            raised = YES;
        }
        [afterTeardown fulfill];
    });
    [self waitForExpectations:@[afterTeardown] timeout:5.0];

    XCTAssertFalse(raised, @"Invocation after teardown must not raise across the bridge");
    XCTAssertNil(result, @"Degraded invocation returns a null in the non-null return slot "
                         @"(see NOTE: the boundary-sanitize fix should make this an empty string)");
}

@end
