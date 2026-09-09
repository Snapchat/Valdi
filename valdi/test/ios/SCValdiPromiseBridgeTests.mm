// Copyright © 2026 Snap, Inc. All rights reserved.

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#include <string>

#import "valdi_core/SCValdiBridgedPromise+CPP.h"
#import "valdi_core/SCValdiResolvablePromise.h"
#import "valdi_core/cpp/Utils/Error.hpp"
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

- (void)testCancelReleasesRegisteredCallbacks
{
    // Regression: cancel cleared _peer but not _callbacks, and
    // _doOnCompleteWithCallbackUntyped never drains them once canceled. A callback that
    // retains the promise then kept it alive forever.
    __weak SCValdiResolvablePromise *weakPromise = nil;

    @autoreleasepool {
        SCValdiResolvablePromise *promise = [SCValdiResolvablePromise new];
        weakPromise = promise;

        auto peer = ValdiIOS::PromiseFromSCValdiPromise(promise, nullptr);
        XCTAssertTrue(peer != nullptr);

        [promise onCompleteWithCallbackBlock:^(id _Nullable value, NSError *_Nullable error) {
            // Strongly captures the promise, reproducing the observed self-retain.
            (void)promise;
        }];

        [promise cancel];
    }

    XCTAssertNil(weakPromise, @"canceling a promise did not release its registered callbacks");
}

- (void)testCancelForwardsFailureToRegisteredCallbacks
{
    SCValdiResolvablePromise *promise = [SCValdiResolvablePromise new];

    __block BOOL callbackInvoked = NO;
    __block id receivedValue = @"unset";
    __block NSError *receivedError = nil;
    [promise onCompleteWithCallbackBlock:^(id _Nullable value, NSError *_Nullable error) {
        callbackInvoked = YES;
        receivedValue = value;
        receivedError = error;
    }];

    [promise cancel];

    XCTAssertTrue(callbackInvoked, @"cancel should forward a failure to registered callbacks");
    XCTAssertNil(receivedValue, @"cancel should not forward a value");
    XCTAssertNotNil(receivedError, @"cancel should forward a cancellation error");
    XCTAssertEqual(receivedError.code, Valdi::kPromiseCanceledErrorCode,
                   @"the cancellation error should carry the dedicated error code");
}

- (void)testCancelForwardsProducerResultWhenCancelCallbackSettles
{
    SCValdiResolvablePromise *promise = [SCValdiResolvablePromise new];
    __weak SCValdiResolvablePromise *weakPromise = promise;
    [promise setCancelCallback:^{
        [weakPromise fulfillWithError:[NSError errorWithDomain:@"test" code:42 userInfo:nil]];
    }];

    __block NSError *receivedError = nil;
    [promise onCompleteWithCallbackBlock:^(id _Nullable value, NSError *_Nullable error) {
        receivedError = error;
    }];

    [promise cancel];

    XCTAssertNotNil(receivedError, @"the producer's terminal result should reach the callbacks");
    XCTAssertEqualObjects(receivedError.domain, @"test", @"cancel must not mask the producer's real result");
    XCTAssertEqual(receivedError.code, 42);
}

- (void)testCancelOnBridgedChainForwardsObjCProducerResult
{
    // Bridged chain regression (see PR #48811 history): a C++ promise chained onto an Obj-C
    // producer propagates cancel across the bridge; when the producer answers by settling, the
    // C++ consumer must receive that real result, not a synthetic canceled error.
    SCValdiResolvablePromise *objcPromise = [SCValdiResolvablePromise new];
    __weak SCValdiResolvablePromise *weakObjcPromise = objcPromise;
    [objcPromise setCancelCallback:^{
        [weakObjcPromise fulfillWithError:[NSError errorWithDomain:@"test"
                                                              code:7
                                                          userInfo:@{NSLocalizedDescriptionKey : @"upstream failure"}]];
    }];

    auto peer = ValdiIOS::PromiseFromSCValdiPromise(objcPromise, nullptr);
    auto downstream = Valdi::makeShared<Valdi::ResolvablePromise>();
    downstream->fulfillWithPromiseResult(peer);

    BOOL failed = NO;
    BOOL succeeded = NO;
    std::string failureMessage;
    // The function-based onComplete overload lives on Promise and is hidden by the
    // Ref<PromiseCallback> override on ResolvablePromise.
    Valdi::Promise& downstreamPromise = *downstream;
    downstreamPromise.onComplete([&](const Valdi::Result<Valdi::Value>& result) {
        if (result) {
            succeeded = YES;
        } else {
            failed = YES;
            failureMessage = std::string(result.error().getMessage().toStringView());
        }
    });

    downstream->cancel();

    XCTAssertTrue(failed, @"the Obj-C producer's terminal result should reach the C++ consumer");
    XCTAssertFalse(succeeded);
    XCTAssertTrue(failureMessage.find("upstream failure") != std::string::npos,
                  @"cancel must not mask the producer's real result, got: %s", failureMessage.c_str());
}

