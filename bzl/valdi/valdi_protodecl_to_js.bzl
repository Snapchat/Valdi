NODEJS_TOOLCHAIN_TYPE = "@rules_nodejs//nodejs:toolchain_type"

def _protodecl_to_js_dir_impl(ctx):
    # Emits under: bazel-bin/<pkg>/<target>/__protodecl__/
    outdir = ctx.actions.declare_directory("%s/__protodecl__" % ctx.label.name)

    script = ctx.actions.declare_file(ctx.label.name + "_emit.sh")
    ctx.actions.write(
        output = script,
        is_executable = True,
        content = r"""#!/usr/bin/env bash
        set -euo pipefail
        OUT="$1"; shift
        rm -rf "$OUT"; mkdir -p "$OUT"

        node="$NODE"

        # For each input .protodecl, emit OUT/<rewritten path>.protodecl.js exporting a Uint8Array
        for f in "$@"; do
          # <SEG_BEFORE_LAST_SRC>/src/<AFTER_LAST_SRC>  (fallback: drop first segment)
          if [[ "$f" == *src/* ]]; then
            prefix="${f%src/*}"           # before LAST "src/<...>"
            prefix="${prefix%/}"          # drop trailing slash
            seg="${prefix##*/}"           # segment before that last src/
            after="${f##*src/}"           # AFTER the last "src/"
            rel="${seg:+$seg/}src/$after"
          else
            rel="${f#*/}"
          fi

          base="${rel%.*}"
          dest="$OUT/$base.protodecl.js"
          mkdir -p "$(dirname "$dest")"

          # Emit CommonJS that exports a base64 string of the raw bytes
          "$node" -e 'const fs=require("fs");const b=fs.readFileSync(process.argv[1]);process.stdout.write("module.exports="+JSON.stringify(b.toString("base64"))+";");' "$f" > "$dest"
        done
        """,
    )

    inputs = depset(transitive = [t[DefaultInfo].files for t in ctx.attr.srcs])
    args = [outdir.path] + [f.path for f in inputs.to_list()]

    node_info = ctx.toolchains[NODEJS_TOOLCHAIN_TYPE].nodeinfo
    node_path = node_info.target_tool_path
    node_inputs = node_info.tool_files

    ctx.actions.run(
        inputs = depset([script], transitive = [inputs]),
        outputs = [outdir],
        tools = node_inputs,
        executable = script,
        arguments = args,
        env = {"NODE": node_path},
        progress_message = "protodecl → js into %s" % outdir.path,
    )
    return [DefaultInfo(files = depset([outdir]))]

protodecl_to_js_dir = rule(
    implementation = _protodecl_to_js_dir_impl,
    attrs = {
        "srcs": attr.label_list(allow_files = True),
    },
    toolchains = [
        NODEJS_TOOLCHAIN_TYPE,
    ],
)

def _collapse_protodecl_paths_impl(ctx):
    # Final collapsed tree
    outdir = ctx.actions.declare_directory(ctx.label.name)

    script = ctx.actions.declare_file(ctx.label.name + "_collapse.sh")
    ctx.actions.write(
        output = script,
        is_executable = True,
        content = r"""#!/usr/bin/env bash
set -euo pipefail
OUT="$1"; shift
rm -rf "$OUT"; mkdir -p "$OUT"

rewrite_and_copy() {
  local src="$1" core rel

  # If coming from .../__protodecl__/..., drop that segment first
  if [[ "$src" == *"/__protodecl__/"* ]]; then
    core="${src#*__protodecl__/}"
  else
    core="$src"
  fi

  # Rewrite to: <SEG_BEFORE_LAST_SRC>/src/<AFTER_LAST_SRC>
  if [[ "$core" == *src/* ]]; then
    prefix="${core%src/*}"        # before LAST "src/<...>"
    prefix="${prefix%/}"
    seg="${prefix##*/}"           # segment before that last src/
    after="${core##*src/}"        # AFTER the last "src/"
    rel="${seg:+$seg/}src/$after"
  else
    # Fallback: drop first path segment
    rel="${core#*/}"
  fi

  local dest="$OUT/$rel"
  mkdir -p "$(dirname "$dest")"
  cp -f "$src" "$dest"
}

for p in "$@"; do
  if [[ -d "$p" ]]; then
    # Tree artifact: walk all files
    while IFS= read -r -d '' f; do
      rewrite_and_copy "$f"
    done < <(find "$p" -type f -print0)
  else
    rewrite_and_copy "$p"
  fi
done
""",
    )

    inputs = depset(transitive = [t[DefaultInfo].files for t in ctx.attr.srcs])
    args = [outdir.path] + [f.path for f in inputs.to_list()]

    ctx.actions.run(
        inputs = depset([script], transitive = [inputs]),
        outputs = [outdir],
        executable = script,
        arguments = args,
        progress_message = "Collapsing protodecl paths into %s" % outdir.path,
    )
    return [DefaultInfo(files = depset([outdir]))]

collapse_protodecl_paths = rule(
    implementation = _collapse_protodecl_paths_impl,
    attrs = {
        "srcs": attr.label_list(allow_files = True),
    },
)
