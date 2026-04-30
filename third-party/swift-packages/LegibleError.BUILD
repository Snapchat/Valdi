load("@build_bazel_rules_swift//swift:swift.bzl", "swift_library")

swift_library(
    name = "LegibleError",
    srcs = glob(["Sources/*.swift"]),
    copts = ["-DSWIFT_PACKAGE"],
    module_name = "LegibleError",
    visibility = ["//visibility:public"],
)
