load("@bazel_skylib//lib:paths.bzl", "paths")
load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load(":valdi_compiled.bzl", "ValdiModuleInfo", "build_dependency_entries")
load(":valdi_paths.bzl", "get_ids_yaml_dts_path", "get_resources_dirs", "get_sql_dts_paths", "get_strings_dts_path", "infer_base_output_dir", "resolve_module_dir_and_name", "resolve_relative_project_path")
load(":valdi_run_compiler.bzl", "generate_config", "run_valdi_compiler")
load(":valdi_toolchain_type.bzl", "VALDI_TOOLCHAIN_TYPE")

_BUILD_DIR = ".valdi_build/projectsync"
_TYPESCRIPT_GENERATED_TS_DIR = paths.join(_BUILD_DIR, "generated_ts")

def _prepare_explicit_input_list_file(ctx, module_name):
    """ Write a JSON file with the list of all files that should be passed to the Valdi compiler. """

    all_inputs = []
    all_inputs += ctx.files.sql_srcs
    all_inputs += ctx.files.strings_json_srcs
    all_inputs += ctx.files.res

    if ctx.file.ids_yaml:
        all_inputs.append(ctx.file.ids_yaml)

    dependencies = depset(direct = all_inputs)
    dependencies_list = dependencies.to_list()

    files = []
    (module_directory, _) = resolve_module_dir_and_name(ctx.label)

    for dep in dependencies_list:
        dep_path = dep.path

        relative_project_path = resolve_relative_project_path(dep, module_name, module_directory)

        files.append({"file": dep_path, "relative_project_path": relative_project_path})

    explicit_input_list_file = ctx.actions.declare_file("projectsync_explicit_input_list.json")

    module_content = ctx.attr.target[ValdiModuleInfo].module_definition

    dep_entries = build_dependency_entries(ctx.attr.target[ValdiModuleInfo].deps.to_list())

    content = json.encode_indent({"entries": dep_entries + [
        {
            "module_name": module_name,
            "module_path": module_directory,
            "module_content": module_content,
            "files": files,
        },
    ]}, indent = "  ")
    ctx.actions.write(output = explicit_input_list_file, content = content)

    return (explicit_input_list_file, dependencies_list, module_directory)

def _get_files_output_paths(ctx, module_name, module_directory):
    outputs = []

    # tablename.d.ts files
    outputs += get_sql_dts_paths(ctx.attr.sql_db_names, ctx.files.sql_srcs, module_name, module_directory)

    # ids.d.ts
    outputs += get_ids_yaml_dts_path(_TYPESCRIPT_GENERATED_TS_DIR, module_name, ctx.file.ids_yaml)

    # Strings.d.ts
    strings_json_srcs = ctx.files.strings_json_srcs
    outputs += get_strings_dts_path(_TYPESCRIPT_GENERATED_TS_DIR, module_name, strings_json_srcs)

    # res.ts (and res/<subdir>.ts for nested resource dirs)
    for res_dir in get_resources_dirs(ctx.files.res):
        if res_dir == "res":
            ts_basename = "res.ts"
        else:
            ts_basename = paths.join("res", res_dir + ".ts")
        res_file = paths.join(_TYPESCRIPT_GENERATED_TS_DIR, module_name, ts_basename)
        outputs.append(res_file)

    return outputs

def _declare_compiler_outputs(ctx, module_name, module_directory):
    files_output_paths = _get_files_output_paths(ctx, module_name, module_directory)

    outputs = [ctx.actions.declare_file(path) for path in files_output_paths]

    return (infer_base_output_dir(files_output_paths, outputs), outputs)

def _prepare_arguments(args, log_level, config_yaml_file, explicit_input_list_file, module_name, base_output_dir):
    """ Prepare arguments for the Valdi compiler invocation. """

    args.use_param_file("@%s", use_always = True)
    args.set_param_file_format("multiline")

    args.add("--config", config_yaml_file)
    args.add("--explicit-input-list-file", explicit_input_list_file)

    args.add("--build-dir", paths.join("$PWD", base_output_dir, _BUILD_DIR))

    args.add("--module", module_name)

    args.add("--disable-disk-cache")
    args.add("--skip-remove-orphan-files")

    args.add("--log-level", log_level)
    args.add("--compile")
    args.add("--generate-ts-res-files")

    # Disable concurrency, to free CPU cores to do the actual build.
    # Valdi compiler is fast enough to keep up with Bazel parallelization.
    args.add("--disable-concurrency")

    # Configure compilation
    # TODO: should we create a Bazel rule for this configuration?
    args.add("--config-value", "build_dir=" + explicit_input_list_file.dirname)
    args.add("--config-value", "project_name=" + module_name)

    return args

