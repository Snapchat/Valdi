# Shim to use different dependencies for open source and internal valdi

INTERNAL_BUILD = False

def valdi_compiler_companion_files():
    if INTERNAL_BUILD:
        return ["@valdi_compiler_companion//:all_files"]
    return native.glob([
        "compiler_companion/**/*.js",
        "compiler_companion/**/*.js.map",
        "compiler_companion/**/*.node",
    ])

def bundle_js():
    if INTERNAL_BUILD:
        return "@valdi_compiler_companion//:bundle.js"
    return "//compiler_companion:bundle.js"

def jscore_library():
    if INTERNAL_BUILD:
        return "@jscore_libs//:linux/x86_64/libjsc.so"
    return None
