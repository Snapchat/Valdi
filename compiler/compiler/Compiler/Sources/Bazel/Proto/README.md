# Bazel Worker Protocol - Swift Generated Code

This directory contains Swift code generated from the [Bazel Worker Protocol](https://github.com/bazelbuild/bazel/blob/master/src/main/protobuf/worker_protocol.proto) protobuf definitions.

## Generating the Swift Code

Follow these steps to regenerate `worker_protocol.pb.swift`:

### 1. Install swift-protobuf

Build and install `swift-protobuf` following the instructions at:
https://github.com/apple/swift-protobuf

**Important:** Ensure you use a version compatible with the one defined in the compiler's `Package.swift`: Compiler/Package.swift#L15

The code in this directory was generated with **swift-protobuf version 1.25.2**.

### 2. Generate the Swift Code

From the repository root, run:

```bash
cd src/open_source/third-party/bazel/worker
protoc --swift_out=. worker_protocol.proto
```

This will generate `worker_protocol.pb.swift` in the same directory.

### 3. Copy the Generated File

Copy the generated file to this directory:

```bash
cp worker_protocol.pb.swift ../../../compiler/compiler/Compiler/Sources/Bazel/Proto/
```

Or use the full path from the repository root:

```bash
cp src/open_source/third-party/bazel/worker/worker_protocol.pb.swift \
   src/open_source/compiler/compiler/Compiler/Sources/Bazel/Proto/
```
