# Valdi Startup Benchmark

## Introduction

This module helps with measuring how long it takes in total to create the entirety of the Valdi runtime, render a very simple component and get the result. This only looks at the C++ and JS side, so it doesn't take in account the iOS/Android integration which provides some additional dependencies, nor the Snapchat iOS and Android integration which might provide further dependencies. It gives us an idea of the best possible startup performance for Valdi, in the ideal, non contended scenario, if we minimize the overhead of its integration (DI causing no overhead etc...).

## Running the benchmark

To test load latency with modules compiled as JS source:

```sh
# Build the benchmark binary
bzl build @valdi//valdi:startup_benchmark --snap_flavor=production -c opt --//bzl/valdi:js_bytecode_format=none

# Run for QuickJS
bazel-bin/valdi/startup_benchmark --js_engine quickjs
# Run for Hermes
bazel-bin/valdi/startup_benchmark --js_engine hermes
# Run for JSCore
bazel-bin/valdi/startup_benchmark --js_engine jscore
```

to test load latency with modules compiled as QuickJS bytecode:

```sh
# Build the benchmark binary
bzl build @valdi//valdi:startup_benchmark --snap_flavor=production -c opt --//bzl/valdi:js_bytecode_format=quickjs

# Run for QuickJS
bazel-bin/valdi/startup_benchmark --js_engine quickjs
```

to test load latency with modules compiled as Hermes bytecode:

```sh
# Build the benchmark binary
bzl build  @valdi//valdi:startup_benchmark --snap_flavor=production -c opt --//bzl/valdi:js_bytecode_format=hermes

# Run for QuickJS
bazel-bin/valdi/startup_benchmark --js_engine hermes
```

to test load latency with modules compiled with TSN, first make sure that the `module.yaml` of the valdi_core module has a very inclusive include_patterns (or remove the include_patterns altogether) so that all the files will end up being compiled with TSN, and then run:

```sh
# Build the benchmark binary
bzl build @valdi//valdi:startup_benchmark --snap_flavor=production -c opt --//bzl/valdi:js_bytecode_format=quickjs

# Run for QuickJS with TSN
bazel-bin/valdi/startup_benchmark --js_engine quickjs_tsn
```

In any of the `bzl build` command, you can specify the `--@valdi//bzl/runtime_flags:enable_logging` argument if you want runtime logs. This is useful to make sure the modules are loaded in the right mode for example:

```log
[0.003: INFO] Loaded JS module coreutils/src/unicode/TextCoding in 296.2 us (load mode: js bytecode) own memory usage: 229376 total memory usage: 229376
[0.004: INFO] Loaded JS module valdi_core/src/Valdi in 242.3 us (load mode: native) own memory usage: 0 total memory usage: 0
[0.005: INFO] Loaded JS module valdi_core/src/BuildType in 84.3 us (load mode: js source) own memory usage: 0 total memory usage: 0
```
