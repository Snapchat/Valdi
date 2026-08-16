#include "valdi/standalone_http/CurlHTTPRequestManager.hpp"

#include "valdi_core/Cancelable.hpp"
#include "valdi_core/HTTPRequest.hpp"
#include "valdi_core/HTTPRequestManagerCompletion.hpp"
#include "valdi_core/HTTPResponse.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/Error.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

#include <curl/curl.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Valdi {

namespace {

constexpr long kMaxRedirects = 10;
constexpr long kConnectTimeoutSeconds = 30;
constexpr int kPollTimeoutMs = 200;

const char* const kCaBundleCandidates[] = {
    "/etc/ssl/cert.pem",                       // macOS, Alpine
    "/etc/ssl/certs/ca-certificates.crt",      // Debian, Ubuntu
    "/etc/pki/tls/certs/ca-bundle.crt",        // RHEL, Fedora
    "/etc/ssl/ca-bundle.pem",                  // openSUSE
};

std::string resolveCaBundle(const StringBox& configured) {
    if (!configured.isEmpty()) {
        return std::string(configured.toStringView());
    }

    for (const char* candidate : kCaBundleCandidates) {
        struct stat info;
        if (stat(candidate, &info) == 0 && S_ISREG(info.st_mode)) {
            return candidate;
        }
    }

    return {};
}

class CurlTask : public snap::valdi_core::Cancelable {
public:
    CurlTask(snap::valdi_core::HTTPRequest request,
             std::shared_ptr<snap::valdi_core::HTTPRequestManagerCompletion> completion)
        : request(std::move(request)), _completion(std::move(completion)) {}

    void cancel() override {
        cancelled.store(true);
    }

    void complete(const Result<snap::valdi_core::HTTPResponse>& result) {
        std::shared_ptr<snap::valdi_core::HTTPRequestManagerCompletion> completion;
        {
            std::lock_guard<std::mutex> guard(_mutex);
            completion = std::move(_completion);
        }

        if (completion == nullptr) {
            return;
        }

        if (result.success()) {
            completion->onComplete(result.value());
        } else {
            completion->onFail(result.error().toString());
        }
    }

    snap::valdi_core::HTTPRequest request;
    std::atomic_bool cancelled{false};

    std::string responseBody;
    Value responseHeaders;
    curl_slist* requestHeaders = nullptr;

private:
    std::mutex _mutex;
    std::shared_ptr<snap::valdi_core::HTTPRequestManagerCompletion> _completion;
};

size_t writeBodyCallback(char* data, size_t size, size_t count, void* userData) {
    auto* task = static_cast<CurlTask*>(userData);
    task->responseBody.append(data, size * count);
    return size * count;
}

size_t writeHeaderCallback(char* data, size_t size, size_t count, void* userData) {
    auto* task = static_cast<CurlTask*>(userData);

    std::string line(data, size * count);
    auto separator = line.find(':');
    if (separator != std::string::npos) {
        auto name = line.substr(0, separator);
        auto value = line.substr(separator + 1);

        auto isTrimmable = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
        while (!value.empty() && isTrimmable(value.front())) {
            value.erase(value.begin());
        }
        while (!value.empty() && isTrimmable(value.back())) {
            value.pop_back();
        }

        task->responseHeaders.setMapValue(std::string_view(name), Value(StringBox::fromString(value)));
    }

    return size * count;
}

int progressCallback(void* userData, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    auto* task = static_cast<CurlTask*>(userData);
    return task->cancelled.load() ? 1 : 0;
}

class CurlHTTPRequestManager : public snap::valdi_core::HTTPRequestManager {
public:
    explicit CurlHTTPRequestManager(std::string caBundle) : _caBundle(std::move(caBundle)) {
        _multi = curl_multi_init();
        _thread = std::thread([this]() { run(); });
    }

    ~CurlHTTPRequestManager() override {
        {
            std::lock_guard<std::mutex> guard(_mutex);
            _stopping = true;
        }
        curl_multi_wakeup(_multi);
        if (_thread.joinable()) {
            _thread.join();
        }
        curl_multi_cleanup(_multi);
    }

    std::shared_ptr<snap::valdi_core::Cancelable> performRequest(
        const snap::valdi_core::HTTPRequest& request,
        const std::shared_ptr<snap::valdi_core::HTTPRequestManagerCompletion>& completion) override {
        auto task = std::make_shared<CurlTask>(request, completion);

        {
            std::lock_guard<std::mutex> guard(_mutex);
            _pending.push_back(task);
        }
        curl_multi_wakeup(_multi);

        return task;
    }

private:
    void run() {
        while (true) {
            std::vector<std::shared_ptr<CurlTask>> pending;
            {
                std::lock_guard<std::mutex> guard(_mutex);
                if (_stopping && _active.empty() && _pending.empty()) {
                    break;
                }
                pending.swap(_pending);
            }

            for (const auto& task : pending) {
                addTask(task);
            }

            int running = 0;
            curl_multi_perform(_multi, &running);

            int numfds = 0;
            curl_multi_poll(_multi, nullptr, 0, kPollTimeoutMs, &numfds);

            drainMessages();
        }

        for (auto& entry : _active) {
            curl_multi_remove_handle(_multi, entry.first);
            finish(entry.first, entry.second, Error(STRING_LITERAL("Request manager shutting down")));
        }
        _active.clear();
    }

