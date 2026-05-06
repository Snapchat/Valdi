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
    ],
    data = [
        "pngquant.c",
        "pngquant_opts.h",
        "rwpng.c",
        "rwpng.h",
    ],
    link_deps = [
        "@pngquant_crates//:imagequant-sys",
        "@pngquant_crates//:libpng-sys",
    ],
    build_script_env = {
        "CFLAGS": "-I$${pwd}/external/pngquant_crates__imagequant-sys-4.1.0 -I$${pwd}/external/pngquant_crates__libpng-sys-1.1.11/vendor -I$${pwd}/external/pngquant_crates__libz-sys-1.1.28/src/zlib",
    },
    crate_name = "build_script_build",
    deps = [
        "@pngquant_crates//:cc",
        "@pngquant_crates//:dunce",
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
        "@pngquant_crates//:getopts",
        "@pngquant_crates//:imagequant-sys",
        "@pngquant_crates//:libc",
        "@pngquant_crates//:libpng-sys",
        "@pngquant_crates//:wild",
    ],
)

genrule(
    name = "pnglibconf_h",
    srcs = ["@pngquant_crates__libpng-sys-1.1.11//:vendor/scripts/pnglibconf.h.prebuilt"],
    outs = ["pnglibconf.h"],
    cmd = "cp $(location @pngquant_crates__libpng-sys-1.1.11//:vendor/scripts/pnglibconf.h.prebuilt) $@",
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
        ":pnglibconf_h",
        "@pngquant_crates__imagequant-sys-4.1.0//:libimagequant.h",
    ],
    copts = [
        "-w",
        "-DPNGQUANT_NO_MAIN=1",
        "-DNDEBUG=1",
        "-Iexternal/pngquant_crates__imagequant-sys-4.1.0",
        "-Iexternal/pngquant_crates__libpng-sys-1.1.11/vendor",
    ],
    includes = [
        ".",
        "../pngquant_crates__imagequant-sys-4.1.0",
        "../pngquant_crates__libpng-sys-1.1.11/vendor",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@pngquant_crates//:imagequant-sys",
        "@pngquant_crates//:libpng-sys",
    ],
)
