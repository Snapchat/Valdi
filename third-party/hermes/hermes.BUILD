load("@bazel_skylib//lib:selects.bzl", "selects")
load("@valdi//third-party/hermes:cmake_configure.bzl", "cmake_configure")

selects.config_setting_group(
    name = "apple",
    match_any = [
        "@platforms//os:ios",
        "@valdi//bzl/conditions:macos",
    ],
)

COMPILER_FLAGS_COMPAT = [
    "-O3",  # Always set optimization on
    "-DNDEBUG",
    "-Wno-deprecated-volatile",
    "-Wno-parentheses",
    "-Wno-unused-label",
    "-Wno-shorten-64-to-32",
    "-Wno-deprecated-this-capture",
    "-Wno-deprecated-pragma",
    "-Wno-unused-variable",
    "-Wno-implicit-fallthrough",
    "-Wno-nontrivial-memcall",
    "-Wno-unknown-warning-option",
]

COMPILER_FLAGS = COMPILER_FLAGS_COMPAT + [
    "-fno-exceptions",
]

DEFINES_BASE = [
    "HERMESVM_ALLOW_COMPRESSED_POINTERS",
    "HERMESVM_ALLOW_CONCURRENT_GC",
    "HERMESVM_ALLOW_INLINE_ASM",
    "HERMESVM_GC_HADES",
    "HERMESVM_HEAP_SEGMENT_SIZE_KB=4096",
    "HERMESVM_INDIRECT_THREADING",
    "HERMES_CHECK_NATIVE_STACK",
    "HERMES_PLATFORM_UNICODE=HERMES_PLATFORM_UNICODE_LITE",
] + select({
    ":apple": ["HERMES_ENABLE_INTL"],
    "//conditions:default": ["HERMESVM_ALLOW_CONTIGUOUS_HEAP"],
})

DEFINES_FULL = DEFINES_BASE + [
    "HERMES_ENABLE_DEBUGGER",
    "HERMES_MEMORY_INSTRUMENTATION",
]

DEFINES_LEAN = DEFINES_BASE + [
    "HERMESVM_LEAN",
]

cc_library(
    name = "hermes_headers",
    hdrs = glob([
        "include/**/*.h",
        "include/**/*.def",
        "include/**/*.inc",
    ]),
    copts = ["-Wno-deprecated-this-capture"],
    strip_include_prefix = "include",
    deps = [],
)

cc_library(
    name = "hermes_api_headers",
    hdrs = glob([
        "API/hermes/**/*.h",
    ]),
    copts = ["-Wno-deprecated-this-capture"],
    strip_include_prefix = "API",
    deps = [],
)

cc_library(
    name = "jsi_headers",
    hdrs = glob([
        "API/jsi/**/*.h",
    ]),
    strip_include_prefix = "API/jsi",
    deps = [],
)

cc_library(
    name = "hermes_public_headers",
    hdrs = glob([
        "public/**/*.h",
    ]),
    strip_include_prefix = "public",
    deps = [],
)

cc_library(
    name = "llvh_headers",
    hdrs = glob([
        "external/llvh/include/**/*.h",
        "external/llvh/include/**/*.def",
    ]),
    strip_include_prefix = "include",
    deps = [],
)

cc_library(
    name = "zip_headers",
    hdrs = glob([
        "external/zip/**/*.h",
    ]),
    deps = [],
)

cc_library(
    name = "dtoa_headers",
    hdrs = glob([
        "external/dtoa/**/*.h",
    ]),
    deps = [],
)

ALWAYS_ENABLED = ["//conditions:default"]

