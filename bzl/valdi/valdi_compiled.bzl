"""
Bazel rules for invoking the Valdi compiler and generating .valdimodule and source files.

Each valdi module should have a BUILD.bazel file that invokes the valdi_module() macro
instead of invoking this rule directly. The macro invokes this rule and takes care of properly
wrapping generated artifacts.
"""

load("@bazel_skylib//lib:paths.bzl", "paths")
load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load(
    "common.bzl",
    "ANDROID_RESOURCE_VARIANT_DIRECTORIES",
    "BUILD_DIR",
    "IOS_API_NAME_SUFFIX",
    "IOS_DEFAULT_MODULE_NAME_PREFIX",
    "IOS_OUTPUT_BASE",
    "NODE_MODULES_BASE",
    "TYPESCRIPT_DUMPED_SYMBOLS_DIR",
    "TYPESCRIPT_GENERATED_TS_DIR",
    "base_relative_dir",
)
load(":localizable_strings.bzl", "supported_langs_mapping")
load(":valdi_paths.bzl", "get_ids_yaml_dts_path", "get_legacy_vue_srcs_dts_paths", "get_resources_dts_paths", "get_sql_dts_paths", "get_strings_dts_path", "infer_base_output_dir", "output_declaration_compiled_file_path_for_source_file", "output_declaration_file_path_for_source_file", "replace_prefix", "resolve_module_dir_and_name", "resolve_relative_project_path")
load(":valdi_run_compiler.bzl", "generate_config", "resolve_compiler_executable", "run_valdi_compiler")
load(":valdi_toolchain_type.bzl", "VALDI_TOOLCHAIN_TYPE")

# These paths let us find/declare the expected output locations

_ANDROID_ASSETS_DIR = "assets"
_ANDROID_RESOURCES_DIR = "res"
_ANDROID_SOURCE_MAPS_DIR = "source-maps"
_ANDROID_GENERATED_SRC_DIR = "src"
_ANDROID_GENERATED_NATIVE_SRC_DIR = "native"

_IOS_ASSETS_DIR = "assets"
_IOS_SOURCE_MAPS_DIR = "source-maps"
_IOS_GENERATED_SRC_DIR = "src"
_IOS_GENERATED_NATIVE_SRC_DIR = "native"

_WEB_RESOURCES_DIR = "res"

# Kill the compiler after 15 minutes
COMPILER_TIMEOUT_SECONDS = 15 * 60
# We don't include the temporary directory here - that should only be used for hot reloading

_VALDI_BASE_MODULE_NAME = "__valdi_base__"

ValdiModuleInfo = provider(
    doc = "Outputs of valdi module compilation",
    fields = {
        "name": "The name of this valdi module. Usually it's snake_cased (e.g. your_module)",
        "deps": "The deps of the modules",
        "disable_annotation_processing": "When set to true, will not expect any Valdi annotation processing to occur for this module. In practice, that means that the .dumped-symbols.json output will not be created",
        "android_debug_resource_files": "Android debug resources",
        "android_release_resource_files": "Android release resources",
        "android_debug_valdimodule": "generated .valdimodule file for this Valdi module",
        "android_release_valdimodule": "generated .valdimodule file for this Valdi module",
        "android_debug_srcjar": "generated .srcjar file for this Valdi module",
        "android_release_srcjar": "generated .srcjar file for this Valdi module",
        "android_debug_nativesrc": "generated .c file containing all the generated C code",
        "android_release_nativesrc": "generated .c file containing all the generated C code",
        "android_debug_sourcemaps": "generated .map.json files for this Valdi module",
        "android_debug_sourcemap_archive": "A tree artifact containing archive of source and source maps",
        "android_release_sourcemap_archive": "A tree artifact containing archive of source and source maps",
        "prepared_upload_artifact": "Generated prepared artifact meant for uploading the assets",
        "exported_deps": "Dependencies that this module's API depends on and, as such, are exported as dependencies together with the module.",
        "ios_module_name": "The name of the iOS module based on module.yaml content",
        "ios_debug_resource_files": "iOS debug resources",
        "ios_debug_bundle_resources": "iOS debug resources bundle",
        "ios_release_resource_files": "iOS release resources",
        "ios_release_bundle_resources": "iOS release resources bundle",
        "ios_debug_valdimodule": "generated .valdimodule file for this Valdi module",
        "ios_release_valdimodule": "generated .valdimodule file for this Valdi module",
        "ios_debug_sourcemaps": "generated .map.json files for this Valdi module",
        "ios_debug_generated_hdrs": "Will contain the generated Objective-C header file, only if single_file_codegen is enabled",
        "ios_debug_generated_srcs": "A tree artifact containing all the generated objective-c/swift code",
        "ios_debug_api_generated_hdrs": "Will contain the generated Objective-C API header file, only if single_file_codegen is enabled",
        "ios_debug_api_generated_srcs": "A tree artifact containing all the generated objective-c/swift code (for the API-only module)",
        "ios_release_generated_hdrs": "Will contain the generated Objective-C header file, only if single_file_codegen is enabled",
        "ios_release_generated_srcs": "A tree artifact containing all the generated objective-c/swift code",
        "ios_release_api_generated_hdrs": "Will contain the generated Objective-C API header file, only if single_file_codegen is enabled",
        "ios_release_api_generated_srcs": "A tree artifact containing all the generated objective-c/swift code (for the API-only module)",
        "ios_debug_nativesrc": "generated .c file containing all the generated C code",
        "ios_release_nativesrc": "generated .c file containing all the generated C code",
        "cpp_srcs": "generated .cpp files containing all the generated C++ code",
        "cpp_hdrs": "generated .hpp files containing all the generated C++ header files",
        "ios_debug_sourcemap_archive": "A tree artifact containing archive of source and source maps",
        "ios_release_sourcemap_archive": "A tree artifact containing archive of source and source maps",
        "ios_sql_assets": "generated files for .sql files for this Valdi module",
        "ios_dependency_data": "generated dependency data for this Valdi module",
        "protodecl_srcs": ".protodecl",
        "intermediates": "The intermediates files that should be used to compile another module that depends on this module",
        "base_path": "The base path to the module",
        "module_definition": "The generated content of the module definition.",
        "web_debug_sources": "A tree artifact containing all the generated web sources",
        "web_release_sources": "A tree artifact containing all the generated web sources",
        "web_debug_resource_files": "Web resource files",
        "web_release_resource_files": "Web resource files",
        "web_debug_strings": "Web debug strings",
        "web_release_strings": "Web release strings",
        "web_deps": "Web ts/js dependencies",
    },
)

def _valdi_compiled_impl(ctx):
    """ Invokes the Valdi compiler for a module and generates all outputs.

    Args:
        ctx: The context of the current rule invocation

    Returns:
        A list of providers consisting of:
            DefaultInfo provider with all outputs
            ValdiModuleInfo provider
    """
    module_name = ctx.attr.module

    directory_name = paths.basename(ctx.label.package)
    if directory_name != ctx.label.name:
        fail("Valdi module name must match the name of the directory where the valdi_module() target is defined (expected directory name '{}', found '{}')".format(ctx.label.name, directory_name))

    (module_yaml, module_definition) = _resolve_module_yaml(ctx, module_name)

    # NPM scoped module names are postfixed with its scope during compilation
    # to avoid namespace conflicts.
    module_scope = _get_module_scope(ctx.label.package)
    if module_scope:
        module_name += module_scope

    outputs = _invoke_valdi_compiler(ctx, module_name, module_yaml)

    if not ctx.attr.single_file_codegen:
        outputs += _compress_generated_android_srcs(ctx, module_name, outputs)

    return [
        DefaultInfo(files = depset([o for o in outputs if o.path.endswith(".valdimodule")])),
        _create_valdi_module_info(ctx, module_name, module_yaml, module_definition, outputs),
    ]

