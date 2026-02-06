""" This file contains a convenient macro to wrap valdi_compiled rule. Use this macro instead of direct valdi_compiled rule invocation. """

load("@android_macros//:android.bzl", "android_library")
load("@bazel_skylib//lib:new_sets.bzl", "sets")
load("@build_bazel_rules_apple//apple:resources.bzl", "apple_bundle_import", "apple_resource_group")
load("@build_bazel_rules_swift//swift:swift.bzl", "swift_library")
load("@rules_hdrs//hmap:hmap.bzl", "headermap")
load("@rules_hdrs//umbrella_header:umbrella_header.bzl", "umbrella_header")
load("@rules_kotlin//kotlin:android.bzl", "kt_android_library")
load("@snap_macros//:internal/sourcegen/valdi_context_factory.bzl", "valdi_context_factory_headers", "valdi_context_factory_impl")
load("//bzl:asset_package.bzl", "asset_package")
load("//bzl:client_objc_library.bzl", "client_objc_library")
load("//bzl/valdi:valdi_module_android_common.bzl", "COMMON_ANDROIDX_DEPS", "COMMON_FRAMEWORK_DEPS")
load("//bzl/valdi:valdi_module_native.bzl", "valdi_module_native")
load("//bzl/valdi/source_set:utils.bzl", "source_set_select")
load(
    "common.bzl",
    "IOS_API_NAME_SUFFIX",
    "IOS_OUTPUT_BASE",
    "IOS_SWIFT_SUFFIX",
)
load(":extract_cpp_srcs.bzl", "extract_cpp_srcs")
load(":extract_objc_srcs.bzl", "extract_objc_srcs")
load(":extract_swift_srcs.bzl", "extract_swift_srcs")
load(":generate_android_manifest.bzl", "generate_android_manifest")
load(":valdi_compiled.bzl", "valdi_compiled", _valdi_hotreload = "valdi_hotreload")
load(":valdi_module_info_extractor.bzl", "extract_transitive_valdi_module_output", "extract_valdi_module_native_output", "extract_valdi_module_output")
load(":valdi_projectsync.bzl", "valdi_projectsync")
load(":valdi_static_resource.bzl", "valdi_static_resource")
load(":valdi_test.bzl", "valdi_test")

COMPILER_FLAGS = [
    # '-Os', # Uncomment to enable optimizations
    "-Wno-c11-extensions",
    "-Wno-c99-extensions",
    "-Wno-keyword-macro",
    "-Wno-vla-extension",
    "-Wno-zero-length-array",
    "-Wno-gnu-statement-expression",
    "-Wno-gnu-conditional-omitted-operand",
    "-Wno-pedantic",
    "-fno-exceptions",
    "-fvisibility=default",
]

