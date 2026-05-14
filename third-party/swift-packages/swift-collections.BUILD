load("@build_bazel_rules_swift//swift:swift.bzl", "swift_library")

# Internal utilities shared by all collection modules.
swift_library(
    name = "_CollectionsUtilities",
    srcs = glob(["Sources/_CollectionsUtilities/**/*.swift"]),
    copts = [
        "-DSWIFT_PACKAGE",
        "-DCOLLECTIONS_RANDOMIZED_TESTING",
    ],
    module_name = "_CollectionsUtilities",
    visibility = ["//visibility:public"],
)

swift_library(
    name = "DequeModule",
    srcs = glob(["Sources/DequeModule/**/*.swift"]),
    copts = [
        "-DSWIFT_PACKAGE",
        "-DCOLLECTIONS_RANDOMIZED_TESTING",
    ],
    module_name = "DequeModule",
    visibility = ["//visibility:public"],
    deps = [":_CollectionsUtilities"],
)

swift_library(
    name = "OrderedCollections",
    srcs = glob(["Sources/OrderedCollections/**/*.swift"]),
    copts = [
        "-DSWIFT_PACKAGE",
        "-DCOLLECTIONS_RANDOMIZED_TESTING",
    ],
    module_name = "OrderedCollections",
    visibility = ["//visibility:public"],
    deps = [":_CollectionsUtilities"],
)

swift_library(
    name = "BitCollections",
    srcs = glob(["Sources/BitCollections/**/*.swift"]),
    copts = [
        "-DSWIFT_PACKAGE",
        "-DCOLLECTIONS_RANDOMIZED_TESTING",
    ],
    module_name = "BitCollections",
    visibility = ["//visibility:public"],
    deps = [":_CollectionsUtilities"],
)

swift_library(
    name = "HashTreeCollections",
    srcs = glob(["Sources/HashTreeCollections/**/*.swift"]),
    copts = [
        "-DSWIFT_PACKAGE",
        "-DCOLLECTIONS_RANDOMIZED_TESTING",
    ],
    module_name = "HashTreeCollections",
    visibility = ["//visibility:public"],
    deps = [":_CollectionsUtilities"],
)

swift_library(
    name = "HeapModule",
    srcs = glob(["Sources/HeapModule/**/*.swift"]),
    copts = [
        "-DSWIFT_PACKAGE",
        "-DCOLLECTIONS_RANDOMIZED_TESTING",
    ],
    module_name = "HeapModule",
    visibility = ["//visibility:public"],
    deps = [":_CollectionsUtilities"],
)

swift_library(
    name = "RopeModule",
    srcs = glob(["Sources/RopeModule/**/*.swift"]),
    copts = [
        "-DSWIFT_PACKAGE",
        "-DCOLLECTIONS_RANDOMIZED_TESTING",
    ],
    module_name = "RopeModule",
    visibility = ["//visibility:public"],
    deps = [":_CollectionsUtilities"],
)

# The Collections module re-exports all sub-modules and is what consumers import.
swift_library(
    name = "Collections",
    srcs = glob(["Sources/Collections/**/*.swift"]),
    copts = [
        "-DSWIFT_PACKAGE",
        "-DCOLLECTIONS_RANDOMIZED_TESTING",
    ],
    module_name = "Collections",
    visibility = ["//visibility:public"],
    deps = [
        ":BitCollections",
        ":DequeModule",
        ":HashTreeCollections",
        ":HeapModule",
        ":OrderedCollections",
        ":RopeModule",
    ],
)