# valdi_compiled rule:
#
# Compiles a valdi module, generating:
# - a .valdimodule file asset bundle which can be included in an Android/iOS app
# - any native classes/interfaces for Android or iOS
# - a resource with images for any modules with non-downloadable images
# - compiled TypeScript declaration (.d.ts) files that can be used to compile other Valdi modules
valdi_compiled = rule(
    implementation = _valdi_compiled_impl,
    toolchains = [VALDI_TOOLCHAIN_TYPE],
    attrs = {
        "module": attr.string(
            doc = "The module name",
            mandatory = True,
        ),
        "module_yaml": attr.label(
            doc = "The module.yaml file of this module",
            allow_single_file = ["module.yaml"],
        ),
        "ids_yaml": attr.label(
            doc = "Optional ids.yaml file for this module",
            allow_single_file = ["ids.yaml"],
        ),
        "strings_dir": attr.string(
            doc = "The directory where the strings are located",
        ),
        "strings_json_srcs": attr.label_list(
            doc = "String source files for this module",
            allow_files = True,
        ),
        "android_output_target": attr.string(
            doc = "The Android build type to build for",
            mandatory = True,
            values = ["debug", "release"],
        ),
        "android_export_strings": attr.bool(
            doc = "Export Android string resources",
            default = True,
        ),
        "ios_module_name": attr.string(
            mandatory = True,
        ),
        "ios_output_target": attr.string(
            doc = "The iOS build type to build for",
            mandatory = True,
            values = ["debug", "release"],
        ),
        "compilation_mode": attr.string(
            doc = "The compilation mode to use for the module",
            mandatory = True,
            values = ["js", "js_bytecode", "native"],
        ),
        "android_class_path": attr.string(
            doc = "The class path to use on Android for generated classes",
        ),
        "srcs": attr.label_list(
            doc = "List of sources for this module",
            mandatory = True,
            allow_files = [".js", ".ts", ".tsx", ".json"],
        ),
        "legacy_vue_srcs": attr.label_list(
            doc = "List of legacy .vue sources for this module",
            allow_files = [".vue"],
        ),
        "legacy_style_srcs": attr.label_list(
            doc = "List of legacy .css sources for this module",
            allow_files = [".css", ".scss"],
        ),
        "protodecl_srcs": attr.label_list(
            doc = "List of Protobuf files for this module",
            allow_files = [".protodecl"],
        ),
        "res": attr.label_list(
            doc = "List of resources for this module",
            allow_files = True,
        ),
        "deps": attr.label_list(
            doc = "List of other Valdi modules this module depends on",
            providers = [[ValdiModuleInfo]],
        ),
        "web_deps": attr.label_list(
            doc = "List of web dependencies for web",
        ),
        "disable_annotation_processing": attr.bool(
            doc = "When set to true, will not expect any Valdi annotation processing to occur for this module.",
            default = False,
        ),
        "sql_srcs": attr.label_list(
            doc = "List of SQL files for this module",
            allow_files = [".sql", ".sq", ".sqm", "sql_types.yaml", "sql_manifest.yaml"],
        ),
        "sql_db_names": attr.string_list(),
        "has_dependency_data": attr.bool(
            doc = "True if the module has associated dependency data",
            default = False,
        ),
        "downloadable_assets": attr.bool(
            doc = "Controls how module's assets are acquired: downloaded from Bolt or bundled with the module",
            default = True,
        ),
        "strip_assets": attr.bool(
            doc = "Whether assets should be stripped from the output",
            default = False,
        ),
        "inline_assets": attr.bool(
            doc = "Sets whether assets should be bundled inline within the .valdimodule",
            default = False,
        ),
        "log_level": attr.label(
            doc = "The log level for the Valdi compiler",
            default = "@valdi//bzl/valdi:compilation_log_level",
        ),
        "localization_mode": attr.label(
            doc = "Defines how the localized strings should be processed",
            default = "@valdi//bzl/valdi:localization_mode",
        ),
        "js_bytecode_format": attr.label(
            doc = "The JavaScript Bytecode format to use for the module",
            default = "@valdi//bzl/valdi:js_bytecode_format",
        ),
        "prepared_upload_artifact_name": attr.string(
            doc = "The name for a prepared artifact meant for uploading the assets",
        ),
        "valdi_copts": attr.string_list(
            doc = "Additional options to provide to the Valdi compiler",
        ),
        "single_file_codegen": attr.bool(
            doc = "Whether codegen should occur in a single file or one file per type",
            default = True,
        ),
        "disable_code_coverage": attr.bool(
            doc = "Disable code-coverage reporting for this module",
            default = False,
        ),
        "disable_dependency_verification": attr.bool(
            doc = "Disable dependency verifications for this module",
            default = False,
        ),
        "disable_hotreload": attr.bool(
            doc = "Disable hotreload from rebuilding this module",
            default = False,
        ),
        "exclude_patterns": attr.string_list(
            doc = "Exclude files that match the listed patterns",
        ),
        "exclude_globs": attr.string_list(
            doc = "Exclude files that match the listed globs",
        ),
        # NOTE: Valdi base should probably be moved to its own directory
        "_valdi_base": attr.label(
            doc = "The base module that all Valdi modules depend on",
            default = "@valdi//modules:valdi_base",
        ),
        "_template": attr.label(
            doc = "The template config.yaml file",
            allow_single_file = True,
            default = "valdi_config.yaml.tpl",
        ),
    },
)

def _invoke_valdi_compiler(ctx, module_name, module_yaml):
    """ Invoke valdi Compiler for the requested module.

        This function takes care of reproducing the required directory structure which Valdi expects
        (source files should live in subdirectories relative to where the config.yaml files lives, for example).

    Args:
        ctx: The context of the current rule invocation
        module_name: The name of the module to compile

    Returns:
        A list of files that are produced by the Valdi compiler
    """

    # Get command line flags
    # Set on command line as:
    # --define flag_name="flag_value"
    # Set in .bazelrc as:
    # common --define flag_name=flag_value
    module_upload_base_url = ctx.var.get("module_upload_base_url")
    enable_web = ctx.var.get("enable_web")
    disable_minify_web = ctx.var.get("disable_minify_web", False)

    #############
    # 1. Generate the project config file that is currently required by the Valdi compiler.
    #    The Valdi compiler uses the project config file location as the initial base directory
    #    (the base_dir read from the config is then resolved relative to the project config file's location)
    config_yaml_file = generate_config(ctx)

    #############
    # 2. Prepare the explicit input list file for the compiler.
    (explicit_input_list_file, all_inputs, module_directory) = _prepare_explicit_input_list_file(ctx, module_yaml)

    localization_mode = ctx.attr.localization_mode[BuildSettingInfo].value
    js_bytecode_format = ctx.attr.js_bytecode_format[BuildSettingInfo].value

    #############
    # 3. Gather all outputs produced by the Valdi compiler
    (base_output_dir, outputs) = _declare_compiler_outputs(ctx, module_name, module_directory, localization_mode, enable_web)

    prepared_upload_artifact_file = None
    if ctx.attr.prepared_upload_artifact_name:
        prepared_upload_artifact_file = ctx.actions.declare_file(ctx.attr.prepared_upload_artifact_name)
        outputs.append(prepared_upload_artifact_file)

    #############
    # 4. Prepare the Valdi compiler arguments
    disable_downloadable_assets = not ctx.attr.downloadable_assets

    valdi_copts = ctx.attr.valdi_copts

    if module_upload_base_url:
        # Make a mutable copy
        valdi_copts = valdi_copts[:]
        valdi_copts.append("--config-value")
        valdi_copts.append("module_upload_base_url={}".format(module_upload_base_url))

    args = _prepare_arguments(ctx.actions.args(), ctx.attr.log_level[BuildSettingInfo].value, localization_mode, js_bytecode_format, config_yaml_file, explicit_input_list_file, module_name, base_output_dir, disable_downloadable_assets, ctx.configuration.default_shell_env, prepared_upload_artifact_file, ctx.attr.inline_assets, valdi_copts, enable_web, disable_minify_web)

    #############
    # 5. Set up the action that executes the Valdi compiler

    run_valdi_compiler(
        ctx = ctx,
        args = args,
        outputs = outputs,
        inputs = all_inputs + [config_yaml_file, explicit_input_list_file],
        mnemonic = "ValdiCompile",
        progress_message = "Compiling Valdi module: " + module_name,
        use_worker = True,
    )

    return outputs

def _get_direct_dependencies(file, files, module_yaml):
    """ Return a list of files that are direct dependencies of the current module.

    Args:
        file: The file struct from the ctx.attr object
        files: The files struct from the ctx.attr object
    """
    output_files = [module_yaml]

    output_files += files.srcs
    output_files += files.legacy_vue_srcs
    output_files += files.legacy_style_srcs
    output_files += files.protodecl_srcs
    output_files += files.sql_srcs
    output_files += files.strings_json_srcs
    output_files += files.res

    if file.ids_yaml:
        output_files.append(file.ids_yaml)

    return output_files

def _get_transitive_intermediates(deps):
    return [dep[ValdiModuleInfo].intermediates for dep in deps]

def _resolve_valdi_base_entry(valdi_base_files, for_hotreloading):
    all_files = []
    tsconfig_file_entry = None
    for e in valdi_base_files:
        all_files.append(e)

        if e.path.endswith("/tsconfig.json"):
            if tsconfig_file_entry:
                fail("Found multiple tsconfig.json file in valdi_base target")
            tsconfig_file_entry = e

    if not tsconfig_file_entry:
        fail("Could not resolve tsconfig.json in valdi_base target")
    valdi_base_dir, _ = resolve_module_dir_and_name(tsconfig_file_entry.owner)

    resolved_files = []
    for e in all_files:
        dep_path = e.path
        relative_project_path = replace_prefix(dep_path, valdi_base_dir + "/", "")
        if for_hotreloading:
            dep_path = _resolve_hotreload_path(dep_path)[0]
        resolved_files.append({"file": dep_path, "relative_project_path": relative_project_path})

    module_definition = """
name: {}

ios:
  output_target: release

android:
  output_target: release

web:
  output_target: release

path_prefix: ''
""".format(_VALDI_BASE_MODULE_NAME)

    if for_hotreloading:
        valdi_base_dir = _resolve_hotreload_path(valdi_base_dir)[0]

    return {"module_name": _VALDI_BASE_MODULE_NAME, "module_path": valdi_base_dir, "files": resolved_files, "module_content": module_definition}

