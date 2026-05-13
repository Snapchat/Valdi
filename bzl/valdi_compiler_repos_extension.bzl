# Module extension to expose compiler tool repos to the valdi_toolchain sub-module.
#
# For each repo (compiler, pngquant, jscore), first checks if the binary exists
# locally (e.g. from repo-archiver or a local build). If not, downloads from GCS
# using the URLs in open_source_archives.bzl — matching the old WORKSPACE-era
# setup_additional_dependencies() behavior.

load(":open_source_archives.bzl", "ARCHIVES")

SOURCES_FILEGROUP_BUILD_FILE_CONTENT = """
exports_files(glob(["**"]))
filegroup(
    name = "all_files",
    srcs = glob(["**/*"]),
    visibility = ["//visibility:public"],
)
"""

def _compiler_local_or_remote_impl(ctx):
    # Resolve the valdi workspace root via a known label.
    # Works whether @valdi is the main workspace or an external dep.
    valdi_root = ctx.path(Label("@valdi//:MODULE.bazel")).dirname
    target_path = valdi_root.get_child(ctx.attr.target_dir)

    # Check if the local directory has actual content (not just BUILD files).
    # On dev machines, repo-archiver populates these directories with binaries.
    # On CI or fresh clones, only the BUILD files exist — fall through to download.
    check = ctx.execute(["test", "-d", str(target_path)])
    if check.return_code == 0:
        result = ctx.execute(["ls", "-1", str(target_path)])
        file_list = [f for f in result.stdout.strip().split("\n") if f]
        non_build_files = [f for f in file_list if f not in ["BUILD", "BUILD.bazel", "WORKSPACE", "archive-url", ".gitkeep"]]

        if non_build_files:
            # Local binaries exist — symlink everything
            has_build_file = False
            for f in file_list:
                if f in ["BUILD", "BUILD.bazel"]:
                    has_build_file = True
                    break

            for f in file_list:
                ctx.symlink(str(target_path) + "/" + f, f)

            if not has_build_file:
                ctx.file("BUILD.bazel", SOURCES_FILEGROUP_BUILD_FILE_CONTENT)
            return

    # No local binaries — download from GCS archive
    if ctx.attr.archive_url and ctx.attr.archive_hash:
        url = ctx.attr.archive_url.replace("gs://", "https://storage.googleapis.com/")
        ctx.download_and_extract(
            url = url,
            sha256 = ctx.attr.archive_hash,
        )
        ctx.file("BUILD.bazel", SOURCES_FILEGROUP_BUILD_FILE_CONTENT)
    else:
        # No URL configured — create empty repo (analysis succeeds, execution fails)
        ctx.file("BUILD.bazel", SOURCES_FILEGROUP_BUILD_FILE_CONTENT)

_compiler_local_or_remote = repository_rule(
    implementation = _compiler_local_or_remote_impl,
    attrs = {
        "target_dir": attr.string(mandatory = True),
        "archive_url": attr.string(default = ""),
        "archive_hash": attr.string(default = ""),
    },
    local = True,
)

def _get_archive(name):
    """Look up an archive entry by repo name."""
    for _, info in ARCHIVES.items():
        if info["name"] == name:
            return info
    return None

def _valdi_compiler_repos_impl(module_ctx):
    repos = {
        "valdi_compiler_macos": "bin/compiler/macos",
        "valdi_compiler_linux": "bin/compiler/linux",
        "valdi_pngquant_macos": "bin/pngquant/macos",
        "valdi_pngquant_linux": "bin/pngquant/linux",
        "jscore_libs": "third-party/jscore/libs",
    }
    for name, target_dir in repos.items():
        archive = _get_archive(name)
        _compiler_local_or_remote(
            name = name,
            target_dir = target_dir,
            archive_url = archive["url"] if archive else "",
            archive_hash = archive["hash"] if archive else "",
        )

valdi_compiler_repos = module_extension(
    implementation = _valdi_compiler_repos_impl,
)
