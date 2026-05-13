# Using a local Valdi compiler for testing

When you change the code generator (ObjC, Swift, Kotlin), Bazel must use your built compiler instead of the prebuilt one.

## One-time setup

From the **client repo root** (directory that contains `src/open_source`):

```bash
cd src/open_source/compiler/compiler
./scripts/use_local_compiler.sh
```

This will:
1. Build the compiler (`update_compiler.sh -o bin/macos`)
2. Package it as `bin/macos.tar.gz`
3. Update `src/open_source/bzl/open_source_archives.bzl` to point `valdi_compiler_macos` at that file (and set the correct hash)

Then clear Bazel’s cache so it re-fetches the compiler:

```bash
# From client repo root
rm -rf ~/.cache/bazel/*/external/valdi_compiler_macos
# or
bzl clean --expunge
```

## Reverting to the prebuilt compiler

In `src/open_source/bzl/open_source_archives.bzl`, set the `valdi_compiler_macos` entry back to the `gs://` URL and the original hash (or simply revert the local change with git).