def _prepare_explicit_input_list_file(ctx, module_yaml):
    """ Write a JSON file with the list of all files that should be passed to the Valdi compiler. """

    direct_dependencies = _get_direct_dependencies(ctx.file, ctx.files, module_yaml)
    transitive_dependencies = _get_transitive_intermediates(ctx.attr.deps)

    dependencies = depset(direct = direct_dependencies, transitive = transitive_dependencies)
    dependencies_list = dependencies.to_list()

    json_entries = []
    current_files = []
    direct_module_directory = None
    current_module_directory = None
    current_module_name = None

    json_entries.append(_resolve_valdi_base_entry(ctx.files._valdi_base, False))

    for dep in dependencies_list:
        dep_path = dep.path
        file_name = paths.basename(dep_path)

        if file_name == "module.yaml":
            current_files = []

            current_module_directory, current_module_name = resolve_module_dir_and_name(dep.owner)

            if dep == module_yaml:
                direct_module_directory = current_module_directory

            scope_name = _get_module_scope(current_module_directory)
            if scope_name:
                current_module_name = current_module_name + scope_name

            json_entries.append({
                "module_name": current_module_name,
                "module_path": current_module_directory,
                "files": current_files,  # Subsequent files will append to that array reference
            })

        if not current_module_directory:
            fail("First entry in the list should be a module.yaml. Got {}".format(dep.path))
        relative_project_path = resolve_relative_project_path(dep, current_module_name, current_module_directory)

        current_files.append({"file": dep_path, "relative_project_path": relative_project_path})

    if not direct_module_directory:
        fail("Could not resolve module directory")

    explicit_input_list_file = ctx.actions.declare_file("explicit_input_list.json")

    content = json.encode_indent({"entries": json_entries}, indent = "  ")
    ctx.actions.write(output = explicit_input_list_file, content = content)

    return (explicit_input_list_file, ctx.files._valdi_base + dependencies_list, direct_module_directory)

def _declare_compiler_outputs(ctx, module_name, module_directory, localization_mode, enable_web):
    files_output_paths = _get_files_output_paths(ctx, module_name, module_directory, localization_mode, enable_web)
    directories_output_paths = _get_directories_output_paths(ctx)
    outputs = []
    outputs += _declare_files(ctx, files_output_paths)
    outputs += _declare_directories(ctx, directories_output_paths)

    return (infer_base_output_dir(files_output_paths + directories_output_paths, outputs), outputs)

def _get_files_output_paths(ctx, module_name, module_directory, localization_mode, enable_web):
    filtered_srcs = _get_compiled_srcs(ctx)
    outputs = _get_srcs_dts_paths(filtered_srcs, module_name, module_directory)

    if enable_web:
        outputs += _get_srcs_js_paths(filtered_srcs + ctx.files.protodecl_srcs, module_name, module_directory, bool(ctx.files.res), bool(ctx.file.ids_yaml), bool(ctx.attr.strings_dir))
        outputs += _get_srcs_vue_paths(ctx.files.legacy_vue_srcs, module_name, module_directory)

    outputs += get_legacy_vue_srcs_dts_paths(ctx.files.legacy_vue_srcs, module_name, module_directory)

    android_output_target, ios_output_target = ctx.attr.android_output_target, ctx.attr.ios_output_target

    if _will_generate_valdimodule(ctx):
        # Gather .valdimodule which are only generated if there are source files
        outputs = _append_debug_and_maybe_release(outputs, android_output_target, _get_android_valdi_module_paths(module_name))
        outputs = _append_debug_and_maybe_release(outputs, ios_output_target, _get_ios_valdi_module_paths(module_name))

        # Gather .map.json, which are only generarated if there are source files
        outputs = _append_debug_and_maybe_release(outputs, android_output_target, _get_android_valdi_module_map_paths(module_name))
        outputs = _append_debug_and_maybe_release(outputs, ios_output_target, _get_ios_valdi_module_map_paths(module_name))

    # Gather .c files
    outputs = _append_debug_and_maybe_release(outputs, android_output_target, _get_android_generated_native_src_paths(module_name))
    outputs = _append_debug_and_maybe_release(outputs, ios_output_target, _get_ios_generated_native_src_paths(ctx.attr.ios_module_name, module_name))

    # tablename.d.ts files
    outputs += get_sql_dts_paths(ctx.attr.sql_db_names, ctx.files.sql_srcs, module_name, module_directory)

    # ids.d.ts
    outputs += get_ids_yaml_dts_path(TYPESCRIPT_GENERATED_TS_DIR, module_name, ctx.file.ids_yaml)

    # ids.xml
    outputs = _append_debug_and_maybe_release(outputs, android_output_target, _get_android_ids_xml_paths(module_name, ctx.file.ids_yaml))

    # Strings.d.ts
    strings_json_srcs = ctx.files.strings_json_srcs
    outputs += get_strings_dts_path(TYPESCRIPT_GENERATED_TS_DIR, module_name, strings_json_srcs)

    if enable_web:
        # Just debug for now
        outputs = _append_debug_and_maybe_release(outputs, "debug", _get_web_string_resource_paths(module_name, strings_json_srcs, ctx.attr.strings_dir))

    if localization_mode == "external":
        # Android strings-xx.xml
        outputs = _append_debug_and_maybe_release(outputs, android_output_target, _get_android_string_resource_paths(module_name, strings_json_srcs))

        # iOS Localizable.strings
        outputs = _append_debug_and_maybe_release(outputs, ios_output_target, _get_ios_string_resource_paths(module_name, strings_json_srcs))

    # res.d.ts
    outputs += get_resources_dts_paths(module_name, ctx.files.res)

    if ctx.files.res and not ctx.attr.downloadable_assets and not ctx.attr.strip_assets and not ctx.attr.inline_assets:
        basenames = _extract_image_resources_basenames(ctx.files.res)

        outputs = _append_debug_and_maybe_release(outputs, android_output_target, _get_android_image_resources_paths(module_name, basenames))
        outputs = _append_debug_and_maybe_release(outputs, ios_output_target, _get_ios_image_resources_paths(module_name, basenames))
        if enable_web:
            # Just debug for now
            renamed_resources = _extract_renamed_resources(ctx.files.res)
            outputs += _get_web_resource_paths(module_name, renamed_resources)[0]
            outputs += _get_web_generated_resource_paths(module_name, outputs)

    outputs.append(_get_dumped_compilation_metadata(module_name))

    if ctx.attr.has_dependency_data:
        outputs.append(_get_dependency_data_path(module_name))

    if ctx.attr.single_file_codegen:
        outputs = _append_debug_and_maybe_release(outputs, android_output_target, _get_android_generated_src(module_name))
        outputs = _append_debug_and_maybe_release(outputs, ios_output_target, _get_ios_generated_src(ctx.attr.ios_module_name))
        outputs = _append_debug_and_maybe_release(outputs, ios_output_target, _get_ios_generated_api_src(ctx.attr.ios_module_name))

        # C++ always outputs to release configuration
        outputs += _get_cpp_generated_src(module_name)

    return outputs

def _get_directories_output_paths(ctx):
    outputs = []

    # Android source maps.
    outputs = _append_debug_and_maybe_release(outputs, ctx.attr.android_output_target, _get_android_source_map_dir())

    # iOS generated sources. Debug is always generated
    ios_module_name = ctx.attr.ios_module_name

    if not ctx.attr.single_file_codegen:
        # Android generated sources. Debug is always generated
        outputs = _append_debug_and_maybe_release(outputs, ctx.attr.android_output_target, _get_android_generated_src_dir())

        outputs = _append_debug_and_maybe_release(outputs, ctx.attr.ios_output_target, _get_ios_generated_src_dir(ios_module_name))
        outputs = _append_debug_and_maybe_release(outputs, ctx.attr.ios_output_target, _get_ios_generated_api_src_dir(ios_module_name))

        # C++ generated sources when single_file_codegen is disabled
        # C++ always outputs to release configuration
        outputs.append(_get_cpp_generated_src_dir())

    # Ios source maps.
    outputs = _append_debug_and_maybe_release(outputs, ctx.attr.ios_output_target, _get_ios_source_map_dir())

    # iOS SQL assets used by Buck
    if ctx.attr.sql_db_names:
        outputs.append(paths.join(IOS_OUTPUT_BASE, ios_module_name, "sql"))

    return outputs

def _will_generate_valdimodule(ctx):
    """ Returns true if the module is expected to generate a .valdimodule file

    Will be true if source code files are found, a source file is defined as any of .ts,
    .tsx, .js, .vue, .sql, .sq, .sqm files.

    Will be true if assets are found as well, as the module will end up having a res.js file
    generated.

    Args:
        ctx: The context of the current rule invocation
    """
    if len(ctx.files.res) > 0:
        return True
    valid_extensions = ["tsx", "ts", "js", "vue", "sql", "sq", "sqm"]
    potential_sources = ctx.files.srcs + ctx.files.legacy_vue_srcs + ctx.files.sql_srcs
    matching_files = [f for f in potential_sources if f.extension in valid_extensions and not f.basename.endswith(".d.ts")]

    return len(matching_files) != 0

def _get_compiled_srcs(ctx):
    return [src for src in ctx.files.srcs if src.basename != "tsconfig.json"]

def _get_srcs_dts_paths(srcs, module_name, module_directory):
    return [
        output_declaration_file_path_for_source_file(f, module_name, module_directory)
        for f in srcs
        if f.extension in ["tsx", "ts"] and not f.basename.endswith(".d.ts")
    ]

def _get_srcs_js_paths(srcs, module_name, module_directory, has_resources, has_ids, has_strings):
    out = []

    for f in srcs:
        if f.extension in ["tsx", "ts", "js"] and not f.basename.endswith(".d.ts") and not ".spec." in f.basename:
            out.append(output_declaration_compiled_file_path_for_source_file(f, module_name, module_directory))

    # Generated by the compiler but only if there are resources
    if has_resources:
        out.append(paths.join("web/debug/assets/", module_name, "res.js"))

    if has_ids:
        out.append(paths.join("web/debug/assets/", module_name, "ids.js"))

    return out

