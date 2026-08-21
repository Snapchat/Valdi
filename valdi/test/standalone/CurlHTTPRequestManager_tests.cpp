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
#include <cstring>
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
// is portable, since SO_NOSIGPIPE is BSD-only and MSG_NOSIGNAL is Linux-only. Ignoring the signal
// works everywhere, and a test binary has no use for it anyway.
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

// Releases a serving thread blocked in accept(). Closing the listener does not do it portably,
// because on Linux accept() stays blocked, and shutdown() is a no-op on a listening socket. The
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

    // Nothing is ever written back, so the peer going away is all this server can observe. That
    // makes it the one visible effect of a transfer being aborted.
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
    // Drained so that closing the connection does not reset it before curl reads back. A declared
    // body is drained too, so that asserting on what was sent does not depend on the body having
    // arrived in the same packet as the headers.
    static std::string readRequest(int connection) {
        std::string request;
        char buffer[512];
        while (request.find("\r\n\r\n") == std::string::npos) {
            auto received = ::recv(connection, buffer, sizeof(buffer), 0);
            if (received <= 0) {
                return request;
            }
            request.append(buffer, static_cast<size_t>(received));
        }

        auto expected = request.find("\r\n\r\n") + 4 + declaredBodySize(request);
        while (request.size() < expected) {
            auto received = ::recv(connection, buffer, sizeof(buffer), 0);
            if (received <= 0) {
                break;
            }
            request.append(buffer, static_cast<size_t>(received));
        }
        return request;
    }

    static size_t declaredBodySize(const std::string& request) {
        static const std::string kField = "\r\nContent-Length: ";

        auto at = request.find(kField);
        if (at == std::string::npos) {
            return 0;
        }
        return static_cast<size_t>(std::stoul(request.substr(at + kField.size())));
    }

    int _listener = -1;
    uint16_t _port = 0;
    std::atomic_bool _stopping{false};
    mutable std::mutex _mutex;
    std::vector<std::string> _requestLines;
    std::vector<std::string> _requests;
    std::thread _thread;
};

snap::valdi_core::HTTPRequest makeRequest(const char* method, const char* url) {
    return snap::valdi_core::HTTPRequest(
        StringBox::fromCString(url), StringBox::fromCString(method), Value(), std::nullopt, 0);
}

snap::valdi_core::HTTPRequest makeGet(const char* url) {
    return makeRequest("GET", url);
}

Value makeHeaders(std::initializer_list<std::pair<const char*, const char*>> entries) {
    Value headers;
    for (const auto& entry : entries) {
        headers.setMapValue(std::string_view(entry.first), Value(StringBox::fromCString(entry.second)));
    }
    return headers;
}

