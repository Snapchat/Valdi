load("@bazel_skylib//lib:paths.bzl", "paths")
load("@build_bazel_rules_apple//apple/internal/utils:bundle_paths.bzl", "bundle_paths")
load("@build_bazel_rules_swift//swift:swift.bzl", "SwiftInfo")

HmapInfo = provider(
    "Stores mappings of import paths to header files.",
    fields = {
        "mappings": "Transitive header mappings represented as a depset of tuples, each tuple defines an import path and the header file it maps to.",
    },
)
HeaderTreeInfo = provider(
    "Carries a generated header tree without publishing it as a target's default output.",
    fields = {
        "tree": "Tree artifact containing generated headers.",
    },
)
ModuleImportMappingInfo = provider(
    "Stores mappings of import paths to tuples of header, label and module name.",
    fields = {
        "mapping": "Transitive mappings represented as a depset mapping import paths to a (header path, label, module name) tuple.",
    },
)

# This is the list of file extensions that are added to the generated hmap.
# If a file has an extension not in this list, it will not be added to the hmap.
MAPPED_FILE_TYPES = [".h", ".hh", ".hpp", ".inc"]

def _make_hmap(actions, args, headermap_builder, output, mapping, extra_inputs = []):
    """Creates an hmap file.

    Args:
        actions: The ctx.actions struct
        args: The ctx.actions.args struct for passing arguments to the tool
        headermap_builder: Executable pointing to the hmap tool
        output: The output file that will contain the built hmap
        mapping: A depset of tuples, each tuple representing mapping of import paths to a header path
        extra_inputs: Additional input files for the action
    """

    inputs = [] + extra_inputs
    for import_path, hdr in mapping.to_list():
        args.add(hdr)
        args.add(import_path)
        inputs.append(hdr)

    args.set_param_file_format(format = "multiline")
    args.use_param_file("@%s")

    actions.run(
        mnemonic = "HmapCreate",
        arguments = [args],
        executable = headermap_builder,
        outputs = [output],
        inputs = inputs,
    )
    return len(inputs) > 0

def _make_header_tree_hmap_inputs(
        args,
        header_tree_artifact_providers,
        namespace = None):
    """Updates args and creates inputs for tree artifacts containing headers.

    Args:
        args: A ctx.actions.args struct to which the header files will be added
        header_tree_artifact_providers: A list of providers with DefaultInfo containing tree artifacts with headers
        namespace: The prefix to be used for header imports (optional)
    """
    inputs = []
    mappings = {}
    for provider in header_tree_artifact_providers:
        if HeaderTreeInfo in provider:
            default_info_files = [provider[HeaderTreeInfo].tree]
        else:
            default_info_files = provider[DefaultInfo].files.to_list()

        if len(default_info_files) < 1:
            fail("header_tree_artifact_providers target must contain at least one tree artifact containing headers in DefaultInfo.files")

        for file in default_info_files:
            if file.is_directory:
                inputs.append(file)
                args.add_all(
                    [file],
                    expand_directories = True,
                    omit_if_empty = True,
                    allow_closure = True,
                    map_each = lambda file: _map_tree_artifact_headers(namespace, file),
                )
                import_path = _get_tree_import_path(namespace, file)
                if import_path:
                    mappings[import_path] = file

    return inputs, mappings

def _create_headermap_mapping(hdrs_lists, also_map_without_namespace, namespace = None):
    """Creates a dictionary mapping import paths to header files.
    By default, import path is the header's basename. If a namespace is

    specified, an additional entry for prefixed import (header name
    prefixed with the namespace) is added.

    Args:
        hdrs_lists: An array of enumerables containing headers to be added to the hmap
        namespace: The prefix to be used for header imports (optional)
    """
    mapping = {}
    for hdrs in hdrs_lists:
        for header in hdrs:
            file_extension = paths.split_extension(header.path)[1]
            if file_extension not in MAPPED_FILE_TYPES:
                continue

            header_name = header.basename
            if namespace:
                mapping[namespace + "/" + header_name] = header
                if also_map_without_namespace:
                    mapping[header_name] = header
            else:
                mapping[header_name] = header

    return mapping

