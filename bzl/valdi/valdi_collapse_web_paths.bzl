def _dest(rel):
    # Handle package.json - keep it at root
    if rel.endswith("package.json"):
        return "package.json"

    if "web_native" in rel:
        return "native"

    if "protodecl_collapsed" in rel:
        return "src"

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

    # build a small manifest of src → dest
    manifest = ctx.actions.declare_file(ctx.label.name + ".manifest")
    lines = []
    for f in ctx.files.srcs:
        # Check if file should be excluded (JSX.d.ts when exclude_jsx is True)
        excluded = False
        if exclude_jsx and "valdi_tsx/src/JSX.d.ts" in f.short_path:
            excluded = True

        if not excluded:
            lines.append("{}\t{}".format(f.path, _dest(f.short_path)))

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

def _dest_native(rel):
    parts = rel.split("/")

    # 2) Keep "<parent>/web/<tail>" where "web" is the marker
    for i, seg in enumerate(parts):
        if seg == "web":
            parent = parts[i - 1] if i > 0 else ""
            tail = "/".join(parts[i + 1:])
            base = (parent + "/web") if parent else "web"
            return base + ("/" + tail if tail else "")

    # 3) If there's no "web" segment, just return the path
    return "/".join(parts)

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