def valdi_module(
        name,
        srcs,
        ios_module_name = None,
        ios_output_target = "release",
        android_output_target = "release",
        res = [],
        protodecl_srcs = [],
        deps = [],
        module_yaml = None,
        strings_dir = None,
        disable_annotation_processing = False,
        async_strict_mode = False,
        ids_yaml = None,
        sql_db_names = None,
        sql_srcs = [],
        downloadable_assets = None,
        test_target_name = "test",
        compilation_mode = "js_bytecode",
        # class path of generated files
        android_class_path = None,
        android_export_strings = None,
        prepared_upload_artifact_name = None,
        # TODO: start using this
        no_compiled_valdimodule_output = False,
        inline_assets = False,
        single_file_codegen = True,
        has_ios_exports = True,
        has_android_exports = True,
        disable_code_coverage = False,
        disable_dependency_verification = False,
        disable_hotreload = False,
        visibility = visibility,
        ios_language = "objc",
        ios_generated_context_factories = [],
        ios_deps = [],
        android_deps = [],
        native_deps = [],
        web_deps = [],
        exclude_patterns = None,
        exclude_globs = None,
        **kwargs):
    """ A convenient macro to wrap valdi_compiled rule. Use this macro instead of direct valdi_compiled rule invocation.

    The macro generates a few targets:
        - {name}_compiled: the main target, which invokes Valdi compiler and generates .valdimodule file
        - {name}_kt: the kt_android_library target, which wraps the generated Kotlin files
        - {name}_objc: the Objective-C library target, which wraps the generated Objective-C files
        - {name}_objc_api: the Objective-C API library target, which wraps the generated Objective-C API files
        - {name}_resource_group: the apple_resource_group target, which wraps the generated resources for a MacOS application

    The macro also creates a few aliases:
        - {name}: a convenience alias to the {name}_compiled target
        - {ModuleName}: a convenience alias to the {name}_objc target
        - {ModuleName}Types: a convenience alias to the {name}_objc_api target

    Args:
        name: The name of the valdi module.
        module_yaml: The module.yaml file containing the module configuration.
        ios_module_name: The name of the iOS module from module.yaml
        ios_output_target: The iOS output target: "release" or "debug".
        android_class_path: Class path to use when generating Android source files.
        android_output_target: The Android output target: "release" or "debug".
        android_export_strings: Flag to indicate if string resources will be exported.
        srcs: The source files for the valdi module.
        res: The resource files for the valdi module.
        protodecl_srcs: The proto declaration source files.
        deps: The dependencies of the valdi module.
        strings_dir: The directory containing the strings files.
        disable_annotation_processing: Flag to disable annotation processing.
        async_strict_mode: When true, raise exceptions for sync calls made by native code.
        disable_dependency_verification: Flag to disable verification of module dependencies
        disable_code_coverage: Flag to disable code coverage reporting.
        disable_hotreload: Flag to disable hotreload
        ids_yaml: The YAML file containing the IDs.
        sql_db_names: The names of the SQL databases.
        sql_srcs: The SQL source files.
        single_file_codegen: Flag to indicate if the module should use single file codegen mde
        has_ios_exports: Flag to indicate if the module has iOS native exports (default True).
        has_android_exports: Flag to indicate if the module has Android native exports (default True).
        downloadable_assets: Flag to indicate if the module resources are downloaded remotely or bundled with the app.
        compilation_mode: The JavaScript compilation mode of the valdi module. Can be "js_bytecode", "native" or "js".
        no_compiled_valdimodule_output: Flag to indicate that the module doesn't produce compiled valdi module output.
        visibility: The visibility of the Bazel target
        ios_language: The language of the iOS target: "objc", "swift" or "objc, swift".
        exclude_patterns: file patterns to exclude from the module
        exclude_globs: glob patterns to exclude from the module
        **kwargs: Additional keyword arguments.
    """
    downloadable_assets = True if downloadable_assets == None else downloadable_assets

    if not ios_module_name:
        ios_module_name = name

    ### 1. Collect strings files
    strings_json_srcs = []
    if strings_dir:
        strings_json_srcs = native.glob([strings_dir + "/strings-*.json"])

        if not strings_json_srcs:
            fail("you provided a strings_dir, but there were no strings-*.json files in there")

    ### 2. Compile the valdi module
    valdi_compiled(
        name = name,
        ios_module_name = ios_module_name,
        ios_output_target = ios_output_target,
        android_output_target = android_output_target,
        android_export_strings = android_export_strings,
        module = name,
        module_yaml = module_yaml,
        deps = [_valdi_compiled_target_for_target(dep) for dep in deps],
        web_deps = web_deps,
        srcs = srcs,
        res = res,
        protodecl_srcs = protodecl_srcs,
        ids_yaml = ids_yaml,
        strings_json_srcs = strings_json_srcs,
        disable_annotation_processing = disable_annotation_processing,
        async_strict_mode = async_strict_mode,
        sql_db_names = sql_db_names,
        sql_srcs = sql_srcs,
        has_ios_exports = has_ios_exports,
        has_android_exports = has_android_exports,
        has_dependency_data = len(ios_generated_context_factories) > 0,
        prepared_upload_artifact_name = prepared_upload_artifact_name,
        # TODO(simon): We should figure out a way to get these flags to be automatically resolved
        # based on the target platform. That would require getting rid of cfg = "exec , but we would
        # need to prevent modules to be compiled 3 times or more when targetting Android.
        downloadable_assets = True if prepared_upload_artifact_name else select({
            "@valdi//bzl/valdi:upload_assets": downloadable_assets,
            "@valdi//bzl/valdi:bundle_assets": False,
            "@valdi//bzl/valdi:strip_assets": False,
            "@valdi//bzl/valdi:inline_assets": False,
        }),
        strip_assets = False if prepared_upload_artifact_name else select({
            "@valdi//bzl/valdi:strip_assets": downloadable_assets,
            "@valdi//bzl/valdi:upload_assets": False,
            "@valdi//bzl/valdi:bundle_assets": False,
            "@valdi//bzl/valdi:inline_assets": False,
        }),
        inline_assets = True if inline_assets else select({
            "@valdi//bzl/valdi:strip_assets": False,
            "@valdi//bzl/valdi:upload_assets": False,
            "@valdi//bzl/valdi:bundle_assets": False,
            "@valdi//bzl/valdi:inline_assets": True,
        }),
        compilation_mode = compilation_mode,
        visibility = visibility,
        tags = [
            "valdi_compiled",
        ],
        strings_dir = strings_dir,
        android_class_path = android_class_path,
        single_file_codegen = single_file_codegen,
        disable_code_coverage = disable_code_coverage,
        disable_dependency_verification = disable_dependency_verification,
        disable_hotreload = disable_hotreload,
        exclude_patterns = exclude_patterns,
        exclude_globs = exclude_globs,
        **kwargs
    )

    # ### 2.1  Create a convenience alias to the compiled valdi module to build it by default
    compiled_module_target = native.package_relative_label(name)

    all_valdi_module_deps = sets.to_list(sets.make(deps))

    ### 3. Setup the Android target named {name}_kt
    _setup_android_target(name, all_valdi_module_deps, android_deps, compiled_module_target, visibility, android_output_target, has_android_exports)

    ### 4. Setup the iOS target named {name}_objc
    # A module has resource bundle if there are resources on the disk and resources are not downloaded remotely at runtime.
    _setup_ios_target(name, all_valdi_module_deps, ios_deps, compiled_module_target, ios_module_name, sql_db_names, ios_generated_context_factories, bool(res), downloadable_assets, ios_output_target, visibility, ios_language, single_file_codegen, has_ios_exports)

    ### 5. Setup the C++ target named {name}_cpp
    _setup_cpp_target(name, all_valdi_module_deps, compiled_module_target, visibility, single_file_codegen)

    #### 6. Setup Web target
    _setup_web_target(name, all_valdi_module_deps, compiled_module_target, visibility, compilation_mode, web_deps)

    ### 7. Setup the native targets named {name}_native
    _setup_native_target(name, all_valdi_module_deps, native_deps, compiled_module_target, visibility)

    ### 8. Setup the test target
    _setup_test_target(name, test_target_name, srcs)

    ### 9. Setup the prepared upload artifact if set
    if prepared_upload_artifact_name:
        _setup_prepared_upload_artifact(name, prepared_upload_artifact_name, compiled_module_target, visibility)

    ### 10. Setup the hotreload target
    _valdi_hotreload(
        name = name + "_hotreload",
        targets = [":{}".format(name)],
    )

    ### 11: Setup projectsync target, used for VSCode autocompletion
    _setup_projectsync_target(
        name = name + "_projectsync",
        target = ":{}".format(name),
        res = res,
        ids_yaml = ids_yaml,
        strings_json_srcs = strings_json_srcs,
        sql_db_names = sql_db_names,
        sql_srcs = sql_srcs,
        visibility = visibility,
    )

