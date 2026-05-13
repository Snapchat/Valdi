def _transition_impl(settings, attr):
    return {
        "@snap_platforms//flavors:snap_flavor": "production",
        "@valdi//bzl/runtime_flags:enable_asserts_override": False,
        "@valdi//bzl/runtime_flags:enable_logging": False,
        "@valdi//bzl/runtime_flags:enable_tracing": False,
        "@valdi//bzl/runtime_flags:enable_debug": False,
    }

valdi_toolchain_transition = transition(
    implementation = _transition_impl,
    inputs = [],
    outputs = [
        "@snap_platforms//flavors:snap_flavor",
        "@valdi//bzl/runtime_flags:enable_asserts_override",
        "@valdi//bzl/runtime_flags:enable_logging",
        "@valdi//bzl/runtime_flags:enable_tracing",
        "@valdi//bzl/runtime_flags:enable_debug",
    ],
)

def _valdi_toolchain_binary_impl(ctx):
    bin_file = ctx.attr.name
    files = ctx.attr.bin[DefaultInfo].files

    f = ctx.actions.declare_file(bin_file)
    ctx.actions.symlink(output = f, target_file = files.to_list()[0], is_executable = True)

    return [DefaultInfo(executable = f, files = files)]

valdi_toolchain_binary = rule(
    implementation = _valdi_toolchain_binary_impl,
    executable = True,
    cfg = valdi_toolchain_transition,
    attrs = {
        "bin": attr.label(),
    },
)
