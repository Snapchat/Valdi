load(
    "@rules_android//rules:rules.bzl",
    _aar_import = "aar_import",
)

def aar_import(**kwargs):
    patched_kwargs = dict(kwargs)
    existing = patched_kwargs.get("deps", [])
    patched_kwargs["deps"] = existing + [
        "@rules_kotlin//kotlin/compiler:kotlin-stdlib",
    ]

    _aar_import(**patched_kwargs)
