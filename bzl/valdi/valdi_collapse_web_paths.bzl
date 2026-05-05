load(":valdi_compiled.bzl", "ValdiModuleInfo")

def _dest_native(rel):
    """Canonical path for a file in the native/ tree: <module>/web/<file> or module path.

    Used by both collapse_web_paths (where to put the file) and generate_register_native_modules
    (require path). Must be the single source of truth for native layout."""
    parts = rel.split("/")
    for i, seg in enumerate(parts):
        if seg == "web":
            parent = parts[i - 1] if i > 0 else ""
            tail = "/".join(parts[i + 1:])
            base = (parent + "/web") if parent else "web"
            return base + ("/" + tail if tail else "")
    return "/".join(parts)

# Dest path substrings that should not be registered (test files, etc).
# Shared by collapse_web_paths and generate_register_native_modules so native layout stays in sync.
_REGISTER_NATIVE_EXCLUDE_SUBSTRINGS = [
    "/test/",
    ".test.",
    ".spec.",
    "_debugging/",  # Optional/debug-only modules (e.g. ..._debugging/web/); not bundled in web package
]

def _should_register_native_module(dest_path):
    """Exclude test files and modules that are not suitable for web bundle."""
    for sub in _REGISTER_NATIVE_EXCLUDE_SUBSTRINGS:
        if sub in dest_path:
            return False
    return True

def _is_native_module_js(rel):
    """True if this short_path is a native module .js that gets require(pkg/native/...) in RegisterNativeModules.js."""
    d = _dest_native(rel)
    if not d.endswith(".js"):
        return False
    if "/web/debug/" in d or "/web/release/" in d:
        return False
    return _should_register_native_module(d)

def _should_exclude_from_package(short_path):
    """True if this file should not be copied into the collapsed package (test files, tree root, or unregistered native .js)."""
    for sub in _REGISTER_NATIVE_EXCLUDE_SUBSTRINGS:
        if sub in short_path:
            return True

    # web_native tree root (directory): skip so we only copy expanded files, not the whole tree
    idx = short_path.rfind("web_native")
    if idx >= 0 and not short_path[idx + len("web_native"):].lstrip("/"):
        return True
    d = _dest_native(short_path)
    if d.endswith(".js") and not _should_register_native_module(d):
        return True
    return False

def _dest(rel):
    """Maps a source short_path to its destination path in the collapsed npm package."""

    # Handle package.json - keep it at root
    if rel.endswith("package.json"):
        return "package.json"

    # Generated RegisterNativeModules.js goes at src/ for NPM package consumers
    if rel.endswith("RegisterNativeModules.js"):
        return "src/RegisterNativeModules.js"

    if "protodecl_collapsed" in rel:
        return "src"

    # Single source of truth for native module .js: same predicate as generate_register_native_modules.
    # Any path that would get a require(pkg/native/...) there goes to native/<dest_native> here.
    if _is_native_module_js(rel):
        return "native/" + _dest_native(rel)

    # Handle external repository paths (short_path starts with ../ for external repos)
    # and regular source paths. Extract everything after /src/valdi_modules/src/valdi/
    # Works with any external repo name (e.g., ../<repo>/src/valdi_modules/src/valdi/...)
    valdi_marker = "/src/valdi_modules/src/valdi/"

    # Try to find and strip the valdi marker from the path
    rel2 = rel
    if valdi_marker in rel:
        idx = rel.find(valdi_marker)
        rel2 = rel[idx + len(valdi_marker):]
    elif rel.startswith("src/valdi_modules/src/valdi/"):
        # Handle direct paths (non-external)
        rel2 = rel[len("src/valdi_modules/src/valdi/"):]

    parts = rel2.split("/")

    # Handle TypeScript declaration files (.d.ts) from .valdi_build/compile/typescript/output/
    # These should go into src/<module_name>/...
    for i in range(len(parts)):
        if parts[i] == ".valdi_build" and i + 3 < len(parts):
            if parts[i + 1] == "compile" and parts[i + 2] == "typescript" and parts[i + 3] == "output":
                # Skip to the module name and path after "output"
                if i + 4 < len(parts):
                    tail = "/".join(parts[i + 4:])
                    return "src/{}".format(tail)

    for i in range(len(parts) - 3):
        if (parts[i + 1] == "web" and
            parts[i + 2] in ["debug", "release"] and
            parts[i + 3] in ["assets", "res"]):
            tail = "/".join(parts[i + 4:])
            return "src/{}".format(tail)

    # Handle source .d.ts files from any path containing /src/valdi_modules/src/valdi/
    # These should go into src/<module_name>/src/...
    if rel.endswith(".d.ts") and valdi_marker in rel:
        # rel2 already has the marker stripped, so it's <module_name>/src/...
        # Return it as src/<module_name>/src/...
        return "src/{}".format(rel2)

    return rel

