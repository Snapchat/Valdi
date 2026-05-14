load("@build_bazel_rules_swift//swift:swift.bzl", "swift_interop_hint", "swift_library")

swift_interop_hint(
    name = "CBacktrace_swift_interop",
    module_name = "CBacktrace",
)

cc_library(
    name = "CBacktrace",
    srcs = select({
        "@bazel_tools//src/conditions:darwin": glob(
            ["Sources/CBacktrace/*.c"],
            exclude = ["Sources/CBacktrace/elf.c"],
        ),
        "//conditions:default": glob(["Sources/CBacktrace/*.c"]),
    }) + glob(["Sources/CBacktrace/*.h"]),
    hdrs = ["Sources/CBacktrace/include/backtrace.h"],
    aspect_hints = [":CBacktrace_swift_interop"],
    includes = ["Sources/CBacktrace/include"],
    visibility = ["//visibility:public"],
)

swift_library(
    name = "Backtrace",
    srcs = glob(["Sources/Backtrace/*.swift"]),
    copts = ["-DSWIFT_PACKAGE"],
    module_name = "Backtrace",
    visibility = ["//visibility:public"],
    deps = [":CBacktrace"],
)
