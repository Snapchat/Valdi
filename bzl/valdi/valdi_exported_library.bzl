load("@build_bazel_rules_apple//apple:apple.bzl", "apple_xcframework")
load("@build_bazel_rules_swift//swift:swift.bzl", "swift_library")
load("//bzl:expand_template.bzl", "expand_template")
load("//bzl/android:collect_android_assets.bzl", "collect_android_assets")
load("//bzl/valdi:rewrite_hdrs.bzl", "rewrite_hdrs")
load("//bzl/valdi:suffixed_deps.bzl", "get_suffixed_deps")
load("//bzl/valdi:valdi_collapse_web_paths.bzl", "collapse_native_paths", "collapse_web_paths", "generate_register_native_modules")
load("//bzl/valdi:valdi_protodecl_to_js.bzl", "collapse_protodecl_paths", "protodecl_to_js_dir")
load("//bzl/valdi/source_set:utils.bzl", "source_set_select")
load("//valdi:valdi.bzl", "valdi_android_aar")

_PRESERVED_MODULE_NAMES = ["UIKit", "Foundation", "CoreFoundation", "CoreGraphics", "QuartzCore"]

DEFAULT_ANDROID_EXCLUDED_CLASS_PATTERNS = [
    "org/intellij/.*",
    "org/jetbrains/.*",
    "kotlin/.*",
    # TODO(simon): Figure out why this is needed
    "com/snap/valdi/support/R\\.class",
    "com/snap/valdi/support/R\\$.*\\.class",
    "android/support/.*",
    "androidx/.*",
    "META-INF",
]

DEFAULT_JAVA_DEPS = ["@valdi//valdi:valdi_android_support"]