def _create_cc_headermap_mapping(
        cc_hdrs,
        include_prefix = None,
        strip_include_prefix = None,
        strip_include_prefixes = [],
        includes = []):
    """Creates a dictionary to map import paths to header files for C/C++ headers.

    By default, import path is the header's relative path. If an include_prefix is
    specified, an additional entry for prefixed import is added.

    Args:
        cc_hdrs: An array of headers to be added to the hmap
        include_prefix: The prefix to be used for header imports (optional)
        strip_include_prefix: Path prefix to remove from header imports (optional)
        strip_include_prefixes: List of include directories to be stripped from header imports
        includes: List of include directories for cc_hdrs
    """
    all_strip_include_prefixes = [strip_include_prefix] if strip_include_prefix else []
    all_strip_include_prefixes.extend(strip_include_prefixes)

    mapping = {}
    for header in cc_hdrs:
        file_extension = paths.split_extension(header.path)[1]
        if file_extension not in MAPPED_FILE_TYPES:
            continue
        header_relpath = bundle_paths.owner_relative_path(header)

        if all_strip_include_prefixes:
            for strip_include_prefix in all_strip_include_prefixes:
                if header_relpath.startswith(strip_include_prefix):
                    import_path = paths.relativize(header_relpath, strip_include_prefix)
                    if include_prefix:
                        import_path = include_prefix + "/" + import_path
                    mapping[import_path] = header
        elif include_prefix:
            import_path = include_prefix + "/" + header_relpath
            mapping[import_path] = header
        else:
            mapping[header_relpath] = header

        for include in includes:
            if include.startswith("./"):
                include = include[2:]
            elif include.startswith("."):
                include = include[1:]

            if header_relpath.startswith(include):
                import_path = paths.relativize(header_relpath, include)
                mapping[import_path] = header

    return mapping

def _headermap_impl(ctx):
    """Implementation of the headermap() rule.

    Creates an action that calls the headermap builder tool to generate
    a binary .hmap file from the collected header mappings.

    Args:
        ctx: The rule context

    Returns:
        A list of providers including CcInfo and HmapInfo
    """

    hdrs_lists = [ctx.files.hdrs]
    namespace = ctx.attr.namespace
    also_map_without_namespace = ctx.attr.also_map_without_namespace
    output = ctx.outputs.headermap

    args = ctx.actions.args()
    args.add("--output", output)

    hmap_mapping = _create_headermap_mapping(
        hdrs_lists = hdrs_lists,
        also_map_without_namespace = also_map_without_namespace,
        namespace = namespace,
    )

    if ctx.files.cc_hdrs:
        cc_hmap_mapping = _create_cc_headermap_mapping(
            cc_hdrs = ctx.files.cc_hdrs,
            includes = ctx.attr.cc_includes,
            include_prefix = ctx.attr.cc_include_prefix,
            strip_include_prefix = ctx.attr.cc_strip_include_prefix,
            strip_include_prefixes = ctx.attr.cc_strip_include_prefixes,
        )
        hmap_mapping.update(cc_hmap_mapping)

    if ctx.attr.extra_mapping:
        hmap_mapping.update(
            {p: h.files.to_list()[0] for h, p in ctx.attr.extra_mapping.items()},
        )

    swift_header_providers_mapping = {}
    for maybe_swift_provider in (ctx.attr.swift_header_providers or []):
        if SwiftInfo not in maybe_swift_provider:
            continue

        for direct_module in maybe_swift_provider[SwiftInfo].transitive_modules.to_list():
            swift_module = direct_module.swift
            if not swift_module:
                continue
            direct_public_headers = direct_module.clang.compilation_context.direct_public_headers
            for generated_header in direct_public_headers:
                module_name = swift_module.swiftmodule.basename.split(".")[0]
                swift_header_providers_mapping[module_name + "/" + generated_header.basename] = generated_header

    transitive_mappings = []
    for dep in ctx.attr.deps:
        if HmapInfo in dep and hasattr(dep[HmapInfo], "mappings"):
            transitive_mappings.append(dep[HmapInfo].mappings)

    mapping_tuples = hmap_mapping.items()
    swift_header_providers_mapping_tuples = swift_header_providers_mapping.items()

    # When writing this target's hmap, include the mappings from
    # swift_header_providers, but don't include transitive mappings
    # because Swift headers belong to a different target.
    all_mapping_tuples = mapping_tuples + swift_header_providers_mapping_tuples
    if ctx.attr.merge_deps_mappings:
        hmap_mappings_depset = depset(
            all_mapping_tuples,
            transitive = transitive_mappings,
        )
    else:
        hmap_mappings_depset = depset(all_mapping_tuples)

    if ctx.attr.header_tree_artifact_providers:
        tree_header_inputs, header_tree_mapping = _make_header_tree_hmap_inputs(
            args = args,
            header_tree_artifact_providers = ctx.attr.header_tree_artifact_providers,
            namespace = namespace,
        )
    else:
        tree_header_inputs = []
        header_tree_mapping = {}

    did_map_headers = _make_hmap(
        actions = ctx.actions,
        args = args,
        headermap_builder = ctx.executable._headermap_builder,
        output = ctx.outputs.headermap,
        mapping = hmap_mappings_depset,
        extra_inputs = tree_header_inputs,
    )

    header_mapping_provider = HmapInfo(
        mappings = depset(
            mapping_tuples + header_tree_mapping.items(),
            transitive = transitive_mappings,
        ),
    )

    if not did_map_headers:
        # do not propagate the headermap and includes if hmap is empty
        cc_info_provider = CcInfo()
    elif ctx.attr.add_to_includes:
        cc_info_provider = CcInfo(
            compilation_context = cc_common.create_compilation_context(
                headers = depset([ctx.outputs.headermap]),
                includes = depset([ctx.outputs.headermap.path]),
            ),
        )
    else:
        cc_info_provider = CcInfo(
            compilation_context = cc_common.create_compilation_context(
                headers = depset([ctx.outputs.headermap]),
            ),
        )

    return [
        cc_info_provider,
        header_mapping_provider,
    ]

