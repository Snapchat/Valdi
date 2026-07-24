load(
    "common.bzl",
    "NODE_MODULES_BASE",
)
load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load(":valdi_toolchain_type.bzl", "VALDI_TOOLCHAIN_TYPE")

# TODO: modify the compiler so that we don't need to pass in the config file and instead can just pass in all of these options as arguments
# Generates a config.yaml file on the fly from a template,
# so that we can customize the input and output paths for Bazel.
# Eventually the parts of the yaml file we need to customize should be passed directly
# to the Valdi compiler and then we won't need to tweak the yaml file anymore.
# TODO(vasily): we could probably remove these completely and just symlink the source config + overrides
def generate_config(ctx):
    config_file = "valdi_config.yaml"
    out = ctx.actions.declare_file(config_file)
    toolchain = ctx.toolchains[VALDI_TOOLCHAIN_TYPE].info

    companion_path = toolchain.companion.files.to_list()[0].path
    minify_config_path = toolchain.minify_config.files.to_list()[0].path
    compiler_toolbox_path = toolchain.compiler_toolbox.files.to_list()[0].path
    native_api_min_version = ctx.attr._native_api_min_version[BuildSettingInfo].value

    native_api_min_version_config = ""
    if native_api_min_version >= 0:
        native_api_min_version_config = "native_api_min_version: {}".format(native_api_min_version)

    ctx.actions.expand_template(
        output = out,
        template = ctx.file._template,
        substitutions = {
            "{IOS_OUTPUT_BASE}": "ios",
            "{ANDROID_OUTPUT_BASE}": "android",
            "{COMPANION_BINARY}": companion_path + "/scripts/run.sh",
            "{MINIFY_CONFIG_PATH}": "$PWD/" + minify_config_path,
            "{COMPILER_TOOLBOX_PATH}": "$PWD/" + compiler_toolbox_path,
            "{NODE_MODULES_DIR}": NODE_MODULES_BASE,
            "{NATIVE_API_MIN_VERSION_CONFIG}": native_api_min_version_config,
        },
    )
    return out

def resolve_compiler_executable(ctx, toolchain, include_tools):
    """ Resolves the tools required to run the Valdi compiler action.

        * compiler binary
        * compiler_toolbox binary
        * companion tool (see //src/valdi_internal/compiler/companion:bin_wrapper)
        * minify config file
        * sqldelight compiler binary

    Args:
        ctx: The context of the current rule invocation
        toolchain: The toolchain info for the current rule invocation

    Returns:
        A tuple of (executable: File, inputs: depset, tools: list[FilesToRunProvider])
    """

    inputs_depsets = []
    inputs_depsets.append(toolchain.compiler.files)
    inputs_depsets.append(toolchain.companion.files)

    # Bazel 8 removed ctx.resolve_tools. Passing each tool's FilesToRunProvider
    # to ctx.actions.run(tools = ...) makes Bazel resolve its runfiles (and the
    # input manifests that ctx.resolve_tools used to return) automatically.
    tools = [toolchain.companion[DefaultInfo].files_to_run]
    if include_tools:
        inputs_depsets.append(toolchain.compiler_toolbox.files)
        inputs_depsets.append(toolchain.minify_config.files)
        inputs_depsets.append(toolchain.sqldelight_compiler.files)

        tools.append(toolchain.sqldelight_compiler[DefaultInfo].files_to_run)

    return (toolchain.compiler.files.to_list()[0], depset(transitive = inputs_depsets), tools)

def run_valdi_compiler(ctx, args, outputs, inputs, mnemonic, progress_message, use_worker, include_tools = True, worker_protocol = "proto"):
    """ Run the Valdi compiler with the provided arguments.
    This will resolve the Valdi toolchain with the compiler executable
    and emits outputs.
    """
    toolchain = ctx.toolchains[VALDI_TOOLCHAIN_TYPE].info
    (executable, tool_inputs, tools) = resolve_compiler_executable(ctx, toolchain, include_tools)

    companion_bin_wrapper = toolchain.companion.files.to_list()[0]
    compiler_toolbox = toolchain.compiler_toolbox.files.to_list()[0]
    minify_config = toolchain.minify_config.files.to_list()[0]
    client_sql = toolchain.sqldelight_compiler.files.to_list()

    args.add("--bazel")
    args.add("--direct-companion-path", companion_bin_wrapper)
    args.add("--direct-compiler-toolbox-path", compiler_toolbox)
    args.add("--direct-minify-config-path", minify_config)

    if client_sql:
        args.add("--direct-client-sql-path", client_sql[0])

    env = {
        # required for the companion app execution under Bazel
        "BAZEL_BINDIR": ctx.bin_dir.path,
        # Enable Swift backtracing support to get stacktraces (see https://github.com/apple/swift/blob/main/docs/Backtracing.rst#how-do-i-configure-backtracing)
        # "SWIFT_BACKTRACE": "enable=yes,threads=all", Disabled due to crash on MacOS 15.4 Compiler Toolbox error: swift runtime: backtrace-on-crash is not supported for privileged executables.
    }

    # Allow to pass the GCP service account via --action_env parameter
    if "VALDI_COMPILER_GCP_SERVICE_ACCOUNT" in ctx.configuration.default_shell_env:
        env["VALDI_COMPILER_GCP_SERVICE_ACCOUNT"] = ctx.configuration.default_shell_env["VALDI_COMPILER_GCP_SERVICE_ACCOUNT"]

    action_arguments = [args]
    if use_worker:
        action_arguments = ["--persistent_worker_protocol=" + worker_protocol, args]

    ctx.actions.run(
        outputs = outputs,
        inputs = depset(direct = inputs, transitive = [tool_inputs]),
        executable = executable,
        tools = tools,
        arguments = action_arguments,
        env = env,
        use_default_shell_env = True,
        toolchain = VALDI_TOOLCHAIN_TYPE,
        mnemonic = mnemonic,
        progress_message = progress_message,
        execution_requirements = {"supports-workers": "1" if use_worker else "0", "requires-worker-protocol": worker_protocol},
    )
