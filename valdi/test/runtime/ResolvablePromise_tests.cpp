//
//  ResolvablePromise_tests.cpp
//  valdi-pc
//

#include <gtest/gtest.h>

#include <string>

#include "valdi_core/cpp/Utils/Error.hpp"
#include "valdi_core/cpp/Utils/ResolvablePromise.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

using namespace Valdi;

namespace ValdiTest {

namespace {

struct TrackingState {
    bool destroyed = false;
    bool succeeded = false;
    bool failed = false;
    std::string failureMessage;
    int32_t failureCode = 0;

    bool invoked() const {
        return succeeded || failed;
    }
};

class TrackingPromiseCallback final : public PromiseCallback {
public:
    explicit TrackingPromiseCallback(TrackingState& state) : _state(state) {}

    ~TrackingPromiseCallback() final {
        _state.destroyed = true;
    }

    void onSuccess(const Value& value) final {
        _state.succeeded = true;
    }

    void onFailure(const Error& error) final {
        _state.failed = true;
        _state.failureMessage = std::string(error.getMessage().toStringView());
        _state.failureCode = error.getErrorCode();
    }

private:
    TrackingState& _state;
};

} // namespace

TEST(ResolvablePromiseTests, cancelForwardsCanceledFailureToRegisteredCallbacks) {
    TrackingState state;

    auto promise = makeShared<ResolvablePromise>();
    {
        auto callback = makeShared<TrackingPromiseCallback>(state);
        promise->onComplete(callback);
    }
    ASSERT_FALSE(state.destroyed) << "a pending promise should retain its registered callbacks";

    promise->cancel();

    EXPECT_TRUE(state.failed) << "cancel should forward a failure to registered callbacks";
    EXPECT_FALSE(state.succeeded);
    EXPECT_EQ(state.failureMessage, "Promise canceled");
    EXPECT_EQ(state.failureCode, kPromiseCanceledErrorCode);
    // Moving the callbacks out of _callbacks still releases them, so a callback that retains
    // the promise does not leak it (MEM-98539).
    EXPECT_TRUE(state.destroyed) << "cancel should release registered callbacks";
}

TEST(ResolvablePromiseTests, cancelForwardsProducerResultWhenCancelCallbackSettles) {
    TrackingState state;

    auto promise = makeShared<ResolvablePromise>();
    auto* promisePtr = promise.get();
    promise->setCancelCallback([promisePtr]() { promisePtr->fulfill(Error("real failure")); });
    {
        auto callback = makeShared<TrackingPromiseCallback>(state);
        promise->onComplete(callback);
    }

    promise->cancel();

    EXPECT_TRUE(state.failed) << "the producer's terminal result should reach the callbacks";
    EXPECT_EQ(state.failureMessage, "real failure") << "cancel must not mask the producer's real result";
    EXPECT_EQ(state.failureCode, 0);
    EXPECT_TRUE(state.destroyed);
}

TEST(ResolvablePromiseTests, cancelForwardsProducerSuccessWhenCancelCallbackSettles) {
    TrackingState state;

    auto promise = makeShared<ResolvablePromise>();
    auto* promisePtr = promise.get();
    promise->setCancelCallback([promisePtr]() { promisePtr->fulfill(Value::undefined()); });
    {
        auto callback = makeShared<TrackingPromiseCallback>(state);
        promise->onComplete(callback);
    }

    promise->cancel();

    EXPECT_TRUE(state.succeeded) << "a producer settling successfully during cancel should reach the callbacks";
    EXPECT_FALSE(state.failed);
    EXPECT_TRUE(state.destroyed);
}

TEST(ResolvablePromiseTests, cancelOnChainedPromiseForwardsUpstreamResult) {
    // Regression (see PR #48811 / #117404 history): canceling a promise chained onto an upstream
    // producer propagates the cancel upstream; when the producer answers by settling, the consumer
    // must receive that real result, not a synthetic canceled error.
    TrackingState state;

    auto upstream = makeShared<ResolvablePromise>();
    auto* upstreamPtr = upstream.get();
    upstream->setCancelCallback([upstreamPtr]() { upstreamPtr->fulfill(Error("upstream failure")); });

    auto downstream = makeShared<ResolvablePromise>();
    downstream->fulfillWithPromiseResult(upstream);
    {
        auto callback = makeShared<TrackingPromiseCallback>(state);
        downstream->onComplete(callback);
    }

    downstream->cancel();

    EXPECT_TRUE(state.failed);
    EXPECT_EQ(state.failureMessage, "upstream failure") << "the upstream's real result should reach the consumer";
    EXPECT_EQ(state.failureCode, 0);
    EXPECT_TRUE(state.destroyed);
}

TEST(ResolvablePromiseTests, cancelOnChainedPromiseForwardsCanceledFailureWhenUpstreamDoesNotSettle) {
    TrackingState state;

    auto upstream = makeShared<ResolvablePromise>();
    upstream->setCancelCallback([]() {
        // Producer acknowledges the cancel but never settles.
    });

    auto downstream = makeShared<ResolvablePromise>();
    downstream->fulfillWithPromiseResult(upstream);
    {
        auto callback = makeShared<TrackingPromiseCallback>(state);
        downstream->onComplete(callback);
    }

    downstream->cancel();

    EXPECT_TRUE(state.failed) << "an unsettled cancel must still settle the consumer";
    EXPECT_EQ(state.failureMessage, "Promise canceled");
    EXPECT_EQ(state.failureCode, kPromiseCanceledErrorCode);
    EXPECT_TRUE(state.destroyed);
}

TEST(ResolvablePromiseTests, onCompleteAfterCancelIsServedTheCanceledFailure) {
    TrackingState state;

    auto promise = makeShared<ResolvablePromise>();
    promise->cancel();

    {
        auto callback = makeShared<TrackingPromiseCallback>(state);
        promise->onComplete(callback);
    }

    EXPECT_TRUE(state.failed) << "a late registrant on a canceled promise must not hang";
    EXPECT_EQ(state.failureMessage, "Promise canceled");
    EXPECT_EQ(state.failureCode, kPromiseCanceledErrorCode);
    EXPECT_TRUE(state.destroyed);
}

TEST(ResolvablePromiseTests, fulfillAfterCancelIsDropped) {
    // A producer that settles asynchronously after cancel() returned must not assert or re-notify:
    // the synthetic canceled failure already reached (and released) the callbacks.
    TrackingState state;

    auto promise = makeShared<ResolvablePromise>();
    {
        auto callback = makeShared<TrackingPromiseCallback>(state);
        promise->onComplete(callback);
    }

    promise->cancel();
    ASSERT_TRUE(state.failed);
    ASSERT_EQ(state.failureCode, kPromiseCanceledErrorCode);

    promise->fulfill(Value::undefined());

    EXPECT_FALSE(state.succeeded) << "a late producer settle after cancel must be dropped";

    TrackingState lateState;
    {
        auto callback = makeShared<TrackingPromiseCallback>(lateState);
        promise->onComplete(callback);
    }
    EXPECT_TRUE(lateState.failed) << "the recorded result should remain the canceled failure";
    EXPECT_EQ(lateState.failureCode, kPromiseCanceledErrorCode);
}

TEST(ResolvablePromiseTests, cancelAfterFulfillIsANoOp) {
    TrackingState state;

    auto promise = makeShared<ResolvablePromise>();
    {
        auto callback = makeShared<TrackingPromiseCallback>(state);
        promise->onComplete(callback);
    }

    promise->fulfill(Value::undefined());
    ASSERT_TRUE(state.succeeded);

    promise->cancel();

    EXPECT_FALSE(state.failed) << "cancel after settle must not re-notify";

    TrackingState lateState;
    {
        auto callback = makeShared<TrackingPromiseCallback>(lateState);
        promise->onComplete(callback);
    }
    EXPECT_TRUE(lateState.succeeded) << "the recorded result should remain the fulfilled value";
}

TEST(ResolvablePromiseTests, onCompleteBetweenCancelPhasesIsDrainedByCancel) {
    // cancel() releases the mutex to run the producer's cancel callback before recording a result,
    // so a callback registered from inside that window lands in _callbacks with no result yet. The
    // second phase has to drain it, otherwise it is retained forever and its awaiter hangs.
    TrackingState state;

    auto promise = makeShared<ResolvablePromise>();
    auto* promisePtr = promise.get();
    promise->setCancelCallback([promisePtr, &state]() {
        auto callback = makeShared<TrackingPromiseCallback>(state);
        promisePtr->onComplete(callback);
    });

    promise->cancel();

    EXPECT_TRUE(state.failed) << "a callback registered between cancel's two phases must not hang";
    EXPECT_EQ(state.failureCode, kPromiseCanceledErrorCode);
    EXPECT_TRUE(state.destroyed) << "and must still be released";
}

TEST(ResolvablePromiseTests, setCancelCallbackAfterCancelFiresImmediately) {
    auto promise = makeShared<ResolvablePromise>();
    promise->cancel();

    bool cancelCallbackFired = false;
    promise->setCancelCallback([&cancelCallbackFired]() { cancelCallbackFired = true; });

    EXPECT_TRUE(cancelCallbackFired)
        << "a producer attaching to an already-canceled promise must still be told to cancel";
}

TEST(ResolvablePromiseTests, fulfillReleasesRegisteredCallbacks) {
    TrackingState state;

    auto promise = makeShared<ResolvablePromise>();
    {
        auto callback = makeShared<TrackingPromiseCallback>(state);
        promise->onComplete(callback);
    }
    ASSERT_FALSE(state.destroyed);

    promise->fulfill(Value::undefined());

    EXPECT_TRUE(state.destroyed) << "fulfill should release registered callbacks";
    EXPECT_TRUE(state.invoked()) << "fulfill should forward to registered callbacks";
}

} // namespace ValdiTest
