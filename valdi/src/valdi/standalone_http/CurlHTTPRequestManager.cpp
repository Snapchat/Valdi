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
#include "valdi_core/cpp/Utils/ValueMap.hpp"

#include <curl/curl.h>

#include <atomic>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <strings.h>
#include <sys/stat.h>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Valdi {

namespace {

constexpr int kMaxRedirects = 10;
constexpr long kConnectTimeoutSeconds = 30;
// Only bounds how long an idle thread sits: curl_multi_poll returns at the sooner of this and the
// multi handle's own next timer, and new work, cancellation and shutdown all wake it early.
constexpr int kPollTimeoutMs = 10000;

// The curl command line tool reads these, libcurl does not, so we honour them here to let a
// caller point at their own trust store.
const char* const kCaBundleVariables[] = {
    "CURL_CA_BUNDLE",
    "SSL_CERT_FILE",
};

// Only the distributions @curl's own compiled-in CURL_CA_BUNDLE misses, since libcurl applies that
// itself and probing for the same paths here would override a deliberate --@curl//:ca_bundle.
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

// Always a map, never null: HTTPTypes.d.ts declares headers as StringMap<string>, and iOS, Android
// and web all hand back {} when a response carried none.
Value emptyHeaderMap() {
    return Value(makeShared<ValueMap>());
}

// A cancelable can outlive the manager, since JavaScript may hold one and cancel after teardown.
// Tasks reach the multi handle through this rather than a back-pointer: the manager clears it once
// its thread has joined and before curl_multi_cleanup, so a late cancel finds nothing to poke.
class PollWaker {
public:
    explicit PollWaker(CURLM* multi) : _multi(multi) {}

    void wake() {
        std::lock_guard<std::mutex> guard(_mutex);
        if (_multi != nullptr) {
            curl_multi_wakeup(_multi);
        }
    }

    void detach() {
        std::lock_guard<std::mutex> guard(_mutex);
        _multi = nullptr;
    }

private:
    std::mutex _mutex;
    CURLM* _multi;
};

class CurlTask : public snap::valdi_core::Cancelable {
public:
    CurlTask(snap::valdi_core::HTTPRequest request,
             std::shared_ptr<snap::valdi_core::HTTPRequestManagerCompletion> completion,
             std::shared_ptr<PollWaker> waker)
        : request(std::move(request)), _completion(std::move(completion)), _waker(std::move(waker)) {
        // The caller's body views a live JavaScript ArrayBuffer, so it is copied here, on the thread
        // that called performRequest. Reading it from the curl thread would race whatever
        // JavaScript writes next. The view is then dropped so nothing downstream can reach for it.
        if (this->request.body) {
            const auto& body = this->request.body.value();
            requestBody = makeShared<ByteBuffer>(body.begin(), body.end());
            this->request.body.reset();
        }
    }

    void cancel() override {
        // Dropped before the flag goes up, because the curl thread turns an aborted transfer into a
        // "Request was cancelled" failure and must not find a completion still attached.
        dropCompletion();

        cancelled.store(true);

        // Nothing reads that flag until curl next runs the progress callback, and an idle transfer
        // leaves the poll parked for up to kPollTimeoutMs. Waking it frees the socket now.
        _waker->wake();
    }

    // Used by cancel() and by shutdown. Neither the iOS nor the Android manager promises a
    // completion for a cancelled or torn-down request, so there is nothing to report. Dropping is
    // the contract, not a thread-safety measure: a JS backed completion dispatches to the JS thread.
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

    // Null when the request has no body. This owns the payload for the life of the task, which
    // outlives every easy handle made from it, so curl can read it in place.
    Ref<ByteBuffer> requestBody;

    // The write callback fills this and the response takes it directly, so the payload is never
    // copied. No reserve up front: that would size an allocation from a Content-Length the server
    // chose.
    Ref<ByteBuffer> responseBody = makeShared<ByteBuffer>();
    Value responseHeaders = emptyHeaderMap();
    curl_slist* requestHeaders = nullptr;

private:
    std::mutex _mutex;
    std::shared_ptr<snap::valdi_core::HTTPRequestManagerCompletion> _completion;
    std::shared_ptr<PollWaker> _waker;
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
        task->responseHeaders = emptyHeaderMap();
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