def _get_srcs_vue_paths(srcs, module_name, module_directory):
    out = []

    for f in srcs:
        if f.extension in ["vue"] and not f.basename.endswith(".d.ts"):
            out.append(output_declaration_compiled_file_path_for_source_file(f, module_name, module_directory, replacement_suffix = ".vue.js"))

    return out

def _get_web_resource_paths(module_name, resources_basenames):
    debug_res_dir, release_res_dir = _get_web_res_dir("debug"), _get_web_res_dir("release")
    debug_images, release_images = [], []

    for basename in resources_basenames:
        debug_images.append(paths.join(debug_res_dir, module_name, "res", basename))
        release_images.append(paths.join(release_res_dir, module_name, "res", basename))

    return [debug_images, release_images]

def _get_web_generated_resource_paths(module_name, srcs):
    out = []

    for f in srcs:
        if "header." in f:
            out.append(f)

    return out

def _append_debug_and_maybe_release(lst, output_target, items_lst):
    """ Append the debug and possible release item to the list.

    Args:
        lst: The list to append to
        output_target: The output target to append to. Can be "debug" or "release"
        items_lst: Array[2]: The array of size 2 where lst[0] is the debug item and lst[1] is the release item

    Returns:
        The new list with appended item(s)
    """
    if output_target not in ["debug", "release"]:
        fail("output_target should be either 'debug' or 'release'")

    if len(items_lst) != 2:
        fail("items_lst should have exactly 2 items")

    def concat(lst, item):
        if type(item) == "list":
            return lst + item
        else:
            return lst + [item]

    debug_item, release_item = items_lst

    lst = concat(lst, debug_item)
    if output_target == "release":
        lst = concat(lst, release_item)
    return lst

def _get_valdimodule_file_name(module_name):
    return module_name + ".valdimodule"

def _get_valdimodule_map_file_name(module_name):
    return module_name + ".map.json"

def _get_path_to_android_asset(output_target, asset_name):
    return base_relative_dir("android", output_target, paths.join(_ANDROID_ASSETS_DIR, asset_name))

def _get_android_valdi_module_paths(module_name):
    return [
        _get_path_to_android_asset("debug", _get_valdimodule_file_name(module_name)),
        _get_path_to_android_asset("release", _get_valdimodule_file_name(module_name)),
    ]

def _get_android_valdi_module_map_paths(module_name):
    return [
        _get_path_to_android_asset("debug", _get_valdimodule_map_file_name(module_name)),
        _get_path_to_android_asset("release", _get_valdimodule_map_file_name(module_name)),
    ]

def _get_android_generated_src_dir():
    return [
        base_relative_dir("android", "debug", _ANDROID_GENERATED_SRC_DIR),
        base_relative_dir("android", "release", _ANDROID_GENERATED_SRC_DIR),
    ]

def _get_android_generated_src(module_name):
    file_path = paths.join(_ANDROID_GENERATED_SRC_DIR, "{}.kt".format(module_name))

    return [
        base_relative_dir("android", "debug", file_path),
        base_relative_dir("android", "release", file_path),
    ]

def _get_android_source_map_dir():
    return [
        base_relative_dir("android", "debug", _ANDROID_SOURCE_MAPS_DIR),
        base_relative_dir("android", "release", _ANDROID_SOURCE_MAPS_DIR),
    ]

def _get_native_module_filename(module_name):
    return "{}_native.c".format(module_name)

def _get_android_generated_native_src_paths(module_name):
    filename = _get_native_module_filename(module_name)

    return [
        base_relative_dir("android", "debug", paths.join(_ANDROID_GENERATED_NATIVE_SRC_DIR, filename)),
        base_relative_dir("android", "release", paths.join(_ANDROID_GENERATED_NATIVE_SRC_DIR, filename)),
    ]

def _get_web_generated_js_paths(module_name):
    return [
        base_relative_dir("web", "debug", "assets", module_name, "src"),
        base_relative_dir("web", "release", "assets", module_name, "src"),
    ]

def _get_ios_generated_native_src_paths(ios_module_name, module_name):
    filename = _get_native_module_filename(module_name)

    return [
        base_relative_dir("ios", "debug", paths.join(_IOS_GENERATED_NATIVE_SRC_DIR, ios_module_name, filename)),
        base_relative_dir("ios", "release", paths.join(_IOS_GENERATED_NATIVE_SRC_DIR, ios_module_name, filename)),
    ]

def _get_android_res_dir(output_target):
    return base_relative_dir("android", output_target, _ANDROID_RESOURCES_DIR)

def _get_web_res_dir(output_target):
    return base_relative_dir("web", output_target, _WEB_RESOURCES_DIR)

def _get_ios_generated_src_dir(ios_module_name):
    return [
        base_relative_dir("ios", "debug", paths.join(_IOS_GENERATED_SRC_DIR, ios_module_name)),
        base_relative_dir("ios", "release", paths.join(_IOS_GENERATED_SRC_DIR, ios_module_name)),
    ]

def _get_ios_generated_api_src_dir(ios_module_name):
    return [
        base_relative_dir("ios", "debug", paths.join(_IOS_GENERATED_SRC_DIR, ios_module_name + IOS_API_NAME_SUFFIX)),
        base_relative_dir("ios", "release", paths.join(_IOS_GENERATED_SRC_DIR, ios_module_name + IOS_API_NAME_SUFFIX)),
    ]

def _get_ios_generated_src_helper(output_target, ios_module_name, suffix, ext):
    return base_relative_dir("ios", output_target, paths.join(_IOS_GENERATED_SRC_DIR, ios_module_name + suffix, "{}.{}".format(ios_module_name + suffix, ext)))

def _get_ios_generated_src(ios_module_name):
    debug_srcs = [
        _get_ios_generated_src_helper("debug", ios_module_name, "", "h"),
        _get_ios_generated_src_helper("debug", ios_module_name, "", "m"),
    ]

    release_srcs = [
        _get_ios_generated_src_helper("release", ios_module_name, "", "h"),
        _get_ios_generated_src_helper("release", ios_module_name, "", "m"),
    ]

    return [debug_srcs, release_srcs]

def _get_ios_generated_api_src(ios_module_name):
    debug_srcs = [
        _get_ios_generated_src_helper("debug", ios_module_name, IOS_API_NAME_SUFFIX, "h"),
        _get_ios_generated_src_helper("debug", ios_module_name, IOS_API_NAME_SUFFIX, "m"),
    ]

    release_srcs = [
        _get_ios_generated_src_helper("release", ios_module_name, IOS_API_NAME_SUFFIX, "h"),
        _get_ios_generated_src_helper("release", ios_module_name, IOS_API_NAME_SUFFIX, "m"),
    ]

    return [debug_srcs, release_srcs]

def _get_cpp_generated_src_dir():
    """Get C++ generated source directory for multi-file codegen mode.

    C++ codegen always outputs to release configuration.
    """
    return base_relative_dir("cpp", "release", "src")

def _get_cpp_generated_src(module_name):
    """Get C++ generated source file paths for single_file_codegen mode.

    C++ codegen always outputs to release configuration.
    """
    return [
        base_relative_dir("cpp", "release", "src/valdi_modules/{}/{}.hpp".format(module_name, module_name)),
        base_relative_dir("cpp", "release", "src/valdi_modules/{}/{}.cpp".format(module_name, module_name)),
    ]

def _get_ios_source_map_dir():
    return [
        base_relative_dir("ios", "debug", _IOS_SOURCE_MAPS_DIR),
        base_relative_dir("ios", "release", _IOS_SOURCE_MAPS_DIR),
    ]

def _get_path_to_ios_asset(output_target, asset_name):
    return base_relative_dir("ios", output_target, paths.join(_IOS_ASSETS_DIR, asset_name))

def _get_ios_valdi_module_paths(module_name):
    return [
        _get_path_to_ios_asset("debug", _get_valdimodule_file_name(module_name)),
        _get_path_to_ios_asset("release", _get_valdimodule_file_name(module_name)),
    ]

def _get_ios_valdi_module_map_paths(module_name):
    return [
        _get_path_to_ios_asset("debug", _get_valdimodule_map_file_name(module_name)),
        _get_path_to_ios_asset("release", _get_valdimodule_map_file_name(module_name)),
    ]

def _get_ios_strings_dir(output_target):
    return base_relative_dir("ios", output_target, "strings")

def _get_web_strings_dir(output_target):
    return base_relative_dir("web", output_target, "strings")

def _get_ios_bundle_dir(module_name, output_target):
    return base_relative_dir("ios", output_target, paths.join(_IOS_ASSETS_DIR, module_name + ".bundle"))

def _get_ios_debug_sourcemap_path(ios_module_name):
    return base_relative_dir("ios", "debug", paths.join(_IOS_ASSETS_DIR, _get_valdimodule_map_file_name(ios_module_name)))

def _get_android_debug_sourcemap_path(android_module_name):
    return base_relative_dir("android", "debug", paths.join(_ANDROID_ASSETS_DIR, _get_valdimodule_map_file_name(android_module_name)))

def _get_android_ids_xml_paths(module_name, ids_yaml):
    if not ids_yaml:
        return ([], [])

    ids_xml_name = "{}_ids.xml".format(module_name)

    return [
        paths.join(_get_android_res_dir("debug"), "values", ids_xml_name),
        paths.join(_get_android_res_dir("release"), "values", ids_xml_name),
    ]

