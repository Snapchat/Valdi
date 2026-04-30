"""
Constants for the current Node version and major version that is
used by bazel in this WORKSPACE
"""

load("//bzl:additional_dependencies.bzl", "ADDITIONAL_NODE_URLS")

CURRENT_NODE_VERSION = "22.12.0"
CURRENT_NODE_MAJOR_VERSION = "22"

# These repositories specification are necessary until we update to the latest release of
# the @rules_nodejs bazel rule that supports v22.12.10.
EXTRA_NODE_REPOSITORIES = {
    "22.12.0-darwin_arm64": ("node-v22.12.0-darwin-arm64.tar.gz", "node-v22.12.0-darwin-arm64", "293dcc6c2408da21562d135b0412525e381bb6fe150d688edb58fe850d0f3e13"),
    "22.12.0-darwin_amd64": ("node-v22.12.0-darwin-x64.tar.gz", "node-v22.12.0-darwin-x64", "52bc25dd026db7247c3c00439afdb83e95087248267f02d6c1a7250d1f896173"),
    "22.12.0-linux_arm64": ("node-v22.12.0-linux-arm64.tar.xz", "node-v22.12.0-linux-arm64", "8cfd5a8b9afae5a2e0bd86b0148ca31d2589c0ea669c2d0b11c132e35d90ed68"),
    "22.12.0-linux_ppc64le": ("node-v22.12.0-linux-ppc64le.tar.xz", "node-v22.12.0-linux-ppc64le", "199a606ba1ee86cce6d6b369c71f9d00873d2836a6662592afc3b6a5923e2004"),
    "22.12.0-linux_s390x": ("node-v22.12.0-linux-s390x.tar.xz", "node-v22.12.0-linux-s390x", "9b517f8006eb4b451d40c461cbe64f93c6455566dbe2613387ab02412bc06d35"),
    "22.12.0-linux_amd64": ("node-v22.12.0-linux-x64.tar.xz", "node-v22.12.0-linux-x64", "22982235e1b71fa8850f82edd09cdae7e3f32df1764a9ec298c72d25ef2c164f"),
    "22.12.0-windows_amd64": ("node-v22.12.0-win-x64.zip", "node-v22.12.0-win-x64", "2b8f2256382f97ad51e29ff71f702961af466c4616393f767455501e6aece9b8"),
}

# URLS in priority order of where to download nodejs from.
# We keep nodejs.org in the list for local builds which can't see the
# jenkins mirror; it will fallback with a warning.
NODE_URLS = ADDITIONAL_NODE_URLS + [
    "https://nodejs.org.mirror.proxy.local/dist/v{version}/{filename}",  # Proxy mirror
    "https://nodejs.org/dist/v{version}/{filename}",  # Public fallback
]
