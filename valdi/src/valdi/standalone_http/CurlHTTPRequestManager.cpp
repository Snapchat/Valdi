#include "valdi/standalone_http/CurlHTTPRequestManager.hpp"

#include "valdi/standalone_http/CaStore.hpp"

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

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <strings.h>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Valdi {

namespace {

constexpr int kMaxRedirects = 10;
constexpr long kConnectTimeoutSeconds = 30;
// A backstop while a transfer is in flight, where curl has a timer of its own and curl_multi_poll
// returns at the sooner of the two.
constexpr int kPollTimeoutMs = 10000;

// With nothing in flight curl has no timer, so this is the whole wait, and at kPollTimeoutMs the
// thread would wake for no reason every ten seconds for the life of the process. New work,
// cancellation and shutdown all wake the poll, and curl latches a wakeup arriving before the poll
// begins, so sleeping this long strands nothing. Finite only so a lost wakeup would right itself.
constexpr int kIdlePollTimeoutMs = 24 * 60 * 60 * 1000;

// What curl compiled in as its default. @curl hardcodes this per OS rather than probing, so on any
// Linux it names Debian's bundle whether or not this machine is Debian, and reports it whether or not
// the file is there.
CaStore curlsOwnCaStore() {
    auto* easy = curl_easy_init();
    if (easy == nullptr) {
        return {};
    }

    char* file = nullptr;
    char* path = nullptr;
    curl_easy_getinfo(easy, CURLINFO_CAINFO, &file);
    curl_easy_getinfo(easy, CURLINFO_CAPATH, &path);

    CaStore store{file != nullptr ? file : "", path != nullptr ? path : ""};
    curl_easy_cleanup(easy);

    return store;
}

// Neither the iOS nor the Android manager sets a User-Agent: NSURLSession and HttpURLConnection each
// send the transport's own, and Valdi never names itself. libcurl is the exception in sending none at
// all, which a fair number of CDNs answer with a 403, so it names itself the way the curl tool does.
const char* defaultUserAgent() {
    static const std::string agent = std::string("curl/") + curl_version_info(CURLVERSION_NOW)->version;
    return agent.c_str();
}

// Always a map, never null: HTTPTypes.d.ts declares headers as StringMap<string>, and iOS, Android
// and web all hand back {} when a response carried none.
Value emptyHeaderMap() {
    return Value(makeShared<ValueMap>());
}

// A cancelable can outlive the manager, since JavaScript may hold one and cancel after teardown.
// Tasks reach the multi handle through this rather than a back-pointer, so a cancel arriving before
// the manager has started one or after it has torn it down finds nothing to poke.
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

        // Nothing reads that flag until curl next runs the progress callback, and the poll is parked
        // until something wakes it. Waking it frees the socket now.
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

    snap::valdi_core::HTTPRequest request;
    std::atomic_bool cancelled{false};

    // Null when the request has no body. Owned for the life of the task, which outlives the easy
    // handle made from it, so curl can read it in place.
    Ref<ByteBuffer> requestBody;
    size_t requestBodyOffset = 0;

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

// Not CURLOPT_POSTFIELDS, which would pin curl's own idea of the method to POST for every verb and
// take the RFC 9110 redirect rewrite with it.
size_t readBodyCallback(char* buffer, size_t size, size_t count, void* userData) {
    auto* task = static_cast<CurlTask*>(userData);

    auto sending = std::min(task->requestBody->size() - task->requestBodyOffset, size * count);
    if (sending > 0) {
        std::memcpy(buffer, task->requestBody->data() + task->requestBodyOffset, sending);
        task->requestBodyOffset += sending;
    }
    return sending;
}

// curl rewinds through this when it repeats a request, as it does for a redirect that keeps the body.
int seekBodyCallback(void* userData, curl_off_t offset, int origin) {
    auto* task = static_cast<CurlTask*>(userData);

    if (origin != SEEK_SET || offset < 0 || static_cast<curl_off_t>(task->requestBody->size()) < offset) {
        return CURL_SEEKFUNC_CANTSEEK;
    }

    task->requestBodyOffset = static_cast<size_t>(offset);
    return CURL_SEEKFUNC_OK;
}

size_t writeBodyCallback(char* data, size_t size, size_t count, void* userData) {
    auto* task = static_cast<CurlTask*>(userData);
    task->responseBody->append(data, data + size * count);
    return size * count;
}

size_t writeHeaderCallback(char* data, size_t size, size_t count, void* userData) {
    auto* task = static_cast<CurlTask*>(userData);

    std::string line(data, size * count);

    // Checked before looking for a colon, because a reason phrase is free-form text and may contain
    // one. The reset is what leaves only the last response's headers: curl hands over every hop of a
    // redirect chain and every 1xx block, and a 103 Early Hints carries real headers.
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
    CurlHTTPRequestManager(const StringBox& caBundlePath, int32_t idleTimeoutSeconds, CURLcode globalInit)
        : _idleTimeoutSeconds(idleTimeoutSeconds), _globalInit(globalInit) {
        if (_globalInit != CURLE_OK) {
            // Carrying on would leave curl_easy_init handing back handles whose TLS backend was
            // never set up, so every HTTPS request would fail with an unrelated-looking error.
            return;
        }

        // Probed last, so a deliberate --@curl//:ca_bundle is never overridden. Its existence is what
        // decides, not merely that curl reports one: a compiled-in default naming a file this machine
        // does not have must not suppress the probe, or the store that is here goes unfound.
        _caStore = requestedCaStore(caBundlePath);
        if (_caStore.empty() && !caStoreExists(curlsOwnCaStore())) {
            _caStore = installedCaStore();
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

        // Checked before the multi handle, so the reported cause is the one that actually failed.
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
        // is not recursive.
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
            curl_multi_poll(_multi, nullptr, 0, _active.empty() ? kIdlePollTimeoutMs : kPollTimeoutMs, &numfds);
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
        // cancel() only raises the flag, so a task cancelled while queued arrives here anyway.
        if (task->cancelled.load()) {
            return;
        }

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

        // Methods curl models itself go through their own options, so it rewrites them across a
        // redirect per RFC 9110 15.4. CUSTOMREQUEST only rewrites the request line and curl keeps it
        // for the whole chain, so a verb left to it survives a 303 instead of becoming GET, which is
        // the one place this diverges from iOS and Android.
        auto method = std::string(request.method.toStringView());
        if (method.empty()) {
            method = "GET";
        }

        bool hasBody = task->requestBody != nullptr;

        if (method == "HEAD") {
            curl_easy_setopt(easy, CURLOPT_NOBODY, 1L);
        } else if (method == "POST") {
            curl_easy_setopt(easy, CURLOPT_POST, 1L);
            // Set even with no body, since curl otherwise reads one from stdin.
            curl_easy_setopt(easy,
                             CURLOPT_POSTFIELDSIZE_LARGE,
                             static_cast<curl_off_t>(hasBody ? task->requestBody->size() : 0));
            // Not COPYPOSTFIELDS: the task already owns this buffer for longer than the handle
            // lives, so letting curl copy it again would hold the payload twice.
            curl_easy_setopt(easy,
                             CURLOPT_POSTFIELDS,
                             hasBody ? reinterpret_cast<const char*>(task->requestBody->data()) : "");
        } else if (hasBody) {
            curl_easy_setopt(easy, CURLOPT_UPLOAD, 1L);
            curl_easy_setopt(easy, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(task->requestBody->size()));
            curl_easy_setopt(easy, CURLOPT_READFUNCTION, readBodyCallback);
            curl_easy_setopt(easy, CURLOPT_READDATA, task.get());
            curl_easy_setopt(easy, CURLOPT_SEEKFUNCTION, seekBodyCallback);
            curl_easy_setopt(easy, CURLOPT_SEEKDATA, task.get());

            // CURLOPT_UPLOAD already names the request PUT.
            if (method != "PUT") {
                curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, method.c_str());
            }
        } else if (method == "GET") {
            curl_easy_setopt(easy, CURLOPT_HTTPGET, 1L);
        } else {
            curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, method.c_str());
        }

        // Set before the caller's headers so that one of their own still replaces it.
        curl_easy_setopt(easy, CURLOPT_USERAGENT, defaultUserAgent());

        for (const auto& key : request.headers.sortedMapKeys()) {
            // The transport's to set, not the caller's: curl frames the body itself, and asking for
            // an encoding would get one back that this build has no codec to decode. fetch forbids
            // both header names, so the same JavaScript already loses them on web.
            if (equalsIgnoringCase(key.toStringView(), "content-length") ||
                equalsIgnoringCase(key.toStringView(), "accept-encoding")) {
                continue;
            }

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

        // Also withholds Authorization and Cookie from any host but the one the chain started on.
        curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(easy, CURLOPT_MAXREDIRS, static_cast<long>(kMaxRedirects));

        // Redirects have their own allow-list, already these two, but a first request is checked
        // against this one, which otherwise admits every protocol compiled in.
        curl_easy_setopt(easy, CURLOPT_PROTOCOLS_STR, "http,https");

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

        // curl's own floor is TLS 1.0, and iOS (App Transport Security) and Android both refuse
        // anything below 1.2. Without this the same request gets a weaker floor on the CLI than in
        // the app it shares its code with.
        curl_easy_setopt(easy, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

        if (!_caStore.file.empty()) {
            curl_easy_setopt(easy, CURLOPT_CAINFO, _caStore.file.c_str());
        }
        if (!_caStore.path.empty()) {
            curl_easy_setopt(easy, CURLOPT_CAPATH, _caStore.path.c_str());
        }

        auto added = curl_multi_add_handle(_multi, easy);
        if (added != CURLM_OK) {
            finish(easy, task, Error(StringBox::fromCString(curl_multi_strerror(added))));
            return;
        }
        _active.emplace(easy, task);
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

                // Nothing to decode if nothing arrived, which is also what keeps a HEAD or a 204
                // against a compressing origin from being reported as a failure.
                if (!task->responseBody->empty()) {
                    if (auto encoding = undecodableContentEncoding(task->responseHeaders)) {
                        finish(easy,
                               task,
                               Error(StringBox::fromString(
                                   "The server sent Content-Encoding: " + *encoding +
                                   ", which this build cannot decode. No Accept-Encoding was requested, so the "
                                   "response was compressed unasked.")));
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

        // JavaScript holds the task for as long as it holds the CancelablePromise, so a kept handle
        // should cost the handle rather than the payload.
        task->requestBody = nullptr;
        task->request = snap::valdi_core::HTTPRequest(StringBox(), StringBox(), Value(), std::nullopt, 0);

        curl_easy_cleanup(easy);
    }

    CaStore _caStore;
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

    return std::make_shared<CurlHTTPRequestManager>(caBundlePath, idleTimeoutSeconds, globalInitResult);
}

} // namespace Valdi
