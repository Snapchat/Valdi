load(
    "//bzl/valdi:valdi_android_application_icons.bzl",
    "valdi_android_application_icons",
)
load(
    "//bzl/valdi:valdi_application_icons_helper.bzl",
    "make_application_icons",
)
load(
    "//bzl/valdi:valdi_ios_application_icons.bzl",
    "valdi_ios_application_icons",
)
load(
    "//bzl/valdi:valdi_macos_application_icons.bzl",
    "valdi_macos_application_icons",
)

def valdi_application_icons(src, round_src = None):
    return make_application_icons("all", src, round_src = round_src)
