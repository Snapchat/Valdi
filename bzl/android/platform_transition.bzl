def android_aar_platforms():
    return select({
        "@snap_platforms//conditions:client_repo_arm64": ["@snap_platforms//os:android_arm64"],
        "//conditions:default": [],
    }) + select({
        "@snap_platforms//conditions:client_repo_x86_64": ["@snap_platforms//os:android_x86_64"],
        "//conditions:default": [],
    }) + select({
        "@snap_platforms//conditions:client_repo_arm32": ["@snap_platforms//os:android_arm32"],
        "//conditions:default": [],
    })

def _impl(settings, attr):
    _ignore = settings

    result = {}

    cpus = {
        Label("@snap_platforms//os:android_arm32"): "armeabi-v7a",
        Label("@snap_platforms//os:android_arm64"): "arm64-v8a",
        Label("@snap_platforms//os:android_x86_64"): "x86_64",
    }

    for platform in attr.platforms:
        result[cpus[platform]] = {
            "//command_line_option:platforms": [platform],
        }

    return result

# Transition for the native libs into desired arches
platform_transition = transition(
    implementation = _impl,
    inputs = [],
    outputs = [
        "//command_line_option:platforms",
    ],
)

def _reset_to_host_impl(settings, attr):
    _ignore = attr
    return {
        "//command_line_option:platforms": [settings["//command_line_option:host_platform"]],
    }

# Resets --platforms to the host platform. The classes jar packaged into an aar
# is platform-independent Java bytecode built by a host-side java_binary, but it
# inherits the incoming target platform. When that platform is Android (e.g. a
# top-level `--platforms=<android>` set to build the final .apk), the java_binary's
# deploy-jar step fails toolchain resolution because no JDK runtime toolchain is
# (or should be) registered for Android. Resetting to host makes the JDK resolve
# there. This stays in the target configuration (not exec), so the jar keeps the
# target --java_language_version rather than the tool one.
reset_platform_to_host = transition(
    implementation = _reset_to_host_impl,
    inputs = [
        "//command_line_option:host_platform",
    ],
    outputs = [
        "//command_line_option:platforms",
    ],
)
