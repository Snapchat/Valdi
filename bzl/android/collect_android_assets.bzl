load("//bzl/valdi:valdi_compiled.bzl", "ValdiModuleInfo")

def _collect_valdimodules_from_valdi_deps(valdi_deps, output_target):
    """Extract valdimodules and sourcemaps transitively from Valdi modules.

    Args:
        valdi_deps: List of Valdi module targets with ValdiModuleInfo
        output_target: "debug" or "release" - which outputs to extract
    """

    # Collect all modules (current + transitive via ValdiModuleInfo.deps)
    all_modules = []
    for module in valdi_deps:
        if ValdiModuleInfo not in module:
            continue

        module_info = module[ValdiModuleInfo]

        # Add transitive dependencies
        if hasattr(module_info, "deps") and module_info.deps:
            all_modules.extend(module_info.deps.to_list())

        # Include current module
        all_modules.append(module)

    # Determine which fields to extract based on output_target
    valdimodule_field = "android_{}_valdimodule".format(output_target)
    sourcemaps_field = "android_{}_sourcemaps".format(output_target)

    # Extract assets from each module
    assets = []
    for module in all_modules:
        if ValdiModuleInfo in module:
            module_info = module[ValdiModuleInfo]

            # Add valdimodule
            valdimodule = getattr(module_info, valdimodule_field, None)
            if valdimodule:
                assets.append(valdimodule)

            # Add sourcemaps (only for debug builds)
            if output_target == "debug":
                sourcemaps = getattr(module_info, sourcemaps_field, None)
                if sourcemaps:
                    assets.append(sourcemaps)

    return assets

def _collect_assets_impl(ctx):
    # Collect valdimodules from valdi compiled modules (includes transitive deps)
    all_assets = _collect_valdimodules_from_valdi_deps(ctx.attr.valdi_deps, ctx.attr.output_target)

    # Ensure we have at least one asset (AAR requires assets directory)
    if not all_assets:
        empty = ctx.actions.declare_file("empty")
        ctx.actions.write(empty, "")
        all_assets.append(empty)

    # Package assets into a zip file
    out_zip = ctx.actions.declare_file("{}_assets.zip".format(ctx.label.name))
    scratch = ctx.actions.declare_directory("{}_assets_dir".format(ctx.label.name))

    script = ctx.actions.declare_file("{}_pack_assets.sh".format(ctx.label.name))
    ctx.actions.write(
        script,
        is_executable = True,
        content = """
set -euo pipefail
ITEMS=( {items} )
DEST='{dst}'

for src in "${{ITEMS[@]}}"; do
    if [[ -d "$src" ]]; then
        # Copy directory contents, preferring assets/ subdirectory if it exists
        [[ -d "$src"/assets ]] && src="$src"/assets
        cp -R "$src"/. "$DEST"/
    else
        # Copy individual file
        cp "$src" "$DEST"/
    fi
    chmod -R u+w "$DEST"
done

# Create zip from staging directory
ABS_DEST="$PWD/{zip}"
cd "$DEST" && zip -qqr "$ABS_DEST" .
""".format(
            items = " ".join(['"{}"'.format(f.path) for f in all_assets]),
            dst = scratch.path,
            zip = out_zip.path,
        ),
    )

    ctx.actions.run_shell(
        inputs = depset(all_assets),
        tools = [script],
        outputs = [out_zip, scratch],
        command = script.path,
        progress_message = "Packaging Android assets for %{label}",
    )

    return [DefaultInfo(files = depset([out_zip]))]

collect_android_assets = rule(
    implementation = _collect_assets_impl,
    attrs = {
        "valdi_deps": attr.label_list(
            providers = [ValdiModuleInfo],
            mandatory = True,
            doc = "Valdi compiled module targets (extracts valdimodules transitively via ValdiModuleInfo)",
        ),
        "output_target": attr.string(
            mandatory = True,
            doc = "Output target to extract (debug or release), determined by build configuration",
        ),
        "deps": attr.label_list(
            allow_rules = ["android_library", "aar_import", "kt_android_library"],
            allow_files = False,
            default = [],
            doc = "Android library dependencies (unused, kept for compatibility)",
        ),
    },
    outputs = {"assets_zip": "%{name}_assets.zip"},
    doc = """Collects .valdimodule and .map.json files from Valdi modules for Android AARs.
    
Extracts valdimodules and sourcemaps from each Valdi module and its transitive
dependencies via ValdiModuleInfo.deps. The output_target (debug/release) is 
automatically determined based on the build configuration.
""",
)
