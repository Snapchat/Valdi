"""Analysis tests for manifest handling in valdi_android_library."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts", "unittest")
load("@rules_android//providers:providers.bzl", "StarlarkAndroidResourcesInfo")
load(":valdi_android_library.bzl", "manifest_attrs_error", "valdi_android_library")

def _exported_manifests(target):
    """Returns the manifests this target contributes to a dependent APK's manifest merge.

    A dependent android_binary unions the direct and transitive resource nodes and keeps
    only those with exports_manifest set, so both are inspected here.

    Args:
        target: the Target to inspect.

    Returns:
        A list of manifest Files that would be merged into a dependent APK.
    """
    info = target[StarlarkAndroidResourcesInfo]
    nodes = depset(transitive = [
        info.direct_resources_nodes,
        info.transitive_resources_nodes,
    ]).to_list()

    return [node.manifest for node in nodes if node.exports_manifest and node.manifest]

def _manifest_is_exported_test_impl(ctx):
    env = analysistest.begin(ctx)

    # Bazel defaults exports_manifest off, which silently drops a library's manifest
    # from the final APK: the Kotlin classes ship but the components the manifest
    # declares (providers, services, receivers) do not. See Valdi#135.
    exported = _exported_manifests(analysistest.target_under_test(env))
    asserts.true(
        env,
        len(exported) > 0,
        "a valdi_android_library declaring a manifest must export it, " +
        "otherwise the components it declares are dropped from the APK; " +
        "found no exported manifest",
    )

    return analysistest.end(env)

manifest_is_exported_test = analysistest.make(_manifest_is_exported_test_impl)

def _no_manifest_exports_nothing_test_impl(ctx):
    env = analysistest.begin(ctx)

    # exports_manifest must stay off when no manifest is supplied. Setting it makes
    # rules_android treat the target as defining resources, which then requires a
    # manifest and fails analysis -- and most valdi_android_library targets supply none.
    exported = _exported_manifests(analysistest.target_under_test(env))
    asserts.equals(
        env,
        [],
        exported,
        "a valdi_android_library with no manifest must not export one",
    )

    return analysistest.end(env)

no_manifest_exports_nothing_test = analysistest.make(_no_manifest_exports_nothing_test_impl)

def _manifest_requires_custom_package_test_impl(ctx):
    env = unittest.begin(ctx)

    # Without custom_package, Bazel infers the R package from the target's path. A
    # library under src/android/ infers "android", which aapt2 rejects outright, so
    # require the package explicitly rather than emit that error from deep in aapt2.
    asserts.true(
        env,
        manifest_attrs_error("lib", "AndroidManifest.xml", None) != None,
        "a manifest without custom_package must be rejected",
    )
    asserts.equals(
        env,
        None,
        manifest_attrs_error("lib", "AndroidManifest.xml", "com.example.lib"),
        "a manifest with custom_package must be accepted",
    )
    asserts.equals(
        env,
        None,
        manifest_attrs_error("lib", None, None),
        "a library with no manifest must not require custom_package",
    )

    return unittest.end(env)

manifest_requires_custom_package_test = unittest.make(_manifest_requires_custom_package_test_impl)

def valdi_android_library_test_suite(name):
    """Registers analysis tests for valdi_android_library.

    Args:
        name: name of the generated test_suite target.
    """
    valdi_android_library(
        name = "manifest_fixture",
        srcs = ["testdata/Placeholder.kt"],
        custom_package = "com.snap.valdi.test.withmanifest",
        manifest = "testdata/AndroidManifest.xml",
        tags = ["manual"],
    )

    valdi_android_library(
        name = "no_manifest_fixture",
        srcs = ["testdata/Placeholder.kt"],
        custom_package = "com.snap.valdi.test.nomanifest",
        tags = ["manual"],
    )

    manifest_is_exported_test(
        name = "manifest_is_exported_test",
        size = "small",
        target_under_test = ":manifest_fixture",
    )

    no_manifest_exports_nothing_test(
        name = "no_manifest_exports_nothing_test",
        size = "small",
        target_under_test = ":no_manifest_fixture",
    )

    manifest_requires_custom_package_test(
        name = "manifest_requires_custom_package_test",
        size = "small",
    )

    native.test_suite(
        name = name,
        tests = [
            ":manifest_is_exported_test",
            ":manifest_requires_custom_package_test",
            ":no_manifest_exports_nothing_test",
        ],
    )
