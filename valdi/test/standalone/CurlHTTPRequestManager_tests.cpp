#include "valdi/standalone_http/CurlHTTPRequestManager.hpp"

#include "valdi_core/Cancelable.hpp"
#include "valdi_core/HTTPRequest.hpp"
#include "valdi_core/HTTPRequestManager.hpp"
#include "valdi_core/HTTPRequestManagerCompletion.hpp"
#include "valdi_core/HTTPResponse.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

#include "gtest/gtest.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <future>
#include <mutex>
#include <netinet/in.h>
#include <optional>
#include <string>
#include <sys/resource.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace Valdi;

namespace ValdiTest {

namespace {

class RecordingCompletion : public snap::valdi_core::HTTPRequestManagerCompletion {
public:
    void onComplete(const snap::valdi_core::HTTPResponse& response) override {
        std::lock_guard<std::mutex> guard(_mutex);
        _statusCode = response.statusCode;
        _headers = response.headers;
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

    Value headers() const {
        std::lock_guard<std::mutex> guard(_mutex);
        return _headers;
    }

private:
    mutable std::mutex _mutex;
    std::condition_variable _condition;
    bool _done = false;
    std::optional<int32_t> _statusCode;
    std::optional<std::string> _error;
    size_t _bodySize = 0;
    Value _headers;
};

// A peer that hangs up mid-response raises SIGPIPE on the serving thread, and its default
// disposition takes down the whole binary, every unrelated test with it. Neither per-socket guard
// is portable — SO_NOSIGPIPE is BSD-only and MSG_NOSIGNAL is Linux-only — whereas ignoring the
// signal works everywhere, and a test binary has no use for it in the first place.
void ignoreSigPipe() {
    [[maybe_unused]] static const auto previous = ::signal(SIGPIPE, SIG_IGN);
}

// Peak resident size is a high-water mark for the whole process, so the delta across one transfer
// is what that transfer newly demanded.
size_t peakResidentBytes() {
    rusage usage{};
    ::getrusage(RUSAGE_SELF, &usage);
#ifdef __APPLE__
    return static_cast<size_t>(usage.ru_maxrss);
#else
    return static_cast<size_t>(usage.ru_maxrss) * 1024;
#endif
}

uint16_t bindToLoopback(int socketFd) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ::bind(socketFd, reinterpret_cast<sockaddr*>(&address), sizeof(address));

