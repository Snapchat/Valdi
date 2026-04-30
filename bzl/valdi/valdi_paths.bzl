load("@bazel_skylib//lib:paths.bzl", "paths")
load("@bazel_skylib//lib:sets.bzl", "sets")
load(
    "common.bzl",
    "ANDROID_RESOURCE_VARIANT_DIRECTORIES",
    "TYPESCRIPT_DUMPED_SYMBOLS_DIR",
    "TYPESCRIPT_GENERATED_TS_DIR",
    "TYPESCRIPT_OUTPUT_DIR",
)

def replace_prefix(text, prefix_to_drop, replacement_prefix):
    without_prefix = text.removeprefix(prefix_to_drop)
    with_new_prefix = replacement_prefix + without_prefix
    return with_new_prefix

def replace_suffix(text, suffix_to_drop, replacement_suffix):
    without_suffix = text.removesuffix(suffix_to_drop)
    with_new_suffix = without_suffix + replacement_suffix
    return with_new_suffix

def _resolve_module_name_for_path(module_path, module_name):
    if "@" in module_name and "node_modules" in module_path:
        # Scoped modules are in the form of <module_name>@<scope>
        # Need to re-format it into @<scope>/<module_name> when performing path related operations
        module_components = module_name.split("@")
        name = module_components[0]
        scope = "@" + module_components[1]

        return paths.join(scope, name)

    return module_name

def _fix_base_dir_path(f, path):
    # Under linked build conditions, the path is relative to the linked workspace root.
    # Fix up the path to ensure the rules work correctly.
    owner_package = f.owner.package
    base_dir_idx = path.find(owner_package + "/")
    if base_dir_idx < 0:
        fail("Expecting {} to contain path {}".format(path, owner_package))
    return path[base_dir_idx:]

def _get_all_unique_directories(files):
    directories = sets.make()

    for file in files:
        directory = paths.dirname(file.path)
        sets.insert(directories, directory)

    return sets.to_list(directories)

def _get_relative_project_path_for_source_file(f, module_name, module_directory):
    fixed_path = _fix_base_dir_path(f, f.path)
    module_name = _resolve_module_name_for_path(fixed_path, module_name)

    if f.path.startswith(module_directory):
        # Note: This is fragile and this breaks if there are two directories nested like
        # module_name/module_name/file .
        # Handle items that are within the module we are currently processing
        relative_project_path = replace_prefix(fixed_path, f.owner.package, module_name)
    else:
        # handles items outside of the module
        relative_project_path = replace_prefix(fixed_path, f.owner.package + "/", "")

    return relative_project_path

# File path processing utility for transforming the file path of a module's source file
# into the expected output file path.
def _expected_output_file_path_for_own_source_file(relative_project_path, suffix_to_drop, replacement_suffix, output_dir):
    replacement_prefix = output_dir + "/"
    with_new_prefix = paths.join(replacement_prefix, relative_project_path)
    output_file_path = replace_suffix(with_new_prefix, suffix_to_drop, replacement_suffix)

    return output_file_path

def _get_base_output_dir(declared_output_path, resolved_output_path):
    path_len_diff = len(resolved_output_path) - len(declared_output_path)

    if path_len_diff and resolved_output_path[path_len_diff - 1] == "/":
        path_len_diff -= 1

    return resolved_output_path[:path_len_diff]

def _get_current_directory(path):
    return path[path.rfind("/") + 1:]

# For a given .ts, .tsx, .js source file, return the expected path to the output compiled JS file.
def output_declaration_compiled_file_path_for_source_file(f, module_name, module_directory, replacement_suffix = ".js"):
    suffix_to_drop = "." + f.extension
    output_dir = "web/debug/assets/"

    # For now, this needs to be fixed eventually
    # src/valdi_modules/src/valdi/<module_name>/debug/assets/<module_name>/src
    js_output_path = _expected_output_file_path_for_own_source_file(
        _get_relative_project_path_for_source_file(f, module_name, module_directory),
        suffix_to_drop = suffix_to_drop,
        replacement_suffix = replacement_suffix,
        output_dir = output_dir,
    )
    return js_output_path

# For a given .ts, .tsx, .vue, .module.css, .module.scss source file, return
# the expected path to the output .d.ts file.
def output_declaration_file_path_for_source_file(f, module_name, module_directory):
    suffix_to_drop = "." + f.extension
    replacement_suffix = ".d.ts"
    output_dir = TYPESCRIPT_OUTPUT_DIR

    # 1. src/valdi_modules/src/valdi/coreutils/src/ArrayUtils.ts
    # 2. coreutils/src/ArrayUtils.ts
    # 3. .valdi_build/compile/typescript/output/coreutils/src/ArrayUtils.ts
    # 4. .valdi_build/compile/typescript/output/coreutils/src/ArrayUtils.d.ts
    dts_output_path = _expected_output_file_path_for_own_source_file(
        _get_relative_project_path_for_source_file(f, module_name, module_directory),
        suffix_to_drop = suffix_to_drop,
        replacement_suffix = replacement_suffix,
        output_dir = output_dir,
    )
    return dts_output_path

