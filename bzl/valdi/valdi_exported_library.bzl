load("@aspect_rules_js//npm:defs.bzl", "npm_package")
load("@build_bazel_rules_apple//apple:apple.bzl", "apple_xcframework")
load("//bzl:expand_template.bzl", "expand_template")
load("//bzl/valdi:rewrite_hdrs.bzl", "rewrite_hdrs")
load("//bzl/valdi:suffixed_deps.bzl", "get_suffixed_deps")
load("//bzl/valdi:valdi_collapse_web_paths.bzl", "collapse_native_paths", "collapse_web_paths")
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
        android_excluded_class_path_patterns = DEFAULT_ANDROID_EXCLUDED_CLASS_PATTERNS,
        java_deps = DEFAULT_JAVA_DEPS,
        web_package_name = None,
        npm_scope = "",
        npm_version = "1.0.0"):
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

    apple_xcframework(
        name = "{}_ios".format(name),
        bundle_name = ios_bundle_name,
        deps = ["@valdi//valdi"] + get_suffixed_deps(deps, "_objc"),
        infoplists = [
            "@valdi//bzl/valdi:Info.plist",
        ],
        minimum_os_versions = {
            "ios": "12.0",
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

    package_name = web_package_name
    if npm_scope:
        package_name = npm_scope + "/" + package_name

    expand_template(
        name = "generate_package_json",
        src = "@valdi//bzl/valdi:package.json.tmpl",
        output = "package.json",
        substitutions = {
            "${name}": package_name,
            "${version}": npm_version,
            "${files}": "{}_tree".format(web_package_name),
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

    native.filegroup(
        name = "{}_glob".format(web_package_name),
        srcs = get_suffixed_deps(deps, "_web_srcs_filegroup") + [
            ":{}_protodecl_collapsed".format(web_package_name),
            ":{}_web_native".format(web_package_name),
            ":generate_package_json",  # package.json to root
        ],
    )

    collapse_web_paths(
        name = "{}_tree".format(web_package_name),
        srcs = [":{}_glob".format(web_package_name)],
    )

    npm_package(
        name = web_package_name,
        srcs = [":generate_package_json", ":{}_tree".format(web_package_name)],
    )
