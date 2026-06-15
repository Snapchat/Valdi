// Symbol shims needed only when statically linking the Swift 5.10 runtime on
// Linux (see :local_valdi_compiler). Not part of the SwiftPM build, which
// links on a host whose system libraries provide/resolve these.

#include <cstdio>
#include <cstdlib>

// The Swift 5.10 static runtime archives were built on Ubuntu 22.04 against
// GCC >= 11 libstdc++, which added std::__throw_bad_array_new_length as an
// out-of-line helper (GLIBCXX_3.4.29). Our hermetic sysroot ships an older
// libstdc++ without it, so provide the helper here. It is only reached when
// the element count of a `new T[n]` overflows, where terminating matches the
// libstdc++ behavior of an uncaught std::bad_array_new_length.
namespace std {
[[noreturn]] void __throw_bad_array_new_length();
void __throw_bad_array_new_length() {
    ::fputs("fatal: bad array new length\n", stderr);
    ::abort();
}
}  // namespace std

// Swift 5.10's Foundation on Linux erroneously emits calls to this
// Objective-C ARC intrinsic when retaining the +0 return value of
// CoreFoundation Get-convention functions (DateFormatter, NSString,
// NSLocale, ... bridging paths). The dynamic libFoundation.so carries the
// same dangling reference but only faults via lazy PLT binding if one of
// those paths actually runs; static linking surfaces it at link time.
//
// Contract: after this call the caller owns the value at +1 and will
// release it later. The fast path (return unchanged) is only valid when the
// callee performed the objc_autoreleaseReturnValue handshake -- which never
// happens on Linux, where there is no ObjC runtime. So this must take the
// slow path unconditionally: retain. CF objects on Linux are
// Swift-refcounted (that is what toll-free bridging is built on there), and
// swift_retain is nil-tolerant and returns its argument.
extern "C" void *swift_retain(void *obj);
extern "C" void *objc_retainAutoreleasedReturnValue(void *obj) {
    return swift_retain(obj);
}
