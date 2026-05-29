load("@rules_rust//cargo:defs.bzl", "cargo_build_script", "cargo_toml_env_vars")
load("@rules_rust//rust:defs.bzl", "rust_binary")

package(default_visibility = ["//visibility:public"])

cargo_toml_env_vars(
    name = "cargo_toml_env_vars",
    src = "Cargo.toml",
)

cargo_build_script(
    name = "pngquant_build_script",
    srcs = ["rust/build.rs"],
    compile_data = [
        "pngquant.c",
        "pngquant_opts.h",
        "rwpng.c",
        "rwpng.h",
        "@pngquant_crates__imagequant-sys-4.1.0//:libimagequant.h",
        "@pngquant_crates__libpng-sys-1.1.11//:vendor/png.h",
        "@pngquant_crates__libpng-sys-1.1.11//:vendor/pngconf.h",
        "@pngquant_crates__libpng-sys-1.1.11//:vendor/scripts/pnglibconf.h.prebuilt",
        "@pngquant_crates__libz-sys-1.1.28//:src/zlib/zconf.h",
        "@pngquant_crates__libz-sys-1.1.28//:src/zlib/zlib.h",
    ],
    data = [
        "pngquant.c",
        "pngquant_opts.h",
        "rwpng.c",
        "rwpng.h",
        "@pngquant_crates__imagequant-sys-4.1.0//:libimagequant.h",
        "@pngquant_crates__libpng-sys-1.1.11//:vendor/png.h",
        "@pngquant_crates__libpng-sys-1.1.11//:vendor/pngconf.h",
        "@pngquant_crates__libpng-sys-1.1.11//:vendor/scripts/pnglibconf.h.prebuilt",
        "@pngquant_crates__libz-sys-1.1.28//:src/zlib/zconf.h",
        "@pngquant_crates__libz-sys-1.1.28//:src/zlib/zlib.h",
    ],
    link_deps = [
        "@pngquant_crates__imagequant-sys-4.1.0//:imagequant_sys",
        "@pngquant_crates__libpng-sys-1.1.11//:libpng_sys",
    ],
    build_script_env = {
        "DEP_IMAGEQUANT_INCLUDE_FILE": "$(execpath @pngquant_crates__imagequant-sys-4.1.0//:libimagequant.h)",
        "DEP_PNG_INCLUDE_FILE": "$(execpath @pngquant_crates__libpng-sys-1.1.11//:vendor/png.h)",
    },
    crate_name = "build_script_build",
    deps = [
        "@pngquant_crates__cc-1.2.61//:cc",
        "@pngquant_crates__dunce-1.0.5//:dunce",
    ],
)

rust_binary(
    name = "pngquant",
    srcs = glob(["rust/**/*.rs"]),
    crate_features = [],
    crate_name = "pngquant",
    crate_root = "rust/bin.rs",
    edition = "2021",
    rustc_env_files = [":cargo_toml_env_vars"],
    deps = [
        ":pngquant_build_script",
        "@pngquant_crates__getopts-0.2.24//:getopts",
        "@pngquant_crates__imagequant-sys-4.1.0//:imagequant_sys",
        "@pngquant_crates__libc-0.2.186//:libc",
        "@pngquant_crates__libpng-sys-1.1.11//:libpng_sys",
        "@pngquant_crates__wild-2.2.1//:wild",
    ],
)

genrule(
    name = "libimagequant_h",
    srcs = ["@pngquant_crates__imagequant-sys-4.1.0//:libimagequant.h"],
    outs = ["libimagequant.h"],
    cmd = "cp $(location @pngquant_crates__imagequant-sys-4.1.0//:libimagequant.h) $@",
)

cc_library(
    name = "pngquant_lib",
    srcs = [
        "pngquant.c",
        "rwpng.c",
    ],
    hdrs = [
        "pngquant_opts.h",
        "rwpng.h",
        ":libimagequant_h",
    ],
    copts = [
        "-w",
        "-DPNGQUANT_NO_MAIN=1",
        "-DNDEBUG=1",
    ],
    includes = [
        ".",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@libpng//:libpng",
        "@pngquant_crates__imagequant-sys-4.1.0//:imagequant_sys",
    ],
)

