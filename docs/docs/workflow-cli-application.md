# Building a CLI application

With Valdi, you can build CLI applications using TypeScript, running under the Valdi runtime, into a single binary. The built binary is self contained and will have all the dependencies required for the CLI application to run, including any native dependencies that you might bring into the build.

## Getting started

You can define a CLI application using the `valdi_cli_application()` rule. The rule takes a list of Valdi modules dependencies to include in the build, and `script_path`, which is the path to the script that should be evaluated as the entry point to the application. The script path is in the format `<module_name>/<path_to_file_without_extension>`, for example `my_module/src/my_file`.

Here is an example of a `BUILD.bazel` of a Valdi CLI application:
```python
load("//bzl/valdi:valdi_cli_application.bzl", "valdi_cli_application")
load("//bzl/valdi:valdi_module.bzl", "valdi_module")

valdi_module(
    name = "cli_example",
    srcs = glob([
        "**/*.ts",
        "**/*.tsx",
    ]),
    deps = [],
)

valdi_cli_application(
    name = "cli_example_app",
    script_path = "cli_example/index",
    deps = [":cli_example"],
)
```

## Making network requests

The [`valdi_http`](./stdlib-http.md) module needs a native HTTP client, and a CLI application only links one in when you ask for it. Set `enable_http = True` on `valdi_cli_application()` as well as adding `valdi_http` to your module's `deps`:

```python
valdi_module(
    name = "cli_example",
    srcs = glob([
        "**/*.ts",
    ]),
    deps = ["//src/valdi_modules/src/valdi/valdi_http"],
)

valdi_cli_application(
    name = "cli_example_app",
    script_path = "cli_example/index",
    enable_http = True,
    deps = [":cli_example"],
)
```

It is off by default because it links libcurl and BoringSSL into the binary, which an application that makes no network requests should not have to carry. Leaving it off still builds, and every request rejects at runtime with `No RequestManager set`.

You can then run your application using `valdi install cli`.
You can package your application into a single binary using `valdi package cli`.