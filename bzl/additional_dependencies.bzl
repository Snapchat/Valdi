load("//bzl:nested_repository.bzl", "nested_repository")

# Additional Node.js download mirrors for internal Snap builds
# This gets prepended to the standard mirrors in nodejs_info.bzl
ADDITIONAL_NODE_URLS = []

SOURCES_FILEGROUP_BUILD_FILE_CONTENT = """
exports_files(glob(["**"]))
filegroup(
    name = "all_files",
    srcs = glob(["**/*"]),
    visibility = ["//visibility:public"],
)
"""

def setup_additional_dependencies():
    # Create repositories that point to local files stored in git LFS within the @valdi repository.
    # These binaries are stored locally in the bin/ directory structure.
    #
    # In bzlmod-based internal repo: @valdi is an external repository, so we use nested_repository
    # to point to subdirectories within it.
    #
    # In WORKSPACE-based mirrored repo: "valdi" is the main workspace itself, so we use
    # native.new_local_repository to point to local directories within the workspace.
    
    # Detect if we're in the main workspace or if @valdi is external
    # In WORKSPACE files, we can check if native.existing_rule("valdi") exists
    # If @valdi doesn't exist as an external repo, we're in the main workspace
    is_main_workspace = (native.existing_rule("valdi") == None)
    
    if is_main_workspace:
        # WORKSPACE-based repo: use new_local_repository to reference local directories
        native.new_local_repository(
            name = "valdi_compiler_linux",
            path = "bin/compiler/linux",
            build_file_content = SOURCES_FILEGROUP_BUILD_FILE_CONTENT,
        )
        
        native.new_local_repository(
            name = "valdi_compiler_macos",
            path = "bin/compiler/macos",
            build_file_content = SOURCES_FILEGROUP_BUILD_FILE_CONTENT,
        )
        
        native.new_local_repository(
            name = "valdi_pngquant_macos",
            path = "bin/pngquant/macos",
            build_file_content = SOURCES_FILEGROUP_BUILD_FILE_CONTENT,
        )
        
        native.new_local_repository(
            name = "valdi_pngquant_linux",
            path = "bin/pngquant/linux",
            build_file_content = SOURCES_FILEGROUP_BUILD_FILE_CONTENT,
        )
        
        native.new_local_repository(
            name = "valdi_compiler_companion",
            path = "bin/compiler_companion",
            build_file_content = SOURCES_FILEGROUP_BUILD_FILE_CONTENT,
        )
        
        native.new_local_repository(
            name = "clientsql",
            path = "bin/clientsql",
            build_file_content = SOURCES_FILEGROUP_BUILD_FILE_CONTENT,
        )
        
        native.new_local_repository(
            name = "jscore_libs",
            path = "third-party/jscore/libs",
            build_file_content = SOURCES_FILEGROUP_BUILD_FILE_CONTENT,
        )
    else:
        # Bzlmod-based internal repo: use nested_repository
        nested_repository(
            name = "valdi_compiler_linux",
            source_repo = "valdi",
            target_dir = "bin/compiler/linux",
        )

        nested_repository(
            name = "valdi_compiler_macos",
            source_repo = "valdi",
            target_dir = "bin/compiler/macos",
        )

        nested_repository(
            name = "valdi_pngquant_macos",
            source_repo = "valdi",
            target_dir = "bin/pngquant/macos",
        )

        nested_repository(
            name = "valdi_pngquant_linux",
            source_repo = "valdi",
            target_dir = "bin/pngquant/linux",
        )

        nested_repository(
            name = "valdi_compiler_companion",
            source_repo = "valdi",
            target_dir = "bin/compiler_companion",
        )

        nested_repository(
            name = "clientsql",
            source_repo = "valdi",
            target_dir = "bin/clientsql",
        )

        nested_repository(
            name = "jscore_libs",
            source_repo = "valdi",
            target_dir = "third-party/jscore/libs",
        )

    # Note: valdi_standalone and valdi_compiler_toolbox are built from source,
    # so they don't need separate repositories - they're referenced via @valdi// targets
