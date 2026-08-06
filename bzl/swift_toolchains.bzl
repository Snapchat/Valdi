"""Declarative list of Swift toolchains registered against rules_swift.

Purpose: override rules_swift's Linux autoconfig, which computes its
`root` from `repository_ctx.which("swiftc").dirname.dirname` at repo-
fetch time. When the host's `swiftc` is a swiftly shim, the resolved
root lands on the shim's metadata dir instead of the real toolchain,
and every `swift_binary` link fails with a missing `swiftrt.o`.

Registering a downloaded Swift archive with explicit `root` fixes that.
macOS keeps using rules_swift's autoconfigured xcode-toolchain (xcrun
isn't swiftly-shimmed, so autoconfig works there).

Only x86_64 Linux is registered: we don't have arm64 Linux builders,
so fetching that toolchain archive would be wasted cycles. Add an
arm64 entry here (and matching `use_repo` / `register_toolchains` lines
in MODULE.bazel) if that changes.
"""

load("//bzl/swift_toolchains:rules.bzl", "remote_swift_toolchain")

# Swift release downloads live at https://swift.org/download/. Archives
# are versioned by release; sha256 values are published there.
_SWIFT_VERSION = "5.10.1"

SWIFT_TOOLCHAINS = [
    remote_swift_toolchain(
        name = "swift_toolchain_linux_x86_64",
        url = (
            "https://download.swift.org/swift-{v}-release/ubuntu2204/" +
            "swift-{v}-RELEASE/swift-{v}-RELEASE-ubuntu22.04.tar.gz"
        ).format(v = _SWIFT_VERSION),
        # TODO: pin once the first fetch reports the sha256.
        sha256 = "",
        strip_prefix = "swift-{}-RELEASE-ubuntu22.04".format(_SWIFT_VERSION),
        arch = "x86_64",
        version = _SWIFT_VERSION,
    ),
]
