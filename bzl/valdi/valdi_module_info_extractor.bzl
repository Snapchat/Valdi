load(":valdi_compiled.bzl", "ValdiModuleInfo")
load(":valdi_extract_output_rule_helper.bzl", "extract_valdi_output_rule")

def empty_bundle_directory(name, src, visibility = ["//visibility:private"]):
    """Valid empty .bundle for apple_bundle_import when a valdi module has no bundle resources. Creates a filegroup."""
    native.filegroup(name = name, srcs = [src], visibility = visibility)

def _get_info_or_none(ctx):
    module_info = ctx.attr.compiled_module[ValdiModuleInfo]
    if not hasattr(module_info, ctx.attr.output_name):
        fail("Module {} does not have an output named {}".format(module_info.name, ctx.attr.output_name))

    value = getattr(module_info, ctx.attr.output_name)
    if value:
        if type(value) == "depset":
            return DefaultInfo(files = value)
        elif type(value) == "list":
            return DefaultInfo(files = depset(value))
        else:
            return DefaultInfo(files = depset([value]))

    return None

def _extract_valdi_module_output_impl(ctx):
    info = _get_info_or_none(ctx)
    if info:
        return info

    # When output is empty, use fallback if provided (e.g. for resource bundles so apple_bundle_import gets at least one file).
    fallback_files = ctx.attr.empty_bundle_fallback.files
    if fallback_files.to_list():
        return DefaultInfo(files = fallback_files)
    return DefaultInfo()

def _extract_valdi_module_native_output_impl(ctx):
    info = _get_info_or_none(ctx)
    if info:
        return info

    # If we don't have output sources, we pass an empty source so that we can
    # use the rule in cc_library
    empty_source_depset = ctx.attr._empty_source.files

    return DefaultInfo(
        files = empty_source_depset,
    )

def _get_deps_or_none(module):
    module_info = module[ValdiModuleInfo]
    if not hasattr(module_info, "deps"):
        return None

    return getattr(module_info, "deps")

def _extract_transitive_valdi_module_output_impl(ctx):
    deps = []
    for module in ctx.attr.modules:
        module_deps = _get_deps_or_none(module)

        if module_deps:
            deps.extend(module_deps.to_list())

        # Include current module
        deps.append(module)

    values = []
    for dep in deps:
        value = getattr(dep[ValdiModuleInfo], ctx.attr.output_name)
        if value:
            values.extend(value)

    if values:
        return DefaultInfo(files = depset(values))
    else:
        return DefaultInfo()

extract_valdi_module_output = extract_valdi_output_rule(
    implementation = _extract_valdi_module_output_impl,
    attrs = {
        "output_name": attr.string(
            mandatory = True,
        ),
        "empty_bundle_fallback": attr.label(
            default = Label("//bzl/valdi:empty_bundle_fallback_none"),
            allow_files = True,
        ),
    },
)

extract_valdi_module_native_output = extract_valdi_output_rule(
    implementation = _extract_valdi_module_native_output_impl,
    attrs = {
        "output_name": attr.string(
            mandatory = True,
        ),
        "_empty_source": attr.label(
            default = Label("//bzl/valdi:empty.c"),
            allow_single_file = True,
        ),
    },
)

extract_transitive_valdi_module_output = rule(
    implementation = _extract_transitive_valdi_module_output_impl,
    attrs = {
        "output_name": attr.string(mandatory = True),
        "modules": attr.label_list(
            mandatory = True,
            cfg = "exec",
            providers = [ValdiModuleInfo],
        ),
        "_empty_source": attr.label(
            default = Label("//bzl/valdi:empty.c"),
            allow_single_file = True,
        ),
    },
)
