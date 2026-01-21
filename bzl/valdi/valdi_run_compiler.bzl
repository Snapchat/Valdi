load(
    "common.bzl",
    "NODE_MODULES_BASE",
)
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
    pngquant_path = toolchain.pngquant.files.to_list()[0].path

    ctx.actions.expand_template(
        output = out,
        template = ctx.file._template,
        substitutions = {
            "{IOS_OUTPUT_BASE}": "ios",
            "{ANDROID_OUTPUT_BASE}": "android",
            "{COMPANION_BINARY}": companion_path + "/scripts/run.sh",
            "{MINIFY_CONFIG_PATH}": "$PWD/" + minify_config_path,
            "{COMPILER_TOOLBOX_PATH}": "$PWD/" + compiler_toolbox_path,
            "{PNGQUANT_PATH}": "$PWD/" + pngquant_path,
            "{NODE_MODULES_DIR}": NODE_MODULES_BASE,
        },
    )
    return out

def resolve_compiler_executable(ctx, toolchain, include_tools):
    """ Resolves the tools required to run the Valdi compiler action.

        * compiler binary
        * compiler_toolbox binary
        * companion tool (see //src/valdi_internal/compiler/companion:bin_wrapper)
        * minify config file
        * pngquant binary
        * sqldelight compiler binary

    Args:
        ctx: The context of the current rule invocation
        toolchain: The toolchain info for the current rule invocation

    Returns:
        A tuple of (executable: File, tools: depset, input_manifests: List[RunfilesManifest])
    """

    inputs_depsets = []
    inputs_depsets.append(toolchain.compiler.files)

    inputs_depsets.append(toolchain.companion.files)
    companion = toolchain.companion
    (companion_inputs, companion_runfile_manifests) = ctx.resolve_tools(tools = [companion])
    inputs_depsets.append(companion_inputs)

    manifests = []
    manifests += companion_runfile_manifests
    if include_tools:
        inputs_depsets.append(toolchain.compiler_toolbox.files)
        inputs_depsets.append(toolchain.minify_config.files)
        inputs_depsets.append(toolchain.pngquant.files)
        inputs_depsets.append(toolchain.sqldelight_compiler.files)

        sqldelight_compiler = toolchain.sqldelight_compiler
        (sqldelight_compiler_inputs, sqldelight_compiler_runfile_manifests) = ctx.resolve_tools(tools = [sqldelight_compiler])
        inputs_depsets.append(sqldelight_compiler_inputs)

        manifests += sqldelight_compiler_runfile_manifests

    return (toolchain.compiler.files.to_list()[0], depset(transitive = inputs_depsets), manifests)

def run_valdi_compiler(ctx, args, outputs, inputs, mnemonic, progress_message, use_worker, include_tools = True):
    """ Run the Valdi compiler with the provided arguments.
    This will resolve the Valdi toolchain with the compiler executable
    and emits outputs.
    """
    toolchain = ctx.toolchains[VALDI_TOOLCHAIN_TYPE].info
    (executable, tools, input_manifests) = resolve_compiler_executable(ctx, toolchain, include_tools)

    companion_bin_wrapper = toolchain.companion.files.to_list()[0]
    compiler_toolbox = toolchain.compiler_toolbox.files.to_list()[0]
    pngquant = toolchain.pngquant.files.to_list()[0]
    minify_config = toolchain.minify_config.files.to_list()[0]
    client_sql = toolchain.sqldelight_compiler.files.to_list()

    args.add("--bazel")
    args.add("--direct-companion-path", companion_bin_wrapper)
    args.add("--direct-compiler-toolbox-path", compiler_toolbox)
    args.add("--direct-pngquant-path", pngquant)
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

    ctx.actions.run(
        outputs = outputs,
        inputs = inputs,
        executable = executable,
        tools = tools,
        input_manifests = input_manifests,
        arguments = [args],
        env = env,
        use_default_shell_env = True,
        toolchain = VALDI_TOOLCHAIN_TYPE,
        mnemonic = mnemonic,
        progress_message = progress_message,
        execution_requirements = {"supports-workers": "1" if use_worker else "0", "requires-worker-protocol": "json"},
    )
