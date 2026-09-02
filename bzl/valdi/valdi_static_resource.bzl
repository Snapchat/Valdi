load(":valdi_run_compiler.bzl", "run_valdi_compiler")
load(":valdi_toolchain_type.bzl", "VALDI_TOOLCHAIN_TYPE")

def _valdi_static_resource_impl(ctx):
    output_file = ctx.actions.declare_file("{}.cpp".format(ctx.label.name))

    args = ctx.actions.args()

    args.add("--gen-static-res")

    for f in ctx.files.srcs:
        args.add("--input", f)
    args.add("--out", output_file)

    run_valdi_compiler(
        ctx = ctx,
        args = args,
        outputs = [output_file],
        inputs = ctx.files.srcs,
        mnemonic = "ValdiGenStaticResource",
        progress_message = "Generating static Valdi resource",
        use_worker = False,
        include_client_sql_tools = False,
    )

    return [
        DefaultInfo(
            files = depset([output_file]),
        ),
    ]

valdi_static_resource = rule(
    implementation = _valdi_static_resource_impl,
    toolchains = [VALDI_TOOLCHAIN_TYPE],
    attrs = {
        "srcs": attr.label_list(
            cfg = "exec",
            doc = "List of files to package as static resources",
            mandatory = True,
        ),
    },
)
