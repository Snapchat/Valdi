""" Helper rule to extract C++ generated sources from valdi_compiled targets. """

load(":valdi_compiled.bzl", "ValdiModuleInfo")
load(":valdi_extract_output_rule_helper.bzl", "extract_valdi_output_rule")

# Extracts the generated C++ sources directory tree artifact from the compiled_module target's providers and
# filters by extension (.cpp or .hpp), exposing the filtered files via the DefaultInfo provider.
#
# This is needed because when single_file_codegen is disabled, the compiler generates multiple files
# in the same directory, and we need to separate .cpp files (for srcs) from .hpp files (for hdrs).
#
# C++ codegen always outputs to release configuration.
def _extract_cpp_srcs_impl(ctx):
    module = ctx.attr.compiled_module[ValdiModuleInfo]

    # C++ codegen always outputs to release configuration
    cpp_srcs = module.cpp_srcs

    output = ctx.actions.declare_directory(ctx.attr.name)

    # Build the strip prefix pattern for shell (add trailing slash if non-empty)
    strip_prefix = ctx.attr.strip_prefix
    if strip_prefix and not strip_prefix.endswith("/"):
        strip_prefix = strip_prefix + "/"

    # For .cpp files, generate a dummy file if no sources are found
    # This prevents cc_library from failing when there are no C++ sources
    #
    # Note: We use -L with find to follow symlinks, as tree artifacts may contain symlinks
    # to files in other configurations, and -L with cp to dereference when copying.
    if ctx.attr.extension == ".cpp":
        command = """
        mkdir -p {output}
        find -L {input}/ -type f -name "*{extension}" -print0 | while IFS= read -r -d '' file; do
            rel="${{file#"{input}/"}}"
            stripped="${{rel#{strip_prefix}}}"
            dest_dir="{output}/$(dirname "$stripped")"
            mkdir -p "$dest_dir"
            cp -L "$file" "$dest_dir/"
        done
        if [ -z "$(find {output} -type f -name "*{extension}" -print -quit)" ]; then
            echo "// Empty dummy C++ file generated because no sources were found" > {output}/empty_dummy.cpp
        fi
        """.format(
            input = cpp_srcs.path,
            extension = ctx.attr.extension,
            output = output.path,
            strip_prefix = strip_prefix,
        )
    else:
        command = """
        mkdir -p {output}
        find -L {input}/ -type f -name "*{extension}" -print0 | while IFS= read -r -d '' file; do
            rel="${{file#"{input}/"}}"
            stripped="${{rel#{strip_prefix}}}"
            dest_dir="{output}/$(dirname "$stripped")"
            mkdir -p "$dest_dir"
            cp -L "$file" "$dest_dir/"
        done
        """.format(
            input = cpp_srcs.path,
            extension = ctx.attr.extension,
            output = output.path,
            strip_prefix = strip_prefix,
        )

    ctx.actions.run_shell(
        inputs = [cpp_srcs],
        outputs = [output],
        command = command,
        mnemonic = "PrepValdiCppSrcs",
    )

    return [
        DefaultInfo(
            files = depset(
                [output],
            ),
        ),
    ]

extract_cpp_srcs = extract_valdi_output_rule(
    implementation = _extract_cpp_srcs_impl,
    attrs = {
        "extension": attr.string(
            mandatory = True,
            values = [".cpp", ".hpp"],
        ),
        "strip_prefix": attr.string(
            default = "",
            doc = "Prefix to strip from file paths when extracting (e.g., 'cpp/release/src')",
        ),
        "_empty_source": attr.label(
            default = Label("//bzl/valdi:empty.c"),
            allow_single_file = True,
        ),
    },
)
