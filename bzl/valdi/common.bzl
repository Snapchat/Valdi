load("@bazel_skylib//lib:paths.bzl", "paths")

# These paths let us find/declare the expected output locations

_ANDROID_OUTPUT_BASE = "android"
_ANDROID_DEBUG_ONLY_BASE = paths.join(_ANDROID_OUTPUT_BASE, "debug")
_ANDROID_RELEASE_READY_BASE = paths.join(_ANDROID_OUTPUT_BASE, "release")
_ANDROID_METADATA_BASE = _ANDROID_OUTPUT_BASE  # metadata is written to the base directory for Android

_ANDROID_OUTPUT_STRINGS_BASE_DIR = paths.join(_ANDROID_OUTPUT_BASE, "strings")

_IOS_OUTPUT_BASE = "ios"
_IOS_DEBUG_ONLY_BASE = paths.join(_IOS_OUTPUT_BASE, "debug")
_IOS_RELEASE_READY_BASE = paths.join(_IOS_OUTPUT_BASE, "release")
_IOS_METADATA_BASE = paths.join(_IOS_OUTPUT_BASE, "metadata")

_IOS_OUTPUT_STRINGS_BASE_DIR = paths.join(_IOS_OUTPUT_BASE, "strings")

_IOS_API_NAME_SUFFIX = "Types"

_IOS_SWIFT_SUFFIX = "Swift"

_WEB_OUTPUT_BASE = "web"
_WEB_DEBUG_ONLY_BASE = paths.join(_WEB_OUTPUT_BASE, "debug")
_WEB_RELEASE_READY_BASE = paths.join(_WEB_OUTPUT_BASE, "release")

_CPP_OUTPUT_BASE = "cpp"
_CPP_DEBUG_ONLY_BASE = paths.join(_CPP_OUTPUT_BASE, "debug")
_CPP_RELEASE_READY_BASE = paths.join(_CPP_OUTPUT_BASE, "release")
_CPP_METADATA_BASE = _CPP_OUTPUT_BASE  # metadata is written to the base directory for C++

# NOTE(vfomin): this should probably go into the config file
_IOS_DEFAULT_MODULE_PREFIX = "SCC"

_NODE_MODULES_BASE = "src/valdi_modules/node_modules"

################
##
## Exported symbols
##
################

IOS_OUTPUT_BASE = _IOS_OUTPUT_BASE
ANDROID_OUTPUT_STRINGS_BASE_DIR = _ANDROID_OUTPUT_STRINGS_BASE_DIR
IOS_OUTPUT_STRINGS_BASE_DIR = _IOS_OUTPUT_STRINGS_BASE_DIR

IOS_API_NAME_SUFFIX = _IOS_API_NAME_SUFFIX
IOS_SWIFT_SUFFIX = _IOS_SWIFT_SUFFIX
IOS_DEFAULT_MODULE_NAME_PREFIX = _IOS_DEFAULT_MODULE_PREFIX

NODE_MODULES_BASE = _NODE_MODULES_BASE

# Contains all supported variant directories
# Reference ImageResolver for the correct list - https://github.com/Snapchat/Valdi/blob/main/compiler/compiler/Compiler/Sources/Images/ImageVariantResolver.swift
ANDROID_RESOURCE_VARIANT_DIRECTORIES = [
    "drawable-mdpi",
    "drawable-hdpi",
    "drawable-xhdpi",
    "drawable-xxhdpi",
    "drawable-xxxhdpi",
]

BUILD_DIR = ".valdi_build/compile"
TYPESCRIPT_OUTPUT_DIR = paths.join(BUILD_DIR, "typescript/output")
TYPESCRIPT_GENERATED_TS_DIR = paths.join(BUILD_DIR, "generated_ts")
TYPESCRIPT_DUMPED_SYMBOLS_DIR = paths.join(BUILD_DIR, "typescript/dumped_symbols")

def base_relative_dir(platform, output_target, relative_dir):
    """Helper function for constructing paths relative to the _BASE_DIR.

    Args:
        platform: The platform (android, ios, web, or cpp).
        output_target: The output target (debug or release).
        relative_dir: The relative directory.

    Returns:
        The constructed path relative to the _BASE_DIR.
    """
    if platform not in ["android", "ios", "web", "cpp"]:
        fail("Unexpected platform: {platform}".format(platform = platform))

    if output_target not in ["debug", "release", "metadata"]:
        fail("Unexpected output_target: {output_target}".format(output_target = output_target))

    if output_target == "debug":
        base = _IOS_DEBUG_ONLY_BASE
        if platform == "android":
            base = _ANDROID_DEBUG_ONLY_BASE
        elif platform == "web":
            base = _WEB_DEBUG_ONLY_BASE
        elif platform == "cpp":
            base = _CPP_DEBUG_ONLY_BASE
    elif output_target == "release":
        base = _IOS_RELEASE_READY_BASE
        if platform == "android":
            base = _ANDROID_RELEASE_READY_BASE
        elif platform == "web":
            base = _WEB_RELEASE_READY_BASE
        elif platform == "cpp":
            base = _CPP_RELEASE_READY_BASE
    elif platform == "android":
        base = _ANDROID_METADATA_BASE
    elif platform == "cpp":
        base = _CPP_METADATA_BASE
    else:
        base = _IOS_METADATA_BASE
    return paths.join(base, relative_dir)