cmake_configure(
    name = "llvm-config.h",
    src = "external/llvh/include/llvh/Config/llvm-config.h.cmake",
    cmake_vars = {
        "LLVM_HOST_TRIPLE": {
            "@valdi//bzl/conditions:ios_arm64": "arm-apple-darwin22.6.0",
            "@valdi//bzl/conditions:ios_x86_64": "x86_64-apple-darwin23.0.0",
            "@valdi//bzl/conditions:ios_arm64_sim": "arm-apple-darwin22.6.0",
            "@valdi//bzl/conditions:macos_x86_64": "x86_64-apple-darwin23.0.0",
            "@valdi//bzl/conditions:macos_arm64": "arm-apple-darwin22.6.0",
            "@valdi//bzl/conditions:android_arm32": "arm-none-android",
            "@valdi//bzl/conditions:android_arm64": "arm-none-android",
            "@valdi//bzl/conditions:android_x64": "x86_64-none-android",
            "@valdi//bzl/conditions:linux": "x86_64-unknown-linux-gnu",
        },
        "LLVM_VERSION_MAJOR": {
            "//conditions:default": "",
        },
        "LLVM_VERSION_MINOR": {
            "//conditions:default": "",
        },
        "LLVM_VERSION_PATCH": {
            "//conditions:default": "",
        },
        "PACKAGE_VERSION": {
            "//conditions:default": "8.0.0svn",
        },
        "LLVM_ON_UNIX": {
            "//conditions:default": "1",
        },
    },
    cmakedefines = {
        "LLVM_ENABLE_DUMP": [],
        "LINK_POLLY_INTO_TOOLS": [],
        "LLVM_DEFAULT_TARGET_TRIPLE": [],
        "LLVM_NATIVE_ARCH": [],
        "LLVM_NATIVE_ASMPARSER": [],
        "LLVM_NATIVE_ASMPRINTER": [],
        "LLVM_NATIVE_DISASSEMBLER": [],
        "LLVM_NATIVE_TARGET": [],
        "LLVM_NATIVE_TARGETINFO": [],
        "LLVM_NATIVE_TARGETMC": [],
        "LLVM_ON_UNIX": ALWAYS_ENABLED,
        "LLVM_HOST_TRIPLE": ALWAYS_ENABLED,
    },
    cmakedefines_01 = {
        "LLVM_ENABLE_THREADS": ALWAYS_ENABLED,
        "LLVM_HAS_ATOMICS": ALWAYS_ENABLED,
        "LLVM_ON_UNIX": ALWAYS_ENABLED,
        "LLVM_USE_INTEL_JITEVENTS": [],
        "LLVM_USE_OPROFILE": [],
        "LLVM_USE_PERF": [],
        "LLVM_FORCE_ENABLE_STATS": [],
    },
    output = "llvh/Config/llvm-config.h",
)

cc_library(
    name = "prepared_headers",
    hdrs = {},
    defines = [],
    deps = [
        ":llvm-config.h",
        "@valdi//third-party/hermes:prepared_headers_base",
    ],
)

cc_library(
    name = "llvh",
    srcs = glob(
        ["external/llvh/lib/**/*.cpp"],
    ),
    hdrs = glob([
        "external/llvh/**/*.h",
        "external/llvh/**/*.inc",
    ]),
    copts = COMPILER_FLAGS,
    defines = [],
    tags = [
        "app_size_owners=VALDI",
    ],
    deps = [
        ":llvh_headers",
        ":prepared_headers",
    ],
)

cc_library(
    name = "dtoa",
    srcs = glob(
        [
            "external/dtoa/**/*.c",
            "external/dtoa/**/*.cpp",
        ],
    ),
    hdrs = glob([
        "external/dtoa/**/*.h",
        "external/dtoa/**/*.inc",
    ]),
    copts = COMPILER_FLAGS,
    include_prefix = "dtoa",
    local_defines = [
        "IEEE_8087",
        "Long=int",
        "NO_HEX_FP",
        "NO_INFNAN_CHECK",
        "MULTIPLE_THREADS",
    ],
    tags = [
        "app_size_owners=VALDI",
    ],
    deps = [],
)

cc_library(
    name = "all_headers",
    copts = ["-Wno-deprecated-this-capture"],
    deps = [
        ":dtoa_headers",
        ":hermes_api_headers",
        ":hermes_headers",
        ":hermes_public_headers",
        ":jsi_headers",
        ":llvh_headers",
        ":prepared_headers",
        ":zip_headers",
    ],
)

HERMES_LIBS = [
    "ADT",
    "AST",
    "AST2JS",
    "BCGen",
    "ConsoleHost",
    "FrontEndDefs",
    "Inst",
    "InternalBytecode",
    "IR",
    "IRGen",
    "Optimizer",
    "Parser",
    "Platform",
    "Regex",
    "SourceMap",
    "Support",
    "Utils",
    "VM",
]

HERMES_LEAN_LIBS = [
    "ADT",
    "AST",
    "BCGen",
    "ConsoleHost",
    "FrontEndDefs",
    "Inst",
    "InternalBytecode",
    "IR",
    "Platform",
    "Regex",
    "SourceMap",
    "Support",
    "Utils",
    "VM",
]

