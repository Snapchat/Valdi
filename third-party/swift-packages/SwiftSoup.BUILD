load("@build_bazel_rules_swift//swift:swift.bzl", "swift_library")

swift_library(
    name = "SwiftSoup",
    # Sources live directly in Sources/, not a Sources/SwiftSoup/ subdirectory.
    srcs = glob(["Sources/*.swift"]),
    copts = ["-DSWIFT_PACKAGE"],
    module_name = "SwiftSoup",
    visibility = ["//visibility:public"],
)