attr_aspects = ["deps", "implementation_deps", "private_deps"]
library_kinds = ("cc_library", "cc_proto_library", "objc_library", "swift_library", "swift_proto_library", "mixed_language_library")
binary_kinds = ("ios_application", "ios_extension", "ios_imessage_extension", "ios_framework", "ios_ui_test", "ios_unit_test", "app_clip", "cc_binary", "macos_command_line_application", "watch_application")

def _collect_header_mappings(attr, attribute_names):
    direct_header_mappings = []
    module_import_mappings = []
    for attr_name in attribute_names:
        if hasattr(attr, attr_name):
            for dep in getattr(attr, attr_name):
                files = dep.files.to_list()
                if len(files) > 0 and files[0].path.endswith(".hmap"):
                    if HmapInfo in dep and hasattr(dep[HmapInfo], "mappings"):  # generated by hmap rule
                        direct_header_mappings.append(dep[HmapInfo].mappings)
                    else:  # generated copies of hmap rule that don't provide HmapInfo
                        fail("Update the hmap rule version to generate HmapInfo provider for %s" % dep.label)

                else:  # Generated by the hmap aspect
                    if ModuleImportMappingInfo in dep and hasattr(dep[ModuleImportMappingInfo], "mapping"):
                        module_import_mappings.append(dep[ModuleImportMappingInfo].mapping)

    return direct_header_mappings, module_import_mappings

def _hmap_aspect_impl(target, ctx):
    """Implementation of the hmap_aspect.

    Collects header mappings from library and binary targets, creating
    a mapping from import paths to headers, labels, and module names.

    Args:
        target: The target being analyzed
        ctx: The aspect context

    Returns:
        A list containing ModuleImportMappingInfo provider
    """
    if ModuleImportMappingInfo in target:
        return []

    if not (ctx.rule.kind in library_kinds + binary_kinds):
        return [ModuleImportMappingInfo()]

    if HmapInfo in target:  # This is an hmap target
        return [ModuleImportMappingInfo()]

    direct_header_mappings, transitive_module_import_mappings = _collect_header_mappings(ctx.rule.attr, attr_aspects)
    if not direct_header_mappings and not transitive_module_import_mappings:
        return [ModuleImportMappingInfo()]

    module_import_mapping = {}
    module_name = getattr(ctx.rule.attr, "module_name", None)
    label = ctx.label
    for import_path, hdr in depset(transitive = direct_header_mappings).to_list():
        module_import_mapping[import_path] = (hdr, label, module_name)

    module_import_mapping_provider = ModuleImportMappingInfo(
        mapping = depset(
            module_import_mapping.items(),
            transitive = transitive_module_import_mappings,
        ),
    )

    return [
        module_import_mapping_provider,
    ]

hmap_aspect = aspect(
    implementation = _hmap_aspect_impl,
    attr_aspects = attr_aspects,
    provides = [ModuleImportMappingInfo],
    required_aspect_providers = [
        [apple_common.Objc],
        [CcInfo],
        [SwiftInfo],
    ],
    doc = """Aspect that provides a mapping of import paths to the headers,
label and module name of the library target that owns the headers.
    """,
)

