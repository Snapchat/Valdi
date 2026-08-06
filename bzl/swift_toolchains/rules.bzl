"""Public entry points for the swift_toolchains package.

Usage pattern -- see //bzl/swift_toolchains.bzl for the declarative list
and //bzlmod/swift_toolchains_extension.bzl for the module extension glue.
Mirrors @rules_android//contrib_rules/remote_android_sdk_repository:repositories.bzl.
"""

load(
    ":remote_swift_toolchain_repository.bzl",
    _remote_swift_toolchain_repository = "remote_swift_toolchain_repository",
)

# Re-export so consumers load everything from //bzl/swift_toolchains:rules.bzl.
remote_swift_toolchain_repository = _remote_swift_toolchain_repository

def remote_swift_toolchain(
        name,
        url,
        sha256,
        strip_prefix,
        arch,
        version,
        swift_relative_path = "usr/bin/swift"):
    """Dict-returning helper describing one Swift toolchain entry.

    Use this to populate the SWIFT_TOOLCHAINS list in
    //bzl/swift_toolchains.bzl. Mirrors the `remote_android_sdk(...)`
    helper used for Android SDK entries in //bzl/android_toolchains.bzl.

    Args:
        name: Repository name. Also used for the toolchain label
            (`@<name>//:rules_swift_toolchain`).
        url: HTTP URL of the archive.
        sha256: SHA-256 of the archive.
        strip_prefix: Top-level directory inside the archive to strip.
        arch: Architecture string as used by rules_swift's swift_toolchain
            (e.g. "x86_64", "aarch64"). Matches the "{arch}" segment in
            the toolchain's `lib/swift/linux/{arch}` layout.
        version: Swift release version (e.g. "5.10.1"). rules_swift 4.x gates
            features on it; ignored on 3.x, which has no such attribute.
        swift_relative_path: Path to the swift binary inside the archive
            after `strip_prefix` is applied. Defaults to "usr/bin/swift",
            which is the layout swift.org ships.

    Returns:
        A dict consumed by `make_swift_toolchains`.
    """
    return dict(
        name = name,
        url = url,
        sha256 = sha256,
        strip_prefix = strip_prefix,
        arch = arch,
        version = version,
        swift_relative_path = swift_relative_path,
    )

def make_swift_toolchains(toolchains):
    """Instantiates each configured Swift toolchain repo.

    Intended to be called from a bzlmod module extension (see
    //bzlmod/swift_toolchains_extension.bzl).

    Args:
        toolchains: List of dicts as returned by `remote_swift_toolchain`.
    """
    for entry in toolchains:
        _remote_swift_toolchain_repository(
            name = entry["name"],
            url = entry["url"],
            sha256 = entry["sha256"],
            strip_prefix = entry["strip_prefix"],
            arch = entry["arch"],
            version = entry["version"],
            swift_relative_path = entry["swift_relative_path"],
        )
