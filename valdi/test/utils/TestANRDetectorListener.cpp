#include "TestANRDetectorListener.hpp"
#include "valdi/runtime/Utils/AsyncGroup.hpp"
#include "valdi_core/cpp/Utils/Exception.hpp"

using namespace Valdi;

namespace ValdiTest {

TestANRDetectorListener::TestANRDetectorListener() = default;
TestANRDetectorListener::~TestANRDetectorListener() = default;

Valdi::JavaScriptANR TestANRDetectorListener::waitForANR() {
    std::unique_lock<Mutex> lock(_mutex);
    if (_anr) {
        return _anr.value();
    }

    auto group = makeShared<AsyncGroup>();
    group->enter();
    _callback = [group]() { group->leave(); };

    lock.unlock();

    if (!group->blockingWaitWithTimeout(std::chrono::seconds(5))) {
        throw Exception("Failed to wait for ANR");
    }

    lock.lock();

    return _anr.value();
}

Valdi::JavaScriptANRBehavior TestANRDetectorListener::onANR(Valdi::JavaScriptANR anr) {
    std::lock_guard<Mutex> lock(_mutex);
    _anr = {std::move(anr)};
    auto callback = std::move(_callback);
    if (callback) {
        callback();
    }

    return Valdi::JavaScriptANRBehavior::KEEP_GOING;
}

std::optional<Valdi::JavaScriptANR> TestANRDetectorListener::getLastANR() {
    std::lock_guard<Mutex> lock(_mutex);
    return _anr;
}

} // namespace ValdiTest
