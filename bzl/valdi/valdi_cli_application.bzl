load("//bzl:expand_template.bzl", "expand_template")
load("//bzl/valdi:suffixed_deps.bzl", "get_suffixed_deps")

def valdi_cli_application(
        name,
        script_path,
        visibility = ["//visibility:public"],
        enable_http = False,
        terminal = False,
        deps = []):
    """ Builds a Valdi CLI application into a single self contained binary.

    Args:
        name: The name of the generated binary.
        script_path: The entry point script, as <module_name>/<path_without_extension>.
        visibility: The visibility of the Bazel target.
        enable_http: Links a libcurl backed HTTP client into the binary, which valdi_http needs at
            runtime. Off by default because it pulls in curl and BoringSSL, which an application
            making no network requests should not have to carry. Left off, requests reject with
            "No RequestManager set".
        terminal: Registers the native terminal backend that valdi_tui renders through, takes ownership
            of the controlling terminal (raw mode, keyboard input, resize, Ctrl+C) and restores it when
            the process ends or is signalled, and runs the JS runtime as macOS or Linux. Required for
            any application that calls valdi_tui's render(); without it render() fails because the
            backend is not registered. Off by default so other applications link no terminal code.
        deps: The Valdi modules to include in the build.
    """
    main_target = "{}_main".format(name)

    includes = []
    if enable_http:
        includes.append('#include "valdi/standalone_http/CurlHTTPRequestManager.hpp"')
        request_manager = "    options.requestManager = Valdi::makeCurlHTTPRequestManager();"
    else:
        # No backend, so valdi_http rejects every request with "No RequestManager set".
        request_manager = "    options.requestManager = nullptr;"

    if terminal:
        includes.extend([
            '#include "valdi/standalone_runtime/StandaloneMainQueue.hpp"',
            '#include "valdi/standalone_runtime/ValdiStandaloneRuntime.hpp"',
            '#include "valdi/terminal/Host/TerminalHost.hpp"',
            '#include "valdi/terminal/Modules/TerminalModuleFactoriesProvider.hpp"',
        ])
        providers = "\n".join([
            "    auto& terminalHost = Valdi::Terminal::TerminalHost::shared();",
            "    options.moduleFactoriesProviders.emplace_back(Valdi::makeTerminalModuleFactoriesProvider(&terminalHost));",
            "    options.beforeRun = [&terminalHost](Valdi::ValdiStandaloneRuntime& runtime) {",
            "        // After the crash handlers valdiCLIRun installed, so the terminal is restored before they print.",
            "        terminalHost.installSignalHandlers();",
            "        terminalHost.setExitHandler([queue = Valdi::weakRef(runtime.getMainQueue().get())](int code) {",
            "            auto mainQueue = queue.lock();",
            "            if (mainQueue != nullptr) {",
            "                mainQueue->exit(code);",
            "            }",
            "        });",
            "    };",
        ])
        platform = "Valdi::Terminal::hostPlatformType()"
    else:
        providers = ""
        platform = "Valdi::PlatformTypeIOS"

    expand_template(
        name = main_target,
        src = "@valdi//bzl/valdi/app_templates:cli_main.cpp.tpl",
        output = "main.cpp",
        substitutions = {
            "@VALDI_SCRIPT_PATH@": script_path,
            "@VALDI_EXTRA_INCLUDES@": "\n".join(includes),
            "@VALDI_REQUEST_MANAGER@": request_manager,
            "@VALDI_PROVIDERS@": providers,
            "@VALDI_PLATFORM@": platform,
        },
    )

    native.cc_binary(
        name = name,
        linkshared = False,
        visibility = visibility,
        srcs = [":{}".format(main_target)],
        tags = ["valdi_cli_application"],
        deps = [
                   "@valdi//valdi:cli_runner",
                   "@valdi//src/valdi_modules/src/valdi/valdi_core:valdi_core_native",
               ] + (["@valdi//valdi:valdi_standalone_http"] if enable_http else []) +
               (["@valdi//valdi:valdi_terminal"] if terminal else []) +
               get_suffixed_deps(deps, "_native"),
    )
