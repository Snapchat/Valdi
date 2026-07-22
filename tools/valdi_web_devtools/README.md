# valdi-web-devtools

Dev tooling for running a Valdi `valdi_exported_library` in a web playground:
webpack configuration helpers, an HMR dev server, integration-test scaffolding,
and the `valdi_web_playground` Bazel macro that stitches them together.

## Wiring a playground

A playground has three coupled files: two Bazel entries and one webpack config.
Three fields must agree across them; the runtime validates the match and fails
loudly if they diverge (see [Failure modes](#failure-modes) below).

### `BUILD.bazel`

```starlark
load("@composer_npm//:defs.bzl", "npm_link_all_packages")
load("@composer_npm//<path/to/your/playground>:webpack/package_json.bzl", webpack_bin = "bin")
load("@valdi//bzl/valdi:valdi_exported_library.bzl", "valdi_exported_library")
load("@valdi//tools/valdi_web_devtools:valdi_web_playground.bzl", "valdi_web_playground")

npm_link_all_packages(name = "node_modules")

valdi_exported_library(
    name = "my_lib",
    web_package_name = "my_lib_npm",
    npm_scope = "@myorg",  # OMIT if you want unscoped requires
    deps = [...],
)

valdi_web_playground(
    webpack_bin = webpack_bin,
    npm_package = ":my_lib_npm",
    srcs = glob(["playground/**/*.ts", "playground/**/*.tsx"]) + ["tsconfig.json"],
    test_srcs = glob(["tests/**"]),
)
```

### `playground/webpack.config.js`

```js
// Node-only webpack helpers live under the `/webpack` subpath. The package
// root (`valdi-web-devtools`) is reserved for browser-safe APIs
// (mountRoot, attachHmr) so playground application code can import them
// without dragging Node tooling into the browser bundle.
const { createWebpackConfig } = require('valdi-web-devtools/webpack');

module.exports = createWebpackConfig({
  npmPackageName: 'my_lib_npm',   // matches valdi_exported_library(web_package_name)
  npmScope: '@myorg',              // matches valdi_exported_library(npm_scope); omit if unscoped
  playgroundDir: __dirname,
  entry: './playground/main.ts',
  // Optional per-playground extras:
  resolve: { fallback: { fs: false } },
});
```

Application code that mounts the compiled Valdi module imports the browser
API from the package root:

```ts
import { mountRoot } from 'valdi-web-devtools';
```

## Three-way match

| Field | File | Purpose |
|---|---|---|
| `valdi_exported_library(npm_scope, web_package_name)` | `BUILD.bazel` | Baked into the emitted `require(...)` strings in compiled output. |
| `valdi_web_playground(npm_package)` | `BUILD.bazel` | Bazel label of the exported library above. |
| `createWebpackConfig({npmScope, npmPackageName})` | `playground/webpack.config.js` | Registers `resolve.alias['<scope>/<name>'] → <resolved package path>` so the compiled requires resolve at bundle time. |

The Bazel macro deliberately does not accept `npm_scope`. Scope handling is a
webpack-runtime concern — the `.bzl` never touches the webpack config content,
so any scope arg there would be inert.

## Failure modes

**"createWebpackConfig: package.json ... declares name X but the alias key ... is Y"** —
the three-way match is broken. Either the exported library's `npm_scope` /
`web_package_name` don't line up with the webpack config's `npmScope` /
`npmPackageName`, or the `npm_package` label points at a different library
than the one the webpack config expects. The error names both sides so you can
fix the mismatched file directly.

**"Module not found: `<scope>/<pkg>`" at webpack build time** — should no longer
happen for scope/name mismatches (the validation above intercepts it). If it
does surface for an intra-package path (e.g. `<scope>/<pkg>/src/foo`), the
package resolved but the file inside doesn't exist — check that the exported
library actually built the referenced source.

**"Cannot find module `<peer>`" from inside a webpack action** — a
peerDependency of `valdi-web-devtools` isn't reaching the consumer's webpack
action. The macro enumerates the required peers explicitly in `common_srcs`;
if you added a new peer to `package.json`, add the matching
`:node_modules/<pkg>` label to the list in `valdi_web_playground.bzl`.