def valdi_hotreload(**kwargs):
    _valdi_hotreload(
        **kwargs
    )

def _setup_test_target(name, test_target_name, srcs):
    target = ":{}".format(name)
    valdi_test(name = test_target_name, srcs = srcs, target = target, hot_reload = select({
        "@valdi//bzl/valdi:valdi_hot_reload_enabled": True,
        "//conditions:default": False,
    }), tags = [
        "valdi_test",
    ])

def _setup_projectsync_target(
        name,
        target,
        res,
        strings_json_srcs,
        ids_yaml,
        sql_db_names,
        sql_srcs,
        visibility = visibility):
    valdi_projectsync(
        name = name,
        target = target,
        res = res,
        strings_json_srcs = strings_json_srcs,
        ids_yaml = ids_yaml,
        sql_db_names = sql_db_names,
        sql_srcs = sql_srcs,
        visibility = visibility,
    )

def _setup_prepared_upload_artifact(name, prepared_upload_artifact_name, compiled_module_target, visibility):
    extract_valdi_module_output(
        name = prepared_upload_artifact_name,
        compiled_module = compiled_module_target,
        output_name = "prepared_upload_artifact",
        visibility = visibility,
    )

def _valdi_source_set_select(debug, release, is_release_output_target, release_default_value = []):
    """ Selects the source set based on the source set and module's output target.

    Conceptually, there's output target on Valdi module level which is either "release" or "debug".
    Then, there are source sets, aka build flavors, introduced at build time, which can be *mapped* into either "release" or "debug".

    Typically a "debug" module is only used by other "debug" targets,
    but there are "release" targets that depend on "debug" modules via excplicit "allowed_debug_dependencies" list.

    To align with the existing behavior, such "debug" targets, in "release" build flavor should expose an "empty" Android/iOS targets.
    We achieve that by providing an empty value for the "release" source set.
    """

    return source_set_select(
        debug = debug,
        release = release if is_release_output_target else release_default_value,
    )

def _setup_android_target(name, deps, android_deps, compiled_module_target, visibility, android_output_target, has_android_exports):
    ################
    ####
    #### Configure the Android library target
    ####
    ################
    is_release = android_output_target == "release"

    # Android resources
    extract_valdi_module_output(
        name = "android.debug.valdimodule",
        compiled_module = compiled_module_target,
        output_name = "android_debug_valdimodule",
        visibility = visibility,
    )

    extract_valdi_module_output(
        name = "android.release.valdimodule",
        compiled_module = compiled_module_target,
        output_name = "android_release_valdimodule",
        visibility = visibility,
    )

    # Only extract srcjars if the module has Android exports
    if has_android_exports:
        extract_valdi_module_output(
            name = "android.debug.srcjar",
            compiled_module = compiled_module_target,
            output_name = "android_debug_srcjar",
            visibility = visibility,
        )

        extract_valdi_module_output(
            name = "android.release.srcjar",
            compiled_module = compiled_module_target,
            output_name = "android_release_srcjar",
            visibility = visibility,
        )

    extract_valdi_module_output(
        name = "android.debug.resource_files",
        compiled_module = compiled_module_target,
        output_name = "android_debug_resource_files",
        visibility = visibility,
    )

    extract_valdi_module_output(
        name = "android.release.resource_files",
        compiled_module = compiled_module_target,
        output_name = "android_release_resource_files",
        visibility = visibility,
    )

    extract_valdi_module_output(
        name = "android.debug.sourcemaps",
        compiled_module = compiled_module_target,
        output_name = "android_debug_sourcemaps",
        visibility = visibility,
    )

    extract_valdi_module_output(
        name = "android.debug.sourcemaparchive",
        compiled_module = compiled_module_target,
        output_name = "android_debug_sourcemap_archive",
        visibility = visibility,
    )

    extract_valdi_module_output(
        name = "android.release.sourcemaparchive",
        compiled_module = compiled_module_target,
        output_name = "android_release_sourcemap_archive",
        visibility = visibility,
    )

    kt_deps = [_kt_target_for_target(dep) for dep in deps]

    package_name = "com.snap.valdi.modules.{name}".format(name = name)
    generate_android_manifest(
        name = name + "_manifest",
        package = package_name,
    )

    # Wrap all of the generated Kotlin files in a library that can be used from other Android targets.
    # We use "android_library" here instead of "jvm_library" because the
    # generated code depends on Android classes such as Context.
    kt_android_library(
        name = name + "_api_kt",
        srcs = _valdi_source_set_select(
            debug = [":android.debug.srcjar"] if has_android_exports else ["@valdi//bzl/valdi:empty.kt"],
            release = [":android.release.srcjar"] if has_android_exports else ["@valdi//bzl/valdi:empty.kt"],
            is_release_output_target = is_release,
            release_default_value = ["@valdi//bzl/valdi:empty.kt"],
        ),
        # Both direct and transitive dependencies must be listed because Android repo explicitly forbids transitive dependencies.
        deps = kt_deps + COMMON_ANDROIDX_DEPS + COMMON_FRAMEWORK_DEPS,
        # Export all dependencies for now, but change in the future once we have a way to enforce "no transitive dependencies"
        # rule in the module.yaml
        exports = kt_deps + COMMON_FRAMEWORK_DEPS,
        visibility = visibility,
    )

    android_library(
        name = "{}_kt".format(name),
        tags = [
            "valdi_kt",
        ],
        exports = [":{}_api_kt".format(name)] + android_deps,
        custom_package = package_name,
        manifest = native.package_relative_label(name + "_manifest"),
        assets = _valdi_source_set_select(
            debug = [
                ":android.debug.valdimodule",
                ":android.debug.sourcemaps",
            ],
            release = [":android.release.valdimodule"],
            is_release_output_target = is_release,
        ),
        assets_dir = _valdi_source_set_select(
            debug = "android/debug/assets",
            release = "android/release/assets",
            is_release_output_target = is_release,
            release_default_value = "",
        ),
        resource_files = _valdi_source_set_select(
            debug = [":android.debug.resource_files"],
            release = [":android.release.resource_files"],
            is_release_output_target = is_release,
        ),
        visibility = visibility,
    )

