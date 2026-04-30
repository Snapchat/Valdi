load("@build_bazel_rules_swift//swift:swift.bzl", "swift_library")

swift_library(
    name = "SWXMLHash",
    srcs = glob(["Source/**/*.swift"]),
    copts = ["-DSWIFT_PACKAGE"],
    module_name = "SWXMLHash",
    visibility = ["//visibility:public"],
)
