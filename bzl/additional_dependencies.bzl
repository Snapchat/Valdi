# Additional Node.js download mirrors for internal Snap builds
# This gets prepended to the standard mirrors in nodejs_info.bzl
ADDITIONAL_NODE_URLS = []

def setup_additional_dependencies(bzlmod = False):
    # No additional repo definitions needed for open-source builds.
    # Compiler builds from source (use_local_compiler=true).
    # JSCore prebuilt is not used (QuickJS is the default on Linux; iOS/macOS use system framework).
    # The valdi_compiler_repos module extension handles any remaining repo setup.
    pass
