load("@rules_rust//crate_universe:defs.bzl", "crate", "crates_repository", "render_config")
load("@rules_rust//crate_universe:repositories.bzl", "crate_universe_dependencies")
load("@rules_rust//rust:repositories.bzl", "rules_rust_dependencies", "rust_register_toolchains")

def valdi_initialize_rust_workspace():
    rules_rust_dependencies()
    rust_register_toolchains(
        edition = "2024",
        extra_target_triples = [
            "aarch64-apple-ios",
            "aarch64-apple-ios-sim",
            "aarch64-linux-android",
            "armv7-linux-androideabi",
            "x86_64-apple-ios",
            "x86_64-linux-android",
        ],
        versions = ["1.87.0"],
    )

    crate_universe_dependencies()
    crates_repository(
        name = "resvg_crates",
        cargo_lockfile = "@resvg//:Cargo.lock",
        lockfile = "@valdi//third-party/resvg:cargo-bazel-lock.json",
        packages = {
            "base64": crate.spec(version = "0.22.1"),
            "data-url": crate.spec(version = "0.3.2"),
            "flate2": crate.spec(default_features = False, features = ["rust_backend"], version = "1.1.5"),
            "imagesize": crate.spec(version = "0.14.0"),
            "kurbo": crate.spec(version = "0.13.0"),
            "log": crate.spec(version = "0.4.29"),
            "pico-args": crate.spec(features = ["eq-separator"], version = "0.5.0"),
            "rgb": crate.spec(version = "0.8.52"),
            "roxmltree": crate.spec(version = "0.21.1"),
            "simplecss": crate.spec(version = "0.2.2"),
            "siphasher": crate.spec(version = "1.0.1"),
            "strict-num": crate.spec(version = "0.1.1"),
            "svgtypes": crate.spec(version = "0.16.1"),
            "tiny-skia": crate.spec(version = "0.12.0"),
            "tiny-skia-path": crate.spec(version = "0.12.0"),
            "xmlwriter": crate.spec(version = "0.1.0"),
        },
        render_config = render_config(default_package_name = ""),
        supported_platform_triples = [
            "aarch64-apple-darwin",
            "aarch64-linux-android",
            "armv7-linux-androideabi",
            "x86_64-apple-ios",
            "x86_64-linux-android",
            "x86_64-apple-darwin",
            "x86_64-unknown-linux-gnu",
        ],
    )

    crates_repository(
        name = "pngquant_crates",
        cargo_lockfile = "@valdi//third-party/pngquant:Cargo.lock",
        lockfile = "@valdi//third-party/pngquant:cargo-bazel-lock.json",
        packages = {
            "cc": crate.spec(version = "1.0.72"),
            "dunce": crate.spec(version = "1.0.4"),
            "getopts": crate.spec(version = "0.2.21"),
            "imagequant-sys": crate.spec(version = "4.1.0"),
            "libc": crate.spec(version = "0.2.112"),
            "libpng-sys": crate.spec(version = "1.1.9"),
            "libz-sys": crate.spec(version = "1.1.28"),
            "wild": crate.spec(version = "2.2.0"),
        },
        render_config = render_config(default_package_name = ""),
        supported_platform_triples = [
            "aarch64-apple-darwin",
            "x86_64-apple-darwin",
            "x86_64-unknown-linux-gnu",
        ],
    )
