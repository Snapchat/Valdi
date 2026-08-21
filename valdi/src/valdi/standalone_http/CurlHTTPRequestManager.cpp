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
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Valdi {

namespace {

constexpr int kMaxRedirects = 10;
constexpr long kConnectTimeoutSeconds = 30;
// curl_multi_poll waits for the shorter of this and the multi handle's own next timer, so an
// active transfer is still serviced on curl's schedule. This only bounds how long an idle thread
// sits before rechecking. New work and shutdown both wake the poll, so a short value buys nothing.
constexpr int kPollTimeoutMs = 10000;

// The curl command line tool reads these, libcurl does not, so we honour them here to let a
// caller point at their own trust store.
const char* const kCaBundleVariables[] = {
    "CURL_CA_BUNDLE",
    "SSL_CERT_FILE",
};

// Only distributions the @curl build defaults miss. @curl already compiles CURL_CA_BUNDLE with
// the macOS and Debian-family paths (curl+/BUILD.bazel:341-346), so libcurl applies those itself,
// and probing for them here would override a deliberate --@curl//:ca_bundle.
const char* const kCaBundleCandidates[] = {
    "/etc/pki/tls/certs/ca-bundle.crt",        // RHEL, Fedora
    "/etc/ssl/ca-bundle.pem",                  // openSUSE
};

std::string resolveCaBundle(const StringBox& configured) {
    if (!configured.isEmpty()) {
        return std::string(configured.toStringView());
    }

    for (const char* variable : kCaBundleVariables) {
        const char* value = std::getenv(variable);
        if (value != nullptr && *value != '\0') {
            return value;
        }
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

        // A cancelled request reports nothing back, matching the iOS and Android managers.
        dropCompletion();
    }

    // Shutdown uses this too. Completions reach JavaScript directly with no thread hop, so firing
    // one from the curl thread while another thread sits in join() destroying the manager would
    // enter the engine twice over. Neither platform manager promises a completion at teardown, so
    // there is nothing to report.
    void dropCompletion() {
        std::lock_guard<std::mutex> guard(_mutex);
        _completion = nullptr;
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

    // Rewritten in place as redirects are followed, so that each hop is issued from the same
    // record the first one was.
    snap::valdi_core::HTTPRequest request;
    int redirects = 0;
    std::atomic_bool cancelled{false};

    // The write callback fills this and the response takes it directly, so the payload is never
    // copied. ByteBuffer grows to the next power of two, so appending stays amortised constant
    // time with no reserve up front. Reserving would size an allocation from a Content-Length the
    // server chose.
    Ref<ByteBuffer> responseBody = makeShared<ByteBuffer>();
    Value responseHeaders;
    curl_slist* requestHeaders = nullptr;

private:
    std::mutex _mutex;
    std::shared_ptr<snap::valdi_core::HTTPRequestManagerCompletion> _completion;
};

size_t writeBodyCallback(char* data, size_t size, size_t count, void* userData) {
    auto* task = static_cast<CurlTask*>(userData);
    task->responseBody->append(data, data + size * count);
    return size * count;
}

size_t writeHeaderCallback(char* data, size_t size, size_t count, void* userData) {
    auto* task = static_cast<CurlTask*>(userData);

    std::string line(data, size * count);

    // Check this before looking for a colon, because a reason phrase is free-form text and may
    // contain one. The reset discards anything an informational response carried, since curl hands
    // a 1xx header block to this callback too and a 103 Early Hints brings real headers with it.
    if (line.rfind("HTTP/", 0) == 0) {
        task->responseHeaders = Value();
        return size * count;
    }

    auto separator = line.find(':');
    if (separator == std::string::npos) {
        return size * count;
    }

    auto name = line.substr(0, separator);
    auto value = line.substr(separator + 1);

    auto isTrimmable = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!value.empty() && isTrimmable(value.front())) {
        value.erase(value.begin());
    }
    while (!value.empty() && isTrimmable(value.back())) {
        value.pop_back();
    }

    // Join repeated headers, matching what NSURLResponse hands back. The response header map is
    // string to string, so earlier values have nowhere else to go, and dropping them loses whole
    // Set-Cookie lines.
    auto existing = task->responseHeaders.getMapValue(std::string_view(name));
    if (!existing.isNullOrUndefined()) {
        value = std::string(existing.toStringBox().toStringView()) + ", " + value;
    }

    task->responseHeaders.setMapValue(std::string_view(name), Value(StringBox::fromString(value)));

    return size * count;
}

int progressCallback(void* userData, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    auto* task = static_cast<CurlTask*>(userData);
    return task->cancelled.load() ? 1 : 0;
}

class CurlHTTPRequestManager : public snap::valdi_core::HTTPRequestManager {
public:
    CurlHTTPRequestManager(std::string caBundle, int32_t idleTimeoutSeconds, CURLcode globalInit)
        : _caBundle(std::move(caBundle)), _idleTimeoutSeconds(idleTimeoutSeconds), _globalInit(globalInit) {
        if (_globalInit != CURLE_OK) {
            // Carrying on would leave curl_easy_init handing back handles whose TLS backend was
            // never set up, so every HTTPS request would fail with an unrelated-looking error.
            return;
        }

        _multi = curl_multi_init();
        if (_multi == nullptr) {
            return;
        }
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

        // Check this before the multi handle, so the reported cause is the one that actually failed.
        if (_globalInit != CURLE_OK) {
            task->complete(Error(StringBox::fromString(std::string("Failed to initialise libcurl: ") +
                                                       curl_easy_strerror(_globalInit))));
            return task;
        }

        if (_multi == nullptr) {
            task->complete(Error(STRING_LITERAL("Failed to create a curl multi handle")));
            return task;
        }

        bool stopping = false;
        {
            std::lock_guard<std::mutex> guard(_mutex);
            stopping = _stopping;
            if (!stopping) {
                _pending.push_back(task);
            }
        }

        // Fail outside the lock, because a completion is free to queue another request and _mutex
        // is not recursive. Queueing the task instead would strand it: run() has already drained
        // _pending for the last time and nothing will service it again.
        if (stopping) {
            task->complete(Error(STRING_LITERAL("Request manager shutting down")));
            return task;
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
                if (_stopping) {
                    break;
                }
                pending.swap(_pending);
            }

            for (const auto& task : pending) {
                addTask(task);
            }

            int running = 0;
            curl_multi_perform(_multi, &running);

            // This has to run before the poll. perform is what queues CURLMSG_DONE, and once
            // nothing is running curl has no timeout to report, so the poll would sleep out its
            // full timeout before a finished transfer got reported.
            drainMessages();

            int numfds = 0;
            curl_multi_poll(_multi, nullptr, 0, kPollTimeoutMs, &numfds);
        }

        // Drop outstanding work instead of failing it; see CurlTask::dropCompletion.
        for (auto& entry : _active) {
            entry.second->dropCompletion();
            curl_multi_remove_handle(_multi, entry.first);
            releaseHandle(entry.first, entry.second);
        }
        _active.clear();

        std::vector<std::shared_ptr<CurlTask>> pending;
        {
            std::lock_guard<std::mutex> guard(_mutex);
            pending.swap(_pending);
        }
        for (const auto& task : pending) {
            task->dropCompletion();
        }
    }

    void addTask(const std::shared_ptr<CurlTask>& task) {
        auto* easy = curl_easy_init();
        if (easy == nullptr) {
            task->complete(Error(STRING_LITERAL("Failed to create a curl handle")));
            return;
        }

        const auto& request = task->request;

        curl_easy_setopt(easy, CURLOPT_URL, std::string(request.url.toStringView()).c_str());

        if (request.body) {
            const auto& body = request.body.value();
            curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
            curl_easy_setopt(easy, CURLOPT_COPYPOSTFIELDS, reinterpret_cast<const char*>(body.data()));
        }

        // Methods curl models itself are set through their own options so that its redirect
        // handling knows what the request is. CURLOPT_CUSTOMREQUEST only rewrites the request
        // line and leaves behaviour alone, which is why it is reserved for the verbs curl has no
        // option for.
        auto method = std::string(request.method.toStringView());
        if (method == "HEAD") {
            curl_easy_setopt(easy, CURLOPT_NOBODY, 1L);
        } else if (method == "POST") {
            curl_easy_setopt(easy, CURLOPT_POST, 1L);
            if (!request.body) {
                // Without fields curl reads the body from the read callback, which is stdin.
                curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(0));
                curl_easy_setopt(easy, CURLOPT_COPYPOSTFIELDS, "");
            }
        } else if (method.empty() || method == "GET") {
            if (request.body) {
                // The fields above turned this into a POST; name GET to keep the request line.
                curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, "GET");
            } else {
                curl_easy_setopt(easy, CURLOPT_HTTPGET, 1L);
            }
        } else {
            curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, method.c_str());
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

        curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, kConnectTimeoutSeconds);
        curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);

        // Without this, abandoning a name lookup joins the resolver thread, and getaddrinfo cannot
        // be interrupted. Cancelling or shutting down mid-lookup would block this thread until the
        // resolver gave up, taking every other request with it. curl detaches the thread instead,
        // and it frees its own state once the lookup returns.
        curl_easy_setopt(easy, CURLOPT_QUICK_EXIT, 1L);

        if (_idleTimeoutSeconds > 0) {
            // An inactivity timeout rather than an overall cap: below one byte a second for this
            // long counts as stalled, so a slow but progressing download is left alone.
            curl_easy_setopt(easy, CURLOPT_LOW_SPEED_LIMIT, 1L);
            curl_easy_setopt(easy, CURLOPT_LOW_SPEED_TIME, static_cast<long>(_idleTimeoutSeconds));
        }

        if (!_caBundle.empty()) {
            curl_easy_setopt(easy, CURLOPT_CAINFO, _caBundle.c_str());
        }

        auto added = curl_multi_add_handle(_multi, easy);
        if (added != CURLM_OK) {
            finish(easy, task, Error(StringBox::fromCString(curl_multi_strerror(added))));
            return;
        }
        _active.emplace(easy, task);
    }

    // Redirects are followed here rather than by CURLOPT_FOLLOWLOCATION, because that option keeps
    // CURLOPT_CUSTOMREQUEST for the whole chain: Curl_http_method takes the request line from it
    // unconditionally, and the redirect handlers only ever touch curl's own idea of the method. A
    // DELETE would stay a DELETE across a 303 that has to become a GET, and a PUT would keep its
    // verb while curl dropped the body out from under it. The rewriting below is RFC 9110 15.4.
    static void retarget(CurlTask& task, long statusCode, const char* location) {
        task.request.url = StringBox::fromCString(location);

        auto method = std::string(task.request.method.toStringView());
        bool toGet = statusCode == 303 ? (method != "GET" && method != "HEAD")
                                      : ((statusCode == 301 || statusCode == 302) && method == "POST");
        if (toGet) {
            task.request.method = STRING_LITERAL("GET");
            task.request.body.reset();
        }

        // curl only withholds a redirect's body from the write callback when it is following the
        // redirect itself, so this hop's is ours to discard.
        task.responseBody->clear();
        task.responseHeaders = Value();
        task.redirects++;
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

                // Only set for a 3xx carrying a Location, and curl has already resolved it against
                // the request URL, so a relative target arrives absolute.
                char* location = nullptr;
                curl_easy_getinfo(easy, CURLINFO_REDIRECT_URL, &location);
                if (location != nullptr) {
                    if (task->redirects >= kMaxRedirects) {
                        finish(easy, task, Error(STRING_LITERAL("Maximum number of redirects followed")));
                        continue;
                    }
                    retarget(*task, statusCode, location);
                    releaseHandle(easy, task);
                    // curl_multi_add_handle asks for this handle to run immediately, so the next hop
                    // is serviced on the following pass rather than after a poll timeout.
                    addTask(task);
                    continue;
                }

                snap::valdi_core::HTTPResponse response(static_cast<int32_t>(statusCode),
                                                        task->responseHeaders,
                                                        {task->responseBody->toBytesView()});
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
        releaseHandle(easy, task);
    }

    void releaseHandle(CURL* easy, const std::shared_ptr<CurlTask>& task) {
        if (task->requestHeaders != nullptr) {
            curl_slist_free_all(task->requestHeaders);
            task->requestHeaders = nullptr;
        }
        curl_easy_cleanup(easy);
    }

    std::string _caBundle;
    int32_t _idleTimeoutSeconds = 0;
    CURLcode _globalInit = CURLE_OK;
    CURLM* _multi = nullptr;
    std::thread _thread;
    std::mutex _mutex;
    bool _stopping = false;
    std::vector<std::shared_ptr<CurlTask>> _pending;
    std::unordered_map<CURL*, std::shared_ptr<CurlTask>> _active;
};

} // namespace

Shared<snap::valdi_core::HTTPRequestManager> makeCurlHTTPRequestManager(const StringBox& caBundlePath,
                                                                       int32_t idleTimeoutSeconds) {
    static CURLcode globalInitResult = CURLE_OK;
    static std::once_flag globalInit;
    std::call_once(globalInit, []() { globalInitResult = curl_global_init(CURL_GLOBAL_DEFAULT); });

    return std::make_shared<CurlHTTPRequestManager>(
        resolveCaBundle(caBundlePath), idleTimeoutSeconds, globalInitResult);
}

} // namespace Valdi
