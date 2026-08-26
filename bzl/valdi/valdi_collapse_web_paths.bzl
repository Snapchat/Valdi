load(":valdi_compiled.bzl", "ValdiModuleInfo")
load(":valdi_web_workers.bzl", "validate_web_workers")

def _repository_relative_short_path(file):
    """Returns a File.short_path relative to the root of its owning repository."""
    rel = file.short_path
    if not rel.startswith("../"):
        return rel

    repo_name = file.owner.repo_name
    if not repo_name:
        fail("External file has no owning repository: {}".format(rel))

    external_prefix = "../{}/".format(repo_name)
    if not rel.startswith(external_prefix):
        fail("External file path {} does not match owning repository {}".format(rel, repo_name))

    return rel[len(external_prefix):]

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
    """True if this short_path is a native module .js auto-registered by RegisterNativeModules.js."""
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

    # Don't exclude native .js based on registration — compiler-generated package
    # files may require these native web implementation files directly.
    # Registration exclusion only affects auto-registration in RegisterNativeModules.js.
    return False

def _all_modules(modules):
    return depset(
        direct = modules,
        transitive = [m[ValdiModuleInfo].deps for m in modules],
    )

def _dest(rel):
    """Maps a source short_path to its destination path in the collapsed npm package."""

    if rel.endswith("_web_module_file_entries"):
        return "."

    # Handle package.json - keep it at root
    if rel.endswith("package.json"):
        return "package.json"

    if rel.endswith(".valdi_api_version.json"):
        return "src/valdi_api_version.json"

    # Generated RegisterNativeModules.js goes at src/ for NPM package consumers
    if rel.endswith("RegisterNativeModules.js"):
        return "src/RegisterNativeModules.js"

    if "protodecl_collapsed" in rel:
        return "src"

    # Single source of truth for native module .js: same predicate as generate_register_native_modules.
    # Any path that would be registered there goes to native/<dest_native> here.
    if _is_native_module_js(rel):
        return "native/" + _dest_native(rel)

    parts = rel.split("/")
    for i in range(1, len(parts)):
        if parts[i] == "res":
            module_name = parts[i - 1]
            tail = "/".join(parts[i:])
            return "src/{}/{}".format(module_name, tail)

    # Handle external repository paths (short_path starts with ../ for external repos)
    # and regular source paths. Extract everything after /src/valdi_modules/src/valdi/
    # Works with any external repo name (e.g., ../<repo>/src/valdi_modules/src/valdi/...)
    valdi_marker = "/src/valdi_modules/src/valdi/"

    # Try to find and strip the valdi marker from the path
    rel2 = rel
    is_valdi_source_path = False
    if valdi_marker in rel:
        idx = rel.find(valdi_marker)
        rel2 = rel[idx + len(valdi_marker):]
        is_valdi_source_path = True
    elif rel.startswith("src/valdi_modules/src/valdi/"):
        # Handle direct paths (non-external)
        rel2 = rel[len("src/valdi_modules/src/valdi/"):]
        is_valdi_source_path = True

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

    # Handle source .d.ts files rooted under Valdi's source tree.
    # These should go into src/<module_name>/src/...
    if rel.endswith(".d.ts") and is_valdi_source_path:
        # rel2 already has the marker stripped, so it's <module_name>/src/...
        # Return it as src/<module_name>/src/...
        return "src/{}".format(rel2)

    return rel

