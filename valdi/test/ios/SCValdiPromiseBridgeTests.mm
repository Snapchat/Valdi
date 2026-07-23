// Copyright © 2026 Snap, Inc. All rights reserved.

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "valdi_core/SCValdiBridgedPromise+CPP.h"
#import "valdi_core/SCValdiResolvablePromise.h"
#import "valdi_core/cpp/Utils/ResolvablePromise.hpp"
#import "valdi_core/cpp/Utils/Shared.hpp"

@interface SCValdiPromiseBridgeTests : XCTestCase
@end

@implementation SCValdiPromiseBridgeTests

- (void)testSynchronouslyResolvedPromiseDoesNotLeakBridgePeer
{
    // Regression: a promise fulfilled before it crosses the bridge gets its peer attached
    // after completion. The peer must not be stored, otherwise the mutual retain between
    // the promise and its peer is never broken and both leak.
    __weak SCValdiResolvablePromise *weakPromise = nil;

    @autoreleasepool {
        SCValdiResolvablePromise *promise = [SCValdiResolvablePromise new];
        weakPromise = promise;
        [promise fulfillWithSuccessValue:@1];

        auto peer = ValdiIOS::PromiseFromSCValdiPromise(promise, nullptr);
        XCTAssertTrue(peer != nullptr);
    }

    XCTAssertNil(weakPromise, @"bridging a synchronously-resolved promise leaked it");
}

- (void)testCanceledPromiseDoesNotLeakBridgePeer
{
    __weak SCValdiResolvablePromise *weakPromise = nil;

    @autoreleasepool {
        SCValdiResolvablePromise *promise = [SCValdiResolvablePromise new];
        weakPromise = promise;
        [promise cancel];

        auto peer = ValdiIOS::PromiseFromSCValdiPromise(promise, nullptr);
        XCTAssertTrue(peer != nullptr);
    }

    XCTAssertNil(weakPromise, @"bridging a canceled promise leaked it");
}

- (void)testPendingPromisePeerIsCachedAndReleasedOnFulfill
{
    __weak SCValdiResolvablePromise *weakPromise = nil;

    @autoreleasepool {
        SCValdiResolvablePromise *promise = [SCValdiResolvablePromise new];
        weakPromise = promise;

        auto peer = ValdiIOS::PromiseFromSCValdiPromise(promise, nullptr);
        auto cachedPeer = ValdiIOS::PromiseFromSCValdiPromise(promise, nullptr);
        XCTAssertEqual(peer.get(), cachedPeer.get(), @"pending promise should reuse its cached peer");

        [promise fulfillWithSuccessValue:@1];
    }

    XCTAssertNil(weakPromise, @"fulfilling a bridged promise did not break the peer retain cycle");
}

- (void)testSetPeerAfterFulfillIsNotStored
{
    SCValdiResolvablePromise *promise = [SCValdiResolvablePromise new];
    [promise fulfillWithSuccessValue:@1];

    auto cppPromise = Valdi::makeShared<Valdi::ResolvablePromise>();
    [promise setPeer:Valdi::unsafeBridgeCast(cppPromise.get())];

    XCTAssertTrue([promise getPeer] == nullptr, @"peer set after completion should be discarded");
}

@end
