"""Collects compilation metadata from a transitive graph of Valdi modules."""

load(":common.bzl", "COMPILATION_METADATA_FILENAME")
load(":valdi_compiled.bzl", "ValdiModuleInfo")

def _valdi_compilation_metadata_impl(ctx):
    intermediates = depset(
        transitive = [
            dep[ValdiModuleInfo].intermediates
            for dep in ctx.attr.deps
        ],
    )
    metadata = [
        file
        for file in intermediates.to_list()
        if file.basename == COMPILATION_METADATA_FILENAME
    ]
    return [DefaultInfo(files = depset(metadata))]

valdi_compilation_metadata = rule(
    implementation = _valdi_compilation_metadata_impl,
    doc = "Returns compilation-metadata.json artifacts for the transitive closure of Valdi module dependencies.",
    attrs = {
        "deps": attr.label_list(
            mandatory = True,
            cfg = "exec",
            providers = [ValdiModuleInfo],
            doc = "Valdi modules whose transitive compilation metadata should be collected.",
        ),
    },
)
