load("@build_bazel_rules_apple//apple:ios.bzl", "ios_application")
load("@build_bazel_rules_apple//apple:versioning.bzl", "apple_bundle_version")
load("//bzl:expand_template.bzl", "expand_template")

def make_short_version(version):
    components = version.split(".")

    if len(components) > 2:
        return "{}.{}".format(components[0], components[1])

    return version

def valdi_ios_application(
        name,
        title,
        root_component_path,
        bundle_id,
        info_plist = None,
        families = None,
        minimum_os_version = None,
        provisioning_profile = None,
        app_icons = None,
        version = None,
        deps = []):
    main_target = "{}_maingen".format(name)
    src_target = "{}_src".format(name)

    if not families:
        families = [
            "iphone",
            "ipad",
        ]

    if not minimum_os_version:
        minimum_os_version = "12.0"

    infoplists = []
    if info_plist:
        infoplists.append(info_plist)
    else:
        plist_target = "{}_plist".format(name)

        expand_template(
            name = plist_target,
            src = "@valdi//bzl/valdi/app_templates:Info.plist.tpl",
            output = "Info.plist",
            substitutions = {
                "@VALDI_BUNDLE_IDENTIFIER@": bundle_id,
            },
        )

        infoplists.append(":{}".format(plist_target))

    expand_template(
        name = main_target,
        src = "@valdi//bzl/valdi/app_templates:ios_main.m.tpl",
        output = "main_ios.m",
        substitutions = {
            "@VALDI_ROOT_COMPONENT_PATH@": root_component_path,
        },
    )

    native.objc_library(
        name = src_target,
        deps = [
            "@valdi//valdi",
        ] + deps,
        srcs = [":{}".format(main_target)],
    )

    resolved_version = None
    if version:
        version_name = "{}_version".format(name)
        apple_bundle_version(
            name = version_name,
            build_version = version,
            short_version_string = make_short_version(version),
        )
        resolved_version = ":{}".format(version_name)

    ios_application(
        name = name,
        bundle_id = bundle_id,
        bundle_name = title,
        families = families,
        infoplists = infoplists,
        deps = [":{}".format(src_target)],
        minimum_os_version = minimum_os_version,
        provisioning_profile = provisioning_profile,
        app_icons = app_icons,
        version = resolved_version,
        tags = ["valdi_ios_application"],
        visibility = ["//visibility:public"],
    )