    // Join repeated headers as NSURLResponse does. The map is string to string, so dropping earlier
    // values would lose whole Set-Cookie lines.
    auto existing = task->responseHeaders.getMapValue(std::string_view(name));
    if (!existing.isNullOrUndefined()) {
        value = std::string(existing.toStringBox().toStringView()) + ", " + value;
    }

    task->responseHeaders.setMapValue(std::string_view(name), Value(StringBox::fromString(value)));

    return size * count;
}

// curl writes custom headers and a custom request line out verbatim, and curl_slist_append takes a
// const char*, so a CR or LF injects a header or a whole second request, and a NUL truncates
// silently. Rejected rather than stripped, so a request never quietly means something else.
bool hasRequestControlCharacters(std::string_view text) {
    return text.find_first_of(std::string_view("\r\n\0", 3)) != std::string_view::npos;
}

// The message to fail with, or nullopt when every field is safe to serialize.
std::optional<std::string> findUnserializableField(const snap::valdi_core::HTTPRequest& request) {
    if (hasRequestControlCharacters(request.method.toStringView())) {
        return "The request method contains a carriage return, newline or NUL";
    }

    for (const auto& key : request.headers.sortedMapKeys()) {
        // Deliberately not naming it: the name is the thing carrying the newline.
        if (hasRequestControlCharacters(key.toStringView())) {
            return "A request header name contains a carriage return, newline or NUL";
        }

        if (hasRequestControlCharacters(request.headers.getMapValue(key).toStringBox().toStringView())) {
            return "The value of request header " + std::string(key.toStringView()) +
                   " contains a carriage return, newline or NUL";
        }
    }

    return std::nullopt;
}

bool equalsIgnoringCase(std::string_view value, std::string_view lowercase) {
    return value.size() == lowercase.size() && strncasecmp(value.data(), lowercase.data(), value.size()) == 0;
}

// This build has no compression codecs, so curl neither asks for an encoded response nor checks
// whether it got one: it only parses Content-Encoding when CURLOPT_ACCEPT_ENCODING is set. Matched
// case-insensitively, since the casing is the origin's and nothing here canonicalizes it.
std::optional<std::string> undecodableContentEncoding(const Value& headers) {
    if (!headers.isMap()) {
        return std::nullopt;
    }

    for (const auto& entry : *headers.getMap()) {
        if (!equalsIgnoringCase(entry.first.toStringView(), "content-encoding")) {
            continue;
        }

        auto encoding = std::string(entry.second.toStringBox().toStringView());

        // identity is a no-op, and the only codec this build has.
        if (encoding.empty() || equalsIgnoringCase(encoding, "identity")) {
            return std::nullopt;
        }
        return encoding;
    }

    return std::nullopt;
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
        _waker = std::make_shared<PollWaker>(_multi);
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
        // After the join, so the run loop still has a handle to poll, and before the cleanup, so a
        // cancel arriving from JavaScript from here on finds nothing rather than a freed handle.
        _waker->detach();
        curl_multi_cleanup(_multi);
    }

    std::shared_ptr<snap::valdi_core::Cancelable> performRequest(
        const snap::valdi_core::HTTPRequest& request,
        const std::shared_ptr<snap::valdi_core::HTTPRequestManagerCompletion>& completion) override {
        auto task = std::make_shared<CurlTask>(request, completion, _waker);

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
        const auto& request = task->request;

        // Checked before a handle exists, so a rejected request leaves nothing to clean up.
        if (auto rejection = findUnserializableField(request)) {
            task->complete(Error(StringBox::fromString(*rejection)));
            return;
        }

        auto* easy = curl_easy_init();
        if (easy == nullptr) {
            task->complete(Error(STRING_LITERAL("Failed to create a curl handle")));
            return;
        }

        curl_easy_setopt(easy, CURLOPT_URL, std::string(request.url.toStringView()).c_str());

        if (task->requestBody != nullptr) {
            curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(task->requestBody->size()));
            // Not COPYPOSTFIELDS: the task already owns this buffer for longer than the handle
            // lives, so letting curl copy it again would hold the payload twice.
            curl_easy_setopt(easy, CURLOPT_POSTFIELDS, reinterpret_cast<const char*>(task->requestBody->data()));
        }

