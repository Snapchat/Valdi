load("@valdi_npm//compiler/companion:webpack/package_json.bzl", webpack_bin = "bin")
load("//bzl:expand_template.bzl", "expand_template")
load("//bzl/valdi:valdi_web_package.bzl", "valdi_web_package", "web_api_version_json")

_WEB_RENDERER_DEP = "@valdi//src/valdi_modules/src/valdi/web_renderer"

def _deps_with_web_renderer(deps):
    if _WEB_RENDERER_DEP in deps:
        return deps
    return deps + [_WEB_RENDERER_DEP]

def _parse_root_component_path(root_component_path):
    parts = root_component_path.split("@")
    if len(parts) != 2 or not parts[0] or not parts[1]:
        fail("root_component_path must be in the form Component@ModulePath")
    return struct(
        component = parts[0],
        module_path = parts[1],
    )

def _package_relative_path(path):
    package_name = native.package_name()
    if package_name:
        return "{}/{}".format(package_name, path)
    return path

def _web_static_site_zip_impl(ctx):
    dist_outputs = ctx.attr.dist[DefaultInfo].files.to_list()
    if len(dist_outputs) != 1:
        fail("Expected exactly one dist directory from {}, got {}".format(ctx.attr.dist.label, len(dist_outputs)))

    dist = dist_outputs[0]
    site_dir = ctx.actions.declare_directory("{}_site".format(ctx.label.name))
    ctx.actions.run_shell(
        inputs = [ctx.file.html, dist],
        outputs = [site_dir],
        command = """
set -euo pipefail
rm -rf "{site_dir}"
mkdir -p "{site_dir}"
cp "{html}" "{site_dir}/index.html"
cp -R "{dist}/." "{site_dir}/"
""".format(
            dist = dist.path,
            html = ctx.file.html.path,
            site_dir = site_dir.path,
        ),
        mnemonic = "ValdiWebStaticSite",
        progress_message = "Assembling Valdi web static site %{label}",
    )

    output_zip = ctx.actions.declare_file("{}.zip".format(ctx.label.name))
    args = ctx.actions.args()
    args.add(ctx.executable._zipper.path)
    args.add(site_dir.path)
    args.add(output_zip.path)
    args.add_all([site_dir])

    ctx.actions.run(
        outputs = [output_zip],
        inputs = [site_dir],
        executable = ctx.executable._zip_relative,
        tools = [ctx.executable._zipper],
        arguments = [args],
        mnemonic = "ValdiWebZip",
        progress_message = "Zipping Valdi web static site %{label}",
    )

    return [DefaultInfo(files = depset([output_zip]))]

_web_static_site_zip = rule(
    implementation = _web_static_site_zip_impl,
    attrs = {
        "dist": attr.label(mandatory = True),
        "html": attr.label(allow_single_file = True, mandatory = True),
        "_zip_relative": attr.label(
            default = "//bzl/android:zip_relative",
            executable = True,
            cfg = "exec",
        ),
        "_zipper": attr.label(
            default = "@bazel_tools//tools/zip:zipper",
            allow_single_file = True,
            executable = True,
            cfg = "exec",
        ),
    },
)

def valdi_web_application(
        name,
        title,
        root_component_path,
        deps,
        web_package_name = None,
        visibility = ["//visibility:public"]):
    if web_package_name == None:
        web_package_name = "{}_npm".format(name)

    parsed_root_component_path = _parse_root_component_path(root_component_path)
    web_deps = _deps_with_web_renderer(deps)
    resolved_package_name = valdi_web_package(
        name = web_package_name,
        deps = web_deps,
        package_name = web_package_name,
    )

    build_dir = "{}_web_build".format(name)
    html_target = "{}_html".format(name)
    entry_target = "{}_entry".format(name)
    path_shim_target = "{}_path_browserify_shim".format(name)
    bytes_loader_target = "{}_bytes_loader".format(name)
    api_version_target = "{}_api_version".format(name)
    webpack_config_target = "{}_webpack_config".format(name)
    webpack_target = "{}_webpack".format(name)
    dist_dir = "{}/dist".format(build_dir)

    expand_template(
        name = html_target,
        src = "@valdi//bzl/valdi/app_templates:web_index.html.tpl",
        output = "{}/index.html".format(build_dir),
        substitutions = {
            "@VALDI_TITLE@": title,
        },
    )

    expand_template(
        name = entry_target,
        src = "@valdi//bzl/valdi/app_templates:web_index.js.tpl",
        output = "{}/src/index.js".format(build_dir),
        substitutions = {
            "@VALDI_COMPONENT_NAME@": parsed_root_component_path.component,
            "@VALDI_PACKAGE_NAME@": resolved_package_name,
            "@VALDI_ROOT_MODULE_PATH@": parsed_root_component_path.module_path,
        },
    )

    expand_template(
        name = path_shim_target,
        src = "@valdi//bzl/valdi/app_templates:web_path_browserify_shim.js.tpl",
        output = "{}/src/path-browserify-shim.js".format(build_dir),
        substitutions = {},
    )

    expand_template(
        name = bytes_loader_target,
        src = "@valdi//bzl/valdi/app_templates:web_bytes_loader.js.tpl",
        output = "{}/src/bytes-loader.js".format(build_dir),
        substitutions = {},
    )

    web_api_version_json(
        name = api_version_target,
        output = "{}/src/valdi_api_version.json".format(build_dir),
    )

    expand_template(
        name = webpack_config_target,
        src = "@valdi//bzl/valdi/app_templates:web_webpack.config.js.tpl",
        output = "{}/webpack.config.js".format(build_dir),
        substitutions = {
            "@VALDI_PACKAGE_NAME@": resolved_package_name,
            "@VALDI_PACKAGE_PATH@": "../{}".format(web_package_name),
        },
    )

    webpack_bin.webpack(
        name = webpack_target,
        srcs = [
            ":{}".format(entry_target),
            ":{}".format(path_shim_target),
            ":{}".format(bytes_loader_target),
            ":{}".format(api_version_target),
            ":{}".format(webpack_config_target),
            ":{}".format(web_package_name),
            "@valdi//bzl/valdi/npm:node_modules",
        ],
        out_dirs = [dist_dir],
        chdir = _package_relative_path(build_dir),
        silent_on_success = False,
    )

    _web_static_site_zip(
        name = name,
        dist = ":{}".format(webpack_target),
        html = ":{}".format(html_target),
        tags = ["valdi_web_application"],
        visibility = visibility,
    )