def _get_android_string_resource_paths(module_name, strings_srcs):
    debug_dir, release_dir = _get_android_res_dir("debug"), _get_android_res_dir("release")

    # This has to match exportStringsFiles.ts#androidStringsXMLName in the companion
    strings_xml_name = "valdi-strings-{}.xml".format(module_name)

    debug_strings_xmls, release_strings_xml = [], []
    strings_jsons_names = [paths.basename(s.path) for s in strings_srcs]
    for strings_json in strings_jsons_names:
        (_, android_values_dir) = supported_langs_mapping[strings_json]

        debug_strings_xmls.append(paths.join(debug_dir, android_values_dir, strings_xml_name))
        release_strings_xml.append(paths.join(release_dir, android_values_dir, strings_xml_name))

    return [debug_strings_xmls, release_strings_xml]

def _get_ios_string_resource_paths(module_name, strings_srcs):
    debug_strings_dir, release_strings_dir = _get_ios_strings_dir("debug"), _get_ios_strings_dir("release")

    # This has to match exportStringsFiles.ts#iosLocalizableStringName in the companion
    string_resource_name = "valdi_modules_{}.strings".format(module_name)

    debug_strings, release_strings = [], []
    strings_jsons_names = [paths.basename(s.path) for s in strings_srcs]
    for strings_json in strings_jsons_names:
        (ios_values_dir, _) = supported_langs_mapping[strings_json]

        debug_strings.append(paths.join(debug_strings_dir, ios_values_dir, string_resource_name))
        release_strings.append(paths.join(release_strings_dir, ios_values_dir, string_resource_name))

    return [debug_strings, release_strings]

def _get_web_string_resource_paths(module_name, strings_srcs, strings_dir):
    debug_strings_dir, release_strings_dir = _get_web_strings_dir("debug"), _get_web_strings_dir("release")

    # This has to match exportStringsFiles.ts#iosLocalizableStringName in the companion
    string_resource_name = "valdi_modules_{}.strings".format(module_name)

    debug_strings, release_strings = [], []
    strings_jsons_names = [paths.basename(s.path) for s in strings_srcs]
    for strings_json in strings_jsons_names:
        (ios_values_dir, _) = supported_langs_mapping[strings_json]

        debug_strings.append(paths.join("web", "debug", "assets", module_name, strings_dir, strings_json))
        release_strings.append(paths.join("web", "release", "assets", module_name, strings_dir, strings_json))

    # Grab the generated files if they exist
    if strings_dir:
        debug_strings.append(paths.join("web", "debug", "assets", module_name, "src", "Strings.js"))
        release_strings.append(paths.join("web", "release", "assets", module_name, "src", "Strings.js"))
    return [debug_strings, release_strings]

def _is_image_ext(ext):
    return ext in [".png", ".webp", ".svg", ".jpg", ".jpeg"]

def _clean_image_file_name(root):
    basename = paths.basename(root).replace("-", "_")
    if "@2x" in basename:
        basename = basename.replace("@2x", "")
    if "@3x" in basename:
        basename = basename.replace("@3x", "")

    return basename

def _extract_image_resources_basenames(resources):
    result = []
    for resource in resources:
        root, ext = paths.split_extension(resource.path)
        if not _is_image_ext(ext):
            continue

        basename = _clean_image_file_name(root)

        result.append(basename)

    return result

def _extract_renamed_resources(resources):
    result = []
    for resource in resources:
        root, ext = paths.split_extension(resource.path)
        if not _is_image_ext(ext):
            continue

        basename = _clean_image_file_name(root)

        web_ext = ext.replace("svg", "png")
        web_name = "{}{}".format(basename, web_ext)

        # This will break if there's a res/res/ folder
        subfolder = resource.path.split("res/", 1)[1] if "/res/" in resource.path else ""

        result.append(paths.join(paths.dirname(subfolder), web_name))
    return result

def _get_android_image_resources_paths(module_name, resources_basenames):
    debug_res_dir, release_res_dir = _get_android_res_dir("debug"), _get_android_res_dir("release")
    debug_images, release_images = [], []
    for basename in resources_basenames:
        # All files end up being converted into webp
        resource_name = "{}_{}.webp".format(module_name, basename)

        for variant in ANDROID_RESOURCE_VARIANT_DIRECTORIES:
            debug_images.append(paths.join(debug_res_dir, variant, resource_name))
            release_images.append(paths.join(release_res_dir, variant, resource_name))

    if len(resources_basenames):
        keep_filename = "valdi_{}_keep.xml".format(module_name)
        debug_images.append(paths.join(debug_res_dir, "raw", keep_filename))
        release_images.append(paths.join(release_res_dir, "raw", keep_filename))

    return [debug_images, release_images]

def _get_ios_image_resources_paths(module_name, resources_basenames):
    debug_bundle_dir, release_bundle_dir = _get_ios_bundle_dir(module_name, "debug"), _get_ios_bundle_dir(module_name, "release")

    debug_images, release_images = [], []
    for basename in resources_basenames:
        # All files end up being converted into png
        resources = [
            "{}@2x.png".format(basename),
            "{}@3x.png".format(basename),
        ]

        for resource_name in resources:
            debug_images.append(paths.join(debug_bundle_dir, resource_name))
            release_images.append(paths.join(release_bundle_dir, resource_name))

    return [debug_images, release_images]

def _get_dumped_compilation_metadata(module_name):
    return paths.join(TYPESCRIPT_DUMPED_SYMBOLS_DIR, module_name, "compilation-metadata.json")

def _get_dependency_data_path(module_name):
    return base_relative_dir("ios", "metadata", "dependencyData.json")

def _declare_files(ctx, paths):
    return [ctx.actions.declare_file(path) for path in paths]

def _declare_directories(ctx, paths):
    return [ctx.actions.declare_directory(path) for path in paths]

def _prepare_arguments(args, log_level, localization_mode, js_bytecode_format, config_yaml_file, explicit_input_list_file, module_name, base_output_dir, disable_downloadable_assets, shell_env, prepared_upload_artifact_file, inline_assets, additional_copts, enable_web, disable_minify_web):
    """ Prepare arguments for the Valdi compiler invocation. """

    args.use_param_file("@%s", use_always = True)
    args.set_param_file_format("multiline")

    args.add("--config", config_yaml_file)
    args.add("--explicit-input-list-file", explicit_input_list_file)

    args.add("--build-dir", paths.join("$PWD", base_output_dir, BUILD_DIR))

    args.add("--module", module_name)
    args.add("--module", _VALDI_BASE_MODULE_NAME)
    args.add("--output-dumped-symbols-dir", paths.join(explicit_input_list_file.dirname, TYPESCRIPT_DUMPED_SYMBOLS_DIR))

    args.add("--only-compile-ts-for-module", module_name)
    args.add("--only-process-resources-for-module", module_name)
    args.add("--only-generate-native-code-for-module", module_name)
    args.add("--only-focus-processing-for-module", module_name)
    args.add("--ts-emit-declaration-files")
    args.add("--ts-keep-comments")

    if localization_mode == "inline":
        args.add("--inline-translations")

    # args.add("--ts-skip-verifying-imports")
    args.add("--disable-disk-cache")
    args.add("--skip-remove-orphan-files")

    args.add("--log-level", log_level)
    args.add("--compile")
    args.add("--android")
    args.add("--ios")

    if enable_web:
        args.add("--web")
        if disable_minify_web:
            args.add("--disable-minify-web")

    # Allow to pass the timeout via --action_env parameter
    args.add("--timeout", shell_env.get("VALDI_COMPILER_TIMEOUT_SECONDS", COMPILER_TIMEOUT_SECONDS))

    # Disable concurrency, to free CPU cores to do the actual build.
    # Valdi compiler is fast enough to keep up with Bazel parallelization.
    args.add("--disable-concurrency")

    if prepared_upload_artifact_file:
        args.add("--prepared-upload-artifact-output", prepared_upload_artifact_file.path)

    # Configure compilation
    # TODO: should we create a Bazel rule for this configuration?
    args.add("--config-value", "build_dir=" + explicit_input_list_file.dirname)
    args.add("--config-value", "project_name=" + module_name)
    args.add("--config-value", "build_file_generation_enabled=false")

    # Android
    args.add("--config-value", "android.codegen_enabled=true")
    args.add("--config-value", "android.output.base={}".format(paths.join("$PWD", base_output_dir, "android")))
    args.add("--config-value", "android.output.debug_path=debug")
    args.add("--config-value", "android.output.release_path=release")
    args.add("--config-value", "android.output.metadata_path=.")
    args.add("--config-value", "android.output.build_file_enabled=false")
    if js_bytecode_format != "none":
        args.add("--config-value", "android.js_bytecode_format={}".format(js_bytecode_format))

    # iOS
    args.add("--config-value", "ios.codegen_enabled=true")
    args.add("--config-value", "ios.default_module_name_prefix=" + IOS_DEFAULT_MODULE_NAME_PREFIX)
    args.add("--config-value", "ios.output.base={}".format(paths.join("$PWD", base_output_dir, "ios")))
    args.add("--config-value", "ios.output.debug_path=debug")
    args.add("--config-value", "ios.output.release_path=release")
    args.add("--config-value", "ios.output.metadata_path=metadata")
    args.add("--config-value", "ios.output.build_file_enabled=false")

    # C++
    args.add("--config-value", "cpp.codegen_enabled=true")
    args.add("--config-value", "cpp.output.base={}".format(paths.join("$PWD", base_output_dir, "cpp")))
    args.add("--config-value", "cpp.output.debug_path=debug")
    args.add("--config-value", "cpp.output.release_path=release")
    args.add("--config-value", "cpp.output.metadata_path=.")

    # Web
    if enable_web:
        args.add("--config-value", "web.output.base={}".format(paths.join("$PWD", base_output_dir, "web")))
        args.add("--config-value", "web.output.debug_path=debug")
        args.add("--config-value", "web.output.release_path=release")
        args.add("--config-value", "web.output.metadata_path=metadata")

    if disable_downloadable_assets:
        args.add("--disable-downloadable-modules")

    if inline_assets:
        args.add("--inline-assets")

        # Limit to only the highest variant, mostly to limit valdimodule size, we don't need
        # more at the moment.
        args.add("--image-variants-filter", "android=4.0;ios=3.0")

    for additional_copt in additional_copts:
        args.add(additional_copt)

    return args