snap::valdi_core::HTTPRequest makeGetWithHeaders(const char* url, const Value& headers) {
    return snap::valdi_core::HTTPRequest(StringBox::fromCString(url), STRING_LITERAL("GET"), headers, std::nullopt, 0);
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
    // A colon in the reason phrase is legal, since RFC 7230 makes it free-form text. It also makes
    // the final status line parse as a header if the colon is looked for before the HTTP/ prefix,
    // which skips the reset that discards the redirect's headers.
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

// A canned 200 for tests that only care about what went out on the request.
std::string okResponse() {
    return "HTTP/1.1 200 OK\r\n"
           "Content-Length: 2\r\n"
           "Connection: close\r\n"
           "\r\n"
           "ok";
}

TEST(CurlHTTPRequestManagerTests, reportsAnEmptyMapForAResponseWithNoHeaders) {
    // No colon anywhere but the status line, which is the only way the header map is never promoted
    // from null. 204 so that curl needs no Content-Length, since any header added to help would
    // itself carry a colon and defeat the test. RFC 9110 makes Date a MUST for an origin with a
    // clock, so this shape comes from hand rolled and embedded servers rather than mainstream ones.
    ScriptedServer server({"HTTP/1.1 204 No Content\r\n\r\n"});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeGet(server.url("/").c_str()), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    ASSERT_EQ(completion->statusCode(), 204);

    auto headers = completion->headers();
    EXPECT_FALSE(headers.isNullOrUndefined())
        << "headers reached JavaScript as null, but HTTPTypes.d.ts declares them as a "
           "StringMap<string> and iOS, Android and web all hand back {}";
    EXPECT_TRUE(headers.isMap()) << "headers must always be a map, even for a response that carried none";
}

TEST(CurlHTTPRequestManagerTests, sendsARequestHeader) {
    ScriptedServer server({okResponse()});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeGetWithHeaders(server.url("/").c_str(), makeHeaders({{"X-Test", "value"}})),
                            completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    ASSERT_EQ(completion->statusCode(), 200);

    auto requests = server.requests();
    ASSERT_EQ(requests.size(), 1u);
    EXPECT_NE(requests[0].find("X-Test: value\r\n"), std::string::npos)
        << "a caller header never reached the wire, or was not formatted as \"name: value\". Request was:\n"
        << requests[0];
}

TEST(CurlHTTPRequestManagerTests, sendsHeadersInAnOrderThatDoesNotDependOnInsertion) {
    ScriptedServer server({okResponse()});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    // Inserted in an order that is neither alphabetical nor reverse alphabetical, so agreeing with
    // the map's own iteration order by chance is unlikely across five keys.
    manager->performRequest(makeGetWithHeaders(server.url("/").c_str(),
                                              makeHeaders({{"X-Delta", "4"},
                                                           {"X-Alpha", "1"},
                                                           {"X-Echo", "5"},
                                                           {"X-Bravo", "2"},
                                                           {"X-Charlie", "3"}})),
                            completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    ASSERT_EQ(completion->statusCode(), 200);

    auto requests = server.requests();
    ASSERT_EQ(requests.size(), 1u);

    std::vector<size_t> positions;
    for (const auto* header : {"X-Alpha: 1\r\n", "X-Bravo: 2\r\n", "X-Charlie: 3\r\n", "X-Delta: 4\r\n", "X-Echo: 5\r\n"}) {
        auto at = requests[0].find(header);
        EXPECT_NE(at, std::string::npos) << header << " never reached the wire. Request was:\n" << requests[0];
        positions.push_back(at);
    }

    // sortedMapKeys imposes this. The guarantee worth having is that the bytes do not depend on the
    // order the caller happened to build the map in, and sorting is how that is achieved.
    EXPECT_TRUE(std::is_sorted(positions.begin(), positions.end()))
        << "headers were not sent in sorted order, so the request bytes depend on map iteration "
           "order. Request was:\n"
        << requests[0];
}

TEST(CurlHTTPRequestManagerTests, overridesACurlDefaultHeader) {
    ScriptedServer server({okResponse()});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(
        makeGetWithHeaders(server.url("/").c_str(), makeHeaders({{"Accept", "application/json"}})), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    ASSERT_EQ(completion->statusCode(), 200);

    auto requests = server.requests();
    ASSERT_EQ(requests.size(), 1u);
    EXPECT_NE(requests[0].find("Accept: application/json\r\n"), std::string::npos)
        << "the caller's Accept did not reach the wire. Request was:\n"
        << requests[0];
    EXPECT_EQ(requests[0].find("Accept: */*"), std::string::npos)
        << "curl's own Accept was sent alongside the caller's, so the server sees two. Request was:\n"
        << requests[0];
}

TEST(CurlHTTPRequestManagerTests, coercesNonStringHeaderValues) {
    ScriptedServer server({okResponse()});

    // JavaScript is not held to HTTPTypes.d.ts, so a number or a boolean can arrive here. The loop
    // runs every value through toStringBox, and this pins what that produces.
    Value headers;
    headers.setMapValue(std::string_view("X-Count"), Value(42));
    headers.setMapValue(std::string_view("X-Flag"), Value(true));

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeGetWithHeaders(server.url("/").c_str(), headers), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    ASSERT_EQ(completion->statusCode(), 200);

    auto requests = server.requests();
    ASSERT_EQ(requests.size(), 1u);
    EXPECT_NE(requests[0].find("X-Count: 42\r\n"), std::string::npos)
        << "a numeric header value was not coerced to its digits. Request was:\n"
        << requests[0];
    EXPECT_NE(requests[0].find("X-Flag: true\r\n"), std::string::npos)
        << "a boolean header value was not coerced to true/false. Request was:\n"
        << requests[0];
}

// curl writes custom headers and a custom request line verbatim, so a CR or LF in caller-supplied
// text ends one header and starts another. With two of them it ends the whole request and starts a
// second one on the same connection. A NUL truncates instead, since curl_slist_append takes a
// const char*. None of it must reach the wire.
TEST(CurlHTTPRequestManagerTests, rejectsAHeaderValueContainingCrlf) {
    ScriptedServer server({okResponse()});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(
        makeGetWithHeaders(server.url("/").c_str(), makeHeaders({{"X-Test", "a\r\nX-Evil: 1"}})), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    EXPECT_TRUE(completion->error().has_value()) << "a header value carrying CRLF was accepted";
    EXPECT_TRUE(server.requestLines().empty())
        << "the request went out anyway, so a caller-supplied string injected a header";
}

TEST(CurlHTTPRequestManagerTests, rejectsAHeaderValueThatWouldSplitTheRequest) {
    ScriptedServer server({okResponse()});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(
        makeGetWithHeaders(server.url("/").c_str(),
                           makeHeaders({{"X-Test", "a\r\n\r\nGET /smuggled HTTP/1.1\r\nHost: evil\r\n"}})),
        completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    EXPECT_TRUE(completion->error().has_value()) << "a header value that ends the request was accepted";
    EXPECT_TRUE(server.requestLines().empty())
        << "a second request was written onto the connection from a header value";
}

TEST(CurlHTTPRequestManagerTests, rejectsAHeaderValueContainingANul) {
    ScriptedServer server({okResponse()});

    Value headers;
    headers.setMapValue(std::string_view("X-Test"), Value(StringBox::fromString(std::string("a\0b", 3))));

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeGetWithHeaders(server.url("/").c_str(), headers), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    EXPECT_TRUE(completion->error().has_value())
        << "a NUL in a header value truncates it at the C string boundary, which must not pass silently";
    EXPECT_TRUE(server.requestLines().empty()) << "a truncated header value was sent";
}

TEST(CurlHTTPRequestManagerTests, rejectsAHeaderNameContainingCrlf) {
    ScriptedServer server({okResponse()});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(
        makeGetWithHeaders(server.url("/").c_str(), makeHeaders({{"X-Test\r\nX-Evil", "1"}})), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    EXPECT_TRUE(completion->error().has_value()) << "a header name carrying CRLF was accepted";
    EXPECT_TRUE(server.requestLines().empty()) << "the request went out anyway";
}

TEST(CurlHTTPRequestManagerTests, rejectsAMethodContainingCrlf) {
    ScriptedServer server({okResponse()});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeRequest("PURGE\r\nX-Evil: 1", server.url("/").c_str()), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    EXPECT_TRUE(completion->error().has_value()) << "a method carrying CRLF was accepted";
    EXPECT_TRUE(server.requestLines().empty()) << "the request line was split across two lines on the wire";
}

TEST(CurlHTTPRequestManagerTests, rejectsAUrlContainingCrlf) {
    // curl parses the URL itself, so this is a guard on curl keeping that promise rather than on
    // anything this manager does. Pointed at a server that would otherwise answer, so passing on a
    // connection failure instead of a rejected URL is not possible.
    ScriptedServer server({okResponse()});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeGet((server.url("/") + "\r\nX-Evil: 1").c_str()), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    EXPECT_TRUE(completion->error().has_value()) << "a URL carrying CRLF was accepted";
    EXPECT_TRUE(server.requestLines().empty()) << "the request reached the wire with CRLF in its target";
}

TEST(CurlHTTPRequestManagerTests, sendsAnEmptyValuedRequestHeader) {
    ScriptedServer server({okResponse()});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeGetWithHeaders(server.url("/").c_str(), makeHeaders({{"X-Trace", ""}})), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    ASSERT_EQ(completion->statusCode(), 200);

    auto requests = server.requests();
    ASSERT_EQ(requests.size(), 1u);
    EXPECT_NE(requests[0].find("X-Trace:\r\n"), std::string::npos)
        << "an empty valued header was dropped, so the same JavaScript sends different bytes here "
           "than it does on iOS and web. Request was:\n"
        << requests[0];
}

TEST(CurlHTTPRequestManagerTests, sendsAWhitespaceOnlyRequestHeaderAsEmpty) {
    ScriptedServer server({okResponse()});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeGetWithHeaders(server.url("/").c_str(), makeHeaders({{"X-Trace", "   "}})),
                            completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    ASSERT_EQ(completion->statusCode(), 200);

    auto requests = server.requests();
    ASSERT_EQ(requests.size(), 1u);
    EXPECT_NE(requests[0].find("X-Trace:\r\n"), std::string::npos)
        << "curl drops a whitespace only value just as it drops an empty one, since it steps over "
           "the whitespace before testing. Request was:\n"
        << requests[0];
}

TEST(CurlHTTPRequestManagerTests, sendsAnEmptyValuedOverrideOfACurlDefault) {
    ScriptedServer server({okResponse()});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeGetWithHeaders(server.url("/").c_str(), makeHeaders({{"Accept", ""}})), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    ASSERT_EQ(completion->statusCode(), 200);

    auto requests = server.requests();
    ASSERT_EQ(requests.size(), 1u);
    EXPECT_EQ(requests[0].find("Accept: */*"), std::string::npos)
        << "curl's own default was sent even though the caller overrode Accept. Request was:\n"
        << requests[0];
    EXPECT_NE(requests[0].find("Accept:\r\n"), std::string::npos)
        << "overriding a header curl generates itself with an empty value loses it twice over: "
           "curl suppresses its default because a custom one exists, then drops the custom one. "
           "Request was:\n"
        << requests[0];
}

TEST(CurlHTTPRequestManagerTests, failsAResponseItCannotDecode) {
    ScriptedServer server({"HTTP/1.1 200 OK\r\n"
                           "Content-Encoding: gzip\r\n"
                           "Content-Length: 3\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "\x1f\x8b\x08"});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeGet(server.url("/").c_str()), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    ASSERT_TRUE(completion->error().has_value())
        << "a body this build cannot decode was handed back as a successful response";
    EXPECT_NE(completion->error().value().find("gzip"), std::string::npos)
        << "the failure should name the encoding it could not decode, but said: " << completion->error().value();
}

TEST(CurlHTTPRequestManagerTests, failsAResponseItCannotDecodeWhateverTheHeaderCasing) {
    // Casing is the origin's choice and nothing here canonicalizes it, so the check has to be
    // insensitive on the header name and on its value.
    ScriptedServer server({"HTTP/1.1 200 OK\r\n"
                           "content-encoding: GZIP\r\n"
                           "Content-Length: 3\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "\x1f\x8b\x08"});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeGet(server.url("/").c_str()), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    ASSERT_TRUE(completion->error().has_value())
        << "a lowercase Content-Encoding went unnoticed, so the response was handed back undecoded";
    EXPECT_NE(completion->error().value().find("GZIP"), std::string::npos)
        << "failed, but not over the encoding, so this passes for the wrong reason: "
        << completion->error().value();
}

TEST(CurlHTTPRequestManagerTests, acceptsAnIdentityContentEncoding) {
    ScriptedServer server({"HTTP/1.1 200 OK\r\n"
                           "Content-Encoding: identity\r\n"
                           "Content-Length: 2\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "ok"});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeGet(server.url("/").c_str()), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    EXPECT_FALSE(completion->error().has_value())
        << "identity means the body is unencoded, so there is nothing to reject: "
        << completion->error().value_or("");
    EXPECT_EQ(completion->statusCode(), 200);
    EXPECT_EQ(completion->bodySize(), 2u);
}

TEST(CurlHTTPRequestManagerTests, ignoresContentEncodingOnABodilessResponse) {
    // A HEAD against an origin that compresses still advertises the encoding the body would have
    // had. There is no body to decode, so it must not be treated as a failure.
    ScriptedServer server({"HTTP/1.1 200 OK\r\n"
                           "Content-Encoding: gzip\r\n"
                           "Content-Length: 569\r\n"
                           "Connection: close\r\n"
                           "\r\n"});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeRequest("HEAD", server.url("/").c_str()), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(10)));
    EXPECT_FALSE(completion->error().has_value())
        << "a HEAD carries no body, so its advertised encoding is nothing to reject: "
        << completion->error().value_or("");
    EXPECT_EQ(completion->statusCode(), 200);
}

TEST(CurlHTTPRequestManagerTests, sendsTheBodyAsItWasWhenTheRequestWasMade) {
    // Big enough to span several socket writes, so a body streamed out of the caller's memory shows
    // the overwrite partway through rather than not at all, and under curl's 1 MiB Expect:
    // 100-continue threshold, which a ScriptedServer would never answer.
    constexpr size_t kBodySize = 512 * 1024;

    ScriptedServer server({"HTTP/1.1 200 OK\r\n"
                           "Content-Length: 2\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "ok"});

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();

    // The body a caller hands over is a view over a live JavaScript ArrayBuffer, and JavaScript is
    // free to write to that array the moment performRequest returns. Overwriting it here stands in
    // for that, so what reaches the wire has to be what the request was made with.
    auto source = makeShared<ByteBuffer>(std::string(kBodySize, 'a'));
    manager->performRequest(snap::valdi_core::HTTPRequest(StringBox::fromCString(server.url("/submit").c_str()),
                                                          STRING_LITERAL("POST"),
                                                          Value(),
                                                          source->toBytesView(),
                                                          0),
                            completion);
    std::memset(source->data(), 'b', source->size());

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(30)));
    ASSERT_EQ(completion->statusCode(), 200);

    auto requests = server.requests();
    ASSERT_EQ(requests.size(), 1u);

    auto body = requests[0].substr(requests[0].find("\r\n\r\n") + 4);
    ASSERT_EQ(body.size(), kBodySize) << "the whole body did not arrive, so there is nothing to judge";

    auto unchanged = static_cast<size_t>(std::count(body.begin(), body.end(), 'a'));
    EXPECT_EQ(unchanged, kBodySize) << unchanged << " of " << kBodySize
                                    << " body bytes were the ones the request was made with; the rest were read "
                                       "on the curl thread, after performRequest had already returned to its caller";
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

TEST(CurlHTTPRequestManagerTests, followsASeeOtherFromACustomVerbWithGet) {
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

    auto start = std::chrono::steady_clock::now();
    manager->performRequest(makeRequest("DELETE", server.url("/thing").c_str()), completion);
    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(30)));
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

    ASSERT_EQ(completion->statusCode(), 200);

    auto requests = server.requestLines();
    ASSERT_EQ(requests.size(), 2u);
    EXPECT_EQ(requests[0], "DELETE /thing HTTP/1.1");
    EXPECT_EQ(requests[1], "GET /final HTTP/1.1")
        << "a 303 must be followed with GET, including for a verb curl only knows as a custom "
           "request line";

    EXPECT_LT(elapsed.count(), 500) << "a two hop redirect took " << elapsed.count()
                                    << " ms, so a hop is not picked up until the poll has slept out "
                                       "its whole timeout";
}

TEST(CurlHTTPRequestManagerTests, dropsThePutBodyFollowingASeeOtherWithGet) {
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
    manager->performRequest(makeRequestWithBody("PUT", server.url("/thing").c_str(), "a=1"), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(30)));
    ASSERT_EQ(completion->statusCode(), 200);

    auto requests = server.requests();
    ASSERT_EQ(requests.size(), 2u);
    EXPECT_EQ(requests[0].substr(0, requests[0].find("\r\n")), "PUT /thing HTTP/1.1");
    EXPECT_EQ(requests[1].substr(0, requests[1].find("\r\n")), "GET /final HTTP/1.1")
        << "a 303 must be followed with GET. Request was:\n"
        << requests[1];
    EXPECT_EQ(requests[1].find("Content-Length"), std::string::npos)
        << "the verb became GET but the body came along with it. Request was:\n"
        << requests[1];
}

TEST(CurlHTTPRequestManagerTests, keepsThePutBodyAcrossAMovedPermanently) {
    ScriptedServer server({"HTTP/1.1 301 Moved Permanently\r\n"
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
    manager->performRequest(makeRequestWithBody("PUT", server.url("/thing").c_str(), "a=1"), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(30)));
    ASSERT_EQ(completion->statusCode(), 200);

    auto requests = server.requests();
    ASSERT_EQ(requests.size(), 2u);
    EXPECT_EQ(requests[1].substr(0, requests[1].find("\r\n")), "PUT /final HTTP/1.1")
        << "only POST turns into GET on a 301. Request was:\n"
        << requests[1];
    EXPECT_NE(requests[1].find("Content-Length: 3\r\n"), std::string::npos)
        << "the redirected PUT was sent with no body at all. Request was:\n"
        << requests[1];
    EXPECT_NE(requests[1].find("\r\n\r\na=1"), std::string::npos)
        << "the redirected PUT declared a body but did not send it. Request was:\n"
        << requests[1];
}

TEST(CurlHTTPRequestManagerTests, keepsThePostBodyAcrossATemporaryRedirect) {
    ScriptedServer server({"HTTP/1.1 307 Temporary Redirect\r\n"
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

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(30)));
    ASSERT_EQ(completion->statusCode(), 200);

    auto requests = server.requests();
    ASSERT_EQ(requests.size(), 2u);
    EXPECT_EQ(requests[1].substr(0, requests[1].find("\r\n")), "POST /final HTTP/1.1")
        << "a 307 must be repeated with the original verb. Request was:\n"
        << requests[1];
    EXPECT_NE(requests[1].find("\r\n\r\na=1"), std::string::npos)
        << "a 307 must be repeated with the original body. Request was:\n"
        << requests[1];
}

TEST(CurlHTTPRequestManagerTests, failsARedirectChainThatNeverEnds) {
    // One response more than kMaxRedirects permits follows, so the last has to be refused rather
    // than followed. This pins the follow count, which the manager now keeps itself instead of
    // leaving to CURLOPT_MAXREDIRS.
    ScriptedServer server(std::vector<std::string>(11,
                                                   "HTTP/1.1 302 Found\r\n"
                                                   "Location: /loop\r\n"
                                                   "Content-Length: 0\r\n"
                                                   "Connection: close\r\n"
                                                   "\r\n"));

    auto manager = makeCurlHTTPRequestManager();
    auto completion = std::make_shared<RecordingCompletion>();
    manager->performRequest(makeGet(server.url("/loop").c_str()), completion);

    ASSERT_TRUE(completion->waitForCompletion(std::chrono::seconds(30)));
    EXPECT_TRUE(completion->error().has_value()) << "an endless redirect chain was reported as a response";
    EXPECT_EQ(server.requestLines().size(), 11u)
        << "the original request plus ten follows is what kMaxRedirects allows";
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
    // Ten pieces 200 ms apart, so about two seconds in total. That is well past the idle timeout
    // below, but never a whole second without data. An overall CURLOPT_TIMEOUT would cut it off.
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
            // Two responses primed, one request given. That is exactly the state
            // followsASeeOtherWithGet is left in when a redirect is not followed, which is the
            // regression it exists to catch, and it leaves the serving thread blocked in accept()
            // waiting for a second connection that never comes.
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

    // Holding the payload once measured 1.03x and holding it twice measured 2.03x, so the threshold
    // sits midway instead of just under the failing value.
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
    // that was merely detached from curl would have fired if it had not been dropped. The manager
    // is left alive on purpose, so this pins the drop on cancel() and not on shutdown.
    ASSERT_TRUE(server.waitForDisconnect(std::chrono::seconds(30)))
        << "curl never hung up, so the cancelled transfer was never aborted";

    EXPECT_FALSE(completion->waitForCompletion(std::chrono::seconds(1)))
        << "a cancelled request must leave its completion uncalled, as on iOS and Android";
}

TEST(CurlHTTPRequestManagerTests, abortsACancelledTransferWithoutWaitingOutThePollTimeout) {
    StallServer server;

    // The idle timeout off, so curl arms no timer of its own and the poll really does sit for
    // kPollTimeoutMs. Under the sixty second default, CURLOPT_LOW_SPEED_TIME has curl re-arm a one
    // second timer that caps the poll, which hides all but a second of the wait and makes this a
    // coin toss rather than a measurement.
    auto manager = makeCurlHTTPRequestManager(StringBox(), 0);
    auto completion = std::make_shared<RecordingCompletion>();
    auto cancelable = manager->performRequest(makeGet(server.url().c_str()), completion);

    ASSERT_TRUE(server.waitForConnection(std::chrono::seconds(5)))
        << "curl never connected, so there is no live transfer to cancel";

    // waitForConnection returns at accept(), which is the same event that makes curl's socket
    // writable and ends its poll. Cancelling straight away is therefore observed by a
    // curl_multi_perform pass that was going to run anyway. Settling first is what puts the curl
    // thread back inside curl_multi_poll, which is the state this test is about.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

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
        << "shutdown reported an in-flight request. Neither the iOS nor the Android manager promises "
           "a completion at teardown, so it should be dropped, the same as a cancellation";
}

} // namespace ValdiTest
