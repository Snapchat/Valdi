WARNING_FLAGS = [
    # All warnings are errors
    "-Werror",
    # Enable most warnings:
    "-Wall",
    "-Wextra",
    "-pedantic",
    "-Wformat",
    "-Wformat-security",
    "-Wimplicit-fallthrough",
    "-Wshorten-64-to-32",
    "-Wstring-conversion",
    "-Wunreachable-code",

    # Disable for a lot of code:
    "-Wno-gnu-anonymous-struct",
    "-Wno-nested-anon-types",
    "-Wno-unused-parameter",
    # Disable for SC_PICK_ASSERTION_ARGS:
    "-Wno-gnu-zero-variadic-macro-arguments",
    # Disable errors for deprecation warnings
    "-Wno-error=deprecated-declarations",
    # Disable generally
    "-Wno-unused-command-line-argument",
    # Disable for gtest MOCK_METHOD():
    "-Wno-extra-semi",
    # Disable for fmt (FMT_STRING)
    "-Wno-unused-local-typedef",
    # Disable for json.h when used with NDK r23
    "-Wno-deprecated-volatile",
    # Disable, used widely e.g. gtest
    "-Wno-deprecated-copy",
    # Can be removed after updating libc++
    "-Wno-deprecated-experimental-coroutine",

    # clang-tidy uses android build flags but a mismatching llvm/clang version.
    # This option suppresses error triggered by -Wno-deprecated-experimental-coroutine which is
    # not available for NDK 23 clang. TODO: remove when updated to NDK 25 or later versions or when libc++ is updated.
    "-Wno-unknown-warning-option",

    # Only disable warnings here that have wide effect; prefer to set copts at the module level
    # when possible.

    # Temporary solution for zlib build errors
    "-Wno-implicit-fallthrough",
    # Third-party headers (e.g. phmap) use [[nodiscard]] patterns that trigger
    # this warning under newer Clang toolchains
    "-Wno-unused-result",
] + select({
    # some protobuf headers trigger a very noisy warning from sysroot about major/minor
    # macros :(
    "@platforms//os:linux": ["-Wno-#pragma-messages"],
    "//conditions:default": [],
})

DJINNI_DESCRIPTION_METHOD_COMPILE_FLAGS = select({
    "@snap_platforms//flavors:production": ["-DDJINNI_DISABLE_DESCRIPTION_METHODS"],
    "//conditions:default": [],
})

TRACE_COMPILE_FLAGS = select({
    "@snap_platforms//toggles:ztrace_bare_enabled": ["-finstrument-function-entry-bare"],
    "//conditions:default": [],
}) + select({
    "@snap_platforms//flavors:diagnostic_production": ["-fsanitize-coverage=func,trace-pc-guard"],
    "//conditions:default": [],
})

GRPC_PROTO_LITE_COMPILE_FLAGS = [
    # Most protos are lite protos and require the types in graphene to be MessageLite:
    "-DGRPC_USE_PROTO_LITE",
]

PLATFORM_COMPILE_FLAGS = select({
    "@platforms//os:linux": [
        "-DBOOST_ASIO_HAS_STD_CHRONO",
    ],
    "//conditions:default": [],
})

COMMON_COMPILE_FLAGS = (
    WARNING_FLAGS +
    DJINNI_DESCRIPTION_METHOD_COMPILE_FLAGS +
    TRACE_COMPILE_FLAGS +
    GRPC_PROTO_LITE_COMPILE_FLAGS +
    PLATFORM_COMPILE_FLAGS
)

OBJC_ONLY_FLAGS = [
    "-fdiagnostics-color",
    "-fno-aligned-allocation",
    "-fvisibility=default",
]

OBJCPP_ONLY_FLAGS = [
    "-ObjC++",
    "-std=c++20",
    "-fno-c++-static-destructors",
]

OBJC_FLAGS = OBJC_ONLY_FLAGS + OBJCPP_ONLY_FLAGS
