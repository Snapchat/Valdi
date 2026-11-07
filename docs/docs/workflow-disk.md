# Disk Management

<!--TODO: Validate the build artifact paths relative to a bootstrapped Valdi project -->

Bazel stores intermediate/final build artifacts locally and may not always clean them up properly. As a result, you may occasionally find it filling up your local disk. If this occurs, the below steps can help reclaim some disk space.

You may also choose to install a tool like `ncdu` to clean up your local disk, deleting IDE caches and older versions.

## Cleanup Steps

#### Bazel & Valdi

In your top level project folder (containing a `WORKSPACE` file):

```bash
cd path/to/your/project
watchman shutdown-server;
bazel clean --expunge;
bazel shutdown;
rm -rf ../.cache/;
```

#### Valdi Modules

We store additional build caches that are module specific, and are worth clearing for any local hot reloader or valdi module build issues. You can do that via the following commands:

```bash
cd path/to/your/project
rm -rf ./.valdi_build
```
