load(
    "//bzl/valdi:valdi_application_icons_helper.bzl",
    "convert_icon",
    "is_application_icons",
    "make_application_icons",
    "toolbox_attr",
    "write_apple_contents_json",
)

def valdi_ios_application_icons(src):
    return make_application_icons("ios", src)

_IOS_ICON_ENTRIES = [
    ("iphone", "60x60", "3x", 180),
    ("iphone", "40x40", "2x", 80),
    ("iphone", "40x40", "3x", 120),
    ("iphone", "60x60", "2x", 120),
    ("iphone", "29x29", "2x", 58),
    ("iphone", "29x29", "1x", 29),
    ("iphone", "29x29", "3x", 87),
    ("iphone", "20x20", "2x", 40),
    ("iphone", "20x20", "3x", 60),
    ("ios-marketing", "1024x1024", "1x", 1024),
    ("ipad", "40x40", "2x", 80),
    ("ipad", "76x76", "2x", 152),
    ("ipad", "29x29", "2x", 58),
    ("ipad", "29x29", "1x", 29),
    ("ipad", "40x40", "1x", 40),
    ("ipad", "83.5x83.5", "2x", 167),
    ("ipad", "20x20", "1x", 20),
    ("ipad", "20x20", "2x", 40),
]

def _ios_application_icons_impl(ctx):
    icon_dir = "{}/Assets.xcassets/AppIcon.appiconset".format(ctx.label.name)
    outputs_by_size = {}
    images = []

    for idiom, point_size, scale, pixel_size in _IOS_ICON_ENTRIES:
        filename = "{}.png".format(pixel_size)
        if pixel_size not in outputs_by_size:
            output = ctx.actions.declare_file("{}/{}".format(icon_dir, filename))
            outputs_by_size[pixel_size] = output
            convert_icon(ctx, ctx.file.src, output, pixel_size)

        images.append({
            "expected-size": str(pixel_size),
            "filename": filename,
            "folder": "Assets.xcassets/AppIcon.appiconset/",
            "idiom": idiom,
            "scale": scale,
            "size": point_size,
        })

    contents = ctx.actions.declare_file("{}/Contents.json".format(icon_dir))
    write_apple_contents_json(ctx, contents, images)

    return [DefaultInfo(files = depset(list(outputs_by_size.values()) + [contents]))]

_ios_application_icons = rule(
    implementation = _ios_application_icons_impl,
    attrs = {
        "src": attr.label(allow_single_file = True, mandatory = True),
        "_toolbox": toolbox_attr(),
    },
)

def generate_valdi_ios_application_icons(name, app_icons):
    if app_icons == None:
        return app_icons

    if not is_application_icons(app_icons, "ios"):
        if hasattr(app_icons, "_valdi_application_icons"):
            fail("ios app_icons must be created with valdi_ios_application_icons()")
        return app_icons

    target_name = "{}_generated_app_icons".format(name)
    _ios_application_icons(
        name = target_name,
        src = app_icons.src,
    )
    return [":{}".format(target_name)]
