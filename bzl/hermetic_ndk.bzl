# Module extension that downloads the Android NDK hermetically so external
# contributors don't need a locally installed NDK.
#
# Delegates to the upstream android_ndk_repository rule from rules_android_ndk,
# pointing it at the downloaded NDK directory.

_NDK_VERSION = "r28"

_NDK_URLS = {
    "linux": "https://dl.google.com/android/repository/android-ndk-{version}-linux.zip",
    "darwin": "https://dl.google.com/android/repository/android-ndk-{version}-darwin.zip",
}

_NDK_SHA256 = {
    "linux": "a186b67e8810cb949514925e4f7a2255548fb55f5e9b0824a6430d012c1b695b",
    "darwin": "19b16241e8e8d4c8e4f3729b8a0a625dd240394e1f1cd072596df891317e22a9",
}

_NDK_STRIP_PREFIX = "android-ndk-{version}"

def _hermetic_ndk_impl(ctx):
    """Download the NDK and set up the android_ndk_repository."""

    # Determine host platform
    if ctx.os.name == "linux":
        platform = "linux"
    elif ctx.os.name == "mac os x":
        platform = "darwin"
    else:
        fail("Unsupported host OS for hermetic NDK: " + ctx.os.name)

    url = _NDK_URLS[platform].format(version = _NDK_VERSION)
    sha256 = _NDK_SHA256[platform]
    strip_prefix = _NDK_STRIP_PREFIX.format(version = _NDK_VERSION)

    # Download and extract the NDK into a subdirectory so we can generate
    # BUILD files at the repo root without conflicting with NDK contents.
    ctx.download_and_extract(
        url = url,
        sha256 = sha256,
        stripPrefix = strip_prefix,
        output = "ndk",
    )

    ndk_path = str(ctx.path("ndk"))

    # --- Reproduce android_ndk_repository logic ---
    # (Mirrors rules_android_ndk/rules.bzl so the result is identical to
    #  what the non-hermetic extension produces.)

    if platform == "linux":
        clang_directory = "toolchains/llvm/prebuilt/linux-x86_64"
    else:
        clang_directory = "toolchains/llvm/prebuilt/darwin-x86_64"

    sysroot_directory = "%s/sysroot" % clang_directory
    executable_extension = ""

    # Create symlinks from ndk/<subpath> into the repo root
    _create_symlinks(ctx, ndk_path, clang_directory, sysroot_directory)

    api_level = ctx.attr.api_level or 31

    result = ctx.execute([clang_directory + "/bin/clang", "--print-resource-dir"])
    if result.return_code != 0:
        fail("Failed to execute clang: %s" % result.stderr)
    stdout = result.stdout.strip()
    clang_resource_directory = stdout.split(clang_directory)[-1].strip("/")

    # Use rules_android_ndk's templates to generate BUILD files
    repository_name = ctx.attr._build.workspace_name

    ctx.template(
        "BUILD.bazel",
        ctx.attr._template_ndk_root,
        {"{clang_directory}": clang_directory},
        executable = False,
    )

    ctx.template(
        "target_systems.bzl",
        ctx.attr._template_target_systems,
        {},
        executable = False,
    )

    ctx.template(
        "%s/BUILD.bazel" % clang_directory,
        ctx.attr._template_ndk_clang,
        {
            "{repository_name}": repository_name,
            "{api_level}": str(api_level),
            "{clang_resource_directory}": clang_resource_directory,
            "{sysroot_directory}": sysroot_directory,
            "{executable_extension}": executable_extension,
        },
        executable = False,
    )

    ctx.template(
        "%s/BUILD.bazel" % sysroot_directory,
        ctx.attr._template_ndk_sysroot,
        {"{api_level}": str(api_level)},
        executable = False,
    )

def _create_symlinks(ctx, ndk_path, clang_directory, sysroot_directory):
    if not ndk_path.endswith("/"):
        ndk_path = ndk_path + "/"

    prefix_len = len(ndk_path)

    for p in ctx.path(ndk_path + clang_directory).readdir():
        repo_relative_path = str(p)[prefix_len:]
        if repo_relative_path != sysroot_directory:
            ctx.symlink(p, repo_relative_path)

    for p in ctx.path(ndk_path + sysroot_directory).readdir():
        repo_relative_path = str(p)[prefix_len:]
        ctx.symlink(p, repo_relative_path)

    ctx.symlink(ctx.path("ndk/sources"), "sources")

    # The original android_ndk_repository creates ndk/sources as a symlink
    # (see TODO(#32) in rules_android_ndk/rules.bzl). Since we extract the
    # NDK into ndk/, ndk/sources already exists as a real directory.

_hermetic_ndk = repository_rule(
    implementation = _hermetic_ndk_impl,
    attrs = {
        "api_level": attr.int(default = 31),
        "_build": attr.label(default = "@rules_android_ndk//:BUILD", allow_single_file = True),
        "_template_ndk_root": attr.label(default = "@rules_android_ndk//:BUILD.ndk_root.tpl", allow_single_file = True),
        "_template_target_systems": attr.label(default = "@rules_android_ndk//:target_systems.bzl.tpl", allow_single_file = True),
        "_template_ndk_clang": attr.label(default = "@rules_android_ndk//:BUILD.ndk_clang.tpl", allow_single_file = True),
        "_template_ndk_sysroot": attr.label(default = "@rules_android_ndk//:BUILD.ndk_sysroot.tpl", allow_single_file = True),
    },
)

def _hermetic_ndk_extension_impl(module_ctx):
    root_modules = [m for m in module_ctx.modules if m.is_root and m.tags.configure]

    api_level = 31
    if root_modules and root_modules[0].tags.configure:
        api_level = root_modules[0].tags.configure[0].api_level

    _hermetic_ndk(
        name = "androidndk",
        api_level = api_level,
    )

hermetic_ndk_extension = module_extension(
    implementation = _hermetic_ndk_extension_impl,
    tag_classes = {
        "configure": tag_class(attrs = {
            "api_level": attr.int(default = 31),
        }),
    },
)