def _output_declaration_file_path_for_legacy_source_file(f, module_name, module_directory):
    if (f.basename.endswith(".module.css") or f.basename.endswith(".module.scss")):
        suffix_to_drop = ".module." + f.extension
        replacement_suffix = ".module." + f.extension + ".d.ts"
        output_dir = TYPESCRIPT_GENERATED_TS_DIR
    elif (f.basename.endswith(".vue")):
        suffix_to_drop = ".vue"
        replacement_suffix = ".vue.d.ts"
        output_dir = TYPESCRIPT_OUTPUT_DIR
    else:
        fail("Unknown extension, can't infer location for cote generated .d.ts file: {}".format(f.path))

    # 1. src/valdi_modules/src/valdi/snapchatter_selection/src/components/SelectionSelector.module.scss
    # 2. snapchatter_selection/src/components/SelectionSelector.module.scss
    # 3. .valdi_build/compile/typescript/output/snapchatter_selection/src/components/SelectionSelector.module.scss
    # 4. .valdi_build/compile/typescript/output/snapchatter_selection/src/components/SelectionSelector.module.scss.d.ts
    dts_output_path = _expected_output_file_path_for_own_source_file(
        _get_relative_project_path_for_source_file(f, module_name, module_directory),
        suffix_to_drop = suffix_to_drop,
        replacement_suffix = replacement_suffix,
        output_dir = output_dir,
    )
    return dts_output_path

def get_sql_dts_paths(db_names, sql_srcs, module_name, module_directory):
    if not db_names:
        return []
    result = []
    sq_srcs = [_get_relative_project_path_for_source_file(src, module_name, module_directory) for src in sql_srcs if src.path.endswith(".sq")]
    for sql_file_path in sq_srcs:
        # The logic below mimics the one used by the SQL Compiler which drops the DB name and outputs everything in the same directory
        # 1. src/valdi_modules/src/valdi/creative_tools_platform/sql/CTPStorage/FeedTree.sq
        # 2. src/valdi_modules/src/valdi/creative_tools_platform/sql/CTPStorage/
        # 3. .valdi_build/compile/generated_ts/creative_tools_platform/src/sqlgen/
        # 4. .valdi_build/compile/generated_ts/creative_tools_platform/src/sqlgen/FeedTreeTypes.d.ts
        # 5. .valdi_build/compile/generated_ts/creative_tools_platform/src/sqlgen/FeedTreeQueries.d.ts
        prefix_to_drop = paths.join(module_name, "sql/")
        db_name = replace_prefix(sql_file_path, prefix_to_drop, "").split("/")[0]

        prefix_to_drop = paths.join(prefix_to_drop, db_name) + "/"
        prefix_to_add = paths.join(TYPESCRIPT_OUTPUT_DIR, module_name, "src/sqlgen/")

        suffix = sql_file_path.removeprefix(prefix_to_drop).removesuffix(".sq")
        output_location = paths.join(prefix_to_add, suffix)

        result.append(output_location + "Types.d.ts")
        result.append(output_location + "Queries.d.ts")

    # Also add database dts files
    for db_name in db_names:
        result.append(paths.join(TYPESCRIPT_OUTPUT_DIR, module_name, "src/sqlgen", db_name + ".d.ts"))

    return result

def get_legacy_vue_srcs_dts_paths(legacy_vue_srcs, module_name, module_directory):
    result = []
    for f in legacy_vue_srcs:
        dts_output_path = _output_declaration_file_path_for_legacy_source_file(f, module_name, module_directory)
        result.append(dts_output_path)
        result.append(replace_suffix(dts_output_path, ".d.ts", ".generated.d.ts"))

    return result

def get_ids_yaml_dts_path(ts_generated_ts_dir, module_name, ids_yaml):
    """ Returns the path to the generated ids.d.ts file. """
    return [paths.join(ts_generated_ts_dir, module_name, "ids.d.ts")] if ids_yaml else []

def get_strings_dts_path(ts_generated_ts_dir, module_name, strings_srcs):
    """ Returns the path to the generated Strings.d.ts file, if any strings_srcs are provided. """
    return [paths.join(ts_generated_ts_dir, module_name, "src", "Strings.d.ts")] if strings_srcs else []