def _compress_generated_android_srcs(ctx, module_name, outputs):
    """ Compress the generated Android sources into .srcjar files.

    Args:
        ctx: The context of the current rule invocation
        module_name: The name of the module to compile
        outputs(List[File]): The list of outputs produce by the Valdi compiler
    """
    result = []

    debug_path, release_path = _get_android_generated_src_dir()
    directories = [f for f in outputs if f.is_directory]

    # Debug directory is always present
    debug_dir = [d for d in directories if d.path.endswith(debug_path)][0]

    # TODO: perhaps we should let compiler generate srcjars for us?
    debug_srcjar = ctx.actions.declare_file("{}.debug.srcjar".format(module_name))
    _zip_dir(ctx, debug_dir, debug_srcjar)
    result.append(debug_srcjar)

    if ctx.attr.android_output_target == "release":
        release_dir = [d for d in directories if d.path.endswith(release_path)][0]
        release_srcjar = ctx.actions.declare_file("{}.release.srcjar".format(module_name))
        _zip_dir(ctx, release_dir, release_srcjar)
        result.append(release_srcjar)

    return result

def _zip_dir(ctx, in_dir, out_file):
    """ Create a zip archive of a directory and store the result in the provided file.

    Note:
        @bazel_tools//tools/zip:zipper can't be used here because it doesn't support recursive zipping
    """
    ctx.actions.run_shell(
        outputs = [out_file],
        inputs = [in_dir],
        # Set all file and directory timestamps to a fixed value before zipping for reproducibility.
        # 199609240000 = 1996-09-24 00:00 (arbitrary, but fixed)
        command = """
find {in_dir} -exec touch -t 199609240000 {{}} +
zip -qr {out_file} {in_dir}
""".format(in_dir = in_dir.path, out_file = out_file.path),
        progress_message = "Zipping {in_dir} into {out_file}".format(in_dir = in_dir.path, out_file = out_file.path),
        mnemonic = "ValdiCompileZipDir",
    )

def _generate_module_definition(ctx, name):
    attr = ctx.attr

    dependencies_str = "\n".join(["  - {}".format(dep[ValdiModuleInfo].name) for dep in attr.deps])

    module_def = """
name: {name}

ios:
  output_target: {ios_output_target}
  module_name: {ios_module_name}

android:
  output_target: {android_output_target}
  {android_class_path}
  export_strings: {android_export_strings}

web:
  output_target: release

single_file_codegen: {single_file_codegen}

allowed_debug_dependencies:
{dependencies_str}

disable_code_coverage: {disable_code_coverage}
disable_hotreload: {disable_hotreload}
disable_annotation_processing: {disable_annotation_processing}
disable_dependency_verification: {disable_dependency_verification}

{exclude_patterns}
{exclude_globs}

compilation_mode: {compilation_mode}
  """.format(
        name = name,
        ios_output_target = attr.ios_output_target,
        ios_module_name = attr.ios_module_name,
        android_output_target = attr.android_output_target,
        android_export_strings = "true" if attr.android_export_strings else "false",
        compilation_mode = attr.compilation_mode,
        single_file_codegen = "true" if attr.single_file_codegen else "false",
        disable_code_coverage = "true" if attr.disable_code_coverage else "false",
        disable_hotreload = "true" if attr.disable_hotreload else "false",
        disable_annotation_processing = "true" if attr.disable_annotation_processing else "false",
        disable_dependency_verification = "true" if attr.disable_dependency_verification else "false",
        dependencies_str = dependencies_str,
        android_class_path = "class_path: {}".format(attr.android_class_path) if attr.android_class_path else "",
        exclude_patterns = _yaml_named_list("exclude_patterns", attr.exclude_patterns),
        exclude_globs = _yaml_named_list("exclude_globs", attr.exclude_globs),
    )

    if attr.strings_dir:
        module_def += "\nstrings_dir: {}".format(attr.strings_dir)

    return module_def

def _yaml_named_list(name, items):
    if not items:
        return ""

    list = "\n".join(["  - {item}".format(item = item) for item in items])

    return "{name}:\n{list}".format(name = name, list = list)

def _resolve_module_yaml(ctx, module_name):
    module_definition = _generate_module_definition(ctx, module_name)

    if ctx.file.module_yaml:
        return (ctx.file.module_yaml, module_definition)
    else:
        module_yaml = ctx.actions.declare_file("module.yaml")
        ctx.actions.write(output = module_yaml, content = module_definition)
        return (module_yaml, module_definition)

def _create_valdi_module_info(ctx, module_name, module_yaml, module_definition, outputs):
    in_declarations = _extract_dts_files(_get_compiled_srcs(ctx))
    out_declarations = _extract_dts_files(outputs)
    dumped_compilation_metadata = _extract_dumped_compilation_metadata(outputs)
    ios_module_name = ctx.attr.ios_module_name
    has_valdimodule = _will_generate_valdimodule(ctx)
    is_android_release, is_ios_release = ctx.attr.android_output_target == "release", ctx.attr.ios_output_target == "release"

    # The order matter here. It should always starts with module_yaml
    direct_intermediates = [module_yaml] + in_declarations + out_declarations + ctx.files.legacy_style_srcs + dumped_compilation_metadata

    base_path = paths.join(ctx.label.workspace_root, ctx.label.package)
    single_file_codegen = ctx.attr.single_file_codegen

    return ValdiModuleInfo(
        # Depsets to enable other modules compilation
        name = module_name,
        intermediates = depset(direct = direct_intermediates, transitive = _get_transitive_intermediates(ctx.attr.deps)),
        module_definition = module_definition,
        base_path = base_path,
        deps = depset(direct = ctx.attr.deps, transitive = [d[ValdiModuleInfo].deps for d in ctx.attr.deps]),

        # Prepared upload artifact
        prepared_upload_artifact = _extract_prepared_upload_artifact(ctx.attr.prepared_upload_artifact_name, outputs) if ctx.attr.prepared_upload_artifact_name else None,

        # Android outputs
        android_debug_resource_files = depset(_extract_android_resources("debug", outputs)),
        android_release_resource_files = depset(_extract_android_resources("release", outputs)),
        android_debug_valdimodule = _extract_valdi_module_android("debug", module_name, outputs) if has_valdimodule else None,
        android_release_valdimodule = _extract_valdi_module_android("release", module_name, outputs) if has_valdimodule and is_android_release else None,
        android_debug_srcjar = _extract_android_srcjar("debug", module_name, single_file_codegen, outputs),
        android_release_srcjar = _extract_android_srcjar("release", module_name, single_file_codegen, outputs) if is_android_release else None,
        android_debug_nativesrc = _extract_android_native_srcs("debug", module_name, outputs),
        android_release_nativesrc = _extract_android_native_srcs("release", module_name, outputs) if is_android_release else None,
        android_debug_sourcemaps = _extract_android_debug_sourcemaps(module_name, outputs) if has_valdimodule else None,
        android_debug_sourcemap_archive = _extract_android_sourcemap_archive("debug", outputs) if has_valdimodule else None,
        android_release_sourcemap_archive = _extract_android_sourcemap_archive("release", outputs) if has_valdimodule else None,

        # iOS outputs
        ios_module_name = ios_module_name,
        ios_debug_valdimodule = _extract_valdi_module_ios("debug", module_name, outputs) if has_valdimodule else None,
        ios_release_valdimodule = _extract_valdi_module_ios("release", module_name, outputs) if has_valdimodule and is_ios_release else None,
        ios_debug_resource_files = depset(_extract_ios_unstructured_resources("debug", outputs)),
        ios_release_resource_files = depset(_extract_ios_unstructured_resources("release", outputs)),
        ios_debug_bundle_resources = depset(_extract_ios_resource_bundles(module_name, "debug", outputs)),
        ios_release_bundle_resources = depset(_extract_ios_resource_bundles(module_name, "release", outputs)),
        ios_debug_generated_hdrs = _extract_ios_generated_hdrs("debug", outputs, ios_module_name) if single_file_codegen else None,
        ios_debug_generated_srcs = _extract_ios_generated_srcs("debug", outputs, ios_module_name, single_file_codegen),
        ios_release_generated_hdrs = _extract_ios_generated_hdrs("release", outputs, ios_module_name) if single_file_codegen else None,
        ios_release_generated_srcs = _extract_ios_generated_srcs("release", outputs, ios_module_name, single_file_codegen),
        ios_debug_api_generated_hdrs = _extract_ios_api_generated_hdrs("debug", outputs, ios_module_name) if single_file_codegen else None,
        ios_debug_api_generated_srcs = _extract_ios_api_generated_srcs("debug", outputs, ios_module_name, single_file_codegen),
        ios_release_api_generated_hdrs = _extract_ios_api_generated_hdrs("release", outputs, ios_module_name) if single_file_codegen else None,
        ios_release_api_generated_srcs = _extract_ios_api_generated_srcs("release", outputs, ios_module_name, single_file_codegen),
        ios_debug_nativesrc = _extract_ios_native_srcs("debug", ios_module_name, module_name, outputs),
        ios_release_nativesrc = _extract_ios_native_srcs("release", ios_module_name, module_name, outputs) if is_ios_release else None,
        ios_debug_sourcemaps = _extract_ios_debug_sourcemaps(module_name, outputs) if has_valdimodule else None,
        ios_debug_sourcemap_archive = _extract_ios_sourcemap_archive("debug", outputs) if has_valdimodule else None,
        ios_release_sourcemap_archive = _extract_ios_sourcemap_archive("release", outputs) if has_valdimodule and is_ios_release else None,
        ios_sql_assets = _extract_ios_sql_assets(ios_module_name, outputs),
        ios_dependency_data = _extract_ios_dependency_data(outputs),

        # C++ outputs (always in release configuration)
        cpp_srcs = _extract_cpp_generated_srcs(outputs, module_name, single_file_codegen),
        cpp_hdrs = _extract_cpp_generated_hdrs(outputs, module_name, single_file_codegen),

        # web outputs
        protodecl_srcs = ctx.files.protodecl_srcs,
        web_debug_sources = _extract_js_files(module_name, "debug", outputs),
        web_release_sources = _extract_js_files(module_name, "release", outputs),
        web_debug_resource_files = _extract_web_resources("debug", outputs),
        web_release_resource_files = _extract_web_resources("release", outputs),
        web_debug_strings = _extract_web_strings("debug", outputs),
        web_release_strings = _extract_web_strings("release", outputs),
        web_deps = _extract_npm_package_files(ctx.attr.web_deps),
    )

