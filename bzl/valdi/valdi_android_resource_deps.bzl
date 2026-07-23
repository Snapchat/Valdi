"""Helpers for packaging cross-platform Valdi resources."""

load("@rules_android//rules:rules.bzl", "android_library")

def valdi_android_resource_deps(name, resources):
    """Creates an Android asset dependency for cross-platform resources."""
    if not resources:
        return []

    android_library(
        name = name,
        assets = resources,
        # Keep these resources independent of the application's assets_dir.
        # The empty root packages source resources relative to their owning
        # Bazel package and generated resources relative to their output root.
        assets_dir = "",
        manifest = "@valdi//bzl/valdi:empty_android_manifest.xml",
    )
    return [":{}".format(name)]
