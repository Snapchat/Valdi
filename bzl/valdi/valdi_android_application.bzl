load("@rules_android//rules:rules.bzl", "android_binary")
load("@rules_kotlin//kotlin:android.bzl", "kt_android_library")
load("@valdi//valdi:valdi.bzl", "valdi_android_aar")
load("//bzl:expand_template.bzl", "expand_template")
load("//bzl/valdi/source_set:utils.bzl", "source_set_select")

def _make_xml_compound_substitution(key_values):
    output = []

    for keyval in key_values:
        output.append('{}="{}"'.format(keyval[0], keyval[1]))

    return " ".join(output)

def valdi_android_application(
        name,
        title,
        root_component_path,
        package,
        app_manifest = None,
        assets = None,
        assets_dir = None,
        resource_files = None,
        icon_name = None,
        round_icon_name = None,
        activity_theme_name = None,
        deps = [],
        native_deps = []):
    src_target = "{}_src".format(name)
    src_activity_target = "{}_activitygen".format(name)
    aar_target = "{}_aar".format(name)

    expand_template(
        name = src_activity_target,
        src = "@valdi//bzl/valdi/app_templates:StartActivity.kt.tpl",
        output = "StartActivity.kt",
        substitutions = {
            "@VALDI_APP_PACKAGE@": package,
            "@VALDI_ROOT_COMPONENT_PATH@": root_component_path,
        },
    )

    resolved_app_manifest = app_manifest

    if not app_manifest:
        app_attributes = []

        if icon_name:
            app_attributes.append(("android:icon", "@mipmap/{}".format(icon_name)))
        if round_icon_name:
            app_attributes.append(("android:roundIcon", "@mipmap/{}".format(round_icon_name)))

        activity_attributes = []

        if activity_theme_name:
            activity_attributes.append(("android:theme", "@style/{}".format(activity_theme_name)))

        app_manifest_target = "{}_app_manifest".format(name)
        resolved_app_manifest = ":{}".format(app_manifest_target)
        expand_template(
            name = app_manifest_target,
            src = source_set_select(
                debug = "@valdi//bzl/valdi/app_templates:AndroidDebugAppManifest.xml.tpl",
                release = "@valdi//bzl/valdi/app_templates:AndroidReleaseAppManifest.xml.tpl",
            ),
            output = "AndroidAppManifest.xml",
            substitutions = {
                "@VALDI_APP_PACKAGE@": package,
                "@VALDI_APP_NAME@": title,
                "@VALDI_APPLICATION_ATTRIBUTES@": _make_xml_compound_substitution(app_attributes),
                "@VALDI_ACTIVITY_ATTRIBUTES@": _make_xml_compound_substitution(activity_attributes),
            },
        )

    kt_android_library(
        name = src_target,
        srcs = [":{}".format(src_activity_target)],
        custom_package = package,
        manifest = "@valdi//bzl/valdi/app_templates:AndroidLibManifest.xml",
        deps = [
            "@valdi//valdi:valdi_android_support",
            "@android_mvn//:androidx_appcompat_appcompat",
        ] + deps,
    )

    valdi_android_aar(
        name = aar_target,
        native_deps = [
            "@valdi//valdi",
        ] + native_deps,
        so_name = "libvaldi.so",
    )

    android_binary(
        name = name,
        custom_package = package,
        manifest = resolved_app_manifest,
        multidex = "native",
        assets = assets,
        assets_dir = assets_dir,
        resource_files = resource_files,
        tags = ["valdi_android_application"],
        deps = [
            ":{}".format(src_target),
            ":{}_import".format(aar_target),
        ],
    )