def _extract_npm_package_files(pkgs):
    # Normalize pkgs to a Python list of targets
    if type(pkgs) == "depset":
        pkgs = pkgs.to_list()

    out = []
    for p in pkgs:
        # DefaultInfo.files is a depset[File]; flatten to list
        out.extend(p[DefaultInfo].files.to_list())
    return out

def _is_test_file(file_obj):
    """Check if a file is a test file that should be excluded from npm packages.

    Test files are identified by:
    - Filename containing '.spec.' or '.test.'
    - Path containing '/test/' directory
    """
    basename = file_obj.basename
    path = file_obj.path

    if ".spec." in basename or ".test." in basename:
        return True

    if "/test/" in path:
        return True

    return False

def _extract_js_files(module_name, output_target, outputs):
    """Extract JavaScript files from compiler outputs, excluding test files."""
    return [
        f
        for f in outputs
        if f.basename.endswith(".js") and not _is_test_file(f)
    ]

def _extract_web_strings(output_target, outputs):
    return [
        f
        for f in outputs
        if "strings/" in f.path and f.basename.endswith(".json") or f.basename.endswith("Strings.js")
    ]

def _extract_dts_files(srcs):
    """ Extract declaration(d.ts) files from the list of source files. """
    return [f for f in srcs if f.basename.endswith(".d.ts")]

def _extract_dumped_compilation_metadata(files):
    return [f for f in files if f.basename.endswith("compilation-metadata.json")]

def _extract_valdi_module_android(output_target, module_name, outputs):
    debug_path, release_path = _get_android_valdi_module_paths(module_name)
    if output_target == "debug":
        return _extract_valdi_module(debug_path, outputs)
    else:
        return _extract_valdi_module(release_path, outputs)

def _extract_valdi_module_ios(output_target, module_name, outputs):
    debug_path, release_path = _get_ios_valdi_module_paths(module_name)
    if output_target == "debug":
        return _extract_valdi_module(debug_path, outputs)
    else:
        return _extract_valdi_module(release_path, outputs)

def _extract_valdi_module(file_path, outputs):
    found = [f for f in outputs if f.path.endswith(file_path)]

    if not found:
        fail("Expected to find {} in outputs".format(file_path))

    return found[0]

def _extract_android_srcjar(output_target, module_name, single_file_codegen, outputs):
    if single_file_codegen:
        file_path = "{}.kt".format(module_name)
    else:
        file_path = "{}.{}.srcjar".format(module_name, output_target)
    found = [f for f in outputs if f.path.endswith(file_path)]

    if not found:
        fail("Expected to find {} in outputs".format(file_path))

    return found[0]

def _extract_android_resources(output_target, outputs):
    res_dir_path = _get_android_res_dir(output_target)

    return [
        f
        for f in outputs
        if res_dir_path in f.path and not f.is_directory
    ]

def _extract_web_resources(output_target, outputs):
    res_dir_path = _get_web_res_dir(output_target)

    out = [
        f
        for f in outputs
        if res_dir_path in f.path and not f.is_directory
    ]

    return out

def _extract_prepared_upload_artifact(name, outputs):
    found = [f for f in outputs if f.path.endswith(name)]

    if not found:
        fail("Expected to find {} in outputs".format(name))

    return found[0]

def _extract_android_native_srcs(output_target, module_name, outputs):
    debug_path, release_path = _get_android_generated_native_src_paths(module_name)
    if output_target == "debug":
        found = [f for f in outputs if f.path.endswith(debug_path)]
    else:
        found = [f for f in outputs if f.path.endswith(release_path)]

    if not found:
        fail("Expected to find generated native c file in outputs")

    return found[0]

def _extract_ios_native_srcs(output_target, ios_module_name, module_name, outputs):
    debug_path, release_path = _get_ios_generated_native_src_paths(ios_module_name, module_name)
    if output_target == "debug":
        found = [f for f in outputs if f.path.endswith(debug_path)]
    else:
        found = [f for f in outputs if f.path.endswith(release_path)]

    if not found:
        fail("Expected to find generated native c file in outputs")

    return found[0]

def _extract_ios_unstructured_resources(output_target, outputs):
    strings_dir = _get_ios_strings_dir(output_target)
    return [
        f
        for f in outputs
        if strings_dir in f.path and not f.is_directory
    ]

def _extract_ios_resource_bundles(module_name, output_target, outputs):
    bundle_dir = _get_ios_bundle_dir(module_name, output_target)
    bundled_resources = [
        f
        for f in outputs
        if bundle_dir in f.path and not f.is_directory
    ]
    return bundled_resources

def _filter_ios_src(outputs, output_target, debug_srcs, release_srcs, ext):
    srcs = debug_srcs if output_target == "debug" else release_srcs

    src_to_check = [f for f in srcs if f.endswith(ext)]
    if len(src_to_check) != 1:
        fail("Expecting to find exactly 1 entry with extension {}. Found {}".format(ext, srcs))

    found = [output for output in outputs if output.path.endswith(src_to_check[0])]
    return found[0] if found else None

def _extract_ios_generated_hdrs(output_target, outputs, ios_module_name):
    debug_src, release_src = _get_ios_generated_src(ios_module_name)

    return _filter_ios_src(outputs, output_target, debug_src, release_src, ".h")

def _extract_ios_api_generated_hdrs(output_target, outputs, ios_module_name):
    debug_src, release_src = _get_ios_generated_api_src(ios_module_name)

    return _filter_ios_src(outputs, output_target, debug_src, release_src, ".h")

def _extract_ios_generated_srcs(output_target, outputs, ios_module_name, single_file_codegen):
    if single_file_codegen:
        debug_src, release_src = _get_ios_generated_src(ios_module_name)

        return _filter_ios_src(outputs, output_target, debug_src, release_src, ".m")
    else:
        debug_src_dir, release_src_dir = _get_ios_generated_src_dir(ios_module_name)

        if output_target == "debug":
            found = [f for f in outputs if f.is_directory and f.path.endswith(debug_src_dir)]
        else:
            found = [f for f in outputs if f.is_directory and f.path.endswith(release_src_dir)]

        return found[0] if found else None

def _extract_ios_api_generated_srcs(output_target, outputs, ios_module_name, single_file_codegen):
    if single_file_codegen:
        debug_api_src, release_api_src = _get_ios_generated_api_src(ios_module_name)

        return _filter_ios_src(outputs, output_target, debug_api_src, release_api_src, ".m")
    else:
        debug_api_src_dir, release_api_src_dir = _get_ios_generated_api_src_dir(ios_module_name)

        if output_target == "debug":
            found = [f for f in outputs if f.is_directory and f.path.endswith(debug_api_src_dir)]
        else:
            found = [f for f in outputs if f.is_directory and f.path.endswith(release_api_src_dir)]
        return found[0] if found else None

def _extract_cpp_generated_srcs(outputs, module_name, single_file_codegen):
    """Extract C++ generated source files (.cpp) from outputs.

    C++ codegen always outputs to release configuration.
    """
    if single_file_codegen:
        srcs = _get_cpp_generated_src(module_name)

        # Filter for .cpp files
        src_to_check = [f for f in srcs if f.endswith(".cpp")]

        if len(src_to_check) != 1:
            fail("Expecting to find exactly 1 .cpp file. Found {}".format(srcs))

        found = [output for output in outputs if output.path.endswith(src_to_check[0])]
        return found[0] if found else None
    else:
        src_dir = _get_cpp_generated_src_dir()
        found = [f for f in outputs if f.is_directory and f.path.endswith(src_dir)]
        return found[0] if found else None

