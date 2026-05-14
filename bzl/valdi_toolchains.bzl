# Module extension that configures C/C++ toolchains for Valdi consumers.
#
# Wraps Apple CC and LLVM toolchain setup so that consumers inherit
# register_toolchains() from Valdi's MODULE.bazel without needing to
# call any extensions themselves.

load(
    "@build_bazel_apple_support//crosstool:osx_cc_configure.bzl",
    "configure_osx_toolchain",
)
load(
    "@toolchains_llvm//toolchain:rules.bzl",
    "llvm_toolchain",
)

_DISABLE_ENV_VAR = "BAZEL_NO_APPLE_CPP_TOOLCHAIN"
_OLD_DISABLE_ENV_VAR = "BAZEL_USE_CPP_ONLY_TOOLCHAIN"

def _apple_cc_autoconf_toolchains_impl(repository_ctx):
    env = repository_ctx.os.environ
    if env.get(_DISABLE_ENV_VAR) == "1" or env.get(_OLD_DISABLE_ENV_VAR) == "1":
        repository_ctx.file("BUILD", "# Apple CC toolchain disabled.")
    else:
        repository_ctx.file(
            "BUILD",
            content = repository_ctx.read(
                Label("@build_bazel_apple_support//crosstool:BUILD.toolchains"),
            ),
        )

_apple_cc_autoconf_toolchains = repository_rule(
    environ = [_DISABLE_ENV_VAR, _OLD_DISABLE_ENV_VAR],
    implementation = _apple_cc_autoconf_toolchains_impl,
    configure = True,
)

def _apple_cc_autoconf_impl(repository_ctx):
    env = repository_ctx.os.environ
    if env.get(_DISABLE_ENV_VAR) == "1" or env.get(_OLD_DISABLE_ENV_VAR) == "1":
        repository_ctx.file("BUILD", "# Apple CC autoconfiguration disabled.")
    else:
        success, error = configure_osx_toolchain(repository_ctx)
        if not success:
            fail("Failed to configure Apple CC toolchain: %s" % error)

_apple_cc_autoconf = repository_rule(
    environ = [
        _DISABLE_ENV_VAR,
        _OLD_DISABLE_ENV_VAR,
        "APPLE_SUPPORT_LAYERING_CHECK_BETA",
        "BAZEL_ALLOW_NON_APPLICATIONS_XCODE",
        "DEVELOPER_DIR",
        "GCOV",
        "USE_CLANG_CL",
        "USER",
        "XCODE_VERSION",
    ],
    implementation = _apple_cc_autoconf_impl,
    configure = True,
)

def _valdi_toolchains_impl(_module_ctx):
    _apple_cc_autoconf_toolchains(name = "local_config_apple_cc_toolchains")
    _apple_cc_autoconf(name = "local_config_apple_cc")
    llvm_toolchain(name = "llvm_toolchain", llvm_version = "16.0.0")

valdi_toolchains = module_extension(implementation = _valdi_toolchains_impl)
