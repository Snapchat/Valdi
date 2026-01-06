load("@bazel_skylib//lib:paths.bzl", "paths")
load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load(":valdi_compiled.bzl", "ValdiModuleInfo")

def _collect_target_runfiles(target):
    # Collect target and its dependencies module infos
    module_infos = [target[ValdiModuleInfo]]
    module_infos += [d[ValdiModuleInfo] for d in target[ValdiModuleInfo].deps.to_list()]

    # Extract valdi modules and source maps
    valdi_modules = [m.android_debug_valdimodule for m in module_infos if m.android_debug_valdimodule]
    source_maps = [m.android_debug_sourcemaps for m in module_infos if m.android_debug_sourcemaps]
    return valdi_modules + source_maps

def _valdi_test_impl(ctx):
    standalone_binary = ctx.executable._valdi_standalone_binary

    path_to_module = ctx.label.package
    test_paths = [f for f in ctx.files.srcs if paths.relativize(f.dirname, path_to_module).startswith("test")]

    has_tests = len(test_paths) > 0

    # Check if code coverage is enabled via `bazel coverage` command or explicit flag
    code_coverage_enabled = ctx.configuration.coverage_enabled or ctx.attr.code_coverage[BuildSettingInfo].value

    # Always include standalone module runfiles
    valdimodules = _collect_target_runfiles(ctx.attr.target)
    valdimodules += _collect_target_runfiles(ctx.attr._valdi_standalone)
    module_paths = ["--module_path {}".format(f.short_path) for f in valdimodules]

    test_script = ctx.actions.declare_file("test_wrapper.sh")
    if has_tests:
        js_engine = ctx.attr.js_engine[BuildSettingInfo].value
        cmd = "{} --js_engine {} --script_path {}".format(standalone_binary.short_path, js_engine, ctx.attr.test_runner)
        if ctx.attr.hot_reload:
            cmd += " --debugger_service --hot_reload"

        if module_paths:
            cmd += " " + " ".join(module_paths)

        cmd += " -- --allow_incomplete_test_run --include_module {}".format(ctx.attr.target[ValdiModuleInfo].name)
        cmd += " --junit_report_filename `basename $XML_OUTPUT_FILE` --junit_report_output_dir `dirname $XML_OUTPUT_FILE`"

        # Add code coverage flags when enabled
        if code_coverage_enabled:
            cmd += " --code_coverage"

            # Use COVERAGE_OUTPUT_FILE if set by Bazel (when using `bazel coverage`),
            # otherwise fallback to a file in TEST_UNDECLARED_OUTPUTS_DIR
            cmd += " --code_coverage_result \"${COVERAGE_OUTPUT_FILE:-$TEST_UNDECLARED_OUTPUTS_DIR/coverage.dat}\""
    else:
        cmd = "echo 'No tests to run'"

    ctx.actions.write(
        output = test_script,
        content = cmd,
        is_executable = True,
    )

    runfiles = ctx.runfiles(
        files = [standalone_binary] + valdimodules,
    )

    return [DefaultInfo(executable = test_script, files = depset([standalone_binary] + valdimodules), runfiles = runfiles)]

valdi_test = rule(
    implementation = _valdi_test_impl,
    test = True,
    fragments = ["coverage"],
    attrs = {
        "target": attr.label(
            mandatory = True,
            doc = "The target to test",
        ),
        "test_runner": attr.string(
            default = "valdi_standalone/src/TestsRunner",
            doc = "The test runner script to evaluate",
        ),
        "hot_reload": attr.bool(
            default = False,
            doc = "Whether to enable hot reload and debugger service",
        ),
        "srcs": attr.label_list(
            doc = "List of sources for this module",
            mandatory = True,
            allow_files = [".js", ".ts", ".tsx", ".json"],
        ),
        "js_engine": attr.label(
            default = Label("@valdi//bzl/valdi:js_engine"),
            doc = "The JS engine to use to run the tests",
        ),
        "code_coverage": attr.label(
            default = Label("@valdi//bzl/valdi:code_coverage_enabled"),
            doc = "Enable code coverage collection during tests",
        ),
        "_valdi_standalone": attr.label(
            default = Label("@valdi//src/valdi_modules/src/valdi/valdi_standalone"),
            doc = "The Valdi Standalone target to be added to all Valdi targets",
        ),
        "_valdi_standalone_binary": attr.label(
            executable = True,
            cfg = "exec",
            default = Label("@valdi_toolchain//:valdi_standalone"),
            doc = "The Valdi Standalone binary used to run the test",
        ),
    },
)
