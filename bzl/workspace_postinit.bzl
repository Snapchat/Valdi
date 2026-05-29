load("@llvm_toolchain//:toolchains.bzl", "llvm_register_toolchains")
load("@pngquant_crates//:defs.bzl", pngquant_crates = "crate_repositories")
load("@resvg_crates//:defs.bzl", resvg_crates = "crate_repositories")
load("@rules_jvm_external//:setup.bzl", "rules_jvm_external_setup")

def valdi_post_initialize_workspace():
    llvm_register_toolchains()
    resvg_crates()
    pngquant_crates()
    rules_jvm_external_setup()