headermap = rule(
    implementation = _headermap_impl,
    output_to_genfiles = True,
    provides = [HmapInfo, CcInfo],
    attrs = {
        "add_to_includes": attr.bool(
            default = True,
            doc = """\
Whether to add the generated hmap to the includes list in the returned CcInfo provider.
The include needs to be propagated only for hmap files that expose public headers;
headermaps for private headers don't need to be propagated and should set this to
`False` to avoid unnecessary include paths.
""",
        ),
        "cc_hdrs": attr.label_list(
            mandatory = False,
            allow_files = True,
            doc = "A list of headers included in the headermap for C-style imports. Only {} are accepted.".format(", ".join(MAPPED_FILE_TYPES)),
        ),
        "cc_include_prefix": attr.string(
            mandatory = False,
            doc = "An import/include path prefix for headers defined with the `cc_hdrs` attribute.",
        ),
        "cc_strip_include_prefix": attr.string(
            mandatory = False,
            doc = "A path prefix to remove from header imports defined with the `cc_hdrs` attribute. This works similarly to `cc_strip_include_prefixes` and can be used when `cc_strip_include_prefix` is defined as a select with string values.",
        ),
        "cc_strip_include_prefixes": attr.string_list(
            mandatory = False,
            doc = "List of include directories necessary for inclusion of the `cc_hdrs` attribute. Each prefix will be stripped from the header import path.",
        ),
        "cc_includes": attr.string_list(
            mandatory = False,
            doc = "List of include directories necessary for inclusion of the `cc_hdrs` attribute. Applies to cc_hdrs only.",
        ),
        "extra_mapping": attr.label_keyed_string_dict(
            allow_files = True,
            mandatory = False,
            doc = "Additional header to import path mapping to add to the generated headermap.",
        ),
        "hdrs": attr.label_list(
            mandatory = False,
            allow_files = True,
            doc = "A list of headers included in the headermap. Only {} are added.".format(", ".join(MAPPED_FILE_TYPES)),
        ),
        "namespace": attr.string(
            mandatory = False,
            doc = "An import prefix for headers defined with the `hdrs` attribute.",
        ),
        "also_map_without_namespace": attr.bool(
            default = True,
            doc = "Whether to also map `hdrs` without the namespace prefix when namespace is set. If set to True, headers will be mapped both with and without the namespace prefix.",
        ),
        "swift_header_providers": attr.label_list(
            mandatory = False,
            doc = "Targets whose Swift generated headers should be mapped in the generated hmap.",
        ),
        "header_tree_artifact_providers": attr.label_list(
            providers = [DefaultInfo],
            doc = "Targets whose tree artifact outputs containing headers should be added to the map. Only {} are added.".format(", ".join(MAPPED_FILE_TYPES)),
        ),
        "deps": attr.label_list(
            aspects = [hmap_aspect],
            mandatory = False,
            providers = [CcInfo],
            doc = "Dependencies that propagate HmapInfo.",
        ),
        "merge_deps_mappings": attr.bool(
            default = False,
            doc = "Whether to include the transitive mappings from dependencies. Can be set to True if dependencies' hmaps are not added to includes.",
        ),
        "_headermap_builder": attr.label(
            executable = True,
            cfg = "exec",
            default = Label(
                "@rules_hdrs//hmap:hmaptool",
            ),
        ),
    },
    outputs = {"headermap": "%{name}.hmap"},
    doc = """\
Creates a binary headermap file from the given headers, suitable for passing to
clang.

This can be used to allow headers to be imported at a consistent path,
regardless of the package structure being used.
    """,
)

def _get_tree_import_path(namespace, header):
    """Gets the import path for a header in a tree artifact.

    Args:
        namespace: The prefix to be used for header imports (optional)
        header: A header file to be mapped

    Returns:
        The import path string, or None if the file extension is not supported
    """
    file_extension = paths.split_extension(header.path)[1]
    if file_extension not in MAPPED_FILE_TYPES:
        return None

    header_name = paths.basename(header.path)
    if namespace:
        return namespace + "/" + header_name
    else:
        return header_name

def _map_tree_artifact_headers(namespace, header):
    """Maps headers from tree artifact to desired import path.

    Args:
        namespace: The prefix to be used for header imports (optional)
        header: A header file to be mapped

    Returns:
        A list containing [header.path, mapped_header_path] if the file
        extension is supported, otherwise an empty list
    """
    mapped_header_path = _get_tree_import_path(namespace, header)
    if mapped_header_path:
        return [header.path, mapped_header_path]
    else:
        return []
