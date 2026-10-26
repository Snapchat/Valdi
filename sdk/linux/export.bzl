"""Describe a Linux SDK using the existing CcInfo and Valdi toolchain contracts."""

load("//bzl/valdi:valdi_toolchain_type.bzl", "VALDI_TOOLCHAIN_TYPE")

def _closure(deps):
    contexts = [dep[CcInfo].compilation_context for dep in deps]
    headers = depset(transitive = [c.headers for c in contexts])
    linkers = depset(
        transitive = [dep[CcInfo].linking_context.linker_inputs for dep in deps],
        order = "topological",
    )
    archives = []
    inputs = []
    link = []
    for linker in linkers.to_list():
        # Preserve repeated flags and their order (e.g. linker groups).
        for flag in linker.user_link_flags:
            link.append({"flag": flag})
        inputs.extend(linker.additional_inputs)
        for lib in linker.libraries:
            archive = lib.pic_static_library or lib.static_library
            if archive:
                archives.append(archive)
                link.append({
                    "archive": archive.path,
                    "alwayslink": lib.alwayslink,
                    "pic": lib.pic_static_library != None,
                    "owner": str(linker.owner),
                })
            elif lib.dynamic_library or lib.interface_library or lib.objects or lib.pic_objects:
                fail("SDK requires an archived static closure: " + str(linker.owner))
    return struct(
        files = depset(inputs, transitive = [headers, depset(archives)]),
        metadata = {
            "headers": [f.path for f in headers.to_list()],
            "includes": depset(transitive = [c.includes for c in contexts]).to_list(),
            "quote_includes": depset(transitive = [c.quote_includes for c in contexts]).to_list(),
            "system_includes": depset(transitive = [c.system_includes for c in contexts]).to_list(),
            "defines": depset(transitive = [c.defines for c in contexts]).to_list(),
            "link": link,
            "additional_inputs": [f.path for f in inputs],
        },
    )

def _export_impl(ctx):
    runtime = _closure(ctx.attr.deps)
    # A single aggregate CcInfo gives the external fixture one complete,
    # topologically ordered link sequence without merging overlapping closures.
    fixture = _closure(ctx.attr.deps + ctx.attr.fixture_deps)
    toolchain = ctx.toolchains[VALDI_TOOLCHAIN_TYPE].info
    tools = {}
    tool_files = []
    for name, target in [
        ("compiler", toolchain.compiler),
        ("toolbox", toolchain.compiler_toolbox),
        ("companion", toolchain.companion),
        ("minify_config", toolchain.minify_config),
    ]:
        files = target[DefaultInfo].files
        tools[name] = {"label": str(target.label), "files": [f.path for f in files.to_list()]}
        tool_files.append(files)
    metadata = ctx.actions.declare_file(ctx.label.name + ".json")
    ctx.actions.write(metadata, json.encode({
        "schema": "valdi.linux-sdk-export/1",
        "runtime": runtime.metadata,
        "fixture": fixture.metadata,
        "tools": tools,
    }))
    return [DefaultInfo(files = depset([metadata], transitive = [runtime.files, fixture.files] + tool_files))]

linux_sdk_export = rule(
    implementation = _export_impl,
    attrs = {
        "deps": attr.label_list(providers = [CcInfo]),
        "fixture_deps": attr.label_list(providers = [CcInfo]),
    },
    toolchains = [VALDI_TOOLCHAIN_TYPE],
)
