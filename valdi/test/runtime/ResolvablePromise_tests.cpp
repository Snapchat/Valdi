//
//  ResolvablePromise_tests.cpp
//  valdi-pc
//

#include <gtest/gtest.h>

#include "valdi_core/cpp/Utils/ResolvablePromise.hpp"
#include "valdi_core/cpp/Utils/Shared.hpp"

using namespace Valdi;

namespace ValdiTest {

namespace {

class TrackingPromiseCallback final : public PromiseCallback {
public:
    TrackingPromiseCallback(bool& destroyed, bool& invoked) : _destroyed(destroyed), _invoked(invoked) {}

    ~TrackingPromiseCallback() final {
        _destroyed = true;
    }

    void onSuccess(const Value& value) final {
        _invoked = true;
    }

    void onFailure(const Error& error) final {
        _invoked = true;
    }

private:
    bool& _destroyed;
    bool& _invoked;
};

} // namespace

TEST(ResolvablePromiseTests, cancelReleasesRegisteredCallbacks) {
    // Regression: cancel cleared _cancelCallback but not _callbacks, and onComplete never
    // drains them once canceled. A callback that retains the promise then leaked it.
    bool destroyed = false;
    bool invoked = false;

    auto promise = makeShared<ResolvablePromise>();
    {
        auto callback = makeShared<TrackingPromiseCallback>(destroyed, invoked);
        promise->onComplete(callback);
    }
    ASSERT_FALSE(destroyed) << "a pending promise should retain its registered callbacks";

    promise->cancel();

    EXPECT_TRUE(destroyed) << "cancel should release registered callbacks";
    EXPECT_FALSE(invoked) << "cancel should drop callbacks, not forward to them";
}

TEST(ResolvablePromiseTests, fulfillReleasesRegisteredCallbacks) {
    bool destroyed = false;
    bool invoked = false;

    auto promise = makeShared<ResolvablePromise>();
    {
        auto callback = makeShared<TrackingPromiseCallback>(destroyed, invoked);
        promise->onComplete(callback);
    }
    ASSERT_FALSE(destroyed);

    promise->fulfill(Value::undefined());

    EXPECT_TRUE(destroyed) << "fulfill should release registered callbacks";
    EXPECT_TRUE(invoked) << "fulfill should forward to registered callbacks";
}

} // namespace ValdiTest
