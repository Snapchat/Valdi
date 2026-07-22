"""Macro for valdi_web playground projects.

Generates webpack bundle, analyze, and HMR dev-server targets from a minimal
set of project-specific parameters. Optionally emits `serve` and
`integration_test` targets when the caller supplies static-server and
puppeteer test-utility labels — this keeps the open-source distribution free
of any specific test harness while remaining a drop-in for Snap's internal
valdi_web consumers.

Usage in BUILD.bazel:

    load("@valdi//tools/valdi_web_devtools:valdi_web_playground.bzl", "valdi_web_playground")

    valdi_web_playground(
        npm_package = ":valdi_web_my_project_npm",
        srcs = glob(["playground/**/*.js"]),
        test_srcs = glob(["tests/**"]),
    )
"""

load("@aspect_rules_js//js:defs.bzl", "js_binary", "js_test")
load("@aspect_rules_js//npm:defs.bzl", "npm_link_package", _npm_package_rule = "npm_package")

def valdi_web_playground(
        webpack_bin,
        npm_package,
        srcs = [],
        test_srcs = [],
        webpack_config = "playground/webpack.config.js",
        serve_entry = "serve.js",
        extra_srcs = [],
        extra_test_data = [],
        test_entry = "tests/run_tests.js",
        hmr_config = None,
        hmr_extra_data = []):
    """Creates playground bundle, analyze, dev, serve, and integration test targets.

    Targets emitted:
        playground_bundle - webpack production bundle
        analyze           - runnable: builds bundle + opens interactive treemap
        stats             - stats.json build output
        playground        - filegroup of index.html + bundle
        serve             - static dev server
        integration_test  - Puppeteer integration tests
        dev               - webpack-dev-server with HMR (only if hmr_config set)

    Args:
        webpack_bin: `bin` module loaded from
            @<your-npm>//path/to/valdi_web_devtools:webpack/package_json.bzl.
            Caller must supply so the invocation uses the caller's webpack
            copy (webpack + ts-loader + config must all resolve through one
            npm tree; mixing two workspace's webpack/webpack-cli otherwise
            breaks module resolution at bundle time).
        npm_package: label for the valdi_exported_library npm package target.
            Scope handling (if the exported library is scoped, e.g.
            "@snapchat/...") is NOT declared here — it lives in the caller's
            `playground/webpack.config.js` via
            `createWebpackConfig({npmScope, npmPackageName})`. See
            `valdi-web-devtools/webpack.js` for the `resolve.alias` contract that
            translates the compiled requires into paths on disk.
        srcs: glob result for playground source files (must be passed from
            BUILD since .bzl can't glob)
        test_srcs: glob result for test files (must be passed from BUILD since
            .bzl can't glob)
        webpack_config: webpack config path relative to chdir
        serve_entry: entry point for the serve js_binary
        extra_srcs: additional srcs for webpack beyond the defaults
        extra_test_data: additional data for integration tests
        test_entry: entry point for the test runner
        hmr_config: optional path (relative to the playground package) to a
            webpack HMR config. When set, the macro emits a `:dev` js_binary
            target that runs webpack-dev-server against workspace source
            files via the shared
            @valdi//tools/valdi_web_devtools:hmr-server wrapper. Also wraps
            `npm_package` in an `npm_package` rule and links it into
            `node_modules/<base>` so node-style resolution works at runtime.
        hmr_extra_data: extra data labels for the :dev js_binary (e.g.
            additional node_modules deps a playground's webpack.hmr.config.js
            requires beyond the common set).
    """

    # Derive npm package name from label (e.g. ":valdi_web_adex_ad_preview_npm" -> "valdi_web_adex_ad_preview_npm")
    npm_package_name = npm_package.lstrip(":")

    # valdi-compiler-js carries the JSX transformer ts-loader needs to
    # compile playground .tsx files (Valdi <view>/<label> intrinsics are
    # statement-form and need rewriting to renderer calls). Hoist the link
    # so the production bundle target picks it up via common_srcs too — the
    # transformer is wired into ts-loader by valdi-web-devtools/webpack when
    # the module is resolvable from the playground.
    if not native.existing_rule("node_modules/valdi-compiler-js"):
        npm_link_package(
            name = "node_modules/valdi-compiler-js",
            src = "@valdi//compiler/companion:package",
            package = "valdi-compiler-js",
            root_package = native.package_name(),
        )

    # valdi-web-devtools ships from open_source/tools/valdi_web_devtools as
    # an npm_package (analogous to valdi-compiler-js). Consumers link it here
    # so playgrounds can `require('valdi-web-devtools/webpack')` from their
    # webpack configs and `import { mountRoot } from 'valdi-web-devtools'`
    # from playground application code (see package.json `exports` for the
    # full subpath map). Kept out of composer_npm's pnpm workspace on purpose — the
    # BUILD.bazel is @valdi_npm-native, and cross-module consumption goes
    # through this npm_link_package. Consumers must declare
    # valdi-web-devtools's peerDependencies (webpack ecosystem) in their own
    # package.json so composer_npm materializes them under the consumer's
    # node_modules; the macro's common_srcs then references those local
    # `:node_modules/*` labels.
    if not native.existing_rule("node_modules/valdi-web-devtools"):
        npm_link_package(
            name = "node_modules/valdi-web-devtools",
            src = "@valdi//tools/valdi_web_devtools:package",
            package = "valdi-web-devtools",
            root_package = native.package_name(),
        )

    # Wrap the exported library as an npm package + link it into node_modules.
    # Used by tsconfig "paths" so ts-loader's type checker can resolve
    # cross-package imports under transpileOnly: false. Without this link the
    # production bundle target sees TS2307 once the Valdi JSX transformer
    # (which requires a real Program) is wired in.
    if not native.existing_rule("{}_pkg".format(npm_package_name)):
        _npm_package_rule(
            name = "{}_pkg".format(npm_package_name),
            srcs = [npm_package],
            package = npm_package_name,
            root_paths = ["{}/{}".format(native.package_name(), npm_package_name)],
            visibility = ["//visibility:public"],
        )

    if not native.existing_rule("node_modules/{}".format(npm_package_name)):
        npm_link_package(
            name = "node_modules/{}".format(npm_package_name),
            src = ":{}_pkg".format(npm_package_name),
            root_package = native.package_name(),
        )

    # When hmr_config is set the JSX transformer wires ts-loader with
    # transpileOnly: false (it needs a real Program for typeChecker access).
    # That makes ts-loader type-check the whole project — types/runtime deps
    # that callers passed via hmr_extra_data (e.g. @types/node, source-map)
    # are needed by the production bundle target too, not just :dev. Fold
    # hmr_extra_data into common_srcs so both stay in sync.
    # valdi-web-devtools's peerDependencies must be materialized in the
    # caller package's own node_modules tree (each consumer's BUILD.bazel
    # calls npm_link_all_packages against its own npm repo). Enumerating
    # them here surfaces the labels into the webpack action's runfiles so
    # `require('ts-loader')` etc. resolve from a single, consistent
    # virtual store.
    #
    # Keep this list in sync with `tools/valdi_web_devtools/package.json`
    # peerDependencies. Adding a peer there without adding the matching
    # `:node_modules/<pkg>` label here will fail at consumer webpack-action
    # time with a bare "Cannot find module <pkg>".
    common_srcs_dict = {}
    for label in [
        ":node_modules/@protobuf-ts/runtime",
        ":node_modules/file-loader",
        ":node_modules/path-browserify",
        ":node_modules/ts-loader",
        ":node_modules/typescript",
        ":node_modules/url-loader",
        ":node_modules/valdi-compiler-js",
        ":node_modules/valdi-web-devtools",
        ":node_modules/webpack",
        ":node_modules/webpack-bundle-analyzer",
        ":node_modules/webpack-cli",
        ":node_modules/webpack-dev-server",
        ":node_modules/{}".format(npm_package_name),
        npm_package,
    ] + srcs + extra_srcs + (hmr_extra_data if hmr_config else []):
        common_srcs_dict[label] = True
    common_srcs = common_srcs_dict.keys()

    # Production bundle
    webpack_bin.webpack(
        name = "playground_bundle",
        srcs = common_srcs,
        args = ["--config", webpack_config],
        chdir = native.package_name(),
        out_dirs = ["dist"],
    )

    # Runnable bundle analyzer: bzl run :analyze --define enable_web=true
    webpack_bin.webpack_binary(
        name = "analyze",
        chdir = native.package_name(),
        data = common_srcs,
        args = ["--config", webpack_config],
        env = {"ANALYZE": "true"},
    )

    # Stats output: bzl build :stats --define enable_web=true
    # Build target (not run) so stats.json lands in bazel-bin for programmatic access.
    webpack_bin.webpack(
        name = "stats",
        srcs = common_srcs,
        args = ["--config", webpack_config],
        chdir = native.package_name(),
        env = {"STATS": "true"},
        out_dirs = ["dist"],
    )

    native.filegroup(
        name = "playground",
        srcs = [
            "playground/index.html",
            ":playground_bundle",
        ],
    )

    # HMR dev server: workspace-live webpack-dev-server with Valdi Fast Refresh.
    # Generates the npm_package wrap + node_modules link + js_binary in one
    # macro invocation so playgrounds only need to provide their own
    # webpack.hmr.config.js. See @valdi//tools/valdi_web_devtools:hmr-server
    # for the wrapper that bridges runfiles into the workspace.
    if hmr_config:
        # Dedupe data labels — srcs already includes tsconfig.json and
        # the hmr_config file via the caller's glob, so we can't list them
        # again without Bazel complaining about duplicate labels.
        # createHmrConfig delegates to createWebpackConfig, whose asset-loader
        # rule does `require.resolve('url-loader')` + `('file-loader')` at
        # config load time. These must be materialized into the :dev runfiles
        # too — omitting them crashes `bzl run :dev` with MODULE_NOT_FOUND.
        hmr_data = {}
        for label in [
            ":node_modules/{}".format(npm_package_name),
            ":node_modules/file-loader",
            ":node_modules/ts-loader",
            ":node_modules/typescript",
            ":node_modules/url-loader",
            ":node_modules/valdi-compiler-js",
            ":node_modules/valdi-web-devtools",
            ":node_modules/webpack",
            ":node_modules/webpack-dev-server",
        ] + srcs + extra_srcs + hmr_extra_data:
            hmr_data[label] = True

        js_binary(
            name = "dev",
            chdir = native.package_name(),
            entry_point = "@valdi//tools/valdi_web_devtools:hmr-server",
            data = hmr_data.keys(),
            args = [
                "--package-path",
                native.package_name(),
                "--config",
                hmr_config,
            ],
        )

    js_binary(
        name = "serve",
        data = [
            ":node_modules/valdi-web-devtools",
            ":playground",
        ],
        entry_point = serve_entry,
    )

    js_test(
        name = "integration_test",
        size = "large",
        timeout = "long",
        chdir = native.package_name(),
        data = [
            "jest.config.js",
            ":playground",
            ":node_modules/@puppeteer/browsers",
            ":node_modules/jest",
            ":node_modules/puppeteer",
            ":node_modules/valdi-web-devtools",
        ] + test_srcs + extra_test_data,
        entry_point = test_entry,
        tags = [
            "integration",
            "requires-network",
        ],
    )
