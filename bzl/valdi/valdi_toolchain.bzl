""" The file contains Bazel helper rules to register Valdi toolchains. """

ValdiCompilerInfo = provider(
    doc = "Information about how to invoke the valdi compiler.",
    fields = [
        "compiler",
        "compiler_toolbox",
        "pngquant",
        "companion",
        "minify_config",
        "sqldelight_compiler",
    ],
)

def _valdi_toolchain_impl(ctx):
    info = platform_common.ToolchainInfo(
        info = ValdiCompilerInfo(
            compiler = ctx.attr.compiler,
            compiler_toolbox = ctx.attr.compiler_toolbox,
            pngquant = ctx.attr.pngquant,
            companion = ctx.attr.compiler_companion,
            minify_config = ctx.attr.minify_config,
            sqldelight_compiler = ctx.attr.sqldelight_compiler,
        ),
    )
    return [info]

# Generic rule definition.
valdi_toolchain = rule(
    implementation = _valdi_toolchain_impl,
    toolchains = [],
    attrs = {
        "compiler": attr.label(
            executable = True,
            cfg = "exec",
            allow_single_file = True,
            doc = "The Valdi compiler to use. Must be a single-file executable. For local development, see //compiler/compiler:local_valdi_compiler.",
        ),
        "compiler_toolbox": attr.label(
            executable = True,
            cfg = "exec",
            allow_single_file = True,
            doc = "The toolbox to use with the compiler.",
        ),
        "compiler_companion": attr.label(
            executable = True,
            cfg = "exec",
            doc = "The companion to use with the compiler.",
        ),
        "minify_config": attr.label(
            default = "//modules:minify_config",
        ),
        "pngquant": attr.label(
            executable = True,
            cfg = "exec",
            allow_single_file = True,
            doc = "The pngquant executable to use.",
        ),
        "sqldelight_compiler": attr.label(
            cfg = "exec",
            doc = "The sqldelight compiler to use.",
        ),
    },
)
