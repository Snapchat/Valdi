load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("//bzl:nested_repository.bzl", "nested_repository")

def local_or_nested_repository(workspace_root, name, path):
    if workspace_root:
        native.local_repository(
            name = name,
            path = workspace_root + "/" + path,
        )
    else:
        nested_repository(
            name = name,
            source_repo = "valdi",
            target_dir = path,
        )

def setup_dependencies(workspace_root = None):
    native.android_sdk_repository(
        name = "androidsdk",
        api_level = 35,  # The API version for Android compileSdk
        build_tools_version = "34.0.0",
    )

    http_archive(
        name = "toolchains_llvm",
        canonical_id = "v1.3.0",
        sha256 = "d3c255b2ceec9eaebb6b5a44c904a48429b8dcb71e630de2f103e7b4aab9f073",
        strip_prefix = "toolchains_llvm-v1.3.0",
        url = "https://github.com/bazel-contrib/toolchains_llvm/releases/download/v1.3.0/toolchains_llvm-v1.3.0.tar.gz",
    )

    http_archive(
        name = "com_google_protobuf",
        sha256 = "da288bf1daa6c04d03a9051781caa52aceb9163586bff9aa6cfb12f69b9395aa",
        strip_prefix = "protobuf-27.0",
        url = "https://github.com/protocolbuffers/protobuf/releases/download/v27.0/protobuf-27.0.tar.gz",
    )

    http_archive(
        name = "rules_cc",
        sha256 = "abc605dd850f813bb37004b77db20106a19311a96b2da1c92b789da529d28fe1",
        strip_prefix = "rules_cc-0.0.17",
        urls = ["https://github.com/bazelbuild/rules_cc/releases/download/0.0.17/rules_cc-0.0.17.tar.gz"],
    )

    http_archive(
        name = "rules_android_ndk",
        sha256 = "89bf5012567a5bade4c78eac5ac56c336695c3bfd281a9b0894ff6605328d2d5",
        strip_prefix = "rules_android_ndk-0.1.3",
        url = "https://github.com/bazelbuild/rules_android_ndk/releases/download/v0.1.3/rules_android_ndk-v0.1.3.tar.gz",
        patches = [
            "@valdi//third-party/rules_android_ndk/patches:expose_bins.patch",
        ],
    )

    http_archive(
        name = "bazel_skylib",
        sha256 = "af87959afe497dc8dfd4c6cb66e1279cb98ccc84284619ebfec27d9c09a903de",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.2.0/bazel-skylib-1.2.0.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.2.0/bazel-skylib-1.2.0.tar.gz",
        ],
    )

    http_archive(
        name = "rules_android",
        url = "https://github.com/bazelbuild/rules_android/archive/5e74650496dff30e97b8eee5a8b12968de3bdec3.zip",
        sha256 = "89f8698e7afa76c6eb60907d1200c33594866c8263a9ef0161e81dc0d397a9e9",
        strip_prefix = "rules_android-5e74650496dff30e97b8eee5a8b12968de3bdec3",
        patch_args = ["-p1"],
        patches = [
            # We can probably move to a more recent upstream commit and abandon these three patches in the future
            # Right now there's an error with regards to io_bazel_rules_go that can't easily be resolved
            "@valdi//third-party/build_bazel_rules_android/patches:rules_android_rules_attrs.patch",
            "@valdi//third-party/build_bazel_rules_android/patches:rules_android_rules_android_local_test.patch",
            "@valdi//third-party/build_bazel_rules_android/patches:rules_android_android_rules.patch",
            # Patch to propgate resources in aar_import rule
            "@valdi//third-party/build_bazel_rules_android/patches:rules_android_rules_aar_import.patch",
            # Patch to fix StarlarkAndroidResourcesInfo provider loading
            "@valdi//third-party/build_bazel_rules_android/patches:rules_android_starlark_providers_fix.patch",
        ],
    )

    http_archive(
        name = "rules_kotlin",
        url = "https://github.com/bazelbuild/rules_kotlin/releases/download/v1.9.0/rules_kotlin-v1.9.0.tar.gz",
        sha256 = "5766f1e599acf551aa56f49dab9ab9108269b03c557496c54acaf41f98e2b8d6",
        patches = ["@valdi//third-party/rules_kotlin:fix_manifest_custom_package.patch"],
    )

    http_archive(
        name = "rules_java",
        sha256 = "976ef08b49c929741f201790e59e3807c72ad81f428c8bc953cdbeff5fed15eb",
        url = "https://github.com/bazelbuild/rules_java/releases/download/7.4.0/rules_java-7.4.0.tar.gz",
    )

    http_archive(
        name = "rules_jvm_external",
        strip_prefix = "rules_jvm_external-6.2",
        sha256 = "808cb5c30b5f70d12a2a745a29edc46728fd35fa195c1762a596b63ae9cebe05",
        url = "https://github.com/bazelbuild/rules_jvm_external/releases/download/6.2/rules_jvm_external-6.2.tar.gz",
    )

    http_archive(
        name = "rules_xcodeproj",
        integrity = "sha256-bNMpoLSjy1hpeFYYKDgliM6pYqyBMb7IRlkjkvPlBJE=",
        url = "https://github.com/MobileNativeFoundation/rules_xcodeproj/releases/download/3.2.0/release.tar.gz",
    )

    http_archive(
        name = "build_bazel_rules_swift",
        sha256 = "5eff717c18bb513285b499add68f2331509cd4e411ff085e96a86b3342c1e5aa",
        url = "https://github.com/bazelbuild/rules_swift/releases/download/3.1.2/rules_swift.3.1.2.tar.gz",
        patch_args = ["-p1"],
        patches = ["@valdi//third-party/rules_swift/patches:rules_swift.patch"],
    )

    # rules_apple relies on rules_shell starting with 4.0.0. For bzlmod users,
    # this dependency is automatically handled. However, to prevent build failures
    # for WORKSPACE usage, we explicitly define the rules_shell dependency here.
    http_archive(
        name = "rules_shell",
        sha256 = "d8cd4a3a91fc1dc68d4c7d6b655f09def109f7186437e3f50a9b60ab436a0c53",
        strip_prefix = "rules_shell-0.3.0",
        url = "https://github.com/bazelbuild/rules_shell/releases/download/v0.3.0/rules_shell-v0.3.0.tar.gz",
    )

    http_archive(
        name = "build_bazel_rules_apple",
        sha256 = "70b0fb2aec1055c978109199bf58ccb5008aba8e242f3305194045c271ca3cae",
        url = "https://github.com/bazelbuild/rules_apple/releases/download/4.0.0/rules_apple.4.0.0.tar.gz",
    )

    # Apple support setup from https://github.com/bazelbuild/apple_support/releases/tag/1.21.0
    http_archive(
        name = "build_bazel_apple_support",
        sha256 = "293f5fe430787f3a995b2703440d27498523df119de00b84002deac9525bea55",
        url = "https://github.com/bazelbuild/apple_support/releases/download/1.21.0/apple_support.1.21.0.tar.gz",
    )

    http_archive(
        name = "bazel_features",
        sha256 = "95fb3cfd11466b4cad6565e3647a76f89886d875556a4b827c021525cb2482bb",
        strip_prefix = "bazel_features-1.10.0",
        url = "https://github.com/bazel-contrib/bazel_features/releases/download/v1.10.0/bazel_features-v1.10.0.tar.gz",
        patches = ["@valdi//third-party/bazel_features:fix_bazel_version.patch"],
    )

    # zoo library
    # Used for its more efficient anonymous Function implementation than std::function
    # https://github.com/thecppzoo/zoo
    # Doesn't do releases
    http_archive(
        name = "zoo",
        url = "https://github.com/thecppzoo/zoo/archive/33b868300145773d8c433b6b3d6643eba6334f7d.zip",
        build_file = "@valdi//third-party/zoo:zoo.BUILD",
        integrity = "sha256-9GIWp7GyVdZn24mvsMGtzhDD2N+W5L5AO3o2tYHtnVk=",
        strip_prefix = "zoo-33b868300145773d8c433b6b3d6643eba6334f7d",
    )

    http_archive(
        name = "hermes",
        url = "https://github.com/facebook/hermes/archive/880b1645b5dca974f4329dc4108692d301abee0d.zip",
        integrity = "sha256-+GQiTtN6H8TQz/+YkVz73dHeYAZ86/9hAUwTlkISx48=",
        strip_prefix = "hermes-880b1645b5dca974f4329dc4108692d301abee0d",
        build_file = "@valdi//third-party/hermes:hermes.BUILD",
    )

    http_archive(
        name = "gtest",
        strip_prefix = "googletest-1.13.0",
        url = "https://github.com/google/googletest/archive/refs/tags/v1.13.0.tar.gz",
        integrity = "sha256-rX/boR6gEcHZJbMonPSvLGajUuGNTHJkOS/q116Rk2M=",
    )

    http_archive(
        name = "boringssl",
        url = "https://boringssl.googlesource.com/boringssl/+archive/82f9853fc7d7360ae44f1e1357a6422c5244bbd8.tar.gz",
    )

    # Used for networking, and other utilities like small_vector
    # TODO: Make the public boost repo compatible with open_source builds
    http_archive(
        name = "boost",
        build_file = "@valdi//third-party/boost:boost.BUILD",
        patches = [
            "@valdi//third-party/boost/patches:asio.patch",
            "@valdi//third-party/boost/patches:global_asio_initializers.patch",
            "@valdi//third-party/boost/patches:interprocess_emscripten.patch",
            "@valdi//third-party/boost/patches:remove_invalid_file_1_78.patch",
        ],
        strip_prefix = "boost_1_78_0",
        integrity = "sha256-hoHxddS9smxSIiZleT7vCEkNd1hSkzD5jTsp3Qc1vMw=",
        url = "https://archives.boost.io/release/1.78.0/source/boost_1_78_0.tar.bz2",
    )

    # phmap library
    # Used for its efficient flat_map implementation
    # https://github.com/greg7mdp/parallel-hashmap/
    http_archive(
        name = "phmap",
        build_file = "@valdi//third-party/phmap:phmap.BUILD",
        strip_prefix = "parallel-hashmap-1.3.12",
        integrity = "sha256-DMIDFEMhkkz7/MQB9C2CBMDdJOJ2DHocCRuqFtl3fAg=",
        url = "https://github.com/greg7mdp/parallel-hashmap/archive/refs/tags/v1.3.12.tar.gz",
    )

    # Local upstream dependencies that are overwritten in parent WORKSPACES

    # From https://github.com/fmtlib/fmt/releases/tag/7.0.3
    http_archive(
        name = "fmt",
        build_file = "@valdi//third-party/fmt:fmt.BUILD",
        integrity = "sha256-XZjFBNAgX5EuIkSezep3a3jOC7CWknM0+AeB5yAITJ8=",
        strip_prefix = "fmt-7.1.3",
        url = "https://github.com/fmtlib/fmt/releases/download/7.1.3/fmt-7.1.3.zip",
    )

    local_or_nested_repository(
        workspace_root = workspace_root,
        name = "android_macros",
        path = "bzl/macros",
    )

    local_or_nested_repository(
        workspace_root = workspace_root,
        name = "snap_macros",
        path = "bzl/valdi/snap_macros",
    )

    local_or_nested_repository(
        workspace_root = workspace_root,
        name = "snap_client_toolchains",
        path = "/bzl/toolchains",
    )

    local_or_nested_repository(
        workspace_root = workspace_root,
        name = "snap_platforms",
        path = "/bzl/platforms",
    )

    local_or_nested_repository(
        workspace_root = workspace_root,
        name = "skia_user_config",
        path = "/third-party/skia_user_config",
    )

    local_or_nested_repository(
        workspace_root = workspace_root,
        name = "rules_hdrs",
        path = "/third-party/rules_hdrs",
    )

    local_or_nested_repository(
        workspace_root = workspace_root,
        name = "valdi_toolchain",
        path = "/bin",
    )

    local_or_nested_repository(
        workspace_root = workspace_root,
        name = "resvg_libs",
        path = "/third-party/resvg/resvg_libs",
    )

    # From https://github.com/open-source-parsers/jsoncpp/releases/tag/1.8.0
    http_archive(
        name = "jsoncpp",
        strip_prefix = "jsoncpp-1.8.0",
        build_file = "@valdi//third-party/jsoncpp:jsoncpp.BUILD",
        integrity = "sha256-TdYW0kzlN9+8IrTdgb9v+NgFd6a7tHzamvuEReRmH5s=",
        url = "https://github.com/open-source-parsers/jsoncpp/archive/refs/tags/1.8.0.zip",
    )

    # From https://github.com/harfbuzz/harfbuzz/releases/tag/12.2.0
    http_archive(
        name = "harfbuzz",
        strip_prefix = "harfbuzz-12.2.0",
        build_file = "@valdi//third-party/harfbuzz:harfbuzz.BUILD",
        integrity = "sha256-7LYDqkJqiyRmVxhme9pkqEwVBNt0VO5Mrb02Lupk5UU=",
        url = "https://github.com/harfbuzz/harfbuzz/releases/download/12.2.0/harfbuzz-12.2.0.tar.xz",
    )

    # From https://github.com/protocolbuffers/protobuf/releases/tag/v3.20.0
    http_archive(
        name = "protobuf_cpp",
        strip_prefix = "protobuf-3.20.0",
        build_file = "@valdi//third-party/protobuf_cpp:protobuf_cpp.BUILD",
        url = "https://github.com/protocolbuffers/protobuf/releases/download/v3.20.0/protobuf-all-3.20.0.tar.gz",
        integrity = "sha256-ocB26D9FtkueMK6JqZC6Sq6ffcGKypD3aQ9FjcWuBoE=",
    )

    http_archive(
        name = "backward-cpp",
        build_file = "@valdi//third-party/backward:backward.BUILD",
        strip_prefix = "backward-cpp-1.6",
        integrity = "sha256-xlTQkj1D8c6iPQhnKWc0mOR0H7JFfoBs+urqeyDJfBA=",
        url = "https://github.com/bombela/backward-cpp/archive/refs/tags/v1.6.tar.gz",
    )

    http_archive(
        name = "xxhash",
        build_file = "@valdi//third-party/xxhash:xxhash.BUILD",
        integrity = "sha256-uu4Mav1PAxZd56TmeYjRbw8rJXtR0OPLkZCTAqJqecQ=",
        url = "https://github.com/Cyan4973/xxHash/archive/refs/tags/v0.8.2.tar.gz",
    )

    # Transitive dependency of skia
    http_archive(
        name = "zlib_chromium",
        url = "https://github.com/simonis/zlib-chromium/archive/93867c6db67801f74c2d0840a271c7aa7fd6716c.zip",
        build_file = "@valdi//third-party/zlib_chromium:zlib_chromium.BUILD",
        integrity = "sha256-aybDRpEOnvUrCW8/wTFSuduz5Rk+YxYGdCUZYVY5G9g=",
        strip_prefix = "zlib-chromium-93867c6db67801f74c2d0840a271c7aa7fd6716c",
        patch_args = ["-p1"],
        patches = [
            "@valdi//third-party/zlib_chromium/patches:apple.patch",
        ],
    )

    # From https://github.com/facebook/zstd/releases/tag/v1.5.6
    http_archive(
        name = "zstd",
        build_file = "@valdi//third-party/zstd:zstd.BUILD",
        url = "https://github.com/facebook/zstd/releases/download/v1.5.6/zstd-1.5.6.tar.gz",
        strip_prefix = "zstd-1.5.6",
        integrity = "sha256-jCngbPQqrMHq/EB3ri7Gxvy5amJhV+BZPV6Co0/UA8E=",
    )

    # From https://github.com/unicode-org/icu/releases/tag/release-68-1
    # Downstream uses a modified 61.1 but the upstream version of 61.1
    # does not compile with C++20
    http_archive(
        name = "icu",
        build_file = "@valdi//third-party/icu:icu.BUILD",
        url = "https://github.com/unicode-org/icu/releases/download/release-68-1/icu4c-68_1-src.tgz",
        strip_prefix = "icu",
        integrity = "sha256-qfLj2LRDS45Th4tDCL0ebuUcnHBC4rGjdqvvtvuyny0=",
    )

    http_archive(
        name = "websocketpp",
        strip_prefix = "websocketpp-0.8.2",
        url = "https://github.com/zaphoyd/websocketpp/archive/refs/tags/0.8.2.tar.gz",
        build_file = "@valdi//third-party/websocketpp:websocketpp.BUILD",
        integrity = "sha256-bOiJ2F7Nwtj6B0CNZ4fnNSUQdQ2qZrWtRKrLR76nZ1U=",
    )

    http_archive(
        name = "test262",
        url = "https://github.com/tc39/test262/archive/refs/heads/main.zip",
        build_file = "@valdi//third-party/test262:test262.BUILD",
        strip_prefix = "test262-main",
        integrity = "sha256-Lv/ZdXIV5f0n11oFTKBq22r0aLGF6HsDp36B1afgVDo=",
    )

    # Deps to support the deps

    http_archive(
        name = "rules_pkg",
        sha256 = "8f9ee2dc10c1ae514ee599a8b42ed99fa262b757058f65ad3c384289ff70c4b8",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_pkg/releases/download/0.9.1/rules_pkg-0.9.1.tar.gz",
            "https://github.com/bazelbuild/rules_pkg/releases/download/0.9.1/rules_pkg-0.9.1.tar.gz",
        ],
    )

    http_archive(
        name = "rules_nodejs",
        sha256 = "8fc8e300cb67b89ceebd5b8ba6896ff273c84f6099fc88d23f24e7102319d8fd",
        urls = ["https://github.com/bazelbuild/rules_nodejs/releases/download/5.8.4/rules_nodejs-core-5.8.4.tar.gz"],
    )

    http_archive(
        name = "build_bazel_rules_nodejs",
        sha256 = "709cc0dcb51cf9028dd57c268066e5bc8f03a119ded410a13b5c3925d6e43c48",
        urls = ["https://github.com/bazelbuild/rules_nodejs/releases/download/5.8.4/rules_nodejs-5.8.4.tar.gz"],
    )

    http_archive(
        name = "aspect_rules_js",
        sha256 = "6b7e73c35b97615a09281090da3645d9f03b2a09e8caa791377ad9022c88e2e6",
        strip_prefix = "rules_js-2.0.0",
        url = "https://github.com/aspect-build/rules_js/releases/download/v2.0.0/rules_js-v2.0.0.tar.gz",
    )

    http_archive(
        name = "aspect_rules_ts",
        sha256 = "6fd16aa24c2e8547b72561ece1c7d307b77a5f98f0402934396f6eefbac59aa2",
        strip_prefix = "rules_ts-3.7.0",
        url = "https://github.com/aspect-build/rules_ts/releases/download/v3.7.0/rules_ts-v3.7.0.tar.gz",
    )

    http_archive(
        name = "aspect_rules_esbuild",
        sha256 = "530adfeae30bbbd097e8af845a44a04b641b680c5703b3bf885cbd384ffec779",
        strip_prefix = "rules_esbuild-0.22.1",
        url = "https://github.com/aspect-build/rules_esbuild/releases/download/v0.22.1/rules_esbuild-v0.22.1.tar.gz",
    )

    http_archive(
        name = "aspect_bazel_lib",
        sha256 = "53cadea9109e646a93ed4dc90c9bbcaa8073c7c3df745b92f6a5000daf7aa3da",
        strip_prefix = "bazel-lib-2.21.2",
        url = "https://github.com/bazel-contrib/bazel-lib/releases/download/v2.21.2/bazel-lib-v2.21.2.tar.gz",
    )

    # fuzzing rules directly copied from https://github.com/bazelbuild/rules_fuzzing/tree/1dbcd9167300ad226d29972f5f9c925d6d81f441?tab=readme-ov-file#configuring-the-workspace
    http_archive(
        name = "rules_fuzzing",
        sha256 = "e6bc219bfac9e1f83b327dd090f728a9f973ee99b9b5d8e5a184a2732ef08623",
        strip_prefix = "rules_fuzzing-0.5.2",
        urls = ["https://github.com/bazelbuild/rules_fuzzing/archive/v0.5.2.zip"],
    )

    http_archive(
        name = "rules_python",
        sha256 = "a644da969b6824cc87f8fe7b18101a8a6c57da5db39caa6566ec6109f37d2141",
        strip_prefix = "rules_python-0.20.0",
        url = "https://github.com/bazelbuild/rules_python/releases/download/0.20.0/rules_python-0.20.0.tar.gz",
    )

    http_archive(
        name = "resvg",
        url = "https://github.com/RazrFalcon/resvg/archive/a739aef5d01360ec238c886bc50674f31458df00.zip",
        build_file = "@valdi//third-party/resvg:resvg.BUILD",
        strip_prefix = "resvg-a739aef5d01360ec238c886bc50674f31458df00",
        integrity = "sha256-kohUhIYyFoaeIHLBhYwq6g7+h34r3i9/IIQoWyJAmSE=",
    )

    http_archive(
        name = "skia",
        url = "https://github.com/google/skia/archive/8d5c6efb04514a31f09a2e865940f99cdf60ce21.zip",
        strip_prefix = "skia-8d5c6efb04514a31f09a2e865940f99cdf60ce21",
        integrity = "sha256-piM7FfzT3xekYxvwEDlejJ8Zj+zaxa3oIKpDsx1y6eQ=",
    )

    http_archive(
        name = "libjpeg_turbo",
        build_file = "@skia//bazel/external/libjpeg_turbo:BUILD.bazel",
        url = "https://chromium.googlesource.com/chromium/deps/libjpeg_turbo/+archive/e14cbfaa85529d47f9f55b0f104a579c1061f9ad.tar.gz",
        patches = [
            "@valdi//third-party/libjpeg_turbo:warning_fix.patch",
        ],
    )

    http_archive(
        name = "libpng",
        build_file = "@skia//bazel/external/libpng:BUILD.bazel",
        url = "https://skia.googlesource.com/third_party/libpng.git/+archive/ed217e3e601d8e462f7fd1e04bed43ac42212429.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@valdi//third-party/libpng:fix_armv7.patch",
        ],
    )

    http_archive(
        name = "libwebp",
        build_file = "@skia//bazel/external/libwebp:BUILD.bazel",
        url = "https://chromium.googlesource.com/webm/libwebp.git/+archive/845d5476a866141ba35ac133f856fa62f0b7445f.tar.gz",
    )

    http_archive(
        name = "zlib_skia",
        build_file = "@skia//bazel/external/zlib_skia:BUILD.bazel",
        url = "https://chromium.googlesource.com/chromium/src/third_party/zlib/+archive/646b7f569718921d7d4b5b8e22572ff6c76f2596.tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@valdi//third-party/zlib_skia:android_ios_x86_64.patch",
        ],
    )

    http_archive(
        name = "freetype",
        build_file = "@skia//bazel/external/freetype:BUILD.bazel",
        url = "https://chromium.googlesource.com/chromium/src/third_party/freetype2.git/+archive/5d4e649f740c675426fbe4cdaffc53ee2a4cb954.tar.gz",
    )

    http_archive(
        name = "expat",
        build_file = "@skia//bazel/external/expat:BUILD.bazel",
        url = "https://chromium.googlesource.com/external/github.com/libexpat/libexpat.git/+archive/624da0f593bb8d7e146b9f42b06d8e6c80d032a3.tar.gz",
    )

    nested_repository(
        name = "expat_config",
        source_repo = "skia",
        target_dir = "/third_party/expat/include",
    )

    nested_repository(
        name = "freetype_config",
        source_repo = "skia",
        target_dir = "/third_party/freetype2/include",
    )

    http_archive(
        name = "ocmock",
        build_file = "//third-party/ocmock:OCMock.build",
        sha256 = "3c2dc673c83418a6213e63a643d966c3f790693b4e8578e0df4f68ae28ae3fea",
        strip_prefix = "ocmock-3.9.4",
        url = "https://github.com/erikdoe/ocmock/archive/refs/tags/v3.9.4.tar.gz",
    )
