def _nested_repository_impl(ctx):
    # Try to resolve using relative path (works when source_repo is external)
    relative_dir = "../{}".format(ctx.attr.source_repo)
    check = ctx.execute(["test", "-d", relative_dir])

    if check.return_code == 0:
        # External repository exists (internal bzlmod setup)
        target_path = "{}/{}".format(relative_dir, ctx.attr.target_dir)
    else:
        # We're in the main workspace (WORKSPACE-based repo)
        # The source_repo is "valdi" which is the workspace itself

        # If workspace_dir is provided, use it
        if ctx.attr.workspace_dir:
            workspace_root = ctx.attr.workspace_dir
        else:
            # For WORKSPACE-based repos, we need to find the workspace root
            # The best way is to parse it from the current path structure
            # We're in: .../external/<repo_name>
            # We need to find where Bazel was originally invoked from

            # Get the current working directory
            pwd_result = ctx.execute(["pwd"])
            current_dir = pwd_result.stdout.strip()

            # Try to find a directory that contains WORKSPACE by going up from a known location
            # We know the output_base path structure, so we can look for workspace metadata
            find_ws = ctx.execute([
                "sh",
                "-c",
                # Look for a file that might have workspace path info
                "cat ../../server/command.log 2>/dev/null | grep -o 'Starting local .* server.*' | head -1",
            ])

            # For now, fail with a helpful message
            fail("nested_repository: Cannot auto-detect workspace root in WORKSPACE-based repos during fetch phase. " +
                 "Current dir: {}. Please pass workspace_dir attribute explicitly, e.g. workspace_dir = '/Users/cholgate/Projects/valdi_staging'. " +
                 "Find output: '{}'".format(current_dir, find_ws.stdout))

        target_path = "{}/{}".format(workspace_root, ctx.attr.target_dir)

    res = ctx.execute([
        "sh",
        "-c",
        "ls -1 {}".format(target_path),
    ])
    if res.return_code != 0:
        fail("Failed to resolve files in target dir {}: {}".format(target_path, res.stderr))

    file_list = res.stdout.strip().split("\n")

    # Check if a BUILD file already exists in the target directory
    has_build_file = False
    for f in file_list:
        if f in ["BUILD", "BUILD.bazel"]:
            has_build_file = True
            break

    # Create symlinks for all files
    for f in file_list:
        ctx.symlink("{}/{}".format(target_path, f), f)

        # Only generate a BUILD file if one doesn't already exist
    if not has_build_file:
        build_content = "# Auto-generated BUILD file for nested_repository\n"
        build_content += "package(default_visibility = [\"//visibility:public\"])\n\n"

        # Export all files so they can be referenced as targets
        build_content += "exports_files(["
        for f in file_list:
            build_content += "\n    \"{}\",".format(f)
        build_content += "\n])\n"

        ctx.file("BUILD.bazel", build_content)

nested_repository = repository_rule(
    implementation = _nested_repository_impl,
    attrs = {
        "source_repo": attr.string(mandatory = True),
        "target_dir": attr.string(mandatory = True),
        "workspace_dir": attr.string(default = ""),
    },
    local = True,
)
