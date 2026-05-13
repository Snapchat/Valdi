"""Swift Package Manager dependencies for the Valdi compiler.

The Valdi compiler (//compiler/compiler:local_valdi_compiler, a swift_binary)
mirrors the deps declared in compiler/compiler/Compiler/Package.swift.

"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Patches applied after extraction. Keyed by repo name.
_PATCHES = {
    # marmelroy/Zip passes an optional `UnsafeMutablePointer<FILE>?` directly to
    # `fwrite` which Swift on Linux rejects (the clang importer types `FILE*` as
    # nullable there). Force-unwrap is safe under the surrounding nil check.
    "zip_lib": ["@valdi//third-party/swift-packages/patches:zip_linux_fwrite.patch"],
}

_SWIFT_COMPILER_PACKAGES = [
    ("swxmlhash", "drmohundro/SWXMLHash", "a853604c9e9a83ad9954c7e3d2a565273982471f", "SWXMLHash.BUILD"),
    ("swift_protobuf", "apple/swift-protobuf", "65e8f29b2d63c4e38e736b25c27b83e012159be8", "swift-protobuf.BUILD"),
    ("legible_error", "mxcl/LegibleError", "bc596702d7ff618c3f90ba480eeb48b3e83a2fbe", "LegibleError.BUILD"),
    ("chalk", "mxcl/Chalk", "a7f58e47a08ca5a84f73acc4bcf6c2c19d990609", "Chalk.BUILD"),
    ("yams", "jpsim/Yams", "0d9ee7ea8c4ebd4a489ad7a73d5c6cad55d6fed3", "Yams.BUILD"),
    ("swift_backtrace", "swift-server/swift-backtrace", "f25620d5d05e2f1ba27154b40cafea2b67566956", "swift-backtrace.BUILD"),
    ("swift_argument_parser", "apple/swift-argument-parser", "46989693916f56d1186bd59ac15124caef896560", "swift-argument-parser.BUILD"),
    ("swiftsoup", "scinfu/SwiftSoup", "f83c097597094a04124eb6e0d1e894d24129af87", "SwiftSoup.BUILD"),
    ("swift_crypto", "apple/swift-crypto", "cc76b894169a3c86b71bac10c78a4db6beb7a9ad", "swift-crypto.BUILD"),
    ("zip_lib", "marmelroy/Zip", "67fa55813b9e7b3b9acee9c0ae501def28746d76", "Zip.BUILD"),
    ("swift_collections", "apple/swift-collections", "94cf62b3ba8d4bed62680a282d4c25f9c63c2efb", "swift-collections.BUILD"),
]

VALDI_COMPILER_SWIFT_REPO_NAMES = [entry[0] for entry in _SWIFT_COMPILER_PACKAGES]

def setup_valdi_compiler_swift_deps():
    """Declares http_archive for every Swift package needed to build the Valdi compiler."""
    for repo_name, github_path, revision, build_file in _SWIFT_COMPILER_PACKAGES:
        repo_basename = github_path.split("/")[1]
        http_archive(
            name = repo_name,
            url = "https://github.com/{}/archive/{}.zip".format(github_path, revision),
            strip_prefix = "{}-{}".format(repo_basename, revision),
            build_file = "@valdi//third-party/swift-packages:{}".format(build_file),
            patches = _PATCHES.get(repo_name, []),
            patch_args = ["-p1"],
        )

def _valdi_compiler_swift_deps_impl(_module_ctx):
    setup_valdi_compiler_swift_deps()

valdi_compiler_swift_deps = module_extension(
    implementation = _valdi_compiler_swift_deps_impl,
)