def _setup_ios_target(name, module_deps, ios_deps, compiled_module_target, ios_module_name, sql_db_names, ios_generated_context_factories, has_resource_bundle, downloadable_assets, ios_output_target, visibility, ios_language, single_file_codegen, has_ios_exports):
    ################
    ####
    #### Configure the objc_library targets
    ####
    ################
    api_objc_deps = [_objc_api_target_for_target(dep) for dep in module_deps]
    objc_deps = [_objc_target_for_target(dep) for dep in module_deps]
    swift_deps = [_ios_target_for_target(dep) for dep in module_deps]

    # iOS resources
    extract_valdi_module_output(
        name = "ios.debug.resource_files",
        compiled_module = compiled_module_target,
        output_name = "ios_debug_resource_files",
    )

    extract_valdi_module_output(
        name = "ios.release.resource_files",
        compiled_module = compiled_module_target,
        output_name = "ios_release_resource_files",
    )

    extract_valdi_module_output(
        name = "ios.debug.valdimodule",
        compiled_module = compiled_module_target,
        output_name = "ios_debug_valdimodule",
    )

    extract_valdi_module_output(
        name = "ios.release.valdimodule",
        compiled_module = compiled_module_target,
        output_name = "ios_release_valdimodule",
    )

    extract_valdi_module_native_output(
        name = "ios.debug.srcs",
        compiled_module = compiled_module_target,
        output_name = "ios_debug_generated_srcs",
    )

    extract_valdi_module_native_output(
        name = "ios.release.srcs",
        compiled_module = compiled_module_target,
        output_name = "ios_release_generated_srcs",
    )

    extract_valdi_module_native_output(
        name = "ios.debug.api.srcs",
        compiled_module = compiled_module_target,
        output_name = "ios_debug_api_generated_srcs",
    )

    extract_valdi_module_native_output(
        name = "ios.release.api.srcs",
        compiled_module = compiled_module_target,
        output_name = "ios_release_api_generated_srcs",
    )

    extract_valdi_module_output(
        name = "ios.debug.sourcemaps",
        compiled_module = compiled_module_target,
        output_name = "ios_debug_sourcemaps",
    )

    extract_valdi_module_output(
        name = "ios.debug.sourcemaparchive",
        compiled_module = compiled_module_target,
        output_name = "ios_debug_sourcemap_archive",
        visibility = visibility,
    )

    extract_valdi_module_output(
        name = "ios.release.sourcemaparchive",
        compiled_module = compiled_module_target,
        output_name = "ios_release_sourcemap_archive",
        visibility = visibility,
    )

    selected_source_set = source_set_select(
        debug = "debug",
        release = "release",
    )

    if single_file_codegen:
        extract_valdi_module_output(
            name = "ios.debug.hdrs",
            compiled_module = compiled_module_target,
            output_name = "ios_debug_generated_hdrs",
        )

        extract_valdi_module_output(
            name = "ios.release.hdrs",
            compiled_module = compiled_module_target,
            output_name = "ios_release_generated_hdrs",
        )

        extract_valdi_module_output(
            name = "ios.debug.api.hdrs",
            compiled_module = compiled_module_target,
            output_name = "ios_debug_api_generated_hdrs",
        )

        extract_valdi_module_output(
            name = "ios.release.api.hdrs",
            compiled_module = compiled_module_target,
            output_name = "ios_release_api_generated_hdrs",
        )

        api_objc_hdrs = []
        api_objc_srcs = []
        objc_hdrs = []
        objc_srcs = []
        swift_srcs = []

        if "swift" in ios_language:
            swift_srcs = source_set_select(
                debug = [":ios.debug.srcs"],
                release = [":ios.release.srcs"],
            )
        if "objc" in ios_language:
            api_objc_hdrs = source_set_select(
                debug = [":ios.debug.api.hdrs"],
                release = [":ios.release.api.hdrs"],
            )
            api_objc_srcs = source_set_select(
                debug = [":ios.debug.api.srcs"],
                release = [":ios.release.api.srcs"],
            )
            objc_hdrs = source_set_select(
                debug = [":ios.debug.hdrs"],
                release = [":ios.release.hdrs"],
            )
            objc_srcs = source_set_select(
                debug = [":ios.debug.srcs"],
                release = [":ios.release.srcs"],
            )

        native.filegroup(
            name = name + "_api_objc_hdrs",
            srcs = api_objc_hdrs,
            visibility = visibility,
        )

        native.filegroup(
            name = name + "_objc_hdrs",
            srcs = objc_hdrs,
            visibility = visibility,
        )
    else:
        api_objc_hdrs_name = name + "_api_objc_hdrs"
        api_objc_srcs_name = name + "_api_objc_srcs"
        objc_hdrs_name = name + "_objc_hdrs"
        objc_srcs_name = name + "_objc_srcs"
        swift_srcs_name = name + "_swift_srcs"

        api_objc_hdrs = [native.package_relative_label(api_objc_hdrs_name)]
        api_objc_srcs = [native.package_relative_label(api_objc_srcs_name)]
        objc_hdrs = [native.package_relative_label(objc_hdrs_name)]
        objc_srcs = [native.package_relative_label(objc_srcs_name)]
        swift_srcs = [native.package_relative_label(swift_srcs_name)]

        # Only extract native sources if the module has iOS exports
        if has_ios_exports:
            extract_objc_srcs(
                name = api_objc_hdrs_name,
                compiled_module = compiled_module_target,
                extension = ".h",
                api_only = True,
                selected_source_set = selected_source_set,
                visibility = visibility,
            )

            extract_objc_srcs(
                name = api_objc_srcs_name,
                compiled_module = compiled_module_target,
                extension = ".m",
                api_only = True,
                selected_source_set = selected_source_set,
            )

            extract_objc_srcs(
                name = objc_hdrs_name,
                compiled_module = compiled_module_target,
                extension = ".h",
                api_only = False,
                selected_source_set = selected_source_set,
                visibility = visibility,
            )

            extract_objc_srcs(
                name = objc_srcs_name,
                compiled_module = compiled_module_target,
                extension = ".m",
                api_only = False,
                selected_source_set = selected_source_set,
            )

            extract_swift_srcs(
                name = swift_srcs_name,
                compiled_module = compiled_module_target,
                selected_source_set = selected_source_set,
            )
        else:
            # Create empty filegroups when there are no exports
            native.filegroup(
                name = api_objc_hdrs_name,
                srcs = [],
                visibility = visibility,
            )
            native.filegroup(
                name = api_objc_srcs_name,
                srcs = [],
            )
            native.filegroup(
                name = objc_hdrs_name,
                srcs = [],
                visibility = visibility,
            )
            native.filegroup(
                name = objc_srcs_name,
                srcs = [],
            )
            native.filegroup(
                name = swift_srcs_name,
                srcs = [],
            )

    # iOS target named {ios_module_name}
    resources = source_set_select(
        debug = [
            ":ios.debug.valdimodule",
            ":ios.debug.resource_files",
            ":ios.debug.sourcemaps",
        ],
        release = [
            ":ios.release.valdimodule",
            ":ios.release.resource_files",
        ],
    )

    if has_resource_bundle:
        extract_valdi_module_output(
            name = "ios.debug.resource_bundle",
            compiled_module = compiled_module_target,
            output_name = "ios_debug_bundle_resources",
        )

        extract_valdi_module_output(
            name = "ios.release.resource_bundle",
            compiled_module = compiled_module_target,
            output_name = "ios_release_bundle_resources",
        )

        apple_bundle_import(
            name = "ios.debug.resource_bundle_import",
            bundle_imports = [":ios.debug.resource_bundle"],
        )

        apple_bundle_import(
            name = "ios.release.resource_bundle_import",
            bundle_imports = [":ios.release.resource_bundle"],
        )

        resources += select({
            "@valdi//bzl/valdi:debug_upload_assets": [] if downloadable_assets else [":ios.debug.resource_bundle_import"],
            "@valdi//bzl/valdi:debug_strip_assets": [] if downloadable_assets else [":ios.debug.resource_bundle_import"],
            "@valdi//bzl/valdi:debug_bundle_assets": [":ios.debug.resource_bundle_import"],
            "@valdi//bzl/valdi:release_upload_assets": [] if downloadable_assets else [":ios.release.resource_bundle_import"],
            "@valdi//bzl/valdi:release_strip_assets": [] if downloadable_assets else [":ios.release.resource_bundle_import"],
            # By design, debug only builds don't produce resource bundles in the release mode as such there's nothing to bundle.
            "@valdi//bzl/valdi:release_bundle_assets": [":ios.release.resource_bundle_import"] if ios_output_target == "release" else [],
        })

    deps = objc_deps + [native.package_relative_label(name + "_api_objc")] + ios_deps + [native.package_relative_label(name + "_native")]

    # Only for Buck
    # TODO(2954): remove Buck hacks
    if sql_db_names:
        extract_valdi_module_output(
            name = "ios.sql_assets",
            compiled_module = compiled_module_target,
            output_name = "ios_sql_assets",
        )

        # The name has to align with the expected name in Buck
        sql_srcs_name = ios_module_name + "Db"
        asset_package(
            name = sql_srcs_name,
            data = [":ios.sql_assets"],
        )
        deps += select({
            "//conditions:default": [],
        })

    ios_api_module_name = ios_module_name + IOS_API_NAME_SUFFIX

    # Generated API objects
    api_generated_objects = []
    for context_name in ios_generated_context_factories:
        context_factory_headers = valdi_context_factory_headers(
            api_module_name = ios_api_module_name,
            context_name = context_name,
        )
        api_generated_objects.append(context_factory_headers)

    # Generated Implementation objects
    impl_generated_objects = []
    for context_name in ios_generated_context_factories:
        context_factory_impl = valdi_context_factory_impl(
            module_name = ios_module_name,
            api_module_name = ios_api_module_name,
            context_name = context_name,
        )
        impl_generated_objects.append(context_factory_impl)

    if ios_generated_context_factories:
        extract_valdi_module_output(
            name = "dependencyData.json",
            compiled_module = compiled_module_target,
            output_name = "ios_dependency_data",
        )

    _exported_objc_lib(
        name = name + "_objc",
        ios_module_name = ios_module_name,
        deps = deps,
        objc_hdrs = objc_hdrs,
        objc_srcs = objc_srcs,
        data = resources,
        visibility = visibility,
        generated_objects = impl_generated_objects,
        single_file_codegen = single_file_codegen,
        has_ios_exports = has_ios_exports,
        tags = [
            "valdi_objc",
        ],
    )

    should_add_alias = name != ios_module_name

    if should_add_alias:
        # iOS target names {ios_module_name}
        native.alias(
            name = ios_module_name,
            actual = native.package_relative_label(name + "_objc"),
            visibility = visibility,
        )

    _exported_objc_lib(
        name = name + "_api_objc",
        ios_module_name = ios_api_module_name,
        deps = api_objc_deps,
        objc_hdrs = api_objc_hdrs,
        objc_srcs = api_objc_srcs,
        generated_objects = api_generated_objects,
        single_file_codegen = single_file_codegen,
        has_ios_exports = has_ios_exports,
        visibility = visibility,
    )

    if should_add_alias:
        # iOS API target named {ios_module_name}Types
        native.alias(
            name = ios_module_name + IOS_API_NAME_SUFFIX,
            actual = native.package_relative_label(name + "_api_objc"),
            visibility = visibility,
        )

    # Add a _Swift suffix to the Swift module name when both ObjC and Swift
    # codegen is enabled.  This suffix is not added when there is only Swift
    # codegen. We expect only a small amount of modules to have dual codegen so
    # this choice aims to optimize for the user experience of the most common
    # cases (ObjC only or Swift only).
    swift_suffix = "_Swift" if ("swift" in ios_language) and ("objc" in ios_language) else ""
    swift_library(
        name = name + "_swift",
        module_name = ios_module_name + swift_suffix,
        srcs = swift_srcs,
        deps = swift_deps + ["@valdi//valdi_core:valdi_core_swift_marshaller"],
        data = resources,
        copts = ["-Osize", "-Xfrontend", "-internalize-at-link", "-Xcc", "-I."],
        linkopts = ["-dead_strip"],
        visibility = visibility,
    )

    if should_add_alias:
        # iOS API target named {ios_module_name}Swift
        native.alias(
            name = ios_module_name + IOS_SWIFT_SUFFIX,
            actual = native.package_relative_label(name + "_swift"),
            visibility = visibility,
        )

    # Target named {name}_ios that will be either the ObjC or Swift depending on
    # the language that was selected. If the language is "objc, swift", then
    # choose _swift. This is because only Swift targets depend on the "_ios"
    # targets. ObjC targets directly depend on "_objc" targets.
    if "swift" in ios_language:
        ios_target = "_swift"
    elif "objc" in ios_language:
        ios_target = "_objc"
    else:
        ios_target = None
    if ios_target:
        native.alias(
            name = name + "_ios",
            actual = native.package_relative_label(name + ios_target),
            visibility = visibility,
        )

    # Provide resource dependencies for MacOS application builds
    resource_deps = [_resource_target_for_target(dep) for dep in module_deps]
    apple_resource_group(
        name = name + "_resource_group",
        resources = resource_deps + resources,
        visibility = visibility,
    )

