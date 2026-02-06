load("@build_bazel_rules_swift//swift:swift.bzl", "swift_interop_hint")
load("@rules_hdrs//hmap:hmap.bzl", "headermap")
load("@rules_hdrs//umbrella_header:umbrella_header.bzl", "umbrella_header")
load("@snap_macros//:library/snap_client_ios_library.bzl", "snap_client_ios_library")

OBJC_ONLY_FLAGS = [
    "-fdiagnostics-color",
    "-fno-aligned-allocation",
]

OBJCPP_ONLY_FLAGS = [
    "-ObjC++",
    "-std=c++20",
    "-fno-c++-static-destructors",
]

OBJC_FLAGS = OBJC_ONLY_FLAGS + OBJCPP_ONLY_FLAGS

def client_objc_library(
        *,
        name,
        deps = [],
        implementation_deps = [],
        generated_objects = [],
        srcs = [],
        hdrs = [],
        copts = [],
        includes = [],
        data = [],
        sdk_frameworks = [],
        enable_swift_interop = False,
        module_name = None,
        alwayslink = False,
        tags = [],
        visibility = [],
        enable_objcpp = True,
        defines = [],
        generate_hmaps = True,
        generate_umbrella_header = True):
    base_copts = OBJC_FLAGS if enable_objcpp else OBJC_ONLY_FLAGS

    hmap_deps = []
    private_hmap_deps = []
    hmap_copts = []
    if hdrs:
        module_name = module_name or name
        if generate_hmaps:
            # setup headermaps
            hmap_name = name + "_hmap"
            headermap(
                name = hmap_name,
                cc_hdrs = hdrs,
                cc_includes = includes,
                tags = ["manual"],
            )
            hmap_deps.append(native.package_relative_label(hmap_name))

        if generate_umbrella_header:
            internal_umbrella_header_name = name + "_umbrella.h"
            umbrella_header(
                name = internal_umbrella_header_name,
                hdrs = hdrs,
                umbrella_header_name = module_name + "-Swift",
                tags = ["manual"],
            )
            umbrella_headers = [":" + internal_umbrella_header_name]
        else:
            umbrella_headers = []

        # The umbrella header is a public header
        hdrs += umbrella_headers

    if srcs and generate_hmaps:
        private_hmap_name = name + "_private_hmap"
        headermap(
            name = private_hmap_name,
            cc_hdrs = srcs,  # srcs include private headers (filtering by extension is done on headermap rule level)
            cc_includes = includes,
            add_to_includes = False,
            tags = ["manual"],
        )
        private_hmap_deps.append(native.package_relative_label(private_hmap_name))
        hmap_copts.append("-I$(execpath :{})".format(private_hmap_name))

    hmap_copts.append("-I.")

    if enable_swift_interop:
        swift_interop_name = name + "_swift_interop_hint"
        swift_interop_hint(
            name = swift_interop_name,
            module_name = module_name,
            system_pcms = select({
                "@valdi//bzl/conditions:explicit_modules": [
                    "@build_bazel_rules_swift//system_sdks:system_sdks",
                ],
                "//conditions:default": [],
            }),
        )
        aspect_hints = [":" + swift_interop_name]
        clang_module_name = module_name or name
    else:
        aspect_hints = ["@build_bazel_rules_swift//swift:no_module"]
        clang_module_name = None

    snap_client_ios_library(
        name = name,
        deps = deps + hmap_deps,
        implementation_deps = implementation_deps + private_hmap_deps,
        generated_objects = generated_objects,
        srcs = srcs,
        hdrs = hdrs,
        copts = base_copts + copts + hmap_copts,
        data = data,
        sdk_frameworks = sdk_frameworks,
        aspect_hints = aspect_hints,
        module_name = clang_module_name,
        defines = defines,
        tags = tags,
        alwayslink = alwayslink,
        visibility = visibility,
    )
