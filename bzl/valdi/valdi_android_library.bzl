load("@rules_kotlin//kotlin:android.bzl", "kt_android_library")

def manifest_attrs_error(name, manifest, custom_package):
    """Validates the manifest/custom_package combination. Exposed for testing.

    Args:
        name: the target name, used in the error message.
        manifest: the manifest attribute value, or None.
        custom_package: the custom_package attribute value, or None.

    Returns:
        An error message string, or None when the combination is valid.
    """
    if not manifest or custom_package:
        return None

    return (
        "valdi_android_library(name = \"{}\") sets `manifest` but not `custom_package`. ".format(name) +
        "Bazel infers the R package from the target's path, which is usually wrong and " +
        "sometimes invalid: a target under a `src/android/` directory infers the package " +
        "\"android\" and fails aapt2 with \"package 'android' can only be built as a " +
        "regular app\". Set `custom_package` to the package declared in the manifest."
    )

def valdi_android_library(
        name,
        **kwargs):
    error = manifest_attrs_error(name, kwargs.get("manifest"), kwargs.get("custom_package"))
    if error:
        fail(error)

    # Bazel drops a library's manifest from the final APK unless the library exports
    # it, so components declared here (providers, services, receivers) would compile
    # but never ship. Only default this on when a manifest is actually present:
    # exports_manifest alone makes rules_android treat the target as defining
    # resources, which then fails for the manifest-less libraries that are the norm.
    if kwargs.get("manifest") and "exports_manifest" not in kwargs:
        kwargs["exports_manifest"] = 1

    kt_android_library(name = name, plugins = [
        "@valdi//valdi:annotation_processor",
    ], **kwargs)