def _setup_web_target(name, deps, compiled_module_target, visibility, compilation_mode, web_deps):
    is_release = compilation_mode == "release"

    # All transitive dependencies for a monolithic npm
    extract_transitive_valdi_module_output(
        name = "web.debug.srcs.all",
        modules = [compiled_module_target],
        output_name = "web_debug_sources",
        visibility = visibility,
    )

    extract_transitive_valdi_module_output(
        name = "web.release.srcs.all",
        modules = [compiled_module_target],
        output_name = "web_release_sources",
        visibility = visibility,
    )

    web_srcs_all = _valdi_source_set_select(
        debug = [":web.debug.srcs.all"],
        release = [":web.release.srcs.all"],
        is_release_output_target = is_release,
    )

    # All transitive dependencies for a monolithic npm
    extract_transitive_valdi_module_output(
        name = "web.debug.resource_files.all",
        modules = [compiled_module_target],
        output_name = "web_debug_resource_files",
        visibility = visibility,
    )

    extract_transitive_valdi_module_output(
        name = "web.release.resource_files.all",
        modules = [compiled_module_target],
        output_name = "web_release_resource_files",
        visibility = visibility,
    )

    web_resource_files_all = _valdi_source_set_select(
        debug = [":web.debug.resource_files.all"],
        release = [":web.release.resource_files.all"],
        is_release_output_target = is_release,
    )

    # All transitive strings for a monolithic npm
    extract_transitive_valdi_module_output(
        name = "web.debug.strings.all",
        modules = [compiled_module_target],
        output_name = "web_debug_strings",
        visibility = visibility,
    )

    extract_transitive_valdi_module_output(
        name = "web.release.strings.all",
        modules = [compiled_module_target],
        output_name = "web_release_strings",
        visibility = visibility,
    )

    web_strings_all = _valdi_source_set_select(
        debug = [":web.debug.strings.all"],
        release = [":web.release.strings.all"],
        is_release_output_target = is_release,
    )

    # All transitive protodecl for a monolithic npm
    extract_transitive_valdi_module_output(
        name = "web.protodecl.all",
        modules = [compiled_module_target],
        output_name = "protodecl_srcs",
        visibility = visibility,
    )

    # All web deps
    extract_transitive_valdi_module_output(
        name = "web.deps.all",
        modules = [compiled_module_target],
        output_name = "web_deps",
        visibility = visibility,
    )

    # All TypeScript declaration files
    extract_transitive_valdi_module_output(
        name = "web.dts.all",
        modules = [compiled_module_target],
        output_name = "web_dts_files",
        visibility = visibility,
    )

    native.filegroup(
        name = "{}_all_web_deps".format(name),
        srcs = [":web.deps.all"],
        visibility = visibility,
    )

    native.filegroup(
        name = "{}_web_protodecl".format(name),
        srcs = [":web.protodecl.all"],
        visibility = visibility,
    )

    native.filegroup(
        name = "{}_web_srcs_filegroup".format(name),
        srcs = web_srcs_all + web_resource_files_all + web_strings_all + [":web.protodecl.all", ":web.deps.all", ":web.dts.all"],
        visibility = visibility,
    )

