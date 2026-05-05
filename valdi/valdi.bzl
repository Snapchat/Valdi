load("@android_macros//:android.bzl", "aar_import")
load("@valdi//bzl:valdi_library.bzl", "COMMON_COMPILE_FLAGS")
load("@valdi//bzl/android:filter_jar.bzl", "filter_jar")
load("@valdi//bzl/android:package_aar.bzl", "package_aar")
load("@valdi//bzl/android:platform_transition.bzl", "android_aar_platforms")

COMPILER_FLAGS = [
    # '-Os', # Uncomment to enable optimizations
    "-Wno-c11-extensions",
    "-Wno-c99-extensions",
    "-Wno-gnu-conditional-omitted-operand",
    "-Wno-gnu-statement-expression",
    "-Wno-implicit-function-declaration",
    "-Wno-keyword-macro",
    "-Wno-pedantic",
    "-Wno-vla-extension",
    "-Wno-zero-length-array",
]

ANDROIDX_RUNTIME_LIBRARIES = [
    "@android_mvn//:androidx_annotation_annotation",
    "@android_mvn//:androidx_appcompat_appcompat",
    "@android_mvn//:androidx_core_core",
    "@android_mvn//:androidx_dynamicanimation_dynamicanimation",
    "@android_mvn//:androidx_lifecycle_lifecycle_common",
    "@android_mvn//:androidx_lifecycle_lifecycle_process",
]

# Creates a set of rules to build an .so that can contain
# the Valdi runtime.
def valdi_android_aar(
        name,
        so_name,
        native_deps,
        excluded_class_path_patterns = [],
        additional_assets = [],
        tags = [],
        java_deps = []):
    jar_name = "{}_jar".format(name)
    import_name = "{}_import".format(name)

    # All of the native (cpp) sources
    native.cc_binary(
        name = so_name,
        additional_linker_inputs = [
            "@valdi//src:android_exports.lst",
            "@valdi//src:build_id_note_symbols.ld",
        ],
        linkopts = [
            "-fuse-ld=lld",
            "--no-undefined",
            "-landroid",
            "-ldl",
            "-llog",
            "-lm",
            "-lz",
            "-Wl,--version-script=$(location @valdi//src:android_exports.lst) $(location @valdi//src:build_id_note_symbols.ld)",
        ],
        linkshared = True,
        target_compatible_with = [
            "@platforms//os:android",
        ],
        visibility = ["//visibility:public"],
        deps = native_deps,
    )

    if len(excluded_class_path_patterns) > 0 and len(java_deps) > 0:
        jar_unfilterd_name = "{}_unfiltered".format(jar_name)
        native.java_binary(
            name = jar_unfilterd_name,
            deploy_env = [],
            runtime_deps = java_deps,
            # Parameter added for open source compatibility
            main_class = "dummy",
        )

        filter_jar(
            name = jar_name,
            jar = ":{}_deploy.jar".format(jar_unfilterd_name),
            excluded_class_path_patterns = excluded_class_path_patterns,
        )
    else:
        # All of the Java Sources
        native.java_binary(
            name = jar_name,
            deploy_env = [],
            runtime_deps = java_deps,
            # Parameter added for open source compatibility
            main_class = "dummy",
        )

        jar_name = "{}_deploy.jar".format(jar_name)

    # A fat aar that contains everything
    package_aar(
        name = name,
        aar = "@valdi//valdi:valdi_android_support.aar",
        classes_jar = ":{}".format(jar_name),
        native_libs = [":{}".format(so_name)],
        platforms = android_aar_platforms(),
        additional_assets = additional_assets,
        tags = tags,
        proguard_spec = "@valdi//src:client_proguard-rules",
        visibility = ["//visibility:public"],
    )

    # Wrap the aar so it can be used in a bazel build
    aar_import(
        name = import_name,
        aar = ":{}".format(name),
        visibility = ["//visibility:public"],
        deps = ANDROIDX_RUNTIME_LIBRARIES,
    )

def valdi_test(name, srcs = [], hdrs = {}, deps = [], data = None):
    lib_name = "{}_lib".format(name)
    native.cc_library(
        name = lib_name,
        testonly = 1,
        alwayslink = True,
        srcs = srcs,
        hdrs = hdrs,
        copts = COMMON_COMPILE_FLAGS + COMPILER_FLAGS,
        data = data,
        deps = ([
            "@gtest//:gtest",
        ] + deps),
    )

    native.cc_test(
        name = name,
        linkstatic = True,
        visibility = ["//visibility:public"],
        deps = [
            ":{}".format(lib_name),
            "@gtest//:gtest_main",
        ],
    )
