load("@rules_rust//rust:defs.bzl", "rust_library", "rust_static_library")

rust_library(
    name = "usvg",
    srcs = glob(["crates/usvg/src/**/*.rs"], exclude = ["crates/usvg/src/main.rs"]),
    crate_name = "usvg",
    crate_root = "crates/usvg/src/lib.rs",
    edition = "2024",
    deps = [
        "@resvg_crates//:base64",
        "@resvg_crates//:data-url",
        "@resvg_crates//:flate2",
        "@resvg_crates//:imagesize",
        "@resvg_crates//:kurbo",
        "@resvg_crates//:log",
        "@resvg_crates//:pico-args",
        "@resvg_crates//:roxmltree",
        "@resvg_crates//:simplecss",
        "@resvg_crates//:siphasher",
        "@resvg_crates//:strict-num",
        "@resvg_crates//:svgtypes",
        "@resvg_crates//:tiny-skia-path",
        "@resvg_crates//:xmlwriter",
    ],
)

rust_library(
    name = "resvg_rust",
    srcs = glob(["crates/resvg/src/**/*.rs"], exclude = ["crates/resvg/src/main.rs"]),
    crate_features = ["raster-images", "gif", "image-webp"],
    crate_name = "resvg",
    crate_root = "crates/resvg/src/lib.rs",
    edition = "2024",
    deps = [
        ":usvg",
        "@resvg_crates//:gif",
        "@resvg_crates//:image-webp",
        "@resvg_crates//:log",
        "@resvg_crates//:pico-args",
        "@resvg_crates//:rgb",
        "@resvg_crates//:svgtypes",
        "@resvg_crates//:tiny-skia",
        "@resvg_crates//:zune-jpeg",
    ],
)

rust_static_library(
    name = "resvg_static",
    srcs = ["crates/c-api/lib.rs"],
    crate_name = "resvg",
    crate_root = "crates/c-api/lib.rs",
    edition = "2024",
    deps = [
        ":resvg_rust",
        "@resvg_crates//:log",
    ],
)

cc_library(
    name = "resvg",
    hdrs = ["crates/c-api/resvg.h"],
    include_prefix = "resvg/c-api",
    strip_include_prefix = "crates/c-api",
    visibility = ["//visibility:public"],
    deps = [":resvg_static"],
)
