load("@build_bazel_rules_swift//swift:swift.bzl", "swift_library")

# swift-crypto has two code paths:
#   - Apple (macOS/iOS/watchOS/tvOS): `Crypto` is a thin @_exported re-export of
#     the system CryptoKit framework. Only CRYPTO_IN_SWIFTPM is defined and no
#     BoringSSL sources are pulled in. This is selected by:
#         #if CRYPTO_IN_SWIFTPM && !CRYPTO_IN_SWIFTPM_FORCE_BUILD_API
#   - Non-Apple (Linux, Android, Windows, WASI): `Crypto` is a pure-Swift
#     implementation backed by the vendored BoringSSL in Sources/CCryptoBoringSSL.
#     On these platforms we also define CRYPTO_IN_SWIFTPM_FORCE_BUILD_API so the
#     #else path is selected.
#
# See Compiler/.build/checkouts/swift-crypto/Package.swift for the canonical
# set of defines and target wiring.

# ---------------------------------------------------------------------------
# BoringSSL C library (vendored inside swift-crypto). Only used on non-Apple
# platforms but available to build everywhere so tests/tooling on any host can
# still analyse it.
#
# The include/ directory ships a pre-written `module.modulemap` which
# rules_swift picks up automatically through the cc_library's `hdrs`; we
# therefore do NOT attach a swift_interop_hint here (that would try to
# generate a competing module map).
# ---------------------------------------------------------------------------
cc_library(
    name = "CCryptoBoringSSL",
    srcs = glob(
        [
            "Sources/CCryptoBoringSSL/crypto/**/*.c",
            "Sources/CCryptoBoringSSL/crypto/**/*.h",
            # Assembly files carry platform/arch guards internally
            # (e.g. `#if defined(__x86_64__) && defined(__linux__)`), so it is
            # safe to feed them to the compiler on every platform; only the
            # matching variant emits code.
            "Sources/CCryptoBoringSSL/crypto/**/*.S",
            "Sources/CCryptoBoringSSL/third_party/**/*.c",
            "Sources/CCryptoBoringSSL/third_party/**/*.h",
            "Sources/CCryptoBoringSSL/third_party/**/*.S",
        ],
        exclude = [
            # Mirrors the excludes in swift-crypto's Package.swift: these files
            # use networking APIs (<netdb.h>) that are not portable (they break
            # WASI in particular) and are not needed for Crypto's functionality.
            "Sources/CCryptoBoringSSL/crypto/bio/connect.c",
            "Sources/CCryptoBoringSSL/crypto/bio/socket_helper.c",
            "Sources/CCryptoBoringSSL/crypto/bio/socket.c",
        ],
    ),
    hdrs = glob(["Sources/CCryptoBoringSSL/include/**/*.h"]) + [
        "Sources/CCryptoBoringSSL/include/module.modulemap",
    ],
    includes = ["Sources/CCryptoBoringSSL/include"],
    linkstatic = True,
    visibility = ["//visibility:public"],
)

cc_library(
    name = "CCryptoBoringSSLShims",
    srcs = ["Sources/CCryptoBoringSSLShims/shims.c"],
    hdrs = glob(["Sources/CCryptoBoringSSLShims/include/**/*.h"]) + [
        "Sources/CCryptoBoringSSLShims/include/module.modulemap",
    ],
    includes = ["Sources/CCryptoBoringSSLShims/include"],
    linkstatic = True,
    visibility = ["//visibility:public"],
    deps = [":CCryptoBoringSSL"],
)

# Swift wrapper around BoringSSL. Consumed by Crypto on non-Apple platforms.
swift_library(
    name = "CryptoBoringWrapper",
    srcs = glob(["Sources/CryptoBoringWrapper/**/*.swift"]),
    copts = [
        "-DSWIFT_PACKAGE",
        "-DCRYPTO_IN_SWIFTPM",
        "-DCRYPTO_IN_SWIFTPM_FORCE_BUILD_API",
    ],
    module_name = "CryptoBoringWrapper",
    visibility = ["//visibility:public"],
    deps = [
        ":CCryptoBoringSSL",
        ":CCryptoBoringSSLShims",
    ],
)

# The `Crypto` module consumers actually import.
#
# - On macOS (and other Apple platforms), the swift sources take the
#   @_exported-CryptoKit path: no extra deps, no FORCE_BUILD_API define.
# - On Linux (and every other non-Apple platform via //conditions:default)
#   we define CRYPTO_IN_SWIFTPM_FORCE_BUILD_API and link the BoringSSL-backed
#   Swift targets so the full pure-Swift implementation is compiled.
swift_library(
    name = "Crypto",
    srcs = glob(["Sources/Crypto/**/*.swift"]),
    copts = [
        "-DSWIFT_PACKAGE",
        "-DCRYPTO_IN_SWIFTPM",
    ] + select({
        "@platforms//os:macos": [],
        "@platforms//os:ios": [],
        "@platforms//os:tvos": [],
        "@platforms//os:watchos": [],
        "//conditions:default": ["-DCRYPTO_IN_SWIFTPM_FORCE_BUILD_API"],
    }),
    module_name = "Crypto",
    visibility = ["//visibility:public"],
    deps = select({
        "@platforms//os:macos": [],
        "@platforms//os:ios": [],
        "@platforms//os:tvos": [],
        "@platforms//os:watchos": [],
        "//conditions:default": [
            ":CCryptoBoringSSL",
            ":CCryptoBoringSSLShims",
            ":CryptoBoringWrapper",
        ],
    }),
)