def _invoke_valdi_compiler(ctx, module_name):
    """ Invoke valdi Compiler for the requested module.

        This function takes care of reproducing the required directory structure which Valdi expects
        (source files should live in subdirectories relative to where the config.yaml files lives, for example).

    Args:
        ctx: The context of the current rule invocation
        module_name: The name of the module to compile

    Returns:
        A list of files that are produced by the Valdi compiler
    """

    #############
    # 1. Generate the project config file that is currently required by the Valdi compiler.
    #    The Valdi compiler uses the project config file location as the initial base directory
    #    (the base_dir read from the config is then resolved relative to the project config file's location)
    config_yaml_file = generate_config(ctx)

    #############
    # 2. Prepare the explicit input list file for the compiler.
    (explicit_input_list_file, all_inputs, module_directory) = _prepare_explicit_input_list_file(ctx, module_name)

    #############
    # 3. Gather all outputs produced by the Valdi compiler
    (base_output_dir, outputs) = _declare_compiler_outputs(ctx, module_name, module_directory)

    #############
    # 5. Set up the action that executes the Valdi compiler

    if len(outputs) > 0:
        args = _prepare_arguments(config_yaml_file = config_yaml_file, base_output_dir = base_output_dir, module_name = module_name, explicit_input_list_file = explicit_input_list_file, args = ctx.actions.args(), log_level = ctx.attr.log_level[BuildSettingInfo].value)

        run_valdi_compiler(
            ctx = ctx,
            args = args,
            outputs = outputs,
            inputs = all_inputs + [config_yaml_file, explicit_input_list_file],
            mnemonic = "ValdiProjectSync",
            progress_message = "Running projectsync for module: " + module_name,
            use_worker = False,
            include_tools = False,
        )

    return outputs

def _to_bazel_str(label, own_repo_name = ""):
    repo = label.repo_name if label.repo_name else own_repo_name
    if repo:
        return "@{}//{}:{}".format(repo, label.package, label.name)
    else:
        return "//{}:{}".format(label.package, label.name)

def _valdi_projectsync_impl(ctx):
    module_name = ctx.attr.target[ValdiModuleInfo].name
    outputs = _invoke_valdi_compiler(ctx, module_name)

    projectsync_json = ctx.actions.declare_file("projectsync.json")

    own_repo = ctx.label.repo_name
    all_deps = []
    for dep in ctx.attr.target[ValdiModuleInfo].deps.to_list():
        all_deps.append(_to_bazel_str(dep.label, own_repo))

    projectsync_json_dict = {
        "target": _to_bazel_str(ctx.attr.target.label, own_repo),
        "dependencies": all_deps,
    }

    if len(outputs) > 0:
        projectsync_json_dict["ts_generated_dir"] = paths.join(
            _TYPESCRIPT_GENERATED_TS_DIR,
            module_name,
        )

    projectsync_json_content = json.encode_indent(projectsync_json_dict, indent = "  ")
    ctx.actions.write(output = projectsync_json, content = projectsync_json_content)

    return [
        DefaultInfo(files = depset(outputs + [projectsync_json])),
    ]

valdi_projectsync = rule(
    implementation = _valdi_projectsync_impl,
    toolchains = [VALDI_TOOLCHAIN_TYPE],
    attrs = {
        "target": attr.label(
            doc = "The module target",
            mandatory = True,
            providers = [[ValdiModuleInfo]],
        ),
        "ids_yaml": attr.label(
            doc = "Optional ids.yaml file for this module",
            allow_single_file = ["ids.yaml"],
        ),
        "strings_json_srcs": attr.label_list(
            doc = "String source files for this module",
            allow_files = True,
        ),
        "res": attr.label_list(
            doc = "List of resources for this module",
            allow_files = True,
        ),
        "sql_srcs": attr.label_list(
            doc = "List of SQL files for this module",
            allow_files = [".sql", ".sq", ".sqm", "sql_types.yaml", "sql_manifest.yaml"],
        ),
        "sql_db_names": attr.string_list(),
        "log_level": attr.label(
            doc = "The log level for the Valdi compiler",
            default = "@valdi//bzl/valdi:compilation_log_level",
        ),
        "_template": attr.label(
            doc = "The template config.yaml file",
            allow_single_file = True,
            default = "valdi_config.yaml.tpl",
        ),
        "_native_api_min_version": attr.label(
            default = "@valdi//bzl/valdi:native_api_min_version",
        ),
    },
)
