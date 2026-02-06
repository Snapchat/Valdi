#pragma once

#include <cassert>
#include <cstdlib>
#include <string>
#include <string_view>

#if defined(__ANDROID__)
#define _SC_SYSTEM_ASSERT(path, line, message) __assert(path, line, message)
#elif defined(__APPLE__)
__attribute__((noreturn)) void __sc_apple_system_assert(const char* expr, const char* path, int line);
#define _SC_SYSTEM_ASSERT(path, line, message) __sc_apple_system_assert(message, path, line)
#elif defined(__GLIBC__)
// Declaring a function from assert.h. It's hidden if NDEBUG is defined
// NOLINTNEXTLINE(readability-identifier-naming)
__attribute__((noinline)) extern "C" void __assert(const char* assertion, const char* file, int line);
#define _SC_SYSTEM_ASSERT(path, line, message) __assert(message, path, line)
#elif defined(EMSCRIPTEN)
#if defined(NDEBUG)
// Declaring a function from assert.h. It's hidden if NDEBUG is defined
void __assert_fail(const char*, const char*, int, const char*);
#endif
#define _SC_SYSTEM_ASSERT(path, line, message) __assert_fail(message, path, line, "unknown_func")
#else
#define _SC_SYSTEM_ASSERT(...) [&] { ::abort(); }()
#endif

// Release assertions are fused by default
// Debug assertions are not fused by default
// This can be overriden by defining _SC_ENABLE_FUSED_ASSERTION
#if !defined(_SC_ENABLE_FUSED_ASSERTION)
#if defined(NDEBUG) && !defined(EMSCRIPTEN)
#define _SC_ENABLE_FUSED_ASSERTION 1
#else
#define _SC_ENABLE_FUSED_ASSERTION 0
#endif
#endif

#if _SC_ENABLE_FUSED_ASSERTION
#define _SC_SYSTEM_ASSERT_MAYBE_FUSED(path, line, message)                                                             \
    if (::snap::fuseAssertion(path ":" #line)) {                                                                       \
        _SC_SYSTEM_ASSERT(path, line, message);                                                                        \
    } else {                                                                                                           \
        ::snap::detail::reportNonFatalAssert(path, line, message);                                                     \
    }
#else
#define _SC_SYSTEM_ASSERT_MAYBE_FUSED(path, line, message) _SC_SYSTEM_ASSERT(path, line, message)
#endif

// Helper macro, don't use in user code
#define _SC_CALL_SYSTEM_ASSERT(msg, expressionStr, file, line)                                                         \
    _SC_SYSTEM_ASSERT(file, line, ::snap::detail::combineString(msg, expressionStr).c_str());
#define _SC_CALL_SYSTEM_ASSERT_MAYBE_FUSED(msg, expressionStr, file, line)                                             \
    _SC_SYSTEM_ASSERT_MAYBE_FUSED(file, line, ::snap::detail::combineString(msg, expressionStr).c_str());

// Helper macro, don't use in user code
#define _SC_WITH_MESSAGE(expr, msg)                                                                                    \
    do {                                                                                                               \
        if constexpr (::snap::kAssertsCompiledIn) {                                                                    \
            if (::snap::releaseAssertsEnabled() && !(expr)) {                                                          \
                _SC_CALL_SYSTEM_ASSERT_MAYBE_FUSED(msg, #expr, __FILE__, __LINE__);                                    \
            }                                                                                                          \
        }                                                                                                              \
    } while (false)

#define _SC_WITHOUT_MESSAGE(expr)                                                                                      \
    do {                                                                                                               \
        if constexpr (::snap::kAssertsCompiledIn) {                                                                    \
            if (::snap::releaseAssertsEnabled() && !(expr)) {                                                          \
                _SC_CALL_SYSTEM_ASSERT_MAYBE_FUSED("", #expr, __FILE__, __LINE__);                                     \
            }                                                                                                          \
        }                                                                                                              \
    } while (false)

#define _SC_PICK_ASSERTION_ARGS(_1, _2, SC_WHICH, ...) SC_WHICH

namespace snap::detail {

// Helper function to combine expression string and user message
std::string combineString(std::string_view msg, const char* expressionStr);

inline const char* messageToCString(const char* s) {
    return s;
}
inline const char* messageToCString(const std::string& s) {
    return s.c_str();
}

void reportNonFatalAssert(std::string_view path, int64_t line, std::string_view message);

} // namespace snap::detail