def _impl(ctx):
    outdir = ctx.actions.declare_directory(ctx.label.name)
    package_name = ctx.attr.package_name
    exclude_jsx = ctx.attr.exclude_jsx_global_declaration

    # Build manifest src → dest. Deduplicate by dest (first source wins) so the same logical file
    # from tree artifact and filegroup doesn't trigger duplicate copies.
    manifest = ctx.actions.declare_file(ctx.label.name + ".manifest")
    seen_dest = {}
    lines = []
    for f in ctx.files.srcs:
        if exclude_jsx and "valdi_tsx/src/JSX.d.ts" in f.short_path:
            continue
        if _should_exclude_from_package(f.short_path):
            continue
        d = _dest(f.short_path)
        if d not in seen_dest:
            seen_dest[d] = True
            lines.append("{}\t{}".format(f.path, d))

    # If excluding JSX global declaration, add stub file from valdi_tsx/web
    if exclude_jsx:
        stub = ctx.file.jsx_stub_file
        lines.append("{}\tsrc/valdi_tsx/src/JSX.d.ts".format(stub.path))

    ctx.actions.write(manifest, "\n".join(lines) + "\n")

    # tiny shell copier with .d.ts import rewriting
    sh = ctx.actions.declare_file(ctx.label.name + ".sh")
    ctx.actions.write(
        output = sh,
        is_executable = True,
        content = """#!/usr/bin/env bash
        set -euo pipefail
        OUT="$1"; MAN="$2"; PKG_NAME="$3"
        rm -rf "$OUT"; mkdir -p "$OUT"

        while IFS=$'\\t' read -r SRC DEST; do
        [ -z "$SRC" ] && continue

        # If SRC is a directory (tree artifact), copy its *contents* into DEST
        if [ -d "$SRC" ]; then
            mkdir -p "$OUT/$DEST"
            # copy contents, not the top-level dir
            cp -R "$SRC/." "$OUT/$DEST/"
        else
            D="$OUT/$(dirname "$DEST")"
            mkdir -p "$D"
            cp -f "$SRC" "$OUT/$DEST"
        fi
        done < "$MAN"
        
        # Rewrite imports in .d.ts files to use full package paths
        # Converts module_name/src/... → PACKAGE_NAME/src/module_name/src/...
        find "$OUT" -name "*.d.ts" -type f | while read -r file; do
            if [[ "$OSTYPE" == "darwin"* ]]; then
                # macOS
                sed -i '' -E "s|from '([a-zA-Z0-9_.-]+/src/[^']+)'|from '${PKG_NAME}/src/\\1'|g" "$file"
                sed -i '' -E "s|from \\"([a-zA-Z0-9_.-]+/src/[^\\"]+)\\"|from \\"${PKG_NAME}/src/\\1\\"|g" "$file"
                sed -i '' -E "s|import '([a-zA-Z0-9_.-]+/src/[^']+)'|import '${PKG_NAME}/src/\\1'|g" "$file"
                sed -i '' -E "s|import \\"([a-zA-Z0-9_.-]+/src/[^\\"]+)\\"|import \\"${PKG_NAME}/src/\\1\\"|g" "$file"
            else
                # Linux
                sed -i -E "s|from '([a-zA-Z0-9_.-]+/src/[^']+)'|from '${PKG_NAME}/src/\\1'|g" "$file"
                sed -i -E "s|from \\"([a-zA-Z0-9_.-]+/src/[^\\"]+)\\"|from \\"${PKG_NAME}/src/\\1\\"|g" "$file"
                sed -i -E "s|import '([a-zA-Z0-9_.-]+/src/[^']+)'|import '${PKG_NAME}/src/\\1'|g" "$file"
                sed -i -E "s|import \\"([a-zA-Z0-9_.-]+/src/[^\\"]+)\\"|import \\"${PKG_NAME}/src/\\1\\"|g" "$file"
            fi
        done
        """,
    )

    ctx.actions.run(
        inputs = [manifest] + ctx.files.srcs,
        outputs = [outdir],
        tools = [sh],
        executable = sh,
        arguments = [outdir.path, manifest.path, package_name],
        progress_message = "Collapsing web paths and rewriting .d.ts imports into {}".format(outdir.path),
    )
    return [DefaultInfo(files = depset([outdir]))]

