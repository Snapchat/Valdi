""" The file contains Bazel helper rules to register Valdi toolchains. """

ValdiCompilerInfo = provider(
    doc = "Information about how to invoke the valdi compiler.",
    fields = [
        "compiler",
        "compiler_toolbox",
        "companion",
        "minify_config",
        "sqldelight_compiler",
        "clientsql_sqlite_validator",
    ],
)

def _valdi_toolchain_impl(ctx):
    info = platform_common.ToolchainInfo(
        info = ValdiCompilerInfo(
            compiler = ctx.attr.compiler,
            compiler_toolbox = ctx.attr.compiler_toolbox,
            companion = ctx.attr.compiler_companion,
            minify_config = ctx.attr.minify_config,
            sqldelight_compiler = ctx.attr.sqldelight_compiler,
            clientsql_sqlite_validator = ctx.attr.clientsql_sqlite_validator,
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
        "sqldelight_compiler": attr.label(
            executable = True,
            cfg = "exec",
            doc = "The ClientSQL generator executable to use.",
        ),
        "clientsql_sqlite_validator": attr.label(
            executable = True,
            cfg = "exec",
            doc = "The hermetic SQLite 3.16 validator used by the ClientSQL generator.",
        ),
    },
)
