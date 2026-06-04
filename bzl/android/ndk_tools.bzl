ANDROID_NDK_TOOLS_TOOLCHAIN_TYPE = "@snap_client_toolchains//:android_ndk_tools_toolchain_type"

def _android_ndk_tools_toolchain_impl(ctx):
    return [
        platform_common.ToolchainInfo(
            readelf = ctx.file.readelf,
            strip = ctx.file.strip,
            strip_libs = ctx.files.strip_libs,
        ),
    ]

android_ndk_tools_toolchain = rule(
    implementation = _android_ndk_tools_toolchain_impl,
    attrs = {
        "readelf": attr.label(allow_single_file = True),
        "strip": attr.label(allow_single_file = True),
        "strip_libs": attr.label(allow_files = True),
    },
)
