load("@build_bazel_rules_swift//swift:swift.bzl", "swift_interop_hint", "swift_library")

swift_interop_hint(
    name = "Minizip_swift_interop",
    module_name = "Minizip",
)

cc_library(
    name = "Minizip",
    srcs = glob(
        ["Zip/minizip/*.c"],
        exclude = ["Zip/minizip/module/**"],
    ),
    hdrs = glob(["Zip/minizip/include/*.h"]),
    aspect_hints = [":Minizip_swift_interop"],
    includes = ["Zip/minizip/include"],
    linkopts = ["-lz"],
    visibility = ["//visibility:public"],
)

swift_library(
    name = "Zip",
    srcs = glob(
        ["Zip/*.swift"],
        exclude = [
            "Zip/minizip/**",
            "Zip/zlib/**",
        ],
    ),
    # -Xcc forwards the flag to the clang importer that parses glibc's
    # stdio.h when processing imports like `import Minizip`; -Wno-nullability
    # silences the nullability diagnostics from that importer. The Swift type
    # checker may still reject strict non-optional / optional mismatches; see
    # also the zip_linux_fwrite patch applied to this archive.
    copts = [
        "-DSWIFT_PACKAGE",
        "-Xcc",
        "-Wno-nullability",
    ],
    module_name = "Zip",
    visibility = ["//visibility:public"],
    deps = [":Minizip"],
)