EXCLUDED_LIB_FILES = [
    "VM/gcs/MallocGC.cpp",
    "Platform/Intl/PlatformIntlAndroid.cpp",
    "Platform/Intl/PlatformIntlICU.cpp",
    "Platform/Intl/PlatformIntlShared.cpp",
]

EXCLUDED_LIB_LEAN_FILES = EXCLUDED_LIB_FILES + [
    "BCGen/HBC/Bytecode.cpp",
    "BCGen/HBC/BytecodeProviderFromSrc.cpp",
    "BCGen/HBC/BytecodeGenerator.cpp",
    "BCGen/HBC/Passes.cpp",
    "BCGen/HBC/Passes/FuncCallNOpts.cpp",
    "BCGen/HBC/Passes/InsertProfilePoint.cpp",
    "BCGen/HBC/Passes/LowerBuiltinCalls.cpp",
    "BCGen/HBC/Passes/OptEnvironmentInit.cpp",
]

EXCLUDED_API_FILES = [
    "hermes/synthtest/**/*.cpp",
    "hermes/inspector/chrome/tests/**/*.cpp",
    "hermes/inspector/chrome/cli/**/*.cpp",
    "jsi/jsi/JSIDynamic.cpp",
    "jsi/jsi/test/**/*.cpp",
    "hermes_abi/**/*.cpp",
    "hermes_sandbox/**/*.cpp",
]

cc_library(
    name = "hermes",
    srcs = glob(
        ["lib/{}/**/*.cpp".format(hermes_lib) for hermes_lib in HERMES_LIBS] + [
            "public/**/*.cpp",
        ],
        exclude = ["lib/{}".format(excluded_file) for excluded_file in EXCLUDED_LIB_FILES],
    ),
    hdrs = glob([
        "lib/**/*.h",
        "lib/**/*.inc",
        "lib/**/*.def",
    ]),
    copts = COMPILER_FLAGS,
    defines = DEFINES_FULL,
    tags = [
        "app_size_owners=VALDI",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":all_headers",
        ":dtoa",
        ":llvh",
    ] + select({
        ":apple": [":hermes_intl_apple"],
        "//conditions:default": [],
    }),
)

cc_library(
    name = "hermes_lean",
    srcs = glob(
        ["lib/{}/**/*.cpp".format(hermes_lib) for hermes_lib in HERMES_LEAN_LIBS] + [
            "public/**/*.cpp",
        ],
        exclude = ["lib/{}".format(excluded_file) for excluded_file in EXCLUDED_LIB_LEAN_FILES],
    ),
    hdrs = glob([
        "lib/**/*.h",
        "lib/**/*.inc",
        "lib/**/*.def",
    ]),
    copts = COMPILER_FLAGS,
    defines = DEFINES_LEAN,
    tags = [
        "app_size_owners=VALDI",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":all_headers",
        ":dtoa",
        ":llvh",
    ] + select({
        ":apple": [":hermes_intl_apple"],
        "//conditions:default": [],
    }),
)

cc_library(
    name = "hermes_api",
    srcs = glob(
        ["API/**/*.cpp"],
        exclude = glob(["API/{}".format(excluded_file) for excluded_file in EXCLUDED_API_FILES]),
    ),
    hdrs = {},
    copts = COMPILER_FLAGS_COMPAT,
    defines = DEFINES_FULL,
    tags = [
        "app_size_owners=VALDI",
    ],
    visibility = ["//visibility:public"],
    deps = [":all_headers"],
)

cc_library(
    name = "hermes_api_lean",
    srcs = glob(
        ["API/**/*.cpp"],
        exclude = glob(["API/{}".format(excluded_file) for excluded_file in EXCLUDED_API_FILES]),
    ),
    hdrs = {},
    copts = COMPILER_FLAGS_COMPAT,
    defines = DEFINES_LEAN,
    tags = [
        "app_size_owners=VALDI",
    ],
    visibility = ["//visibility:public"],
    deps = [":all_headers"],
)

objc_library(
    name = "hermes_intl_apple",
    srcs = glob(
        ["lib/Platform/Intl/*.mm"],
    ),
    hdrs = {},
    copts = COMPILER_FLAGS,
    defines = DEFINES_FULL,
    tags = [
        "app_size_owners=VALDI",
    ],
    deps = [":all_headers"],
)