def valdi_exported_library(
        name,
        ios_bundle_id,
        ios_bundle_name,
        deps,
        ios_swift = False,
        ios_swift_module_names = [],
        android_excluded_class_path_patterns = DEFAULT_ANDROID_EXCLUDED_CLASS_PATTERNS,
        java_deps = DEFAULT_JAVA_DEPS,
        web_package_name = None,
        npm_scope = "",
        npm_version = "1.0.0",
        web_exclude_jsx_global_declaration = False):
    """Exports Valdi modules as platform-specific libraries (xcframework, aar, npm).

    Args:
        ios_swift: If True, includes Swift generated code in the xcframework.
            Requires that dependent valdi_module targets use ios_language = "swift"
            (or ["objc", "swift"] for dual codegen).
        ios_swift_module_names: Swift module names of deps that import each other.
            Only needed for multi-module exports with cross-module Swift imports.
            ValdiCoreSwift is always included automatically.
    """
    if not web_package_name:
        web_package_name = "{}_npm".format(name)

    ios_public_hdrs_name = "{}_ios_hdrs".format(name)
    rewrite_hdrs(
        name = ios_public_hdrs_name,
        module_name = ios_bundle_name,
        preserved_module_names = _PRESERVED_MODULE_NAMES,
        flatten_paths = True,
        srcs = [
            "@valdi//valdi:valdi_ios_public_hdrs",
            "@valdi//valdi_core:valdi_core_ios_public_hdrs",
        ] + get_suffixed_deps(deps, "_api_objc_hdrs") + get_suffixed_deps(deps, "_objc_hdrs"),
    )

    xcframework_deps = ["@valdi//valdi"] + get_suffixed_deps(deps, "_objc")

    if ios_swift:
        merged_swift_name = "{}_merged_swift".format(name)
        module_aliases = ["-Xfrontend", "-module-alias", "-Xfrontend", "ValdiCoreSwift={}".format(ios_bundle_name)]
        for mod in ios_swift_module_names:
            module_aliases += ["-Xfrontend", "-module-alias", "-Xfrontend", "{}={}".format(mod, ios_bundle_name)]

        strip_imports_name = "{}_stripped_swift_srcs".format(name)
        native.genrule(
            name = strip_imports_name,
            srcs = get_suffixed_deps(deps, "_swift_srcs"),
            outs = ["{}_merged_module.swift".format(name)],
            cmd = """
for src in $(SRCS); do
    if [ -d "$$src" ]; then
        find "$$src" -name '*.swift' -exec cat {} \\;
    else
        cat "$$src"
    fi
done | sed '/^import ValdiCoreSwift$$/d' > $@
""",
            target_compatible_with = ["@platforms//os:ios"],
        )

        swift_library(
            name = merged_swift_name,
            module_name = ios_bundle_name,
            srcs = [":{}".format(strip_imports_name)] + [
                "@valdi//valdi_core:valdi_core_swift_marshaller_srcs",
            ],
            deps = [
                "@valdi//valdi_core:valdi_core_cpp_objc",
                "@valdi//valdi_core:valdi_core_swift_interop_lib",
            ] + get_suffixed_deps(deps, "_objc"),
            copts = [
                "-Osize",
                "-Xfrontend",
                "-internalize-at-link",
                "-Xcc",
                "-I.",
            ] + module_aliases,
            linkopts = ["-dead_strip"],
            target_compatible_with = ["@platforms//os:ios"],
        )
        xcframework_deps.append(":{}".format(merged_swift_name))

    apple_xcframework(
        name = "{}_ios".format(name),
        bundle_name = ios_bundle_name,
        deps = xcframework_deps,
        infoplists = [
            "@valdi//bzl/valdi:Info.plist",
        ],
        minimum_os_versions = {
            "ios": "13.0",
        },
        families_required = {
            "ios": [
                "iphone",
                "ipad",
            ],
        },
        ios = {
            "device": ["arm64"],
            "simulator": ["arm64", "x86_64"],
        },
        tags = ["valdi_ios_exported_library"],
        bundle_id = ios_bundle_id,
        public_hdrs = [":{}".format(ios_public_hdrs_name)],
    )

    java_deps = java_deps + get_suffixed_deps(deps, "_kt")

    collect_android_assets(
        name = "{}_android_assets".format(name),
        valdi_deps = deps,
        deps = java_deps,
        output_target = source_set_select(
            debug = "debug",
            release = "release",
        ),
    )

    valdi_android_aar(
        name = "{}_android".format(name),
        java_deps = java_deps,
        native_deps = [
            "@valdi//valdi",
        ] + get_suffixed_deps(deps, "_native"),
        additional_assets = [":{}_android_assets".format(name)],
        excluded_class_path_patterns = android_excluded_class_path_patterns,
        so_name = "lib{}.so".format(name),
        tags = ["valdi_android_exported_library"],
    )

    package_name = web_package_name
    if npm_scope:
        package_name = npm_scope + "/" + package_name

    generate_package_json_name = "{}_generate_package_json".format(name)
    expand_template(
        name = generate_package_json_name,
        src = "@valdi//bzl/valdi:package.json.tmpl",
        output = "{}_package.json".format(name),
        substitutions = {
            "${name}": package_name,
            "${version}": npm_version,
        },
    )

    protodecl_to_js_dir(
        name = "{}_protodecl_js".format(web_package_name),
        srcs = get_suffixed_deps(deps, "_web_protodecl"),
    )

    collapse_protodecl_paths(
        name = "{}_protodecl_collapsed".format(web_package_name),
        srcs = [":{}_protodecl_js".format(web_package_name)],
    )

    collapse_native_paths(
        name = "{}_web_native".format(web_package_name),
        srcs = get_suffixed_deps(deps, "_all_web_deps"),
    )

    generate_register_native_modules(
        name = "{}_register_native_modules".format(web_package_name),
        srcs = get_suffixed_deps(deps, "_all_web_deps"),
        package_name = package_name,
        modules = deps,
    )

    native.filegroup(
        name = "{}_glob".format(web_package_name),
        srcs = get_suffixed_deps(deps, "_web_srcs_filegroup") + [
            ":{}_protodecl_collapsed".format(web_package_name),
            ":{}_web_native".format(web_package_name),
            ":{}_register_native_modules".format(web_package_name),
            ":{}".format(generate_package_json_name),  # package.json to root
        ],
    )

    collapse_web_paths(
        name = web_package_name,
        srcs = [":{}_glob".format(web_package_name)],
        package_name = package_name,
        exclude_jsx_global_declaration = web_exclude_jsx_global_declaration,
    )