def _web_target_for_target(target):
    label = native.package_relative_label(target)
    return label.relative(":" + label.name + "_web")

def npm_package_target_name(name):
    return "{}_npm_package".format(name)

def npm_package_target_for_target(name):
    label = native.package_relative_label(name)
    return label.relative(":" + npm_package_target_name(label.name))

def _exported_objc_lib(name, ios_module_name, objc_hdrs, objc_srcs, single_file_codegen, has_ios_exports = True, **kwargs):
    # setup headermaps
    # When has_ios_exports is False, we use empty hdrs instead of header_tree_artifact_providers
    # because the headermap rule requires at least one tree artifact when using header_tree_artifact_providers
    if single_file_codegen or not has_ios_exports:
        hmap_hdrs = objc_hdrs if has_ios_exports else []
        hmap_header_tree_providers = []
    else:
        hmap_hdrs = []
        hmap_header_tree_providers = objc_hdrs

    hmap_name = name + "_valdi_module_hmap"
    headermap(
        name = hmap_name,
        hdrs = hmap_hdrs,
        header_tree_artifact_providers = hmap_header_tree_providers,
        namespace = ios_module_name,
        tags = ["manual"],
    )
    hmap_deps = [native.package_relative_label(hmap_name)]
    hmap_copts = []

    if single_file_codegen or not has_ios_exports:
        private_hmap_hdrs = objc_hdrs if has_ios_exports else []
        private_hmap_header_tree_providers = []
    else:
        private_hmap_hdrs = []
        private_hmap_header_tree_providers = objc_hdrs

    private_hmap_name = name + "_valdi_module_private_hmap"
    headermap(
        name = private_hmap_name,
        hdrs = private_hmap_hdrs,
        header_tree_artifact_providers = private_hmap_header_tree_providers,
        add_to_includes = False,
        tags = ["manual"],
    )
    private_hmap_deps = [native.package_relative_label(private_hmap_name)]
    hmap_copts.append("-I$(execpath :{})".format(private_hmap_name))

    # headermap values, representing paths to headers, are paths relative to
    # execroot, so we pass '-I.' to correctly resolve hmap header paths.
    # -I. needs to come last after all .hmap includes
    hmap_copts.append("-I.")

    # The umbrella header is a public header
    # Umbrella headers for ObjC libraries are meant to only be imported
    # from -Swift.h generated headers (replacing modular @import).
    # The -Swift.h headers use workspace-relative imports of umbrella
    # headers and exposing it via headermap is not necessary.
    internal_umbrella_header_name = name + "_umbrella.h"
    umbrella_header(
        name = internal_umbrella_header_name,
        hdrs = objc_hdrs if has_ios_exports else [],
        umbrella_header_name = ios_module_name + "-Swift",
        tags = ["manual"],
    )
    umbrella_headers = [":" + internal_umbrella_header_name]

    # When has_ios_exports is False, use empty.c as a fallback source
    # because objc_library requires at least one source file
    actual_objc_srcs = objc_srcs if has_ios_exports else ["@valdi//bzl/valdi:empty.c"]
    actual_objc_hdrs = objc_hdrs if has_ios_exports else []

    client_objc_library(
        name = name,
        hdrs = [] + actual_objc_hdrs + umbrella_headers,
        srcs = [] + actual_objc_srcs,
        enable_swift_interop = True,
        module_name = ios_module_name,
        copts = COMPILER_FLAGS + hmap_copts,
        sdk_frameworks = [
            "UIKit",
            "JavaScriptCore",
            "QuartzCore",
            "CoreGraphics",
            "Security",
        ],
        deps = kwargs.pop("deps", []) + [
            "@valdi//valdi_core:valdi_core_objc",
        ] + hmap_deps,
        implementation_deps = [] + private_hmap_deps,
        tags = kwargs.pop("tags", []) + [
            "exported",
            "objc",
            "app_size_owners=VALDI",
            "res_drop_prefix=" + IOS_OUTPUT_BASE,
        ],
        enable_objcpp = False,
        generate_hmaps = False,
        generate_umbrella_header = False,
        **kwargs
    )

