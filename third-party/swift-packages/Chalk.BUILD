load("@build_bazel_rules_swift//swift:swift.bzl", "swift_library")

swift_library(
    name = "Chalk",
    # Chalk's single source file lives at the package root, not in Sources/.
    srcs = ["Chalk.swift"],
    copts = ["-DSWIFT_PACKAGE"],
    module_name = "Chalk",
    visibility = ["//visibility:public"],
)
