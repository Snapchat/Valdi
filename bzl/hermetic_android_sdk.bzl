# Module extension that downloads the Android SDK hermetically so external
# contributors don't need a locally installed SDK.
#
# Downloads platforms, build-tools, and platform-tools from dl.google.com,
# then generates the same BUILD file as rules_android's android_sdk_repository.

_API_LEVEL = 36
_BUILD_TOOLS_VERSION = "34.0.0"

_PLATFORM_URL = "https://dl.google.com/android/repository/platform-36_r02.zip"
_PLATFORM_SHA256 = "37607369a28c5b640b3a7998868d45898ebcb777565a0e85f9acf36f29631d2e"
_PLATFORM_STRIP_PREFIX = "android-36"

_BUILD_TOOLS_URLS = {
    "linux": "https://dl.google.com/android/repository/build-tools_r34-linux.zip",
    "darwin": "https://dl.google.com/android/repository/build-tools_r34-macosx.zip",
}
_BUILD_TOOLS_SHA256 = {
    "linux": "e858c4b60069d0431051b225d384413b1643e1289b00a4825aed347f25bd510f",
    "darwin": "76bf668fe037b1a69197e298ddae5633d4d7f0f41af7ed17e537c80c1ed8a6f3",
}
_BUILD_TOOLS_STRIP_PREFIX = "android-14"

_PLATFORM_TOOLS_URLS = {
    "linux": "https://dl.google.com/android/repository/platform-tools_r37.0.0-linux.zip",
    "darwin": "https://dl.google.com/android/repository/platform-tools_r37.0.0-darwin.zip",
}
_PLATFORM_TOOLS_SHA256 = {
    "linux": "198ae156ab285fa555987219af237b31102fefe8b9d2bc274708a8d4f2865a07",
    "darwin": "094a1395683c509fd4d48667da0d8b5ef4d42b2abfcd29f2e8149e2f989357c7",
}

def _hermetic_android_sdk_impl(ctx):
    if ctx.os.name == "linux":
        platform = "linux"
    elif ctx.os.name == "mac os x":
        platform = "darwin"
    else:
        fail("Unsupported host OS for hermetic Android SDK: " + ctx.os.name)

    api_level = ctx.attr.api_level
    build_tools_version = ctx.attr.build_tools_version

    # Download platform (same zip for all host OSes)
    ctx.download_and_extract(
        url = _PLATFORM_URL,
        sha256 = _PLATFORM_SHA256,
        stripPrefix = _PLATFORM_STRIP_PREFIX,
        output = "platforms/android-%d" % api_level,
    )

    # Download build-tools (host-OS-specific)
    ctx.download_and_extract(
        url = _BUILD_TOOLS_URLS[platform],
        sha256 = _BUILD_TOOLS_SHA256[platform],
        stripPrefix = _BUILD_TOOLS_STRIP_PREFIX,
        output = "build-tools/%s" % build_tools_version,
    )

    # Download platform-tools (host-OS-specific)
    ctx.download_and_extract(
        url = _PLATFORM_TOOLS_URLS[platform],
        sha256 = _PLATFORM_TOOLS_SHA256[platform],
        stripPrefix = "platform-tools",
        output = "platform-tools",
    )

    # Create empty directories that the template references via glob
    ctx.file("emulator/.empty", "")
    ctx.file("system-images/.empty", "")
    ctx.file("extras/.empty", "")

    # The dummy SDK toolchain in helper.bzl references dummy.jar
    ctx.file("dummy.jar", "")

    # Use rules_android's template and helper to generate BUILD.bazel
    ctx.symlink(ctx.attr._sdk_helper, "helper.bzl")
    ctx.template(
        "BUILD.bazel",
        ctx.attr._sdk_template,
        {
            "__repository_name__": ctx.name,
            "__build_tools_version__": build_tools_version,
            "__build_tools_directory__": build_tools_version,
            "__api_levels__": str(api_level),
            "__default_api_level__": str(api_level),
            "__system_image_dirs__": "",
        },
        executable = False,
    )

_hermetic_android_sdk = repository_rule(
    implementation = _hermetic_android_sdk_impl,
    attrs = {
        "api_level": attr.int(default = _API_LEVEL),
        "build_tools_version": attr.string(default = _BUILD_TOOLS_VERSION),
        "_sdk_template": attr.label(
            default = "@rules_android//rules/android_sdk_repository:template.bzl",
            allow_single_file = True,
        ),
        "_sdk_helper": attr.label(
            default = "@rules_android//rules/android_sdk_repository:helper.bzl",
            allow_single_file = True,
        ),
    },
)

def _hermetic_android_sdk_extension_impl(module_ctx):
    root_modules = [m for m in module_ctx.modules if m.is_root and m.tags.configure]

    api_level = _API_LEVEL
    build_tools_version = _BUILD_TOOLS_VERSION
    if root_modules and root_modules[0].tags.configure:
        tag = root_modules[0].tags.configure[0]
        api_level = tag.api_level
        build_tools_version = tag.build_tools_version

    _hermetic_android_sdk(
        name = "androidsdk",
        api_level = api_level,
        build_tools_version = build_tools_version,
    )

hermetic_android_sdk_extension = module_extension(
    implementation = _hermetic_android_sdk_extension_impl,
    tag_classes = {
        "configure": tag_class(attrs = {
            "api_level": attr.int(default = _API_LEVEL),
            "build_tools_version": attr.string(default = _BUILD_TOOLS_VERSION),
        }),
    },
)
