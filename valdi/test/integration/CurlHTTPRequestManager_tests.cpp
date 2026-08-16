#include "valdi/standalone_http/CurlHTTPRequestManager.hpp"

#include "valdi_core/Cancelable.hpp"
#include "valdi_core/HTTPRequest.hpp"
#include "valdi_core/HTTPRequestManager.hpp"
#include "valdi_core/HTTPRequestManagerCompletion.hpp"
#include "valdi_core/HTTPResponse.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

#include "gtest/gtest.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>

using namespace Valdi;

namespace ValdiTest {

namespace {

class RecordingCompletion : public snap::valdi_core::HTTPRequestManagerCompletion {
public:
    void onComplete(const snap::valdi_core::HTTPResponse& response) override {
        std::lock_guard<std::mutex> guard(_mutex);
        _statusCode = response.statusCode;
        _bodySize = response.body ? response.body.value().size() : 0;
        _done = true;
        _condition.notify_all();
    }

    void onFail(const std::string& error) override {
        std::lock_guard<std::mutex> guard(_mutex);
        _error = error;
        _done = true;
        _condition.notify_all();
    }

    bool waitForCompletion(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(_mutex);
        return _condition.wait_for(lock, timeout, [this]() { return _done; });
    }

    std::optional<int32_t> statusCode() const {
        std::lock_guard<std::mutex> guard(_mutex);
        return _statusCode;
    }

    std::optional<std::string> error() const {
        std::lock_guard<std::mutex> guard(_mutex);
        return _error;
    }

    size_t bodySize() const {
        std::lock_guard<std::mutex> guard(_mutex);
        return _bodySize;
    }

private:
    mutable std::mutex _mutex;
    std::condition_variable _condition;
    bool _done = false;
    std::optional<int32_t> _statusCode;
    std::optional<std::string> _error;
    size_t _bodySize = 0;
};

snap::valdi_core::HTTPRequest makeGet(const char* url) {
    return snap::valdi_core::HTTPRequest(StringBox::fromCString(url), STRING_LITERAL("GET"), Value(), std::nullopt, 0);
}

} // namespace

TEST(CurlHTTPRequestManagerTests, reportsFailureForAnUnresolvableHost) {
    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();

    auto cancelable = manager->performRequest(makeGet("http://this-host-does-not-exist.invalid/"), completion);
    ASSERT_NE(cancelable, nullptr);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(30)));
    ASSERT_FALSE(completion->statusCode().has_value());
    ASSERT_TRUE(completion->error().has_value());
}

TEST(CurlHTTPRequestManagerTests, returnsACancelableThatDoesNotDeadlock) {
    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();

    auto cancelable = manager->performRequest(makeGet("http://this-host-does-not-exist.invalid/"), completion);
    ASSERT_NE(cancelable, nullptr);
    cancelable->cancel();

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(30)));
}

TEST(CurlHTTPRequestManagerTests, doesNotBlockShutdownWithRequestsInFlight) {
    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();

    manager->performRequest(makeGet("http://this-host-does-not-exist.invalid/"), completion);
    manager.reset();

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(30)));
}

} // namespace ValdiTest
