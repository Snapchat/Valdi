"""Expose generated Rust files in the target configuration."""

load(":valdi_compiled.bzl", "ValdiModuleInfo")
load(":valdi_extract_output_rule_helper.bzl", "extract_valdi_output_rule")

def _extract_rust_file(ctx, source, extension, mnemonic):
    output = ctx.actions.declare_file(ctx.label.name + extension)
    if source:
        ctx.actions.run_shell(
            inputs = [source],
            outputs = [output],
            arguments = [source.path, output.path],
            command = 'cp -L "$1" "$2"',
            mnemonic = mnemonic,
        )
    else:
        ctx.actions.write(output = output, content = "")

    return [DefaultInfo(files = depset([output]))]

def _extract_rust_crate_impl(ctx):
    module = ctx.attr.compiled_module[ValdiModuleInfo]
    return _extract_rust_file(ctx, module.rust_srcs, ".rs", "PrepValdiRustCrate")

extract_rust_crate = extract_valdi_output_rule(
    implementation = _extract_rust_crate_impl,
    attrs = {},
)

def _extract_rust_link_params_impl(ctx):
    module = ctx.attr.compiled_module[ValdiModuleInfo]
    source = module.rust_link_apple_params if ctx.attr.format == "apple" else module.rust_link_elf_params
    return _extract_rust_file(ctx, source, ".params", "PrepValdiRustLinkParams")

extract_rust_link_params = extract_valdi_output_rule(
    implementation = _extract_rust_link_params_impl,
    attrs = {
        "format": attr.string(
            mandatory = True,
            values = ["apple", "elf"],
        ),
    },
)
