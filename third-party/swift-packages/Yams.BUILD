load("@build_bazel_rules_swift//swift:swift.bzl", "swift_interop_hint", "swift_library")

swift_interop_hint(
    name = "CYaml_swift_interop",
    module_name = "CYaml",
)

cc_library(
    name = "CYaml",
    srcs = glob([
        "Sources/CYaml/src/*.c",
        "Sources/CYaml/src/*.h",
    ]),
    hdrs = ["Sources/CYaml/include/yaml.h"],
    aspect_hints = [":CYaml_swift_interop"],
    copts = ["-fPIC"],
    includes = ["Sources/CYaml/include"],
    linkstatic = True,
    visibility = ["//visibility:public"],
)

swift_library(
    name = "Yams",
    srcs = glob(["Sources/Yams/**/*.swift"]),
    copts = ["-DSWIFT_PACKAGE"],
    module_name = "Yams",
    visibility = ["//visibility:public"],
    deps = [":CYaml"],
)
