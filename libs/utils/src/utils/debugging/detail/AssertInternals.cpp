#include "utils/debugging/detail/AssertInternals.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>

namespace snap::detail {

std::string combineString(std::string_view msg, const char* expressionStr) {
    if (!msg.empty()) {
        std::string_view expr(expressionStr);
        std::string out;
        out.reserve(msg.length() + 2 + expr.length());
        out += msg;
        out += ": ";
        out += expr;

        return out;
    }

    return expressionStr;
}

} // namespace snap::detail

#if defined(__APPLE__)

#include <cstring>

const size_t __sc_assert_rtn_message_size = 1024;
extern "C" char __sc_assert_rtn_message[1024] __attribute__((weak)) = {0};

#if defined(NDEBUG)
extern "C" __attribute__((noreturn)) __attribute__((noinline)) void __assert_rtn(const char*,
                                                                                 const char*,
                                                                                 int,
                                                                                 const char*);
#endif

// Throwing function
__attribute__((noreturn)) void __sc_apple_system_assert(const char* expr, const char* path, int line) {
    std::strncpy(__sc_assert_rtn_message, expr, __sc_assert_rtn_message_size);
    __sc_assert_rtn_message[__sc_assert_rtn_message_size - 1] = '\0';
    __assert_rtn(reinterpret_cast<const char*>(-1L), path, line, expr);
}

#endif
