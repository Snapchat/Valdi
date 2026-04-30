load("@build_bazel_rules_apple//apple:macos.bzl", "macos_application")
load("//bzl:expand_template.bzl", "expand_template")

def valdi_macos_application(
        name,
        title,
        root_component_path,
        bundle_id,
        window_width,
        window_height,
        window_resizable,
        deps = []):
    main_target = "{}_maingen".format(name)
    plist_target = "{}_plist".format(name)
    src_target = "{}_src".format(name)

    expand_template(
        name = plist_target,
        src = "@valdi//bzl/valdi/app_templates:Info.plist.tpl",
        output = "Info.plist",
        substitutions = {
            "@VALDI_BUNDLE_IDENTIFIER@": bundle_id,
        },
    )

    expand_template(
        name = main_target,
        src = "@valdi//bzl/valdi/app_templates:macos_main.m.tpl",
        output = "main_macos.m",
        substitutions = {
            "@VALDI_ROOT_COMPONENT_PATH@": root_component_path,
            "@VALDI_TITLE@": title,
            "@VALDI_WINDOW_WIDTH@": str(window_width),
            "@VALDI_WINDOW_HEIGHT@": str(window_height),
            "@VALDI_WINDOW_RESIZABLE@": "true" if (window_resizable) else "false",
        },
    )

    native.objc_library(
        name = src_target,
        deps = [
            "@valdi//valdi",
        ] + deps,
        srcs = [":{}".format(main_target)],
    )

    macos_application(
        name = name,
        bundle_id = bundle_id,
        bundle_name = title,
        infoplists = [":{}".format(plist_target)],
        deps = [":{}".format(src_target)],
        minimum_os_version = "15.0",
        tags = ["valdi_macos_application"],
        visibility = ["//visibility:public"],
    )
