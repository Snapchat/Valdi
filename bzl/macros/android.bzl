load(
    "@rules_android//rules:rules.bzl",
    _aar_import = "aar_import",
)

def aar_import(**kwargs):
    patched_kwargs = dict(kwargs)
    existing = patched_kwargs.get("deps", [])
    patched_kwargs["deps"] = existing + [
        # Label() so this resolves against android_macros' repo mapping, not
        # the calling package's. See Valdi_Widgets#20.
        Label("@rules_kotlin//kotlin/compiler:kotlin-stdlib"),
    ]

    _aar_import(**patched_kwargs)