collapse_web_paths = rule(
    implementation = _impl,
    attrs = {
        "srcs": attr.label_list(allow_files = True),
        "package_name": attr.string(mandatory = True, doc = "The NPM package name"),
        "exclude_jsx_global_declaration": attr.bool(default = False, doc = "Exclude valdi_tsx/src/JSX.d.ts and replace with stub to prevent global namespace pollution"),
        "jsx_stub_file": attr.label(
            default = "@valdi//src/valdi_modules/src/valdi/valdi_tsx:web/JSX.stub.d.ts",
            allow_single_file = True,
            doc = "Stub file to use when exclude_jsx_global_declaration is True",
        ),
    },
)

def _impl_native(ctx):
    outdir = ctx.actions.declare_directory(ctx.label.name)

    # Build a manifest of: SRC \t DEST
    manifest = ctx.actions.declare_file(ctx.label.name + ".manifest")
    lines = []
    for f in ctx.files.srcs:
        lines.append("{}\t{}".format(f.path, _dest_native(f.short_path)))
    ctx.actions.write(manifest, "\n".join(lines))

    # Tiny shell that copies into the declared directory
    sh = ctx.actions.declare_file(ctx.label.name + ".sh")
    ctx.actions.write(
        output = sh,
        is_executable = True,
        content = """#!/usr/bin/env bash
            set -euo pipefail
            OUT="$1"; MAN="$2"
            rm -rf "$OUT"; mkdir -p "$OUT"
            while IFS=$'\\t' read -r SRC DEST; do
            [ -z "$SRC" ] && continue
            D="$OUT/$(dirname "$DEST")"
            mkdir -p "$D"
            cp -rf "$SRC" "$OUT/$DEST"
            done < "$MAN"
        """,
    )

    ctx.actions.run(
        inputs = [manifest] + ctx.files.srcs,
        outputs = [outdir],
        tools = [sh],
        executable = sh,
        arguments = [outdir.path, manifest.path],
        progress_message = "Collapsing native paths into {}".format(outdir.path),
    )
    return [DefaultInfo(files = depset([outdir]))]

collapse_native_paths = rule(
    implementation = _impl_native,
    attrs = {
        "srcs": attr.label_list(allow_files = True),
    },
)

