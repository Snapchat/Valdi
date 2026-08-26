load("//bzl:expand_template.bzl", "expand_template")
load("//bzl/valdi:suffixed_deps.bzl", "get_suffixed_deps")
load("//bzl/valdi:valdi_collapse_web_paths.bzl", "collapse_native_paths", "collapse_web_paths", "generate_register_native_modules")
load("//bzl/valdi:valdi_protodecl_to_js.bzl", "collapse_protodecl_paths", "protodecl_to_js_dir")

def _web_api_version_json_impl(ctx):
    output = ctx.actions.declare_file(ctx.attr.output)
    version_files = ctx.files._api_version_file
    if len(version_files) > 1:
        fail("Expected at most one Valdi API version file, got {}".format(len(version_files)))

    if version_files:
        ctx.actions.symlink(output = output, target_file = version_files[0])
    else:
        ctx.actions.write(output = output, content = "0\n")

    return [DefaultInfo(files = depset([output]))]

web_api_version_json = rule(
    implementation = _web_api_version_json_impl,
    attrs = {
        "output": attr.string(mandatory = True),
        "_api_version_file": attr.label(
            default = Label("//bzl/valdi:api_version_file"),
            allow_files = True,
        ),
    },
    doc = "Generate JSON containing the configured Valdi native API version.",
)

def valdi_web_package(
        name,
        deps,
        package_name = None,
        npm_scope = "",
        npm_version = "1.0.0",
        exclude_jsx_global_declaration = False,
        modules = None,
        tags = []):
    if package_name == None:
        package_name = name
    modules = modules or deps

    resolved_package_name = package_name
    if npm_scope:
        resolved_package_name = npm_scope + "/" + resolved_package_name

    package_json_name = "{}_package_json".format(name)
    expand_template(
        name = package_json_name,
        src = "@valdi//bzl/valdi:package.json.tmpl",
        output = "{}.package.json".format(name),
        substitutions = {
            "${name}": resolved_package_name,
            "${version}": npm_version,
        },
    )

    api_version_name = "{}_api_version".format(name)
    web_api_version_json(
        name = api_version_name,
        output = "{}.valdi_api_version.json".format(name),
    )

    protodecl_to_js_dir(
        name = "{}_protodecl_js".format(name),
        srcs = get_suffixed_deps(deps, "_web_protodecl"),
    )

    collapse_protodecl_paths(
        name = "{}_protodecl_collapsed".format(name),
        srcs = [":{}_protodecl_js".format(name)],
    )

    collapse_native_paths(
        name = "{}_web_native".format(name),
        srcs = get_suffixed_deps(deps, "_all_web_deps"),
    )

    generate_register_native_modules(
        name = "{}_register_native_modules".format(name),
        srcs = get_suffixed_deps(deps, "_all_web_deps"),
        package_name = resolved_package_name,
        modules = modules,
    )

    native.filegroup(
        name = "{}_glob".format(name),
        srcs = get_suffixed_deps(deps, "_web_srcs_filegroup") + [
            ":{}_protodecl_collapsed".format(name),
            ":{}_web_native".format(name),
            ":{}_register_native_modules".format(name),
            ":{}".format(package_json_name),
            ":{}".format(api_version_name),
        ],
    )

    collapse_web_paths(
        name = name,
        srcs = [":{}_glob".format(name)],
        package_name = resolved_package_name,
        exclude_jsx_global_declaration = exclude_jsx_global_declaration,
        modules = modules,
        tags = tags,
    )

    return resolved_package_name
