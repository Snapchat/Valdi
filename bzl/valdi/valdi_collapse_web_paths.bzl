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

    # Exclude test files but NOT debug modules — debug modules may be
    # transitive deps whose compiled JS is needed by the web bundler.
    for sub in ["/test/", ".test.", ".spec."]:
        if sub in short_path:
            return True

    # web_native tree root (directory): skip so we only copy expanded files, not the whole tree
    idx = short_path.rfind("web_native")
    if idx >= 0 and not short_path[idx + len("web_native"):].lstrip("/"):
        return True

    # Don't exclude native .js based on registration — all native web .js
    # files should be in the package for shim resolution. Registration
    # exclusion only affects auto-registration in RegisterNativeModules.js.
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

    # Place native module .d.ts alongside their .js counterparts in native/.
    # Check if the corresponding .js would be a native module.
    if rel.endswith(".d.ts") and "/web/" in rel:
        js_rel = rel[:-5] + ".js"  # .d.ts -> .js
        if _is_native_module_js(js_rel):
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

    # Build manifest src -> dest. Deduplicate by dest (first source wins) so the same logical file
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

    # ── Build strings manifest ──
    # module_name \t strings_dir (one line per module with strings).
    # Used by the shell script to generate _strings_preload.js files for web bundlers.
    strings_manifest = ctx.actions.declare_file(ctx.label.name + ".strings_manifest")
    strings_lines = []
    all_modules_for_strings = depset(
        direct = ctx.attr.modules,
        transitive = [m[ValdiModuleInfo].deps for m in ctx.attr.modules],
    )
    for m in all_modules_for_strings.to_list():
        info = m[ValdiModuleInfo]
        if info.strings_dir:
            strings_lines.append("{}\t{}".format(info.name, info.strings_dir))
    ctx.actions.write(strings_manifest, "\n".join(strings_lines) + "\n")

    # ── Build .d.ts manifest ──
    # source_path \t module_name (one line per source .d.ts file).
    # Used by the shell script to place source .d.ts alongside compiled .js.
    # Module name comes from ValdiModuleInfo — no path convention guessing.
    dts_manifest = ctx.actions.declare_file(ctx.label.name + ".dts_manifest")
    dts_lines = []
    all_modules_for_dts = depset(
        direct = ctx.attr.modules,
        transitive = [m[ValdiModuleInfo].deps for m in ctx.attr.modules],
    )
    for m in all_modules_for_dts.to_list():
        info = m[ValdiModuleInfo]
        for f in info.web_dts_files:
            if f.path.endswith(".d.ts"):
                dts_lines.append("{}\t{}".format(f.path, info.name))
    ctx.actions.write(dts_manifest, "\n".join(dts_lines) + "\n")

    # ── Build shim manifest ──
    # Each native module gets a shim at src/<subpath>.js that checks overrides
    # and falls back to the native web implementation.
    shim_manifest = ctx.actions.declare_file(ctx.label.name + ".shims")
    shim_lines = []

    module_id_overrides = {}
    if ctx.attr.modules:
        module_id_overrides = dict(_merge_module_id_overrides_from_modules(ctx.attr.modules))
    module_id_overrides.update(ctx.attr.module_id_overrides)

    seen_native = {}
    for f in ctx.files.srcs:
        if "RegisterNativeModules" in f.short_path:
            continue
        if not _is_native_module_js(f.short_path):
            continue
        dest = _dest_native(f.short_path)
        if dest in seen_native:
            continue
        seen_native[dest] = True

        subpath = _native_subpath(dest)
        default_mid = _module_id_from_native_dest(dest)
        override_raw = module_id_overrides.get(dest, "")
        override_ids = [s.strip() for s in override_raw.split(",") if s.strip()]

        # Collect all unique paths that need shims + all module IDs for auto-registration.
        # Bare names (no "/") like "Graphene" get a shim at src/Graphene.js so
        # webpack resolve.modules can find them via require('Graphene').
        all_paths = {}
        all_paths[subpath] = True
        if default_mid:
            all_paths[default_mid] = True
        for mid in override_ids:
            if mid:
                all_paths[mid] = True

        # Collect ALL module IDs (including magic strings) for auto-registration
        all_ids = [subpath, default_mid] + override_ids

        # Deduplicate and join with comma for the shim template
        unique_ids = []
        seen_ids = {}
        for mid in all_ids:
            if mid and mid not in seen_ids:
                seen_ids[mid] = True
                unique_ids.append(mid)
        ids_str = ",".join(unique_ids)

        for shim_path in all_paths:
            depth = shim_path.count("/") + 1  # +1 for outer src/ dir
            rel_prefix = "../" * depth
            native_require = rel_prefix + "native/" + dest[:-3]

            # Format: SHIM_PATH \t NATIVE_PATH \t ALL_IDS (comma-separated)
            shim_lines.append("{}\t{}\t{}".format(shim_path, native_require, ids_str))

    ctx.actions.write(shim_manifest, "\n".join(shim_lines) + "\n")

    # No Python require transform needed — the companion AST transformer handles
    # variable requires (→ moduleLoader.load) and PrependWebJsProcessor strips
    # extra args from string-literal requires. Shims + resolve.modules handle
    # native module name resolution.

    # Python script for generating package.json exports
    exports_script = ctx.actions.declare_file(ctx.label.name + "_exports.py")
    ctx.actions.write(
        output = exports_script,
        content = """
import json, os, sys

pkg_json_path = sys.argv[1]
out_dir = sys.argv[2]

with open(pkg_json_path) as f:
    pkg = json.load(f)

exports = {}
src_dir = os.path.join(out_dir, 'src')
if os.path.isdir(src_dir):
    for mod in sorted(os.listdir(src_dir)):
        mod_dir = os.path.join(src_dir, mod)
        if os.path.isdir(mod_dir):
            exports['./' + mod + '/src/*'] = {'types': './src/' + mod + '/src/*.d.ts', 'default': './src/' + mod + '/src/*.js'}
            exports['./' + mod + '/*'] = {'default': './src/' + mod + '/*.js'}
            # Directory-as-module: ./mod/src -> ./src/mod/src/index.js
            mod_src_index = os.path.join(mod_dir, 'src', 'index.js')
            if os.path.isfile(mod_src_index):
                entry = {'./' + mod + '/src': {'types': './src/' + mod + '/src/index.d.ts', 'default': './src/' + mod + '/src/index.js'}}
                exports.update(entry)

native_dir = os.path.join(out_dir, 'native')
if os.path.isdir(native_dir):
    exports['./native/*'] = {'default': './native/*.js'}

# Backward compat: existing @snapchat/PKG/src/... import paths
exports['./src/*'] = {'default': './src/*.js'}
# Directory-as-module for all index.js under src/
for dirpath, dirnames, filenames in os.walk(src_dir):
    if 'index.js' in filenames:
        rel = os.path.relpath(dirpath, out_dir)
        key = './' + rel
        if key not in exports:
            dts = './' + rel + '/index.d.ts'
            js = './' + rel + '/index.js'
            entry = {'default': js}
            if os.path.isfile(os.path.join(dirpath, 'index.d.ts')):
                entry = {'types': dts, 'default': js}
            exports[key] = entry

pkg['exports'] = exports

with open(pkg_json_path, 'w') as f:
    json.dump(pkg, f, indent=2)
    f.write('\\n')
""",
    )

    # Python script for generating ValdiModuleOverrides.d.ts
    overrides_script = ctx.actions.declare_file(ctx.label.name + "_overrides_dts.py")
    ctx.actions.write(
        output = overrides_script,
        content = """
import sys, re

shim_man_path = sys.argv[1]
out_path = sys.argv[2]
pkg_name = sys.argv[3] if len(sys.argv) > 3 else ''

# Read shim manifest: SUBPATH \\t NATIVE_PATH \\t ALL_IDS
entries = []       # (subpath, import_path) for ValdiModuleOverrides
module_map = {}    # module_id -> import_path for ValdiModuleMap (all IDs)
seen = set()
with open(shim_man_path) as f:
    for line in f:
        line = line.strip()
        if not line:
            continue
        parts = line.split('\\t')
        subpath = parts[0]
        native_path = parts[1] if len(parts) > 1 else ''
        all_ids = parts[2] if len(parts) > 2 else subpath
        # Convert native_path (../../native/module/web/File) to import path
        import_path = re.sub(r'^(\\.\\./)+'  , '', native_path)
        if subpath not in seen:
            seen.add(subpath)
            entries.append((subpath, import_path))
        # Map ALL module IDs (subpath + magic strings) to the same import
        for mid in all_ids.split(','):
            mid = mid.strip()
            if mid and mid not in module_map:
                module_map[mid] = import_path

with open(out_path, 'w') as f:
    f.write('/**\\n')
    f.write(' * AUTO-GENERATED type definitions for native module overrides.\\n')
    f.write(' * Use with globalThis.__valdiModuleOverrides to override native implementations.\\n')
    f.write(' */\\n\\n')

    # ValdiModuleOverrides — for __valdiModuleOverrides global (subpath keys only)
    f.write('interface ValdiModuleOverrides {\\n')
    for subpath, import_path in sorted(entries):
        if pkg_name:
            f.write("  '%s'?: typeof import('%s/%s');\\n" % (subpath, pkg_name, import_path))
        else:
            f.write("  '%s'?: typeof import('./%s');\\n" % (subpath, import_path))
    f.write('}\\n\\n')

    # ValdiModuleMap — all module IDs (subpath + magic strings) for typed registerModule/load
    f.write('interface ValdiModuleMap {\\n')
    for mid in sorted(module_map.keys()):
        imp = module_map[mid]
        if pkg_name:
            f.write("  '%s': typeof import('%s/%s');\\n" % (mid, pkg_name, imp))
        else:
            f.write("  '%s': typeof import('./%s');\\n" % (mid, imp))
    f.write('}\\n\\n')

    # Augment IModuleLoader — typed registerModule/load via ValdiModuleMap.
    # Works regardless of how moduleLoader is obtained (import or global).
    imodule_path = pkg_name + '/src/valdi_core/src/IModuleLoader' if pkg_name else 'valdi_core/src/IModuleLoader'
    f.write("declare module '%s' {\\n" % imodule_path)
    f.write('  interface IModuleLoader {\\n')
    f.write('    registerModule<K extends string>(id: K, factory: () => K extends keyof ValdiModuleMap ? ValdiModuleMap[K] : unknown): void;\\n')
    f.write('    load<K extends string>(id: K): K extends keyof ValdiModuleMap ? ValdiModuleMap[K] : unknown;\\n')
    f.write('  }\\n')
    f.write('}\\n\\n')

    # Global declaration for __valdiModuleOverrides
    f.write('declare global {\\n')
    f.write('  var __valdiModuleOverrides: Partial<ValdiModuleOverrides> | undefined;\\n')
    f.write('}\\n\\n')

    f.write('export {};\\n')
""",
    )

    # Python script for rewriting internal bare requires to relative paths.
    # Handles both bare native names (Graphene → ./graphene/Graphene.js)
    # and path-style internal requires (valdi_core/src/JSX → ../../valdi_core/src/JSX.js).
    # With --validate flag, checks for remaining bare internal requires and fails if found.
    bare_require_script = ctx.actions.declare_file(ctx.label.name + "_bare_require.py")
    ctx.actions.write(
        output = bare_require_script,
        content = """
import json, os, sys

src_dir = sys.argv[1]
native_map_path = sys.argv[2] if len(sys.argv) > 2 else ''
validate_only = len(sys.argv) > 3 and sys.argv[3] == '--validate'

# Build index of all .js files in src/ (keys without .js extension)
internal = set()
for root, _, files in os.walk(src_dir):
    for fname in files:
        if fname.endswith('.js'):
            rel = os.path.relpath(os.path.join(root, fname), src_dir)
            internal.add(rel[:-3])

# Bare native name rewrites from native_module_map.json (no '/' in key)
bare_names = {}
if native_map_path:
    try:
        with open(native_map_path) as f:
            bare_names = {k: v for k, v in json.load(f).items() if '/' not in k}
    except Exception:
        pass

# Vendored libraries: short npm-style name -> internal path.
# These are libraries copied into the source tree that TypeScript's
# importHelpers / paths config resolves by short name. The compiler
# leaves bare requires for these (they look like npm deps), so we
# resolve them here.
# Per-module packaging note: when modules are split into separate npm
# packages, each package that uses tslib will need its own copy (or
# list it as a real npm dependency). The alias here works because the
# monolith has a single src/ tree where valdi_core/src/tslib.js is
# always reachable by relative path from any module.
# Bare names that map to vendored copies inside the monolith. Strict package
# managers (pnpm, yarn PnP) won't hoist these from transitive deps, so we
# rewrite them to relative paths. Extend this map if a consumer hits an
# unresolved bare require that works under npm but fails under pnpm.
vendored_aliases = {'tslib': 'valdi_core/src/tslib'}
bare_names.update(vendored_aliases)

def find_requires(content):
    found = set()
    for quote in ("'", '"'):
        target = "require(" + quote
        i = 0
        while True:
            idx = content.find(target, i)
            if idx == -1:
                break
            start = idx + len(target)
            end = content.find(quote, start)
            if end == -1:
                break
            found.add(content[start:end])
            i = end + 1
    return found

errors = []
rewritten = 0
for root, _, files in os.walk(src_dir):
    for fname in files:
        if not fname.endswith('.js'):
            continue
        fpath = os.path.join(root, fname)
        with open(fpath) as f:
            content = f.read()

        requires = find_requires(content)
        file_dir = os.path.relpath(os.path.dirname(fpath), src_dir)
        changes = {}

        for arg in requires:
            if arg.startswith('.') or arg.startswith('@'):
                continue
            resolved = bare_names.get(arg, arg)
            key = resolved[:-3] if resolved.endswith('.js') else resolved
            if key not in internal:
                continue
            if validate_only:
                errors.append(os.path.relpath(fpath, src_dir) + ': require(' + arg + ')')
                continue
            target_path = key + '.js'
            rel = os.path.relpath(target_path, file_dir)
            if not rel.startswith('.'):
                rel = './' + rel
            changes[arg] = rel

        if changes:
            for old_arg, new_arg in changes.items():
                for quote in ("'", '"'):
                    old = "require(" + quote + old_arg + quote + ")"
                    new = "require(" + quote + new_arg + quote + ")"
                    content = content.replace(old, new)
            rewritten += 1
            with open(fpath, 'w') as f:
                f.write(content)

if validate_only:
    if errors:
        sys.stderr.write('ERROR: Bare internal requires remain after rewrite:\\n')
        for e in sorted(errors):
            sys.stderr.write('  ' + e + '\\n')
        sys.exit(1)
else:
    sys.stderr.write('Rewrote internal requires in ' + str(rewritten) + ' files\\n')
""",
    )

    # Shell copier + .d.ts rewriting + require mapping + shim generation + strings preload
    sh = ctx.actions.declare_file(ctx.label.name + ".sh")
    native_map_file = ctx.file.native_module_map
    ctx.actions.write(
        output = sh,
        is_executable = True,
        content = """#!/usr/bin/env bash
set -euo pipefail
OUT="$1"; MAN="$2"; PKG_NAME="$3"; NATIVE_MAP="$4"; SHIM_MAN="$5"; EXPORTS_SCRIPT="$6"; OVERRIDES_SCRIPT="$7"; STRINGS_MAN="$8"; DTS_MAN="$9"; BARE_REQ_SCRIPT="${10}"
[ -d "$OUT" ] && chmod -R u+w "$OUT" 2>/dev/null || true
rm -rf "$OUT"; mkdir -p "$OUT"

# ── Step 1: Copy files from manifest ──
# cp preserves mode from source by default and bazel inputs are read-only,
# so the just-copied files are read-only too. Subsequent overwrites by
# a later overlapping entry would fail. Manifest dedup is by exact dest
# path, so dir-vs-file overlaps aren't caught — handle via post-copy
# chmod (scoped to just the freshly-written subtree) and rm -f before
# file overwrites. chmod stays inside the action's own output dir.
while IFS=$'\\t' read -r SRC DEST; do
  [ -z "$SRC" ] && continue
  if [ -d "$SRC" ]; then
    mkdir -p "$OUT/$DEST"
    cp -R "$SRC/." "$OUT/$DEST/"
    chmod -R u+w "$OUT/$DEST" 2>/dev/null || true
  else
    D="$OUT/$(dirname "$DEST")"
    mkdir -p "$D"
    rm -f "$OUT/$DEST"
    cp -f "$SRC" "$OUT/$DEST"
    chmod u+w "$OUT/$DEST" 2>/dev/null || true
  fi
done < "$MAN"

# Step 1's cp -R inherits read-only mode from bazel-out source dirs.
# Step 1b writes .d.ts files into those same dirs, so widen mode first.
[ -d "$OUT/src" ] && chmod -R u+w "$OUT/src" 2>/dev/null || true

# ── Step 1b: Place source .d.ts alongside compiled .js using manifest ──
# Must run BEFORE step 2 so these files get their imports rewritten too.
# The .d.ts manifest maps source_path -> module_name (from ValdiModuleInfo).
# Source .d.ts files may arrive from any repo path structure; the manifest
# provides the correct module name without path convention guessing.
if [ -f "$DTS_MAN" ]; then
  while IFS=$'\\t' read -r DTS_SRC MOD_NAME; do
    [ -z "$DTS_SRC" ] || [ -z "$MOD_NAME" ] && continue
    [ -f "$DTS_SRC" ] || continue
    # Extract the relative path after <module_name>/ in the source path
    VAR="/$DTS_SRC"
    REL="${VAR#*/${MOD_NAME}/}"
    [ "$REL" = "$VAR" ] && continue
    DEST="$OUT/src/$MOD_NAME/$REL"
    DEST_DIR=$(dirname "$DEST")
    mkdir -p "$DEST_DIR"
    # rm -f the dest first: Step 1's cp -R preserves mode from bazel inputs
    # (read-only). Without this, cp -f can't overwrite the existing read-only
    # destination file. Avoid chmod-ing bazel-tracked output paths.
    rm -f "$DEST"
    cp -f "$DTS_SRC" "$DEST"
  done < "$DTS_MAN"
fi

# ── Step 2: Rewrite .d.ts imports to use full package paths ──
# Write sed script to a temp file to avoid shell quoting issues with find -exec.
# Batches all non-native .d.ts via find -exec + (one sed process for thousands of files).
DTS_SED_SCRIPT="$OUT/_dts_rewrite.sed"
cat > "$DTS_SED_SCRIPT" <<SEDEOF
s|from '([a-zA-Z0-9_.-]+/src/[^']+)'|from '${PKG_NAME}/src/\\1'|g
s|from "([a-zA-Z0-9_.-]+/src/[^"]+)"|from "${PKG_NAME}/src/\\1"|g
s|import '([a-zA-Z0-9_.-]+/src/[^']+)'|import '${PKG_NAME}/src/\\1'|g
s|import "([a-zA-Z0-9_.-]+/src/[^"]+)"|import "${PKG_NAME}/src/\\1"|g
SEDEOF
if [[ "$OSTYPE" == "darwin"* ]]; then
  find "$OUT" -not -path "$OUT/native/*" -name "*.d.ts" -type f -exec sed -i '' -E -f "$DTS_SED_SCRIPT" {} +
else
  find "$OUT" -not -path "$OUT/native/*" -name "*.d.ts" -type f -exec sed -i -E -f "$DTS_SED_SCRIPT" {} +
fi

# Step 2b: Native .d.ts need additional fixups for ../src/ paths (per-module).
if [ -d "$OUT/native" ]; then
  find "$OUT/native" -name "*.d.ts" -type f | while read -r file; do
    REL="${file#"$OUT/native/"}"
    MOD_NAME="${REL%%/*}"
    if [[ "$OSTYPE" == "darwin"* ]]; then
      sed -i '' -E \
        -f "$DTS_SED_SCRIPT" \
        -e "s|${PKG_NAME}/src/\\.\\./${MOD_NAME}/src/|${PKG_NAME}/src/${MOD_NAME}/src/|g" \
        -e "s|${PKG_NAME}/src/\\.\\./src/|${PKG_NAME}/src/${MOD_NAME}/src/|g" \
        "$file"
    else
      sed -i -E \
        -f "$DTS_SED_SCRIPT" \
        -e "s|${PKG_NAME}/src/\\.\\./${MOD_NAME}/src/|${PKG_NAME}/src/${MOD_NAME}/src/|g" \
        -e "s|${PKG_NAME}/src/\\.\\./src/|${PKG_NAME}/src/${MOD_NAME}/src/|g" \
        "$file"
    fi
  done
fi
rm -f "$DTS_SED_SCRIPT"

# ── Step 3: Make src/ writable ──
[ -d "$OUT/src" ] && chmod -R u+w "$OUT/src" 2>/dev/null || true

# ── Step 4: Generate native module shims ──
# Each shim checks __valdiModuleOverrides for consumer overrides,
# falls back to the default native web implementation, and auto-registers
# in moduleLoader for runtime valdiRequire/moduleLoader.load() access.
if [ -f "$SHIM_MAN" ]; then
  while IFS=$'\\t' read -r SUBPATH NATIVE_PATH ALL_IDS; do
    [ -z "$SUBPATH" ] && continue
    SHIM_FILE="$OUT/src/$SUBPATH.js"
    SHIM_DIR="$(dirname "$SHIM_FILE")"
    mkdir -p "$SHIM_DIR"
    chmod -R u+w "$SHIM_DIR" 2>/dev/null || true
    rm -f "$SHIM_FILE"

    # Build override check + registration lines for ALL module IDs
    OVERRIDE_CHECKS=""
    REG_LINES=""
    IFS=',' read -ra IDS <<< "$ALL_IDS"
    for MID in "${IDS[@]}"; do
      [ -z "$MID" ] && continue
      OVERRIDE_CHECKS="${OVERRIDE_CHECKS} || (__o && '$MID' in __o && ((module.exports = __o['$MID']), true))"
      REG_LINES="${REG_LINES}
if (__ml && !__ml.hasModuleFactory('$MID')) { __ml.registerModule('$MID', function() { return module.exports; }); }"
    done
    # Strip leading " || "
    OVERRIDE_CHECKS="${OVERRIDE_CHECKS# || }"

    cat > "$SHIM_FILE" << SHIMEOF
var __o = typeof globalThis !== 'undefined' && globalThis.__valdiModuleOverrides;
if (!(${OVERRIDE_CHECKS})) {
  module.exports = require('$NATIVE_PATH');
}
var __ml = typeof globalThis !== 'undefined' && globalThis.moduleLoader;${REG_LINES}
SHIMEOF
  done < "$SHIM_MAN"
fi

# ── Step 4b: Generate NavigationPage registry ──
# Instead of a blanket require.context that maps every .js file, generate
# a small file with explicit requires for only the files that use
# @NavigationPage. requireByComponent() uses this to resolve
# 'Symbol@FilePath' component paths at runtime.
if [ -d "$OUT/src" ]; then
  NAV_FILES=$(grep -rl --include='*.js' 'NavigationPage)(module)' "$OUT/src" 2>/dev/null || true)
  REGISTRY="$OUT/src/_navigation_registry.js"
  {
    echo "var __r = (globalThis.__valdiNavigationPages = globalThis.__valdiNavigationPages || {});"
    if [ -n "$NAV_FILES" ]; then
      for file in $NAV_FILES; do
        REL="${file#"$OUT/src/"}"
        REL_NO_EXT="${REL%.js}"
        echo "__r['${REL_NO_EXT}'] = function() { return require('./${REL_NO_EXT}'); };"
      done
    fi
  } > "$REGISTRY"
fi

# ── Step 4c: Generate worker service registry ──
# Same pattern as NavigationPage registry but for @workerService decorator.
# WorkerServiceExecutor uses moduleLoader.load(task.file) to resolve workers.
if [ -d "$OUT/src" ]; then
  WORKER_FILES=$(grep -rl --include='*.js' 'workerService' "$OUT/src" 2>/dev/null || true)
  WORKER_REGISTRY="$OUT/src/_worker_registry.js"
  {
    echo "var __r = (globalThis.__valdiWorkerModules = globalThis.__valdiWorkerModules || {});"
    if [ -n "$WORKER_FILES" ]; then
      for file in $WORKER_FILES; do
        REL="${file#"$OUT/src/"}"
        REL_NO_EXT="${REL%.js}"
        echo "__r['${REL_NO_EXT}'] = function() { return require('./${REL_NO_EXT}'); };"
      done
    fi
  } > "$WORKER_REGISTRY"
fi

# ── Step 4d: Generate image registry ──
# Per-module map of camelCase keys -> static image requires. Populates
# globalThis.__valdiImageRegistry['<module>/res'] (matches the catalogPath
# passed by AssetCatalog.loadCatalog from each generated res.js), read by
# ValdiWebRuntime.getAssets(). Replaces the runtime require.context that
# previously scanned every image in the package on each catalog lookup.
# Bundles every image in each module's res/ — no tree-shaking, matches
# the existing require.context behavior (which already pulled all images).
if [ -d "$OUT/src" ]; then
  IMG_REGISTRY="$OUT/src/_image_registry.js"
  {
    echo "var __r = (globalThis.__valdiImageRegistry = globalThis.__valdiImageRegistry || {});"
    for MOD_DIR in "$OUT/src"/*/; do
      [ -d "$MOD_DIR/res" ] || continue
      MOD=$(basename "$MOD_DIR")
      IMG_FILES=$(for EXT in png jpg jpeg svg webp gif; do find "$MOD_DIR/res" -maxdepth 1 -type f -name "*.$EXT" 2>/dev/null; done | sort)
      [ -z "$IMG_FILES" ] && continue
      ENTRIES=""
      for F in $IMG_FILES; do
        BASENAME=$(basename "$F")
        STEM="${BASENAME%.*}"
        # Skip scale variants (@2x/@3x and Android density suffixes) —
        # the base asset key represents them at the runtime layer.
        case "$STEM" in
          *@*x|*_xxxhdpi|*_xxhdpi|*_xhdpi|*_hdpi|*_mdpi|*_ldpi) continue ;;
        esac
        # camelCase the stem; split on both '-' and '_' so kebab-case
        # (icon-tick.svg) and snake_case (music_icon.svg) both map.
        CAMEL=$(printf '%s' "$STEM" | awk -F'[-_]' '{ printf("%s", $1); for(i=2;i<=NF;i++) if($i!="") printf("%s%s", toupper(substr($i,1,1)), substr($i,2)) }')
        [ -z "$CAMEL" ] && continue
        ENTRIES="${ENTRIES}  '${CAMEL}': require('./${MOD}/res/${BASENAME}'),
"
      done
      if [ -n "$ENTRIES" ]; then
        echo "__r['${MOD}/res'] = {"
        printf '%s' "$ENTRIES"
        echo "};"
      fi
    done
  } > "$IMG_REGISTRY"
fi

# ── Step 4e: Rewrite internal requires to relative paths ──
# Bare module requires (e.g. require('valdi_core/src/JSX')) become
# relative (e.g. require('../../valdi_core/src/JSX.js')). Also rewrites
# bare native names (e.g. require('Graphene') -> relative path to shim).
# Must run AFTER shim + registry generation so all src/ files exist.
if [ -d "$OUT/src" ]; then
  python3 "$BARE_REQ_SCRIPT" "$OUT/src" "$NATIVE_MAP"
  python3 "$BARE_REQ_SCRIPT" "$OUT/src" "$NATIVE_MAP" --validate
fi

# ── Step 5: Copy native_module_map.json to package root ──
[ -f "$NATIVE_MAP" ] && cp -f "$NATIVE_MAP" "$OUT/native_module_map.json"

# ── Step 6: Generate package.json with exports ──
# Disabled: exports field breaks consumers that use require.context() to scan
# the package directory (e.g. snapchat-web image asset loading). Re-enable
# when consumers migrate to asset registries and HMR support is added.
# Follow-up: tracked in project_valdi_web_followups (re-enable exports field).
# if [ -d "$OUT/src" ] && [ -f "$OUT/package.json" ]; then
#   chmod u+w "$OUT/package.json" 2>/dev/null || true
#   python3 "$EXPORTS_SCRIPT" "$OUT/package.json" "$OUT"
# fi

# ── Step 7: Generate ValdiModuleOverrides.d.ts + .js stub ──
# The .d.ts declares the ValdiModuleOverrides interface + __valdiModuleOverrides
# global. Consumers `import '<pkg>/ValdiModuleOverrides'` to pull the ambient
# declarations into scope. webpack needs a runtime file at that path too, so
# emit an empty .js sibling (the module has no runtime behaviour).
if [ -f "$SHIM_MAN" ]; then
  python3 "$OVERRIDES_SCRIPT" "$SHIM_MAN" "$OUT/ValdiModuleOverrides.d.ts" "$PKG_NAME"
  echo "// Runtime stub — types live in ValdiModuleOverrides.d.ts." > "$OUT/ValdiModuleOverrides.js"
fi

# ── Step 8: Generate _strings_preload.js for each module with locale JSON ──
# Reads the strings manifest (module_name \\t strings_dir) to know
# exactly where each module's locale JSONs live. Generates a preload
# file with lazy require() thunks registered on
# globalThis.__valdiPreloadedStrings. A bundler (e.g. webpack)
# resolves the require() calls at build time. On native this file
# does not exist so the fallback to runtime.getModuleEntry is used.
while IFS=$'\\t' read -r MOD_NAME SDIR; do
    [ -z "$MOD_NAME" ] && continue
    MOD_DIR="$OUT/src/$MOD_NAME"
    MOD_SRC_DIR="$MOD_DIR/src"
    STRINGS_DIR="$MOD_DIR/$SDIR"
    STRINGS_JS="$MOD_SRC_DIR/Strings.js"

    [ -f "$STRINGS_JS" ] || continue
    [ -d "$STRINGS_DIR" ] || continue

    JSONS=$(find "$STRINGS_DIR" -name "*.json" -type f 2>/dev/null | sort)
    [ -z "$JSONS" ] && continue

    PRELOAD="$MOD_SRC_DIR/_strings_preload.js"
    chmod u+w "$MOD_SRC_DIR"
    {
        echo "// Auto-generated: pre-loads locale JSON for bundler resolution"
        echo "(globalThis.__valdiPreloadedStrings = globalThis.__valdiPreloadedStrings || {})"
        printf "  ['%s'] = {\\n" "$MOD_NAME"
        echo "$JSONS" | while IFS= read -r json_file; do
            REL_PATH="${json_file#"$MOD_DIR"/}"
            printf "  '%s': () => require('../%s'),\\n" "$REL_PATH" "$REL_PATH"
        done
        echo "};"
    } > "$PRELOAD"

    # Append require('./_strings_preload') to Strings.js so the
    # bundler pulls in the preload file when this module is imported.
    # Appended (not prepended) to preserve "use strict"; directive.
    # Thunks are lazy so evaluation order doesn't matter.
    chmod u+w "$STRINGS_JS"
    echo "" >> "$STRINGS_JS"
    echo "require('./_strings_preload');" >> "$STRINGS_JS"
done < "$STRINGS_MAN"
""",
    )

    # Collect .d.ts files from modules as explicit inputs (they're copied by the shell via dts_manifest)
    dts_files = []
    for m in all_modules_for_dts.to_list():
        dts_files.extend(m[ValdiModuleInfo].web_dts_files)

    inputs = [manifest, strings_manifest, dts_manifest, shim_manifest, exports_script, overrides_script, bare_require_script] + ctx.files.srcs + dts_files
    args = [outdir.path, manifest.path, package_name]
    if native_map_file:
        inputs.append(native_map_file)
        args.append(native_map_file.path)
    else:
        args.append("/dev/null")
    args.append(shim_manifest.path)
    args.append(exports_script.path)
    args.append(overrides_script.path)
    args.append(strings_manifest.path)
    args.append(dts_manifest.path)
    args.append(bare_require_script.path)

    ctx.actions.run(
        inputs = inputs,
        outputs = [outdir],
        tools = [sh],
        executable = sh,
        arguments = args,
        progress_message = "Collapsing web paths, transforming requires, and rewriting .d.ts imports into {}".format(outdir.path),
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
        "native_module_map": attr.label(
            default = None,
            allow_single_file = True,
            doc = "JSON file mapping require() arguments to package subpaths. Generated by generate_native_module_map.",
        ),
        "modules": attr.label_list(
            default = [],
            providers = [ValdiModuleInfo],
            doc = "Valdi module targets for native module shim generation and strings manifest.",
        ),
        "module_id_overrides": attr.string_dict(
            default = {},
            doc = "Additional BUILD-level overrides for shim generation.",
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

def _json_string(s):
    """Minimal JSON string escaping."""
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'

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

# ── Native module map: JSON mapping for compiler require() transforms ──────

def _native_subpath(dest):
    """Compute the subpath for a native module from its dest_native path.

    dest_native gives paths like "valdi_core/web/DeviceBridge.js".
    Subpath strips /web/ and .js: "valdi_core/DeviceBridge".
    This becomes the package subpath: @snapchat/PKG/valdi_core/DeviceBridge
    """
    if "/web/" in dest:
        idx = dest.index("/web/")
        parent = dest[:idx]
        file_part = dest[idx + 5:]
        if file_part.endswith(".js"):
            file_part = file_part[:-3]
        return parent + "/" + file_part

    # No /web/ — strip .js and return as-is
    if dest.endswith(".js"):
        return dest[:-3]
    return dest

def _generate_native_module_map_impl(ctx):
    """Generate a JSON map from require() argument -> package subpath.

    The Python transform script reads this file to rewrite:
      require("DeviceBridge") -> require("valdi_core/DeviceBridge")
      require("valdi_core/src/DeviceBridge") -> require("valdi_core/DeviceBridge")

    The map includes both magic-string IDs and path-style IDs so the
    transform can handle all native module requires regardless of format.
    """
    module_id_overrides = dict(_merge_module_id_overrides_from_modules(ctx.attr.modules))
    module_id_overrides.update(ctx.attr.module_id_overrides)

    # Build the mapping: module_id -> subpath
    entries = {}
    seen_dest = {}
    for f in ctx.files.srcs:
        dest = _dest_native(f.short_path)
        if not dest.endswith(".js"):
            continue
        if not _should_register_native_module(dest):
            continue
        if dest in seen_dest:
            continue
        seen_dest[dest] = True

        subpath = _native_subpath(dest)

        # Always include the default derivation (path-style or magic-string)
        default_mid = _module_id_from_native_dest(dest)
        entries[default_mid] = subpath

        # Also include any explicit overrides (may add additional IDs)
        override_raw = module_id_overrides.get(dest, "")
        override_ids = [s.strip() for s in override_raw.split(",") if s.strip()]
        for mid in override_ids:
            entries[mid] = subpath

    # Write JSON
    json_lines = []
    for key in sorted(entries.keys()):
        json_lines.append("  {}: {}".format(
            _json_string(key),
            _json_string(entries[key]),
        ))

    out = ctx.actions.declare_file("native_module_map.json")
    ctx.actions.write(output = out, content = "{\n" + ",\n".join(json_lines) + "\n}\n")
    return [DefaultInfo(files = depset([out]))]

generate_native_module_map = rule(
    implementation = _generate_native_module_map_impl,
    attrs = {
        "srcs": attr.label_list(allow_files = True, doc = "Native web source files (from _all_web_deps)."),
        "modules": attr.label_list(
            default = [],
            providers = [ValdiModuleInfo],
            doc = "Valdi module targets whose web_register_native_module_id_overrides contribute to the map.",
        ),
        "module_id_overrides": attr.string_dict(
            default = {},
            doc = "Optional BUILD-level overrides.",
        ),
    },
)
