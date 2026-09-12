load("//bzl:expand_template.bzl", "expand_template")
load("//bzl/valdi:suffixed_deps.bzl", "get_suffixed_deps")

def valdi_cli_application(
        name,
        script_path,
        visibility = ["//visibility:public"],
        enable_http = False,
        deps = []):
    """ Builds a Valdi CLI application into a single self contained binary.

    Args:
        name: The name of the generated binary.
        script_path: The entry point script, as <module_name>/<path_without_extension>.
        visibility: The visibility of the Bazel target.
        enable_http: Links a libcurl backed HTTP client into the binary, which valdi_http needs at
            runtime. Off by default because it pulls in curl and BoringSSL, which an application
            making no network requests should not have to carry. Left off, requests reject with
            "No RequestManager set".
        deps: The Valdi modules to include in the build.
    """
    main_target = "{}_main".format(name)

    # Two whole templates rather than one with the manager spliced in, so both stay readable C++.
    template = "cli_main_http.cpp.tpl" if enable_http else "cli_main.cpp.tpl"

    expand_template(
        name = main_target,
        src = "@valdi//bzl/valdi/app_templates:" + template,
        output = "main.cpp",
        substitutions = {
            "@VALDI_SCRIPT_PATH@": script_path,
        },
    )

    native.cc_binary(
        name = name,
        linkshared = False,
        visibility = visibility,
        srcs = [":{}".format(main_target)],
        tags = ["valdi_cli_application"],
        deps = [
            "@valdi//valdi:cli_runner",
            "@valdi//src/valdi_modules/src/valdi/valdi_core:valdi_core_native",
        ] + (["@valdi//valdi:valdi_standalone_http"] if enable_http else []) +
            get_suffixed_deps(deps, "_native"),
    )
