#import <XCTest/XCTest.h>

#import "valdi/ios/SCValdiRuntimeManager.h"

@interface SCValdiWorkerRuntimeTests : XCTestCase
@end

@implementation SCValdiWorkerRuntimeTests

- (void)testEmptyWorkerSnapshotDoesNotInitializeMainRuntime
{
    SCValdiRuntimeManager *manager = [SCValdiRuntimeManager new];
    __block BOOL mainRuntimeCreated = NO;
    [manager registerMainRuntimeCreatedCallback:^(id<SCValdiRuntimeProtocol> runtime) {
        mainRuntimeCreated = YES;
    }];

    XCTAssertEqual(manager.existingWorkerRuntimes.count, 0U);
    XCTAssertFalse(mainRuntimeCreated);
}

- (void)testWorkerSnapshotContainsReusedWorkerOnlyOnce
{
    SCValdiRuntimeManager *manager = [SCValdiRuntimeManager new];
    XCTestExpectation *created = [self expectationWithDescription:@"worker created"];
    __block id<SCValdiJSRuntime> firstWorker;
    [manager getWorkerOnExecutor:@"SHARED_BACKGROUND" block:^(id<SCValdiJSRuntime> worker) {
        firstWorker = worker;
        [created fulfill];
    }];
    [self waitForExpectations:@[created] timeout:5.0];

    XCTestExpectation *reused = [self expectationWithDescription:@"worker reused"];
    [manager getWorkerOnExecutor:@"SHARED_BACKGROUND" block:^(id<SCValdiJSRuntime> worker) {
        XCTAssertEqual(worker, firstWorker);
        [reused fulfill];
    }];
    [self waitForExpectations:@[reused] timeout:5.0];

    XCTAssertEqualObjects(manager.existingWorkerRuntimes, (@[firstWorker]));
}

- (void)testWorkerSnapshotIncludesEveryExecutorAndDoesNotChangeAfterCreation
{
    SCValdiRuntimeManager *manager = [SCValdiRuntimeManager new];
    XCTestExpectation *firstCreated = [self expectationWithDescription:@"first worker created"];
    [manager getWorkerOnExecutor:@"SHARED_BACKGROUND" block:^(id<SCValdiJSRuntime> worker) {
        [firstCreated fulfill];
    }];
    [self waitForExpectations:@[firstCreated] timeout:5.0];
    NSArray<id<SCValdiJSRuntime>> *snapshot = manager.existingWorkerRuntimes;

    XCTestExpectation *secondCreated = [self expectationWithDescription:@"second worker created"];
    __block id<SCValdiJSRuntime> secondWorker;
    [manager getWorkerOnExecutor:@"BACKGROUND" block:^(id<SCValdiJSRuntime> worker) {
        secondWorker = worker;
        [secondCreated fulfill];
    }];
    [self waitForExpectations:@[secondCreated] timeout:5.0];

    XCTAssertEqual(snapshot.count, 1U);
    XCTAssertEqual(manager.existingWorkerRuntimes.count, 2U);
    XCTAssertTrue([manager.existingWorkerRuntimes containsObject:snapshot.firstObject]);
    XCTAssertTrue([manager.existingWorkerRuntimes containsObject:secondWorker]);
}

@end
