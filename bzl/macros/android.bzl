load(
    "@rules_android//rules:rules.bzl",
    _aar_import = "aar_import",
    _android_binary = "android_binary",
    _android_library = "android_library",
)

# Used by workspace_init.bzl for maven_install(use_starlark_android_rules = ...)
STARLARK_RULES_ANDROID_ENABLED = True

# Re-export for valdi_android_application.bzl and other loaders from @android_macros
android_binary = _android_binary
android_library = _android_library

def aar_import(**kwargs):
    patched_kwargs = dict(kwargs)
    existing = patched_kwargs.get("deps", [])
    patched_kwargs["deps"] = existing + [
        "@rules_kotlin//kotlin/compiler:kotlin-stdlib",
    ]

    _aar_import(**patched_kwargs)