def _setup_cpp_target(name, deps, compiled_module_target, visibility, single_file_codegen):
    cpp_strip_prefix = "cpp/release/src"

    # C++ codegen always outputs to release configuration
    if single_file_codegen:
        # For single file codegen, extract individual .cpp and .hpp files
        extract_valdi_module_output(
            name = "cpp.srcs",
            compiled_module = compiled_module_target,
            output_name = "cpp_srcs",
        )

        extract_valdi_module_output(
            name = "cpp.hdrs",
            compiled_module = compiled_module_target,
            output_name = "cpp_hdrs",
        )

        cpp_srcs = [":cpp.srcs"]
        cpp_hdrs = [":cpp.hdrs"]
    else:
        # For multi-file codegen, use extract_cpp_srcs to filter by extension
        cpp_srcs_name = name + "_cpp_srcs"
        cpp_hdrs_name = name + "_cpp_hdrs"

        extract_cpp_srcs(
            name = cpp_srcs_name,
            compiled_module = compiled_module_target,
            extension = ".cpp",
            strip_prefix = cpp_strip_prefix,
        )

        extract_cpp_srcs(
            name = cpp_hdrs_name,
            compiled_module = compiled_module_target,
            extension = ".hpp",
            strip_prefix = cpp_strip_prefix,
        )

        cpp_srcs = [native.package_relative_label(cpp_srcs_name)]
        cpp_hdrs = [native.package_relative_label(cpp_hdrs_name)]

    # C++ codegen always outputs to release configuration.
    # Both single_file_codegen and multi-file codegen strip the cpp/release/src/ prefix
    # so that #include "valdi_modules/..." works correctly.
    cc_library_kwargs = {
        "name": name + "_cpp",
        "srcs": cpp_srcs,
        "hdrs": cpp_hdrs,
        "deps": [_cpp_target_for_target(dep) for dep in deps] + ["@valdi//valdi_core:valdi_core_cc"],
        "visibility": visibility,
    }

    if single_file_codegen:
        # For single file mode, strip via cc_library's strip_include_prefix
        cc_library_kwargs["strip_include_prefix"] = cpp_strip_prefix
    else:
        # For multi-file mode, the extracted directory contains subdirectories
        # We need to add it as an include path
        cc_library_kwargs["includes"] = [name + "_cpp_hdrs"]

    native.cc_library(**cc_library_kwargs)