def _extract_cpp_generated_hdrs(outputs, module_name, single_file_codegen):
    """Extract C++ generated header files (.hpp) from outputs.

    C++ codegen always outputs to release configuration.
    """
    if single_file_codegen:
        srcs = _get_cpp_generated_src(module_name)

        # Filter for .hpp files
        src_to_check = [f for f in srcs if f.endswith(".hpp")]

        if len(src_to_check) != 1:
            fail("Expecting to find exactly 1 .hpp file. Found {}".format(srcs))

        found = [output for output in outputs if output.path.endswith(src_to_check[0])]
        return found[0] if found else None
    else:
        # For multi-file codegen, headers are in the same directory as sources
        src_dir = _get_cpp_generated_src_dir()
        found = [f for f in outputs if f.is_directory and f.path.endswith(src_dir)]
        return found[0] if found else None

def _extract_android_debug_sourcemaps(module_name, outputs):
    """ Extract android debug sourcemap(map.json) files from the list of source files. """
    android_debug_sourcemap_path = _get_android_debug_sourcemap_path(module_name)
    found = [f for f in outputs if f.path.endswith(android_debug_sourcemap_path)]

    if not found:
        fail("Expected to find {}.map.json in debug outputs".format(module_name))

    return found[0]

def _extract_android_sourcemap_archive(output_target, outputs):
    debug_sourcemap_archive_dir, release_sourcemap_archive_dir = _get_android_source_map_dir()
    if output_target == "debug":
        found = [f for f in outputs if f.is_directory and f.path.endswith(debug_sourcemap_archive_dir)]
    else:
        found = [f for f in outputs if f.is_directory and f.path.endswith(release_sourcemap_archive_dir)]
    return found[0] if found else None

def _extract_ios_sourcemap_archive(output_target, outputs):
    debug_sourcemap_archive_dir, release_sourcemap_archive_dir = _get_ios_source_map_dir()
    if output_target == "debug":
        found = [f for f in outputs if f.is_directory and f.path.endswith(debug_sourcemap_archive_dir)]
    else:
        found = [f for f in outputs if f.is_directory and f.path.endswith(release_sourcemap_archive_dir)]
    return found[0] if found else None

def _extract_ios_debug_sourcemaps(module_name, outputs):
    """ Extract ios debug sourcemap(map.json) files from the list of source files. """
    ios_debug_sourcemap_path = _get_ios_debug_sourcemap_path(module_name)
    found = [f for f in outputs if f.path.endswith(ios_debug_sourcemap_path)]

    if not found:
        fail("Expected to find {}.map.json in debug outputs".format(module_name))

    return found[0]

def _extract_ios_sql_assets(ios_module_name, outputs):
    """ Extract iOS SQL assets used by Bazel. """
    found = [f for f in outputs if f.is_directory and f.path.endswith(paths.join(ios_module_name, "sql"))]
    if found:
        return found[0]
    return None

def _extract_ios_dependency_data(outputs):
    """ Extract iOS dependency data used by Bazel. """
    for f in outputs:
        if f.path.endswith("dependencyData.json"):
            return f
    return None

#############
##
## Private helpers for transforming path strings
##
#############

def _get_module_scope(path_str):
    # Returns @<scope>
    scope_start_idx = path_str.find("@")
    if scope_start_idx < 0:
        # Scope not found
        return ""
    elif scope_start_idx > 0 and path_str[scope_start_idx - 1] != "/":
        # Invalid format, scope should start with '@'
        return ""

    scope_end_idx = path_str.find("/", scope_start_idx)
    if scope_end_idx < 0:
        # Scope should never be the last component of path
        return ""

    return path_str[scope_start_idx:scope_end_idx]

def _resolve_hotreload_path(path):
    if path.startswith("external/"):
        # For external references, we reference the base_path to the
        # exec_root, as Bazel will copy the files there.
        return (paths.join("$BAZEL_EXECROOT", path), True)
    else:
        return (path, False)

def _append_hotreload_explicit_input_list_entry(target, json_entries):
    (base_path, is_external_ref) = _resolve_hotreload_path(target[ValdiModuleInfo].base_path)

    json_entries.append({
        "module_name": target[ValdiModuleInfo].name,
        "module_path": base_path,
        "module_content": target[ValdiModuleInfo].module_definition,
        "monitor": is_external_ref == False,
        "auto_discover": is_external_ref,
        "files": [],
    })

def _prepare_hotreload_explicit_input_list_file(ctx, all_targets):
    json_entries = []

    json_entries.append(_resolve_valdi_base_entry(ctx.files._valdi_base, True))
    for target in all_targets:
        _append_hotreload_explicit_input_list_entry(target, json_entries)

    content = json.encode_indent({"entries": json_entries}, indent = "  ")

    file_list = ctx.actions.declare_file("input_file_list.json")
    ctx.actions.write(output = file_list, content = content)

    return file_list

def _prepare_hotreload_arguments(module_names, config_yaml_file, explicit_input_list_file, toolchain, base_project_path):
    """ Prepare arguments for the Valdi compiler invocation. """

    companion_bin_wrapper = toolchain.companion.files.to_list()[0]
    compiler_toolbox = toolchain.compiler_toolbox.files.to_list()[0]
    pngquant = toolchain.pngquant.files.to_list()[0]
    minify_config = toolchain.minify_config.files.to_list()[0]
    client_sql = toolchain.sqldelight_compiler.files.to_list()

    args = []

    for module_name in module_names:
        args.append("--module")
        args.append(module_name)
    args.append("--config")
    args.append(config_yaml_file.path)
    args.append("--explicit-input-list-file")
    args.append(explicit_input_list_file.path)
    args.append("--direct-companion-path")
    args.append(companion_bin_wrapper.path)
    args.append("--direct-compiler-toolbox-path")
    args.append(compiler_toolbox.path)
    args.append("--direct-pngquant-path")
    args.append(pngquant.path)
    args.append("--direct-minify-config-path")
    args.append(minify_config.path)

    if client_sql:
        args.append("--direct-client-sql-path")
        args.append(client_sql[0].path)

    args.append("--module")
    args.append(_VALDI_BASE_MODULE_NAME)
    args.append("--log-level")
    args.append("info")
    args.append("--monitor")
    args.append("--usb")
    args.append("--inline-translations")

    # Probably need to cd to the base_project_path as part of the script
    args.append("--config-value")
    args.append("base_dir=$PWD/{}".format(base_project_path))

    args.append("--config-value")
    args.append("node_modules_dir=$PWD/{}".format(NODE_MODULES_BASE))

    args.append("--config-value")
    args.append("android.codegen_enabled=false")
    args.append("--config-value")
    args.append("ios.codegen_enabled=false")

    return " ".join(args)

def __common_path(left, right):
    left_components = left.split("/")
    right_components = right.split("/")
    common_components = []

    index = 0
    for l in left_components:
        if index >= len(right_components) or right_components[index] != l:
            break
        index += 1
        common_components.append(l)

    return "/".join(common_components)

def __infer_base_project_path(targets):
    common_path = None
    for t in targets:
        base_path = t[ValdiModuleInfo].base_path
        if not common_path:
            common_path = base_path
        else:
            common_path = __common_path(common_path, base_path)
    return common_path

def _invoke_valdi_hotreloader(ctx):
    toolchain = ctx.toolchains[VALDI_TOOLCHAIN_TYPE].info

    config_yaml_file = generate_config(ctx)
    (executable, tools, input_manifests) = resolve_compiler_executable(ctx, toolchain, include_tools = True)

    all_targets = depset(direct = ctx.attr.targets, transitive = [target[ValdiModuleInfo].deps for target in ctx.attr.targets]).to_list()

    base_project_path = __infer_base_project_path(all_targets)
    explicit_input_list_file = _prepare_hotreload_explicit_input_list_file(ctx, all_targets)

    run_hotreloader = ctx.actions.declare_file("run_hotreloader.sh")

    args = _prepare_hotreload_arguments([target[ValdiModuleInfo].name for target in ctx.attr.targets], config_yaml_file, explicit_input_list_file, toolchain, base_project_path)

    # Here we run a command that outputs commands into run_hotreloader.sh.
    # We do this so that Bazel can resolve the tools and input_manifests of the
    # compiler and its dependencies.
    cmd = """
echo 'export BAZEL_BINDIR={bin_dir}' > {script_path}
echo "export BAZEL_EXECROOT=$PWD" >> {script_path}
echo '{executable} {args}' >> {script_path}
    """.format(bin_dir = ctx.bin_dir.path, executable = executable.path, args = args, script_path = run_hotreloader.path)

    ctx.actions.run_shell(
        outputs = [run_hotreloader],
        inputs = [executable, config_yaml_file, explicit_input_list_file],
        tools = tools,
        command = cmd,
        input_manifests = input_manifests,
        arguments = [],
        env = {},
        toolchain = VALDI_TOOLCHAIN_TYPE,
        mnemonic = "ValdiHotreload",
        progress_message = "Generating hotreloader.sh",
    )

    return (run_hotreloader, [])

def _valdi_hotreload_impl(ctx):
    """Generates a shell script that can start the Compiler in hotreload mode
    """
    (output, transitive_outputs) = _invoke_valdi_hotreloader(ctx)

    return [
        DefaultInfo(executable = output, files = depset([output])),
    ]

valdi_hotreload = rule(
    implementation = _valdi_hotreload_impl,
    toolchains = [VALDI_TOOLCHAIN_TYPE],
    executable = True,
    attrs = {
        "targets": attr.label_list(
            mandatory = True,
            doc = "The targets to hotreload",
        ),
        "_valdi_base": attr.label(
            doc = "The base module that all Valdi modules depend on",
            default = "@valdi//modules:valdi_base",
        ),
        "_template": attr.label(
            doc = "The template config.yaml file",
            allow_single_file = True,
            default = "valdi_config.yaml.tpl",
        ),
    },
)