def _module_id_from_native_dest(dest_path):
    """Derive the Valdi module ID from native path like 'valdi_tsx/web/JSX.js' or 'cof/web/Cof.js'.
    Convention: parent/web/File.js -> if parent equals filename (case-insensitive) use filename else parent/src/filename.
    For paths without /web/ (e.g. my_module/utils/helper.js), insert /src/ only after the first segment: my_module/src/utils/helper.
    """
    if "/web/" not in dest_path:
        no_ext = dest_path[:-3] if dest_path.endswith(".js") else dest_path
        first_slash = no_ext.find("/")
        if first_slash == -1:
            return no_ext
        return no_ext[:first_slash] + "/src/" + no_ext[first_slash + 1:]
    idx = dest_path.index("/web/")
    parent = dest_path[:idx]
    file_part = dest_path[idx + 5:]  # after "/web/"
    if file_part.endswith(".js"):
        file_part = file_part[:-3]
    if parent.split("/")[-1].lower() == file_part.lower():
        return file_part
    return parent + "/src/" + file_part

def _merge_module_id_overrides_from_modules(modules):
    """Collect web_register_native_module_id_overrides from all transitive Valdi modules."""
    all_modules = depset(direct = modules, transitive = [m[ValdiModuleInfo].deps for m in modules])
    merged = {}
    for m in all_modules.to_list():
        overrides = getattr(m[ValdiModuleInfo], "web_register_native_module_id_overrides", None)
        if overrides:
            merged.update(overrides)
    return merged

def _generate_register_native_modules_impl(ctx):
    package_name = ctx.attr.package_name

    # Overrides: first from each module's ValdiModuleInfo, then BUILD-level overrides on top
    module_id_overrides = dict(_merge_module_id_overrides_from_modules(ctx.attr.modules))
    module_id_overrides.update(ctx.attr.module_id_overrides)

    out = ctx.actions.declare_file("RegisterNativeModules.js")
    lines = [
        "",
        "/**",
        " * AUTO-GENERATED - Do not edit. Native module registrations for web runtime.",
        " * Generated from _all_web_deps.",
        " */",
        "",
        "var _cbs = globalThis.__valdiWebViewClassRegistryCallbacks =",
        "  globalThis.__valdiWebViewClassRegistryCallbacks || [];",
        "",
    ]
    seen_dest = {}
    n = 0
    for f in ctx.files.srcs:
        dest = _dest_native(f.short_path)
        if not dest.endswith(".js"):
            continue
        if not _should_register_native_module(dest):
            continue
        if dest in seen_dest:
            continue
        seen_dest[dest] = True
        raw_id = module_id_overrides.get(dest, _module_id_from_native_dest(dest))
        module_ids = [s.strip() for s in raw_id.split(",") if s.strip()]
        if not module_ids:
            module_ids = [raw_id]
        require_path = package_name + "/native/" + dest[:-3]  # strip .js for require()
        var_name = "_n" + str(n)
        n += 1
        lines.append("var {} = require('{}');".format(var_name, require_path))
        for mid in module_ids:
            lines.append("global.moduleLoader.registerModule('{}', () => {});".format(mid, var_name))
        lines.append("if ({v}.webPolyglotViews) {{".format(v = var_name))
        lines.append("  _cbs.push(function(registry) {")
        lines.append("    Object.entries({v}.webPolyglotViews).forEach(function(e) {{ registry.set(e[0], e[1]); }});".format(v = var_name))
        lines.append("  });")
        lines.append("}")
        lines.append("")
    ctx.actions.write(output = out, content = "\n".join(lines))
    return [DefaultInfo(files = depset([out]))]

generate_register_native_modules = rule(
    implementation = _generate_register_native_modules_impl,
    attrs = {
        "srcs": attr.label_list(allow_files = True),
        "package_name": attr.string(mandatory = True, doc = "NPM package name (e.g. @snapchat/valdi_web_snapchat_web_npm)"),
        "modules": attr.label_list(
            default = [],
            providers = [ValdiModuleInfo],
            doc = "Valdi module targets (e.g. deps of valdi_exported_library). Their web_register_native_module_id_overrides are merged to form the module ID map.",
        ),
        "module_id_overrides": attr.string_dict(
            default = {},
            doc = "Optional BUILD-level overrides (native dest path -> runtime module ID). Applied after module-declared overrides.",
        ),
    },
)