def _setup_native_target(name, deps, additional_native_deps, compiled_module_target, visibility):
    ################
    ####
    #### Configure the native library target
    ####
    ################
    extract_valdi_module_native_output(
        name = "android.debug.c",
        compiled_module = compiled_module_target,
        output_name = "android_debug_nativesrc",
    )

    extract_valdi_module_native_output(
        name = "android.release.c",
        compiled_module = compiled_module_target,
        output_name = "android_release_nativesrc",
    )

    extract_valdi_module_native_output(
        name = "ios.debug.c",
        compiled_module = compiled_module_target,
        output_name = "ios_debug_nativesrc",
    )

    extract_valdi_module_native_output(
        name = "ios.release.c",
        compiled_module = compiled_module_target,
        output_name = "ios_release_nativesrc",
    )

    native_deps = [_native_target_for_target(dep) for dep in deps]

    static_res_lib_name = "{}_static_res".format(name)

    valdi_static_resource(
        name = static_res_lib_name,
        srcs = source_set_select(
            debug = [
                ":android.debug.valdimodule",
                ":android.debug.sourcemaps",
            ],
            release = [":android.release.valdimodule"],
        ),
    )

    native_lib_name = "{}_native".format(name)
    android_native_lib_name = "{}_android".format(native_lib_name)
    ios_native_lib_name = "{}_ios".format(native_lib_name)
    desktop_native_lib_name = "{}_desktop".format(native_lib_name)

    valdi_module_native(
        name = android_native_lib_name,
        srcs = source_set_select(
            debug = [":android.debug.c"],
            release = [":android.release.c"],
        ),
        deps = ["{}_android".format(dep) for dep in native_deps] + additional_native_deps,
    )

    valdi_module_native(
        name = ios_native_lib_name,
        # TODO(simon): Make native compilation configurable on a per platform basis
        # Disable on iOS for now to unblock.
        srcs = [],
        # srcs = source_set_select(
        #     debug = [":ios.debug.c"],
        #     release = [":ios.release.c"],
        # ),
        deps = ["{}_ios".format(dep) for dep in native_deps] + additional_native_deps,
    )

    valdi_module_native(
        name = desktop_native_lib_name,
        srcs = source_set_select(
            debug = [":android.debug.c"],
            release = [":android.release.c"],
        ) + [":{}".format(static_res_lib_name)],
        deps = ["{}_desktop".format(dep) for dep in native_deps] + additional_native_deps,
    )

    native.alias(
        name = native_lib_name,
        actual = select({
            "@snap_platforms//conditions:ios": ios_native_lib_name,
            "@snap_platforms//conditions:android": android_native_lib_name,
            "@snap_platforms//conditions:macos": desktop_native_lib_name,
            "@snap_platforms//conditions:linux": desktop_native_lib_name,
        }),
        visibility = visibility,
    )

# Helpers to construct related target label from the original valdi module label
def _valdi_compiled_target_for_target(target):
    label = native.package_relative_label(target)
    return label.relative(":" + label.name)

def _kt_target_for_target(target):
    label = native.package_relative_label(target)
    return label.relative(":" + label.name + "_kt")

def _native_target_for_target(target):
    label = native.package_relative_label(target)
    return label.relative(":" + label.name + "_native")

def _cpp_target_for_target(target):
    label = native.package_relative_label(target)
    return label.relative(":" + label.name + "_cpp")

def _objc_target_for_target(target, target_suffix = ""):
    label = native.package_relative_label(target)
    return label.relative(":" + label.name + target_suffix + "_objc")

def _objc_api_target_for_target(target):
    return _objc_target_for_target(target, target_suffix = "_api")

def _ios_target_for_target(target, target_suffix = ""):
    label = native.package_relative_label(target)
    return label.relative(":" + label.name + target_suffix + "_ios")

def _resource_target_for_target(target):
    label = native.package_relative_label(target)
    return label.relative(":" + label.name + "_resource_group")
