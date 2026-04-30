load("@aspect_rules_esbuild//esbuild:repositories.bzl", "LATEST_ESBUILD_VERSION", "esbuild_register_toolchains")
load("@aspect_rules_ts//ts:repositories.bzl", "rules_ts_dependencies")
load("@bazel_features//:deps.bzl", "bazel_features_deps")
load(
    "@build_bazel_apple_support//lib:repositories.bzl",
    "apple_support_dependencies",
)
load(
    "@build_bazel_rules_apple//apple:repositories.bzl",
    "apple_rules_dependencies",
)
load(
    "@build_bazel_rules_swift//swift:repositories.bzl",
    "swift_rules_dependencies",
)
load("@rules_android//:defs.bzl", "rules_android_workspace")
load("@rules_android_ndk//:rules.bzl", "android_ndk_repository")
load("@rules_cc//cc:repositories.bzl", "rules_cc_dependencies")
load("@rules_fuzzing//fuzzing:init.bzl", "rules_fuzzing_init")
load("@rules_fuzzing//fuzzing:repositories.bzl", "rules_fuzzing_dependencies")
load("@rules_jvm_external//:defs.bzl", "maven_install")
load("@rules_jvm_external//:repositories.bzl", "rules_jvm_external_deps")
load("@rules_kotlin//kotlin:core.bzl", "kt_register_toolchains")
load("@rules_nodejs//nodejs:repositories.bzl", "nodejs_register_toolchains")
load(
    "@rules_shell//shell:repositories.bzl",
    "rules_shell_dependencies",
    "rules_shell_toolchains",
)
load(
    "@rules_xcodeproj//xcodeproj:repositories.bzl",
    "xcodeproj_rules_dependencies",
)
load("@toolchains_llvm//toolchain:deps.bzl", "bazel_toolchain_dependencies")
load("@toolchains_llvm//toolchain:rules.bzl", "llvm_toolchain")
load("@valdi//bzl/common:nodejs_info.bzl", "CURRENT_NODE_VERSION", "EXTRA_NODE_REPOSITORIES", "NODE_URLS")
load("@valdi//bzl/valdi:toolchains.bzl", "register_valdi_toolchains")
load("@valdi//bzl/valdi/npm:repositories.bzl", "valdi_repositories")

# WORKSPACE files are very limited in what they can do. Ideally, platform-specific command line
# flags would be read directly from the WORKSPACE file to determine platform-specific depenencies.
#
# Unfortunately, command line flags are accessible in Bazel through the select() function, which
# is only available in BUILD files and constructs during Bazel's analysis phase.
# WORKSPACE is parsed before that, during the loading phase, so it cannot access select()
#
# WORKSPACES can load repositories and environment variables during the loading phase, so this helper
# works around phase limitations by creating a wrapper "repository" that stores the environment
# variable value. Values stored in this "repository" can then be loaded and used during any phase.
#
# One final quirk is that Bazel requires all repositories to have a BUILD file.
# For this wrapper "repository" to be considered a valid target, a dummy BUILD file is created.
#
def _platform_dependency_rule_impl(repository_ctx):
    config = repository_ctx.os.environ.get("VALDI_PLATFORM_DEPENDENCIES", "")

    # all bazel repos require a BUILD file, make one for this "repo"
    repository_ctx.file("BUILD")

    # this "repo" will have a single file with the environment flag value saved in it
    repository_ctx.file("target_platform.bzl", content = """VALDI_PLATFORM_DEPENDENCIES = {}""".format(repr(config)))

platform_dependency_rule = repository_rule(
    implementation = _platform_dependency_rule_impl,
    environ = ["VALDI_PLATFORM_DEPENDENCIES"],
)