def _impl(ctx):
    outdir = ctx.actions.declare_directory(ctx.label.name)
    package_name = ctx.attr.package_name
    exclude_jsx = ctx.attr.exclude_jsx_global_declaration
    all_modules = _all_modules(ctx.attr.modules)
    web_workers = _collect_web_workers(all_modules)

    # Build manifest src -> dest. Deduplicate by dest (first source wins) so the same logical file
    # from tree artifact and filegroup doesn't trigger duplicate copies.
    manifest = ctx.actions.declare_file(ctx.label.name + ".manifest")
    seen_dest = {}
    lines = []
    for f in ctx.files.srcs:
        rel = _repository_relative_short_path(f)
        if exclude_jsx and "valdi_tsx/src/JSX.d.ts" in rel:
            continue
        if _should_exclude_from_package(rel):
            continue
        d = _dest(rel)
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
    # Used by CompilerToolbox to generate _strings_preload.js files for web bundlers.
    strings_manifest = ctx.actions.declare_file(ctx.label.name + ".strings_manifest")
    strings_lines = []
    for m in all_modules.to_list():
        info = m[ValdiModuleInfo]
        if info.strings_dir:
            strings_lines.append("{}\t{}".format(info.name, info.strings_dir))
    ctx.actions.write(strings_manifest, "\n".join(strings_lines) + "\n")

    # ── Build .d.ts manifest ──
    # source_path \t module_name (one line per source .d.ts file).
    # Used by CompilerToolbox to place source .d.ts alongside compiled .js.
    # Module name comes from ValdiModuleInfo — no path convention guessing.
    # Compiler-generated declarations are already placed by the main manifest
    # using _dest(); copying them again would retain their .valdi_build path.
    dts_manifest = ctx.actions.declare_file(ctx.label.name + ".dts_manifest")
    dts_lines = []
    for m in all_modules.to_list():
        info = m[ValdiModuleInfo]
        for f in info.web_input_dts_files:
            if f.path.endswith(".d.ts"):
                dts_lines.append("{}\t{}".format(f.path, info.name))
    ctx.actions.write(dts_manifest, "\n".join(dts_lines) + "\n")

    # ── Build web worker manifest ──
    # Browser worker entry module path repeated as the registration path and URL path.
    # CollapseWebPaths.cpp turns this into the host registry and its CommonJS bridge.
    web_workers_manifest = ctx.actions.declare_file(ctx.label.name + ".web_workers_manifest")
    web_workers_lines = [
        "{0}\t{0}".format(entry_path)
        for entry_path in sorted(web_workers.keys())
    ]
    ctx.actions.write(web_workers_manifest, "\n".join(web_workers_lines) + "\n")

    # module_name \t no-inline (one line per module that disables image inlining).
    image_policy_manifest = ctx.actions.declare_file(ctx.label.name + ".image_policy_manifest")
    image_policy_lines = sorted([
        "{}\tno-inline".format(module[ValdiModuleInfo].name)
        for module in all_modules.to_list()
        if module[ValdiModuleInfo].web_no_inline_images
    ])
    ctx.actions.write(image_policy_manifest, "\n".join(image_policy_lines) + "\n")

    # The companion AST transformer handles variable requires (→ moduleLoader.load)
    # and PrependWebJsProcessor strips extra args from string-literal requires.
    # Native module package files are emitted by the compiler from each module's
    # explicit web native module overrides.

    # Collect input .d.ts files from modules as explicit inputs.
    dts_files = []
    for m in all_modules.to_list():
        dts_files.extend(m[ValdiModuleInfo].web_input_dts_files)

    compiler_toolbox = ctx.executable._compiler_toolbox
    inputs = [manifest, strings_manifest, dts_manifest, web_workers_manifest, image_policy_manifest] + ctx.files.srcs + dts_files
    args = ctx.actions.args()
    args.add("collapse_web_paths")
    args.add("-o", outdir.path)
    args.add("-m", manifest)
    args.add("-p", package_name)
    args.add("-s", strings_manifest)
    args.add("-d", dts_manifest)
    args.add("-w", web_workers_manifest)
    args.add("-i", image_policy_manifest)

    ctx.actions.run(
        inputs = inputs,
        outputs = [outdir],
        tools = [compiler_toolbox],
        executable = compiler_toolbox,
        arguments = [args],
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
        "modules": attr.label_list(
            default = [],
            cfg = "exec",
            providers = [ValdiModuleInfo],
            doc = "Valdi module targets for strings and declaration manifests.",
        ),
        "_compiler_toolbox": attr.label(
            executable = True,
            cfg = "exec",
            default = Label("@valdi//valdi/compiler/toolbox:valdi_compiler_toolbox"),
        ),
    },
)

