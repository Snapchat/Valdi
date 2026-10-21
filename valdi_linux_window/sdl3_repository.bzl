"""Opt-in development binding to an installed SDL3 (no engine dependency).

This is a prototype system-library binding. A pinned, hermetic SDL3 dependency
and distribution policy must be agreed before promoting this host to supported.
The repository is fetched only when a target depending on it is requested.
"""

def _sdl3_repository_impl(ctx):
    pkg_config = ctx.which("pkg-config")
    if not pkg_config:
        fail("The experimental Linux window host requires pkg-config and SDL3 >= 3.2")
    version = ctx.execute([pkg_config, "--atleast-version=3.2.0", "sdl3"])
    if version.return_code:
        fail("Install SDL3 >= 3.2 development headers/libraries to build the experimental window host")
    includedir = ctx.execute([pkg_config, "--variable=includedir", "sdl3"])
    libraries = ctx.execute([pkg_config, "--libs", "sdl3"])
    if includedir.return_code or libraries.return_code:
        fail("Unable to query SDL3 with pkg-config")
    ctx.symlink(includedir.stdout.strip() + "/SDL3", "include/SDL3")
    ctx.file("BUILD.bazel", """
load("@rules_cc//cc:defs.bzl", "cc_library")
cc_library(
    name = "sdl3",
    hdrs = glob(["include/SDL3/**"]),
    includes = ["include"],
    linkopts = %s,
    visibility = ["//visibility:public"],
)
""" % repr([arg for arg in libraries.stdout.strip().split(" ") if arg]))

_sdl3_repository = repository_rule(
    implementation = _sdl3_repository_impl,
    local = True,
    configure = True,
    environ = ["PATH", "PKG_CONFIG_PATH", "PKG_CONFIG_LIBDIR", "PKG_CONFIG_SYSROOT_DIR"],
)

def _sdl3_extension_impl(module_ctx):
    _sdl3_repository(name = "valdi_system_sdl3")

system_sdl3 = module_extension(implementation = _sdl3_extension_impl)