- (void)testOnCompleteAfterCancelIsServedTheCanceledFailure
{
    SCValdiResolvablePromise *promise = [SCValdiResolvablePromise new];
    [promise cancel];

    __block BOOL callbackInvoked = NO;
    __block NSError *receivedError = nil;
    [promise onCompleteWithCallbackBlock:^(id _Nullable value, NSError *_Nullable error) {
        callbackInvoked = YES;
        receivedError = error;
    }];

    XCTAssertTrue(callbackInvoked, @"a late registrant on a canceled promise must not hang");
    XCTAssertEqual(receivedError.code, Valdi::kPromiseCanceledErrorCode);
}

- (void)testFulfillAfterCancelIsDropped
{
    SCValdiResolvablePromise *promise = [SCValdiResolvablePromise new];

    __block NSUInteger callbackInvocations = 0;
    [promise onCompleteWithCallbackBlock:^(id _Nullable value, NSError *_Nullable error) {
        callbackInvocations += 1;
    }];

    [promise cancel];
    XCTAssertEqual(callbackInvocations, 1u);

    // A producer settling asynchronously after cancel must be dropped, not assert or re-notify.
    [promise fulfillWithSuccessValue:@1];
    XCTAssertEqual(callbackInvocations, 1u, @"a late producer settle after cancel must be dropped");

    __block NSError *receivedError = nil;
    [promise onCompleteWithCallbackBlock:^(id _Nullable value, NSError *_Nullable error) {
        receivedError = error;
    }];
    XCTAssertEqual(receivedError.code, Valdi::kPromiseCanceledErrorCode,
                   @"the recorded result should remain the canceled failure");
}

- (void)testOnCompleteBetweenCancelPhasesIsDrainedByCancel
{
    // cancel releases the mutex to run the producer's cancel block before recording a result, so a
    // callback registered from inside that window lands in _callbacks with nothing recorded yet.
    // The second phase has to drain it, otherwise it is retained forever and its awaiter hangs.
    SCValdiResolvablePromise *promise = [SCValdiResolvablePromise new];
    __weak SCValdiResolvablePromise *weakPromise = promise;
    __block NSUInteger callbackInvocations = 0;
    __block NSError *receivedError = nil;

    [promise setCancelCallback:^{
        [weakPromise onCompleteWithCallbackBlock:^(id _Nullable value, NSError *_Nullable error) {
            callbackInvocations += 1;
            receivedError = error;
        }];
    }];

    [promise cancel];

    XCTAssertEqual(callbackInvocations, 1u, @"a callback registered between cancel's two phases must not hang");
    XCTAssertEqual(receivedError.code, Valdi::kPromiseCanceledErrorCode);
}

- (void)testCancelErrorCodeSurvivesObjCToCppRoundTrip
{
    // The canceled error has to stay identifiable as it crosses back into C++, otherwise consumers
    // are left matching on the message. NSErrorFromError stamps the code into kValdiErrorDomain and
    // ErrorFromNSError reads it back.
    SCValdiResolvablePromise *objcPromise = [SCValdiResolvablePromise new];

    auto peer = ValdiIOS::PromiseFromSCValdiPromise(objcPromise, nullptr);
    auto downstream = Valdi::makeShared<Valdi::ResolvablePromise>();
    downstream->fulfillWithPromiseResult(peer);

    int32_t failureCode = -1;
    std::string failureMessage;
    Valdi::Promise& downstreamPromise = *downstream;
    downstreamPromise.onComplete([&](const Valdi::Result<Valdi::Value>& result) {
        if (!result) {
            failureCode = result.error().getErrorCode();
            failureMessage = std::string(result.error().getMessage().toStringView());
        }
    });

    [objcPromise cancel];

    XCTAssertEqual(failureCode, Valdi::kPromiseCanceledErrorCode,
                   @"the cancellation code should survive Obj-C -> C++, got message: %s", failureMessage.c_str());
}

- (void)testNonValdiDomainErrorCodeIsNotTreatedAsAValdiCode
{
    // Codes from other domains are not Valdi::Error codes, so they must not be smuggled through.
    SCValdiResolvablePromise *objcPromise = [SCValdiResolvablePromise new];

    auto peer = ValdiIOS::PromiseFromSCValdiPromise(objcPromise, nullptr);
    auto downstream = Valdi::makeShared<Valdi::ResolvablePromise>();
    downstream->fulfillWithPromiseResult(peer);

    int32_t failureCode = -1;
    Valdi::Promise& downstreamPromise = *downstream;
    downstreamPromise.onComplete([&](const Valdi::Result<Valdi::Value>& result) {
        if (!result) {
            failureCode = result.error().getErrorCode();
        }
    });

    [objcPromise fulfillWithError:[NSError errorWithDomain:@"test"
                                                      code:Valdi::kPromiseCanceledErrorCode
                                                  userInfo:nil]];

    XCTAssertEqual(failureCode, 0, @"a foreign domain's code must not be read as a Valdi error code");
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