def resolve_relative_project_path(f, module_name, module_directory):
    if f.is_source:
        return _get_relative_project_path_for_source_file(f, module_name, module_directory)
    else:
        # declaration file was generated
        fixed_path = _fix_base_dir_path(f, f.short_path)
        module_name = _resolve_module_name_for_path(fixed_path, module_name)
        short_path = replace_prefix(fixed_path, f.owner.package, module_name)

        output_dir = TYPESCRIPT_OUTPUT_DIR
        if short_path.endswith(".module.css.d.ts") or short_path.endswith(".module.scss.d.ts"):
            output_dir = TYPESCRIPT_GENERATED_TS_DIR
        elif short_path.endswith("/ids.d.ts"):
            output_dir = TYPESCRIPT_GENERATED_TS_DIR
        elif short_path.endswith("/Strings.d.ts"):
            # TODO: wire up the generated Strings.d.ts separately from other declarations?
            #       this way we wouldn't need to detect this just using an .endswith
            output_dir = TYPESCRIPT_GENERATED_TS_DIR
        elif short_path.endswith("/compilation-metadata.json"):
            output_dir = TYPESCRIPT_DUMPED_SYMBOLS_DIR

        # Converts from "src/valdi_modules/src/valdi/test_module_generated_types/.valdi_build/typescript/output/test_module_generated_types/src/dummy.d.ts"
        # to "test_module_generated_types/src/dummy.d.ts"
        # For scoped modules, the path will contain @<scope>/<module_name> which should shorten to
        # "@<scope>/<module_name>/src/dummy.d.ts"
        prefix_to_drop = paths.join(module_name, output_dir) + "/"

        return replace_prefix(short_path, prefix_to_drop, "")

def get_resources_dirs(resources):
    directories = _get_all_unique_directories(resources)

    filtered_variant_directories = []

    for directory in directories:
        # explicitly exclude any of the image variations that valdi accepts
        current_directory = _get_current_directory(directory)

        if current_directory not in ANDROID_RESOURCE_VARIANT_DIRECTORIES:
            filtered_variant_directories.append(current_directory)

    return filtered_variant_directories

def get_resources_dts_paths(module_name, resources):
    filtered_variant_directories = get_resources_dirs(resources)

    resources_paths = []
    for current_directory in filtered_variant_directories:
        # The compiler writes res.ts into generated_ts/ as a side effect (for VSCode/linter autocomplete).
        # It also emits res.d.ts into typescript/output/ via TypeScript compilation (for downstream modules).
        # Both must be declared outputs so they are cached and available to consumers.
        if current_directory == "res":
            ts_basename = "res.ts"
            dts_basename = "res.d.ts"
        else:
            ts_basename = paths.join("res", current_directory + ".ts")
            dts_basename = paths.join("res", current_directory + ".d.ts")

        # Declare the generated .ts file so it gets cached and downloaded by the linter sync
        resources_paths.append(paths.join(TYPESCRIPT_GENERATED_TS_DIR, module_name, ts_basename))

        # Declare the compiled .d.ts file for downstream TypeScript compilation
        resources_paths.append(paths.join(TYPESCRIPT_OUTPUT_DIR, module_name, dts_basename))

    return resources_paths

def infer_base_output_dir(declared_output_paths, resolved_outputs):
    """ Resolves the base output directory given the declared outputs

     When using actions.declare_file() or actions.declare_directory(), Bazel will
     give us a relative path from PWD where we can write the file or directory. That
     relative path will have some arbitrary path components that we can't know in advance,
     for example: actions.declare_file('test.txt') might return 'bazel-out/bla/foo/bar/test.txt'
     _infer_base_output_dir aims to resolve that directory, in this example that would be
     'bazel-out/bla/foo/bar'. We pass that directory to the compiler so that it can write
     files into it.
    """

    base_output_dir = None

    for (declared_output_path, resolved_output) in zip(declared_output_paths, resolved_outputs):
        resolved_base_output_dir = _get_base_output_dir(declared_output_path, resolved_output.path)
        if not base_output_dir:
            base_output_dir = resolved_base_output_dir
        elif base_output_dir != resolved_base_output_dir:
            fail("Base output dir mismatch between {} and {}".format(base_output_dir, resolved_base_output_dir))

    return base_output_dir

def resolve_module_dir_and_name(label):
    module_directory = None
    if label.workspace_root:
        module_directory = paths.join(label.workspace_root, label.package)
    else:
        module_directory = label.package

    # TODO(simon): Remove this assumption. We here resolve the module name
    # based on its directory.
    module_name = paths.basename(module_directory)

    return (module_directory, module_name)
