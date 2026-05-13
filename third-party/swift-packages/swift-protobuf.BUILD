load("@build_bazel_rules_swift//swift:swift.bzl", "swift_library")

swift_library(
    name = "SwiftProtobuf",
    srcs = glob(["Sources/SwiftProtobuf/**/*.swift"]),
    copts = ["-DSWIFT_PACKAGE"],
    module_name = "SwiftProtobuf",
    visibility = ["//visibility:public"],
)
