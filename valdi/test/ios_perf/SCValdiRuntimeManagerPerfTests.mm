//
//  SCValdiRuntimeManagerPerfTests.mm
//  Valdi
//

#import <XCTest/XCTest.h>

#import "valdi/ios/SCValdiRuntimeManager.h"

#include <atomic>
#include <chrono>
#include <memory>

static const NSUInteger kReaderThreads = 8;
static const NSUInteger kReadsPerThread = 10000;

@interface SCValdiRuntimeManagerPerfTests : XCTestCase
@end

@implementation SCValdiRuntimeManagerPerfTests {
    SCValdiRuntimeManager *_manager;
}

- (void)setUp
{
    [super setUp];

    _manager = [SCValdiRuntimeManager new];
    // Pay the one-time runtime creation up front so the measurements below only see
    // steady-state accessor behavior.
    XCTAssertNotNil(_manager.mainRuntime);
}

- (void)tearDown
{
    _manager = nil;
    [super tearDown];
}

- (void)_readMainRuntimeFromConcurrentThreads:(NSUInteger)readsPerThread
{
    dispatch_group_t group = dispatch_group_create();
    dispatch_queue_t queue = dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0);

    for (NSUInteger i = 0; i < kReaderThreads; i++) {
        dispatch_group_async(group, queue, ^{
            for (NSUInteger j = 0; j < readsPerThread; j++) {
                (void)self->_manager.mainRuntime;
                (void)self->_manager.cppInstance;
            }
        });
    }

    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
}

- (void)testMainRuntimeAccessUncontended
{
    [self measureBlock:^{
        [self _readMainRuntimeFromConcurrentThreads:kReadsPerThread];
    }];
}

- (void)testMainRuntimeAccessWhileConfigurationUpdatesHoldManagerLock
{
    auto keepWriting = std::make_shared<std::atomic<bool>>(true);
    dispatch_group_t writers = dispatch_group_create();
    dispatch_queue_t queue = dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0);

    // updateConfiguration holds the manager monitor for a full configuration re-apply,
    // which is exactly the traffic the mainRuntime fast path must not serialize behind.
    for (NSUInteger i = 0; i < 2; i++) {
        dispatch_group_async(writers, queue, ^{
            while (keepWriting->load(std::memory_order_relaxed)) {
                [self->_manager updateConfiguration:^(SCValdiConfiguration *configuration) {
                }];
            }
        });
    }

    [self measureBlock:^{
        [self _readMainRuntimeFromConcurrentThreads:kReadsPerThread];
    }];

    keepWriting->store(false, std::memory_order_relaxed);
    dispatch_group_wait(writers, DISPATCH_TIME_FOREVER);
}

- (void)testMainRuntimeAccessWhileManagerMonitorHeld
{
    static const int64_t kMonitorHoldTimeoutSec = 5;

    dispatch_semaphore_t monitorHeld = dispatch_semaphore_create(0);
    dispatch_semaphore_t releaseMonitor = dispatch_semaphore_create(0);
    dispatch_group_t holder = dispatch_group_create();

    // Simulates any slow operation under the manager monitor (the JS stack-trace capture
    // during ANR reports used to be one, holding it for up to its whole timeout). Runtime
    // accessors must not serialize behind it.
    dispatch_group_async(holder, dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
        @synchronized(self->_manager) {
            dispatch_semaphore_signal(monitorHeld);
            dispatch_semaphore_wait(releaseMonitor,
                                    dispatch_time(DISPATCH_TIME_NOW, kMonitorHoldTimeoutSec * (int64_t)NSEC_PER_SEC));
        }
    });
    XCTAssertEqual(0L, dispatch_semaphore_wait(monitorHeld, dispatch_time(DISPATCH_TIME_NOW, (int64_t)(10 * NSEC_PER_SEC))));

    auto start = std::chrono::steady_clock::now();
    [self _readMainRuntimeFromConcurrentThreads:1000];
    auto elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

    // Accessors that take the monitor would stall until the holder's timeout expires.
    // Half that leaves ample headroom for slow CI machines.
    XCTAssertLessThan(elapsedMs, kMonitorHoldTimeoutSec * 1000 / 2);

    dispatch_semaphore_signal(releaseMonitor);
    dispatch_group_wait(holder, DISPATCH_TIME_FOREVER);
}

@end
