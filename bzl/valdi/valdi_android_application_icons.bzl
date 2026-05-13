load(
    "//bzl/valdi:valdi_application_icons_helper.bzl",
    "convert_icon",
    "is_application_icons",
    "make_application_icons",
    "toolbox_attr",
)

def valdi_android_application_icons(src, round_src = None):
    return make_application_icons("android", src, round_src = round_src)

_ANDROID_ICON_DENSITIES = [
    ("mipmap-mdpi", 48),
    ("mipmap-hdpi", 72),
    ("mipmap-xhdpi", 96),
    ("mipmap-xxhdpi", 144),
    ("mipmap-xxxhdpi", 192),
]

def _android_application_icons_impl(ctx):
    outputs = []

    for density, pixel_size in _ANDROID_ICON_DENSITIES:
        output = ctx.actions.declare_file("{}/{}/app_icon.png".format(ctx.attr.resource_root, density))
        outputs.append(output)
        convert_icon(ctx, ctx.file.src, output, pixel_size)

        round_src = ctx.file.round_src if ctx.file.round_src else ctx.file.src
        round_output = ctx.actions.declare_file("{}/{}/round_app_icon.png".format(ctx.attr.resource_root, density))
        outputs.append(round_output)
        convert_icon(ctx, round_src, round_output, pixel_size, round_icon = not ctx.file.round_src)

    return [DefaultInfo(files = depset(outputs))]

_android_application_icons = rule(
    implementation = _android_application_icons_impl,
    attrs = {
        "resource_root": attr.string(mandatory = True),
        "src": attr.label(allow_single_file = True, mandatory = True),
        "round_src": attr.label(allow_single_file = True),
        "_toolbox": toolbox_attr(),
    },
)

_ANDROID_RESOURCE_DIR_PREFIXES = [
    "anim",
    "animator",
    "color",
    "drawable",
    "font",
    "layout",
    "menu",
    "mipmap",
    "raw",
    "transition",
    "values",
    "xml",
]

def _is_android_resource_dir(path_segment):
    for prefix in _ANDROID_RESOURCE_DIR_PREFIXES:
        if path_segment == prefix or path_segment.startswith("{}-".format(prefix)):
            return True

    return False

def _infer_android_resource_root(resource_files, default_root):
    for resource_file in resource_files or []:
        if type(resource_file) != "string":
            continue

        path_segments = resource_file.split("/")
        for index, path_segment in enumerate(path_segments):
            if _is_android_resource_dir(path_segment):
                return "/".join(path_segments[:index])

    return default_root

def generate_valdi_android_application_icons(name, app_icons, resource_files, icon_name, round_icon_name):
    if app_icons == None:
        return struct(
            resource_files = resource_files,
            icon_name = icon_name,
            round_icon_name = round_icon_name,
        )

    if not is_application_icons(app_icons, "android"):
        fail("android app_icons must be created with valdi_android_application_icons()")

    target_name = "{}_generated_app_icons".format(name)
    resource_root = _infer_android_resource_root(resource_files, target_name)
    kwargs = {
        "name": target_name,
        "resource_root": resource_root,
        "src": app_icons.src,
    }
    if app_icons.round_src:
        kwargs["round_src"] = app_icons.round_src
    _android_application_icons(**kwargs)

    resolved_resource_files = list(resource_files or [])
    resolved_resource_files.append(":{}".format(target_name))

    return struct(
        resource_files = resolved_resource_files,
        icon_name = icon_name or "app_icon",
        round_icon_name = round_icon_name or "round_app_icon",
    )
