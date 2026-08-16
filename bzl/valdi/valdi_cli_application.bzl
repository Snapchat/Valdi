load("//bzl:expand_template.bzl", "expand_template")
load("//bzl/valdi:suffixed_deps.bzl", "get_suffixed_deps")

_HTTP_INCLUDE = "#include \"valdi/standalone_http/CurlHTTPRequestManager.hpp\""

_HTTP_MANAGER = ", Valdi::makeCurlHTTPRequestManager()"

def valdi_cli_application(
        name,
        script_path,
        visibility = ["//visibility:public"],
        enable_http = False,
        deps = []):
    main_target = "{}_main".format(name)

    expand_template(
        name = main_target,
        src = "@valdi//bzl/valdi/app_templates:cli_main.cpp.tpl",
        output = "main.cpp",
        substitutions = {
            "@VALDI_SCRIPT_PATH@": script_path,
            "@VALDI_HTTP_INCLUDE@": _HTTP_INCLUDE if enable_http else "",
            "@VALDI_HTTP_MANAGER@": _HTTP_MANAGER if enable_http else "",
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