def _register_android_deps():
    rules_android_workspace()

    native.register_toolchains(
        "@rules_android//toolchains/android:android_default_toolchain",
        "@rules_android//toolchains/android_sdk:android_sdk_tools",
    )

    android_ndk_repository(name = "androidndk")

    native.register_toolchains("@androidndk//:all")

    kt_register_toolchains()

    maven_install(
        name = "android_mvn",
        aar_import_bzl_label = "@rules_android//rules:rules.bzl",
        artifacts = [
            "androidx.annotation:annotation:1.1.0",
            "androidx.appcompat:appcompat:1.2.0",
            "androidx.appcompat:appcompat-resources:1.2.0",
            "androidx.collection:collection:1.1.0",
            "androidx.constraintlayout:constraintlayout:2.1.4",
            "androidx.customview:customview:1.1.0",
            "androidx.dynamicanimation:dynamicanimation:1.0.0",
            "androidx.fragment:fragment:1.1.0",
            "androidx.interpolator:interpolator:1.0.0",
            "androidx.lifecycle:lifecycle-common:2.2.0",
            "androidx.lifecycle:lifecycle-process:2.2.0",
            "androidx.lifecycle:lifecycle-viewmodel:2.2.0",
            "androidx.recyclerview:recyclerview:1.2.1",
            "androidx.test:core:1.5.0",
            "androidx.test.ext:junit:1.1.3",
            "androidx.test:monitor:1.5.0",
            "androidx.test:runner:1.5.0",
            "com.google.android.material:material:1.2.0",
            "com.google.code.findbugs:jsr305:3.0.2",
            "com.google.code.gson:gson:2.8.6",
            "com.google.protobuf.nano:protobuf-javanano:3.1.0",
            "com.jakewharton.timber:timber:4.7.1",
            "com.squareup.leakcanary:leakcanary-android:2.10",
            "com.squareup.okhttp3:okhttp:4.9.0",
            "com.squareup.okio:okio:2.10.0",
            "io.reactivex.rxjava3:rxandroid:3.0.0",
            "io.reactivex.rxjava3:rxjava:3.1.0",
            "io.reactivex.rxjava3:rxkotlin:3.0.0",
            "javax.inject:javax.inject:1",
            "junit:junit:4.13.2",
            "org.junit.jupiter:junit-jupiter-api:5.9.3",
            "org.junit.jupiter:junit-jupiter-engine:5.9.3",
            "org.junit.jupiter:junit-jupiter-params:5.9.3",
            "org.junit.platform:junit-platform-console:1.9.2",
            "org.junit.platform:junit-platform-launcher:1.9.2",
            "org.junit.platform:junit-platform-reporting:1.9.2",
            "org.junit.vintage:junit-vintage-engine:5.9.3",
            "org.robolectric:android-all:10-robolectric-5803371",
            "org.robolectric:annotations:4.10.3",
            "org.robolectric:robolectric:4.10.3",
        ],
        repositories = [
            "https://repo1.maven.org/maven2",
            "https://maven.google.com",
        ],
        use_starlark_android_rules = True,
        # Let's force the pinned version:
        # https://github.com/bazelbuild/rules_jvm_external#resolving-user-specified-and-transitive-dependency-version-conflicts
        version_conflict_policy = "pinned",
    )

def _register_apple_deps():
    xcodeproj_rules_dependencies(ignore_version_differences = True)
    rules_shell_dependencies()
    rules_shell_toolchains()
    apple_rules_dependencies(ignore_version_differences = True)
    apple_support_dependencies()
    swift_rules_dependencies()

def _register_nodejs_deps():
    rules_fuzzing_dependencies()
    rules_fuzzing_init()
    nodejs_register_toolchains(
        name = "nodejs",
        node_repositories = EXTRA_NODE_REPOSITORIES,
        node_urls = NODE_URLS,
        node_version = CURRENT_NODE_VERSION,
    )

def valdi_initialize_workspace(target_platform = ""):
    # platform strings expected to match BUILD_FLAGS in constants.ts
    is_apple = (target_platform == "ios") or (target_platform == "macos")
    if is_apple or target_platform == "":
        _register_apple_deps()

    rules_jvm_external_deps()
    rules_cc_dependencies()
    llvm_toolchain(
        name = "llvm_toolchain",
        llvm_version = "16.0.0",
    )

    # Required for the objc_library targets. Roughly, the dependency chain is:
    # valdi_core > valdi_compiler_toolbox > valdi_protobuf > utils_core_cc
    # then objc_library from djinni or jscore, etc. depending on build target
    apple_support_dependencies()

    # platform string expected to match BUILD_FLAGS in constants.ts
    if target_platform == "android" or target_platform == "":
        _register_android_deps()

    bazel_features_deps()

    # This is loadbearing for now, removing this breaks copy_to_directory
    # TODO, figure out why
    esbuild_register_toolchains(
        name = "esbuild",
        esbuild_version = LATEST_ESBUILD_VERSION,
    )

    register_valdi_toolchains()
    _register_nodejs_deps()
    bazel_toolchain_dependencies()

    rules_ts_dependencies(
        ts_version = "5.1.3",
    )

    valdi_repositories()