    void addTask(const std::shared_ptr<CurlTask>& task) {
        auto* easy = curl_easy_init();
        if (easy == nullptr) {
            task->complete(Error(STRING_LITERAL("Failed to create a curl handle")));
            return;
        }

        const auto& request = task->request;

        curl_easy_setopt(easy, CURLOPT_URL, std::string(request.url.toStringView()).c_str());

        auto method = std::string(request.method.toStringView());
        if (method == "HEAD") {
            curl_easy_setopt(easy, CURLOPT_NOBODY, 1L);
        } else if (!method.empty() && method != "GET") {
            curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, method.c_str());
        }

        if (request.body) {
            const auto& body = request.body.value();
            curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
            curl_easy_setopt(easy, CURLOPT_COPYPOSTFIELDS, reinterpret_cast<const char*>(body.data()));
        }

        for (const auto& key : request.headers.sortedMapKeys()) {
            auto header = std::string(key.toStringView()) + ": " +
                          std::string(request.headers.getMapValue(key).toStringBox().toStringView());
            task->requestHeaders = curl_slist_append(task->requestHeaders, header.c_str());
        }
        if (task->requestHeaders != nullptr) {
            curl_easy_setopt(easy, CURLOPT_HTTPHEADER, task->requestHeaders);
        }

        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, writeBodyCallback);
        curl_easy_setopt(easy, CURLOPT_WRITEDATA, task.get());
        curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, writeHeaderCallback);
        curl_easy_setopt(easy, CURLOPT_HEADERDATA, task.get());
        curl_easy_setopt(easy, CURLOPT_XFERINFOFUNCTION, progressCallback);
        curl_easy_setopt(easy, CURLOPT_XFERINFODATA, task.get());
        curl_easy_setopt(easy, CURLOPT_NOPROGRESS, 0L);

        curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(easy, CURLOPT_MAXREDIRS, kMaxRedirects);
        curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, kConnectTimeoutSeconds);
        curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);

        if (!_caBundle.empty()) {
            curl_easy_setopt(easy, CURLOPT_CAINFO, _caBundle.c_str());
        }

        _active.emplace(easy, task);
        curl_multi_add_handle(_multi, easy);
    }

    void drainMessages() {
        int remaining = 0;
        while (auto* message = curl_multi_info_read(_multi, &remaining)) {
            if (message->msg != CURLMSG_DONE) {
                continue;
            }

            auto* easy = message->easy_handle;
            auto found = _active.find(easy);
            if (found == _active.end()) {
                curl_multi_remove_handle(_multi, easy);
                curl_easy_cleanup(easy);
                continue;
            }

            auto task = found->second;
            _active.erase(found);
            curl_multi_remove_handle(_multi, easy);

            if (message->data.result == CURLE_OK) {
                long statusCode = 0;
                curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &statusCode);

                snap::valdi_core::HTTPResponse response(static_cast<int32_t>(statusCode),
                                                        task->responseHeaders,
                                                        {makeShared<ByteBuffer>(task->responseBody)->toBytesView()});
                finishHandle(easy, task, Result<snap::valdi_core::HTTPResponse>(response));
            } else if (message->data.result == CURLE_ABORTED_BY_CALLBACK) {
                finish(easy, task, Error(STRING_LITERAL("Request was cancelled")));
            } else {
                finish(easy, task, Error(StringBox::fromCString(curl_easy_strerror(message->data.result))));
            }
        }
    }

    void finish(CURL* easy, const std::shared_ptr<CurlTask>& task, Error&& error) {
        finishHandle(easy, task, Result<snap::valdi_core::HTTPResponse>(std::move(error)));
    }

    void finishHandle(CURL* easy,
                      const std::shared_ptr<CurlTask>& task,
                      const Result<snap::valdi_core::HTTPResponse>& result) {
        task->complete(result);

        if (task->requestHeaders != nullptr) {
            curl_slist_free_all(task->requestHeaders);
            task->requestHeaders = nullptr;
        }
        curl_easy_cleanup(easy);
    }

    std::string _caBundle;
    CURLM* _multi = nullptr;
    std::thread _thread;
    std::mutex _mutex;
    bool _stopping = false;
    std::vector<std::shared_ptr<CurlTask>> _pending;
    std::unordered_map<CURL*, std::shared_ptr<CurlTask>> _active;
};

} // namespace

Shared<snap::valdi_core::HTTPRequestManager> makeCurlHTTPRequestManager(const StringBox& caBundlePath) {
    static std::once_flag globalInit;
    std::call_once(globalInit, []() { curl_global_init(CURL_GLOBAL_DEFAULT); });

    return std::make_shared<CurlHTTPRequestManager>(resolveCaBundle(caBundlePath));
}

} // namespace Valdi