    socklen_t length = sizeof(address);
    ::getsockname(socketFd, reinterpret_cast<sockaddr*>(&address), &length);
    return ntohs(address.sin_port);
}

// Releases a serving thread blocked in accept(). Closing the listener is not portable for this —
// on Linux it leaves accept() blocked — and shutdown() is a no-op on a listening socket, so the
// only reliable release is a connection the thread can actually accept.
void wakeAccept(uint16_t port) {
    int socketFd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    ::connect(socketFd, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    ::close(socketFd);
}

// A port nothing listens on, so connecting is refused immediately.
std::string unusedLoopbackUrl() {
    int socketFd = ::socket(AF_INET, SOCK_STREAM, 0);
    uint16_t port = bindToLoopback(socketFd);
    ::close(socketFd);

    return "http://127.0.0.1:" + std::to_string(port) + "/";
}

// Accepts a connection and never writes a response, so a transfer stays genuinely in
// flight. A refused or unresolvable address will not do: those complete immediately.
class StallServer {
public:
    StallServer() {
        _listener = ::socket(AF_INET, SOCK_STREAM, 0);
        _port = bindToLoopback(_listener);
        ::listen(_listener, 1);

        _thread = std::thread([this]() { serve(); });
    }

    ~StallServer() {
        _stopping.store(true);

        int connection = -1;
        {
            std::lock_guard<std::mutex> guard(_mutex);
            connection = _connection;
        }
        if (connection >= 0) {
            // Releases the serving thread if it is still blocked reading.
            ::shutdown(connection, SHUT_RDWR);
        } else {
            // Nothing ever connected, so the thread is still in accept().
            wakeAccept(_port);
        }

        if (_thread.joinable()) {
            _thread.join();
        }
        // Closed here rather than on the serving thread, so neither fd can be reused under the
        // shutdown above.
        if (connection >= 0) {
            ::close(connection);
        }
        ::close(_listener);
    }

    bool waitForConnection(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(_mutex);
        return _condition.wait_for(lock, timeout, [this]() { return _connection >= 0; });
    }

    // Nothing is ever written back, so the peer going away is the only thing this server can
    // observe — which makes it the one visible effect of a transfer being aborted.
    bool waitForDisconnect(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(_mutex);
        return _condition.wait_for(lock, timeout, [this]() { return _disconnected; });
    }

    std::string url() const {
        return "http://127.0.0.1:" + std::to_string(_port) + "/";
    }

private:
    void serve() {
        int connection = ::accept(_listener, nullptr, nullptr);
        if (connection < 0) {
            return;
        }
        if (_stopping.load()) {
            ::close(connection);
            return;
        }

        {
            std::lock_guard<std::mutex> guard(_mutex);
            _connection = connection;
            _condition.notify_all();
        }

        char buffer[512];
        while (::recv(connection, buffer, sizeof(buffer), 0) > 0) {
        }

        std::lock_guard<std::mutex> guard(_mutex);
        _disconnected = true;
        _condition.notify_all();
    }

    int _listener = -1;
    int _connection = -1;
    bool _disconnected = false;
    uint16_t _port = 0;
    std::atomic_bool _stopping{false};
    std::mutex _mutex;
    std::condition_variable _condition;
    std::thread _thread;
};

// Sends its response in pieces with a pause between them, so a transfer can take longer overall
// than an idle timeout without ever actually going idle.
class DribblingServer {
public:
    DribblingServer(std::string response, size_t pieces, std::chrono::milliseconds gap) {
        ignoreSigPipe();

        _listener = ::socket(AF_INET, SOCK_STREAM, 0);
        _port = bindToLoopback(_listener);
        ::listen(_listener, 1);

        _thread = std::thread([this, response = std::move(response), pieces, gap]() {
            int connection = ::accept(_listener, nullptr, nullptr);
            if (connection < 0) {
                return;
            }
            if (_stopping.load()) {
                ::close(connection);
                return;
            }

            std::string request;
            char buffer[512];
            while (request.find("\r\n\r\n") == std::string::npos) {
                auto received = ::recv(connection, buffer, sizeof(buffer), 0);
                if (received <= 0) {
                    break;
                }
                request.append(buffer, static_cast<size_t>(received));
            }

            auto pieceSize = (response.size() + pieces - 1) / pieces;
            for (size_t sent = 0; sent < response.size(); sent += pieceSize) {
                auto piece = std::min(pieceSize, response.size() - sent);
                if (!sendAll(connection, response.data() + sent, piece)) {
                    break;
                }
                std::this_thread::sleep_for(gap);
            }

            ::shutdown(connection, SHUT_RDWR);
            ::close(connection);
        });
    }

    ~DribblingServer() {
        _stopping.store(true);
        wakeAccept(_port);

        if (_thread.joinable()) {
            _thread.join();
        }
        ::close(_listener);
    }

    std::string url() const {
        return "http://127.0.0.1:" + std::to_string(_port) + "/";
    }

private:
    // ::send places only what fits in the socket buffer and returns, so anything above a few
    // hundred kilobytes needs the loop.
    static bool sendAll(int connection, const char* data, size_t size) {
        while (size > 0) {
            auto written = ::send(connection, data, size, 0);
            if (written <= 0) {
                return false;
            }
            data += written;
            size -= static_cast<size_t>(written);
        }
        return true;
    }

    int _listener = -1;
    uint16_t _port = 0;
    std::atomic_bool _stopping{false};
    std::thread _thread;
};

// Serves canned responses, one per connection, so a redirect chain is deterministic and
// needs no network. Each response should say "Connection: close" to keep curl from
// reusing a connection and leaving a later response unclaimed.
class ScriptedServer {
public:
    explicit ScriptedServer(std::vector<std::string> responses) {
        ignoreSigPipe();

        _listener = ::socket(AF_INET, SOCK_STREAM, 0);
        _port = bindToLoopback(_listener);
        ::listen(_listener, static_cast<int>(responses.size()));

        _thread = std::thread([this, responses = std::move(responses)]() {
            for (const auto& response : responses) {
                int connection = ::accept(_listener, nullptr, nullptr);
                if (connection < 0) {
                    return;
                }
                if (_stopping.load()) {
                    ::close(connection);
                    return;
                }

                auto request = readRequest(connection);
                {
                    std::lock_guard<std::mutex> guard(_mutex);
                    _requestLines.push_back(request.substr(0, request.find("\r\n")));
                    _requests.push_back(std::move(request));
                }

                ::send(connection, response.data(), response.size(), 0);
                ::shutdown(connection, SHUT_RDWR);
                ::close(connection);
            }
        });
    }

    ~ScriptedServer() {
        _stopping.store(true);
        wakeAccept(_port);

        if (_thread.joinable()) {
            _thread.join();
        }
        // Closed only once the serving thread is done with it, so the fd cannot be reused underneath
        // an accept() still in progress.
        ::close(_listener);
    }

    std::string url(const char* path) const {
        return "http://127.0.0.1:" + std::to_string(_port) + path;
    }

    // The request line of each request served, in order, so tests can assert on the verb and
    // path that actually went over the wire.
    std::vector<std::string> requestLines() const {
        std::lock_guard<std::mutex> guard(_mutex);
        return _requestLines;
    }

    // Each request in full, up to the end of its headers, for assertions the request line alone
    // cannot carry.
    std::vector<std::string> requests() const {
        std::lock_guard<std::mutex> guard(_mutex);
        return _requests;
    }

private:
    // Drained so that closing the connection does not reset it before curl reads back.
    static std::string readRequest(int connection) {
        std::string request;
        char buffer[512];
        while (request.find("\r\n\r\n") == std::string::npos) {
            auto received = ::recv(connection, buffer, sizeof(buffer), 0);
            if (received <= 0) {
                break;
            }
            request.append(buffer, static_cast<size_t>(received));
        }
        return request;
    }

    int _listener = -1;
    uint16_t _port = 0;
    std::atomic_bool _stopping{false};
    mutable std::mutex _mutex;
    std::vector<std::string> _requestLines;
    std::vector<std::string> _requests;
    std::thread _thread;
};

snap::valdi_core::HTTPRequest makeGet(const char* url) {
    return snap::valdi_core::HTTPRequest(StringBox::fromCString(url), STRING_LITERAL("GET"), Value(), std::nullopt, 0);
}

snap::valdi_core::HTTPRequest makeRequestWithBody(const char* method, const char* url, const char* body) {
    return snap::valdi_core::HTTPRequest(StringBox::fromCString(url),
                                        StringBox::fromCString(method),
                                        Value(),
                                        makeShared<ByteBuffer>(std::string(body))->toBytesView(),
                                        0);
}

} // namespace

TEST(CurlHTTPRequestManagerTests, keepsOnlyTheFinalResponsesHeaders) {
    ScriptedServer server({"HTTP/1.1 301 Moved Permanently\r\n"
                           "Location: /final\r\n"
                           "X-From-Redirect: yes\r\n"
                           "Content-Length: 0\r\n"
                           "Connection: close\r\n"
                           "\r\n",
                           "HTTP/1.1 200 OK\r\n"
                           "X-From-Final: yes\r\n"
                           "Content-Length: 2\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "ok"});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeGet(server.url("/start").c_str()), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    ASSERT_EQ(completion->statusCode(), 200);
    EXPECT_EQ(completion->bodySize(), 2u);

    auto headers = completion->headers();
    EXPECT_FALSE(headers.getMapValue("X-From-Final").isNullOrUndefined())
        << "the final response's own headers are missing";
    EXPECT_TRUE(headers.getMapValue("X-From-Redirect").isNullOrUndefined())
        << "a header sent only by the redirect survived into the result";
    EXPECT_TRUE(headers.getMapValue("Location").isNullOrUndefined())
        << "the redirect's Location survived into the result";
}

TEST(CurlHTTPRequestManagerTests, resetsHeadersOnAStatusLineWhoseReasonPhraseHasAColon) {
    // A colon in the reason phrase is legal — RFC 7230 makes it free-form text — and it makes the
    // final status line parse as a header if the colon is looked for before the HTTP/ prefix, which
    // skips the reset that discards the redirect's headers.
    ScriptedServer server({"HTTP/1.1 301 Moved Permanently\r\n"
                           "Location: /final\r\n"
                           "X-From-Redirect: yes\r\n"
                           "Content-Length: 0\r\n"
                           "Connection: close\r\n"
                           "\r\n",
                           "HTTP/1.1 200 Enhance your calm: relax\r\n"
                           "X-From-Final: yes\r\n"
                           "Content-Length: 2\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "ok"});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeGet(server.url("/start").c_str()), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    ASSERT_EQ(completion->statusCode(), 200);

    auto headers = completion->headers();
    EXPECT_FALSE(headers.getMapValue("X-From-Final").isNullOrUndefined())
        << "the final response's own headers are missing";
    EXPECT_TRUE(headers.getMapValue("X-From-Redirect").isNullOrUndefined())
        << "a header sent only by the redirect survived, so the final status line was mistaken for a "
           "header and never reset them";
    EXPECT_TRUE(headers.getMapValue("Location").isNullOrUndefined())
        << "the redirect's Location survived into the result";
    EXPECT_TRUE(headers.getMapValue("HTTP/1.1 200 Enhance your calm").isNullOrUndefined())
        << "the status line was stored as though it were a header";
}

TEST(CurlHTTPRequestManagerTests, joinsRepeatedResponseHeaders) {
    ScriptedServer server({"HTTP/1.1 200 OK\r\n"
                           "Set-Cookie: session=abc\r\n"
                           "Set-Cookie: csrf=def\r\n"
                           "Set-Cookie: prefs=ghi\r\n"
                           "Content-Length: 2\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "ok"});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeGet(server.url("/").c_str()), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    ASSERT_EQ(completion->statusCode(), 200);

    auto cookies = completion->headers().getMapValue("Set-Cookie");
    ASSERT_FALSE(cookies.isNullOrUndefined());
    EXPECT_EQ(cookies.toStringBox().toStringView(), "session=abc, csrf=def, prefs=ghi")
        << "repeated response headers must be joined the way NSURLResponse joins them, not "
           "collapsed to whichever one arrived last";
}

TEST(CurlHTTPRequestManagerTests, sendsAGetCarryingABodyAsGet) {
    ScriptedServer server({"HTTP/1.1 200 OK\r\n"
                           "Content-Length: 2\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "ok"});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeRequestWithBody("GET", server.url("/query").c_str(), "{}"), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    ASSERT_EQ(completion->statusCode(), 200);

    auto requests = server.requestLines();
    ASSERT_EQ(requests.size(), 1u);
    EXPECT_EQ(requests[0], "GET /query HTTP/1.1") << "a GET carrying a body must still be sent as GET";
}

TEST(CurlHTTPRequestManagerTests, sendsAnEmptyButPresentBodyAsAnEmptyPost) {
    ScriptedServer server({"HTTP/1.1 200 OK\r\n"
                           "Content-Length: 2\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "ok"});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    // An engaged body of size zero, as httpClient.post(url, new Uint8Array(0)) produces. An empty
    // ByteBuffer never allocates, so its BytesView::data() is null.
    manager->performRequest(makeRequestWithBody("POST", server.url("/submit").c_str(), ""), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)))
        << "the request never completed, so curl was left waiting for a body from somewhere else";
    ASSERT_EQ(completion->statusCode(), 200);

    auto requests = server.requests();
    ASSERT_EQ(requests.size(), 1u);
    EXPECT_EQ(requests[0].substr(0, requests[0].find("\r\n")), "POST /submit HTTP/1.1");
    EXPECT_NE(requests[0].find("Content-Length: 0\r\n"), std::string::npos)
        << "an empty body must still declare a zero length. Request was:\n"
        << requests[0];
}

TEST(CurlHTTPRequestManagerTests, followsASeeOtherWithGet) {
    ScriptedServer server({"HTTP/1.1 303 See Other\r\n"
                           "Location: /final\r\n"
                           "Content-Length: 0\r\n"
                           "Connection: close\r\n"
                           "\r\n",
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Length: 2\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "ok"});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeRequestWithBody("POST", server.url("/submit").c_str(), "a=1"), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    ASSERT_EQ(completion->statusCode(), 200);

    auto requests = server.requestLines();
    ASSERT_EQ(requests.size(), 2u);
    EXPECT_EQ(requests[0], "POST /submit HTTP/1.1");
    EXPECT_EQ(requests[1], "GET /final HTTP/1.1") << "a 303 must be followed with GET, not the original verb";
}

TEST(CurlHTTPRequestManagerTests, reportsALoopbackResponseWithoutWaitingOutThePollTimeout) {
    ScriptedServer server({"HTTP/1.1 200 OK\r\n"
                           "Content-Length: 2\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "ok"});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();

    auto start = std::chrono::steady_clock::now();
    manager->performRequest(makeGet(server.url("/").c_str()), completion);
    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

    ASSERT_EQ(completion->statusCode(), 200);
    EXPECT_LT(elapsed.count(), 100) << "a loopback request was reported after " << elapsed.count()
                                    << " ms; the finished transfer is not drained until curl_multi_poll "
                                       "has slept out its whole timeout";
}

TEST(CurlHTTPRequestManagerTests, servesARequestPromptlyAfterSittingIdle) {
    ScriptedServer server({"HTTP/1.1 200 OK\r\n"
                           "Content-Length: 2\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "ok"});

    auto manager = makeCurlHTTPRequestManager();

    // Long enough that the curl thread is certainly parked in its poll. An idle multi handle has
    // no timer for curl to report, so nothing but the wakeup in performRequest can serve this
    // inside the deadline below.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    auto completion = std::make_shared<RecordingCompletion>();
    auto start = std::chrono::steady_clock::now();
    manager->performRequest(makeGet(server.url("/").c_str()), completion);
    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(30)));
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

    ASSERT_EQ(completion->statusCode(), 200);
    EXPECT_LT(elapsed.count(), 500) << "an idle curl thread took " << elapsed.count()
                                    << " ms to pick up new work, so it was not woken and waited out "
                                       "kPollTimeoutMs instead";
}

TEST(CurlHTTPRequestManagerTests, reportsFailureForAnUnreachableHost) {
    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();

    auto cancelable = manager->performRequest(makeGet(unusedLoopbackUrl().c_str()), completion);
    ASSERT_NE(cancelable, nullptr);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(30)));
    ASSERT_FALSE(completion->statusCode().has_value());
    ASSERT_TRUE(completion->error().has_value());
}

TEST(CurlHTTPRequestManagerTests, failsATransferThatStopsMakingProgress) {
    StallServer server;

    // A one second idle timeout, so the test does not sit out the default.
    auto manager = makeCurlHTTPRequestManager(StringBox(), 1);
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeGet(server.url().c_str()), completion);

    ASSERT_TRUE(server.waitForConnection(std::chrono::seconds(5)))
        << "curl never connected, so a stalled transfer is not being exercised";

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(20)))
        << "a server that accepts and then never answers leaves the request pending forever";
    EXPECT_TRUE(completion->error().has_value()) << "a stalled transfer should be reported as a failure";
}

TEST(CurlHTTPRequestManagerTests, leavesASlowButProgressingTransferAlone) {
    std::string body(400, 'x');
    // Ten pieces 200 ms apart, so about two seconds in total — well past the idle timeout below,
    // but never a whole second without data. An overall CURLOPT_TIMEOUT would cut this off.
    DribblingServer server("HTTP/1.1 200 OK\r\n"
                           "Content-Length: 400\r\n"
                           "Connection: close\r\n"
                           "\r\n" +
                               body,
                           10,
                           std::chrono::milliseconds(200));

    auto manager = makeCurlHTTPRequestManager(StringBox(), 1);
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeGet(server.url().c_str()), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(30)));
    EXPECT_FALSE(completion->error().has_value())
        << "a slow but progressing transfer was cut off: " << completion->error().value_or("");
    EXPECT_EQ(completion->statusCode(), 200);
    EXPECT_EQ(completion->bodySize(), 400u);
}

TEST(CurlHTTPRequestManagerTests, scriptedServerShutsDownWithAResponseUnclaimed) {
    static const char* kResponse = "HTTP/1.1 200 OK\r\n"
                                   "Content-Length: 2\r\n"
                                   "Connection: close\r\n"
                                   "\r\n"
                                   "ok";

    // The work runs on a detached thread so that a destructor which never returns fails this test
    // rather than wedging the whole binary.
    std::promise<void> destroyed;
    auto finished = destroyed.get_future();

    std::thread([destroyed = std::move(destroyed)]() mutable {
        {
            // Primed with two responses but given one request. That is exactly the state
            // followsASeeOtherWithGet is left in when a redirect is not followed — the regression it
            // exists to catch — and it leaves the serving thread blocked in accept() for a second
            // connection that never comes.
            ScriptedServer server({kResponse, kResponse});

            auto manager = makeCurlHTTPRequestManager();
            auto completion = std::make_shared<RecordingCompletion>();
            manager->performRequest(makeGet(server.url("/only").c_str()), completion);
            completion->waitForCompletion(std::chrono::seconds(10));
        }
        destroyed.set_value();
    }).detach();

    EXPECT_EQ(finished.wait_for(std::chrono::seconds(10)), std::future_status::ready)
        << "~ScriptedServer never returned. Its thread is blocked in accept() for a connection that "
           "will never arrive, so a test that fails this way times out instead of reporting which "
           "assertion failed";
}

TEST(CurlHTTPRequestManagerTests, survivesAPeerHangingUpMidResponse) {
    std::string body(4000, 'x');
    // Forty pieces 50 ms apart, so there are plenty of writes left to attempt after the peer goes.
    DribblingServer server("HTTP/1.1 200 OK\r\n"
                           "Content-Length: 4000\r\n"
                           "Connection: close\r\n"
                           "\r\n" +
                               body,
                           40,
                           std::chrono::milliseconds(50));

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    auto cancelable = manager->performRequest(makeGet(server.url().c_str()), completion);

    // Cancelling partway through makes curl close the connection while the server still has most of
    // the response to write, so every later ::send is against a socket the peer has gone from. If
    // SIGPIPE is not suppressed that terminates this binary, taking every unrelated test with it.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    cancelable->cancel();

    EXPECT_FALSE(completion->waitForCompletion(std::chrono::seconds(1)))
        << "a cancelled request must leave its completion uncalled";
}

TEST(CurlHTTPRequestManagerTests, doesNotHoldTheResponseBodyTwice) {
    constexpr size_t kBodySize = 32 * 1024 * 1024;

    DribblingServer server("HTTP/1.1 200 OK\r\n"
                           "Content-Length: " +
                               std::to_string(kBodySize) +
                               "\r\n"
                               "Connection: close\r\n"
                               "\r\n" +
                               std::string(kBodySize, 'x'),
                           1,
                           std::chrono::milliseconds(0));

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();

    auto before = peakResidentBytes();
    manager->performRequest(makeGet(server.url().c_str()), completion);
    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(60)));

    ASSERT_EQ(completion->statusCode(), 200);
    ASSERT_EQ(completion->bodySize(), kBodySize) << "the large body did not arrive intact";

    // Measured at 1.03x holding the payload once and 2.03x holding it twice, so the threshold sits
    // midway rather than just under the failing value.
    auto growth = peakResidentBytes() - before;
    EXPECT_LT(growth, kBodySize + kBodySize / 2)
        << "peak memory grew by " << growth / (1024 * 1024) << " MiB to receive a "
        << kBodySize / (1024 * 1024)
        << " MiB body, so the payload is accumulated in one buffer and then copied whole into another";
}

TEST(CurlHTTPRequestManagerTests, cancellingARequestDropsItsCompletion) {
    StallServer server;

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();

    auto cancelable = manager->performRequest(makeGet(server.url().c_str()), completion);
    ASSERT_NE(cancelable, nullptr);

    ASSERT_TRUE(server.waitForConnection(std::chrono::seconds(5)))
        << "curl never connected, so there is no live transfer to cancel";
    cancelable->cancel();

    // curl hanging up means the abort has been through drainMessages, which is where a completion
    // that was merely detached from curl rather than dropped would have fired. The manager is left
    // alive on purpose, so that this proves cancel() dropped the completion rather than shutdown
    // doing it.
    ASSERT_TRUE(server.waitForDisconnect(std::chrono::seconds(30)))
        << "curl never hung up, so the cancelled transfer was never aborted";

    EXPECT_FALSE(completion->waitForCompletion(std::chrono::seconds(1)))
        << "a cancelled request must leave its completion uncalled, as on iOS and Android";
}

TEST(CurlHTTPRequestManagerTests, abortsACancelledTransferWithoutWaitingOutThePollTimeout) {
    StallServer server;

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    auto cancelable = manager->performRequest(makeGet(server.url().c_str()), completion);

    ASSERT_TRUE(server.waitForConnection(std::chrono::seconds(5)))
        << "curl never connected, so there is no live transfer to cancel";

    auto start = std::chrono::steady_clock::now();
    cancelable->cancel();
    ASSERT_TRUE(server.waitForDisconnect(std::chrono::seconds(30)))
        << "curl never hung up, so the cancelled transfer was never aborted";
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

    EXPECT_LT(elapsed.count(), 100) << "a cancelled transfer was aborted after " << elapsed.count()
                                    << " ms; cancelling does not wake the curl thread, so it sleeps out its "
                                       "poll timeout before consulting the progress callback";
}

TEST(CurlHTTPRequestManagerTests, aCancelledLookupDoesNotStallOtherRequests) {
    ScriptedServer server({"HTTP/1.1 200 OK\r\n"
                           "Content-Length: 2\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "ok"});

    auto manager = makeCurlHTTPRequestManager();

    // A name the resolver takes a long time to give up on, cancelled while the lookup is still
    // running. Where the resolver answers quickly there is no stall to observe and this test simply
    // passes, so it can never fail spuriously.
    auto abandoned = std::make_shared<RecordingCompletion>();
    auto cancelable = manager->performRequest(makeGet("http://this-host-does-not-exist.invalid/"), abandoned);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    cancelable->cancel();

    auto completion = std::make_shared<RecordingCompletion>();
    auto start = std::chrono::steady_clock::now();
    manager->performRequest(makeGet(server.url("/after").c_str()), completion);
    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(60)));
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

    ASSERT_EQ(completion->statusCode(), 200);
    EXPECT_LT(elapsed.count(), 1000)
        << "an unrelated request waited " << elapsed.count()
        << " ms because abandoning the cancelled lookup blocked the one curl thread until the "
           "resolver gave up, stalling every other request with it";
}

TEST(CurlHTTPRequestManagerTests, reportsNothingWhenShutDownBeforeConnecting) {
    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();

    manager->performRequest(makeGet("http://this-host-does-not-exist.invalid/"), completion);
    manager.reset();

    EXPECT_FALSE(completion->waitForCompletion(std::chrono::seconds(1)))
        << "shutdown reported a request that never got off the ground; it should be dropped, the "
           "same as a cancellation";
}

TEST(CurlHTTPRequestManagerTests, shutdownCancelsRequestsInFlight) {
    StallServer server;

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeGet(server.url().c_str()), completion);

    ASSERT_TRUE(server.waitForConnection(std::chrono::seconds(5)))
        << "curl never connected, so shutdown-with-a-live-transfer is not being exercised";

    // The manager is moved into a detached thread so that a destructor which never returns
    // fails this test instead of wedging the whole binary.
    std::promise<void> destroyed;
    auto finished = destroyed.get_future();
    std::thread([manager = std::move(manager), destroyed = std::move(destroyed)]() mutable {
        manager.reset();
        destroyed.set_value();
    }).detach();

    EXPECT_EQ(finished.wait_for(std::chrono::seconds(5)), std::future_status::ready)
        << "~CurlHTTPRequestManager waited for the in-flight transfer instead of cancelling it";
    EXPECT_FALSE(completion->waitForCompletion(std::chrono::seconds(1)))
        << "shutdown reported an in-flight request. Completions reach JavaScript with no thread "
           "hop, so firing one from the curl thread while the destroying thread is inside join() "
           "enters the engine from two threads at once";
}

} // namespace ValdiTest
