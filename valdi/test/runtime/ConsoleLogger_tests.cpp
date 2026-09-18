#include <gtest/gtest.h>

#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include <ostream>
#include <streambuf>
#include <string>

using namespace Valdi;

namespace ValdiTest {

#if !defined(SC_IOS) && !defined(__ANDROID__)

namespace {

class BufferedSink : public std::streambuf {
public:
    std::string delivered;

protected:
    int overflow(int ch) override {
        if (ch != traits_type::eof()) {
            _pending.push_back(static_cast<char>(ch));
        }
        return ch;
    }

    int sync() override {
        delivered += _pending;
        _pending.clear();
        return 0;
    }

    std::streamsize xsputn(const char* s, std::streamsize count) override {
        _pending.append(s, static_cast<size_t>(count));
        return count;
    }

private:
    std::string _pending;
};

} // namespace

TEST(ConsoleLogger, writeDirectReachesTheDeviceBeforeReturning) {
    BufferedSink sink;
    std::ostream stream(&sink);
    ConsoleLogger logger(stream);

    logger.writeDirect("hello\n");

    ASSERT_EQ("hello\n", sink.delivered);
}

TEST(ConsoleLogger, logReachesTheDeviceBeforeReturning) {
    BufferedSink sink;
    std::ostream stream(&sink);
    ConsoleLogger logger(stream);

    logger.log(LogTypeError, "boom");

    ASSERT_NE(std::string::npos, sink.delivered.find("boom"));
}

#endif

} // namespace ValdiTest