        // Methods curl models itself go through their own options, so it sets the transfer up to
        // match: CURLOPT_POST arranges a body reader, CURLOPT_NOBODY suppresses reading one.
        // CUSTOMREQUEST only rewrites the request line, so it is left for verbs curl has no option
        // for.
        auto method = std::string(request.method.toStringView());
        if (method == "HEAD") {
            curl_easy_setopt(easy, CURLOPT_NOBODY, 1L);
        } else if (method == "POST") {
            curl_easy_setopt(easy, CURLOPT_POST, 1L);
            if (task->requestBody == nullptr) {
                // Without fields curl reads the body from the read callback, which is stdin.
                curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(0));
                curl_easy_setopt(easy, CURLOPT_COPYPOSTFIELDS, "");
            }
        } else if (method.empty() || method == "GET") {
            if (task->requestBody != nullptr) {
                // The fields above turned this into a POST; name GET to keep the request line.
                curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, "GET");
            } else {
                curl_easy_setopt(easy, CURLOPT_HTTPGET, 1L);
            }
        } else {
            curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, method.c_str());
        }

        for (const auto& key : request.headers.sortedMapKeys()) {
            auto value = std::string(request.headers.getMapValue(key).toStringBox().toStringView());

            // curl steps over the whitespace after a colon and then drops an empty valued header
            // entirely. Its "name;" form is the documented way to send one, and iOS and web both
            // do. The blank set mirrors curl's ISSPACE, so this catches exactly what it would drop.
            auto blank = value.find_first_not_of(" \t\n\v\f\r") == std::string::npos;
            auto header = blank ? std::string(key.toStringView()) + ";"
                                : std::string(key.toStringView()) + ": " + value;

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
        // be interrupted, so a cancel or shutdown mid-lookup would block this thread and every
        // other request with it. curl detaches the thread instead and it frees its own state.
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

    // Followed here rather than by CURLOPT_FOLLOWLOCATION, which keeps CURLOPT_CUSTOMREQUEST for
    // the whole chain: Curl_http_method takes the request line from it unconditionally while the
    // redirect handlers only touch curl's own idea of the method, so a DELETE would stay a DELETE
    // across a 303 and a PUT would keep its verb while curl dropped the body. Rewriting per RFC
    // 9110 15.4.
    static void retarget(CurlTask& task, long statusCode, const char* location) {
        task.request.url = StringBox::fromCString(location);

        auto method = std::string(task.request.method.toStringView());
        bool toGet = statusCode == 303 ? (method != "GET" && method != "HEAD")
                                      : ((statusCode == 301 || statusCode == 302) && method == "POST");
        if (toGet) {
            task.request.method = STRING_LITERAL("GET");
            task.requestBody = nullptr;
        }

        // curl only withholds a redirect's body from the write callback when it is following the
        // redirect itself, so this hop's is ours to discard.
        task.responseBody->clear();
        task.responseHeaders = emptyHeaderMap();
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

                // Nothing to decode if nothing arrived, which is also what keeps a HEAD or a 204
                // against a compressing origin from being reported as a failure.
                if (!task->responseBody->empty()) {
                    if (auto encoding = undecodableContentEncoding(task->responseHeaders)) {
                        finish(easy,
                               task,
                               Error(StringBox::fromString(
                                   "Cannot decode a response sent with Content-Encoding: " + *encoding +
                                   ". This build has no decompression support, so an Accept-Encoding request "
                                   "header must not be set.")));
                        continue;
                    }
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
    // Never null, so that a cancel is safe whether or not the run loop ever came up.
    std::shared_ptr<PollWaker> _waker = std::make_shared<PollWaker>(nullptr);
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