def _impl_native(ctx):
    outdir = ctx.actions.declare_directory(ctx.label.name)

    # Build a manifest of: SRC \t DEST
    manifest = ctx.actions.declare_file(ctx.label.name + ".manifest")
    lines = []
    for f in ctx.files.srcs:
        rel = _repository_relative_short_path(f)
        lines.append("{}\t{}".format(f.path, _dest_native(rel)))
    ctx.actions.write(manifest, "\n".join(lines))

    compiler_toolbox = ctx.executable._compiler_toolbox
    args = ctx.actions.args()
    args.add("collapse_paths")
    args.add("-o", outdir.path)
    args.add("-m", manifest)
    ctx.actions.run(
        inputs = [manifest] + ctx.files.srcs,
        outputs = [outdir],
        tools = [compiler_toolbox],
        executable = compiler_toolbox,
        arguments = [args],
        progress_message = "Collapsing native paths into {}".format(outdir.path),
    )
    return [DefaultInfo(files = depset([outdir]))]

collapse_native_paths = rule(
    implementation = _impl_native,
    attrs = {
        "srcs": attr.label_list(allow_files = True),
        "_compiler_toolbox": attr.label(
            executable = True,
            cfg = "exec",
            default = Label("@valdi//valdi/compiler/toolbox:valdi_compiler_toolbox"),
        ),
    },
)

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
    # Overrides: first from each module's ValdiModuleInfo, then BUILD-level overrides on top
    module_id_overrides = dict(_merge_module_id_overrides_from_modules(ctx.attr.modules))
    module_id_overrides.update(ctx.attr.module_id_overrides)

    out = ctx.actions.declare_file("{}_RegisterNativeModules.js".format(ctx.label.name))
    lines = [
        "",
        "/**",
        " * AUTO-GENERATED - Do not edit. Native module registrations for web runtime.",
        " * Generated from _all_web_deps.",
        " */",
        "",
        "var _cbs = globalThis.__valdiWebViewClassRegistryCallbacks =",
        "  globalThis.__valdiWebViewClassRegistryCallbacks || [];",
        "function _registerWebPolyglotViews(views) {",
        "  var registry = globalThis.__valdiWebViewClassRegistry;",
        "  if (!(registry instanceof Map)) {",
        "    registry = new Map();",
        "    globalThis.__valdiWebViewClassRegistry = registry;",
        "    _cbs.splice(0).forEach(function(callback) { callback(registry); });",
        "  }",
        "  Object.entries(views).forEach(function(entry) { registry.set(entry[0], entry[1]); });",
        "}",
        "",
    ]
    seen_dest = {}
    n = 0
    for f in ctx.files.srcs:
        rel = _repository_relative_short_path(f)
        dest = _dest_native(rel)
        if not dest.endswith(".js"):
            continue
        if not _should_register_native_module(dest):
            continue
        if dest in seen_dest:
            continue
        seen_dest[dest] = True
        raw_id = module_id_overrides.get(dest, "")
        module_ids = [s.strip() for s in raw_id.split(",") if s.strip()]
        require_path = "../native/" + dest[:-3]  # strip .js for require()
        var_name = "_n" + str(n)
        n += 1
        lines.append("var {} = require('{}');".format(var_name, require_path))
        for mid in module_ids:
            lines.append("globalThis.moduleLoader.registerModule('{}', () => {});".format(mid, var_name))
        lines.append("if ({v}.webPolyglotViews) {{".format(v = var_name))
        lines.append("  _registerWebPolyglotViews({v}.webPolyglotViews);".format(v = var_name))
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
            cfg = "exec",
            providers = [ValdiModuleInfo],
            doc = "Valdi module targets (e.g. deps of valdi_exported_library). Their web_register_native_module_id_overrides are merged to form the module ID map.",
        ),
        "module_id_overrides": attr.string_dict(
            default = {},
            doc = "Optional BUILD-level overrides (native dest path -> runtime module ID). Applied after module-declared overrides.",
        ),
    },
)

def _collect_web_workers(all_modules):
    """Collect and validate worker declarations from direct and transitive modules."""
    workers = {}
    for module in all_modules.to_list():
        info = module[ValdiModuleInfo]
        module_workers = info.web_workers
        validate_web_workers(module_workers, str(module.label))
        for entry_path in module_workers:
            workers[entry_path] = True
    return workers
