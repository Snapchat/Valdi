load(":valdi_compiled.bzl", "ValdiModuleInfo")
load(":valdi_toolchain_type.bzl", "VALDI_TOOLCHAIN_TYPE")

def extract_valdi_output_rule(implementation, attrs):
    # TODO(simon): We use cfg = "exec" to avoid compiling multiple
    # times for each platform as Valdi compilation is already cross-platform.
    # This eliminates the need for compiling a Valdi module 3 times on Android:
    # once for the Kotlin output, once for C output meant to compile for arm64,
    # once for C output meant to compile for armv7. It might be better to use a
    # Bazel transition instead at some point.

    attrs["compiled_module"] = attr.label(
        mandatory = True,
        cfg = "exec",
        providers = [ValdiModuleInfo],
    )

    return rule(
        implementation = implementation,
        attrs = attrs,
        # Not used by the implementations: requiring the type makes this
        # rule's exec platform (which the cfg = "exec" above transitions to)
        # follow valdi toolchain availability, so compiled_module lands in an
        # exec configuration whose valdi toolchain is actually runnable there
        # (see //bzl/valdi:mac_only_execution).
        toolchains = [VALDI_TOOLCHAIN_TYPE],
    )
