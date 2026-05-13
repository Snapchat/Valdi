load(
    "//bzl/valdi:valdi_application_icons_helper.bzl",
    "convert_icon",
    "is_application_icons",
    "make_application_icons",
    "toolbox_attr",
    "write_apple_contents_json",
)

def valdi_macos_application_icons(src):
    return make_application_icons("macos", src)

_MACOS_ICON_ENTRIES = [
    ("mac", "16x16", "1x", 16),
    ("mac", "16x16", "2x", 32),
    ("mac", "32x32", "1x", 32),
    ("mac", "32x32", "2x", 64),
    ("mac", "128x128", "1x", 128),
    ("mac", "128x128", "2x", 256),
    ("mac", "256x256", "1x", 256),
    ("mac", "256x256", "2x", 512),
    ("mac", "512x512", "1x", 512),
    ("mac", "512x512", "2x", 1024),
]

def _macos_application_icons_impl(ctx):
    icon_dir = "{}/Assets.xcassets/AppIcon.appiconset".format(ctx.label.name)
    outputs_by_size = {}
    images = []

    for idiom, point_size, scale, pixel_size in _MACOS_ICON_ENTRIES:
        filename = "{}.png".format(pixel_size)
        if pixel_size not in outputs_by_size:
            output = ctx.actions.declare_file("{}/{}".format(icon_dir, filename))
            outputs_by_size[pixel_size] = output
            convert_icon(ctx, ctx.file.src, output, pixel_size)

        images.append({
            "filename": filename,
            "idiom": idiom,
            "scale": scale,
            "size": point_size,
        })

    contents = ctx.actions.declare_file("{}/Contents.json".format(icon_dir))
    write_apple_contents_json(ctx, contents, images)

    return [DefaultInfo(files = depset(list(outputs_by_size.values()) + [contents]))]

_macos_application_icons = rule(
    implementation = _macos_application_icons_impl,
    attrs = {
        "src": attr.label(allow_single_file = True, mandatory = True),
        "_toolbox": toolbox_attr(),
    },
)

def generate_valdi_macos_application_icons(name, app_icons):
    if app_icons == None:
        return app_icons

    if not is_application_icons(app_icons, "macos"):
        if hasattr(app_icons, "_valdi_application_icons"):
            fail("macos app_icons must be created with valdi_macos_application_icons()")
        return app_icons

    target_name = "{}_generated_app_icons".format(name)
    _macos_application_icons(
        name = target_name,
        src = app_icons.src,
    )
    return [":{}".format(target_name)]
