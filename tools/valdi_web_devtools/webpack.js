// @ts-check
const path = require('path');
const fs = require('fs');

/**
 * @typedef {Object} WebpackConfigOptions
 * @property {string} npmPackageName - npm package name, e.g., 'valdi_web_adex_ad_preview_npm'
 * @property {string} playgroundDir - absolute path to the playground/ directory
 * @property {string} entry - entry point relative to chdir, e.g., './playground/main.ts'
 * @property {string} [npmScope] - npm scope prefix used for the exported library
 *   (e.g. '@snapchat'). Wired into a Bazel-sandbox alias so compiled code
 *   using `<scope>/<pkg>` requires resolves back to the local npm package.
 *   Leave empty if the exported library isn't scoped.
 * @property {Record<string, any>} [resolve] - additional resolve config to merge
 */

/**
 * @typedef {Object} DevServerOptions
 * @property {number} [port] - dev server port (default: 3030)
 */

/**
 * Resolves the npm package path.
 *
 * Priority:
 *  1. `<projectDir>/<npmPackageName>` — the Bazel sandbox layout the
 *     valdi_web_playground macro produces via `npm_package(root_paths=[…])`.
 *  2. `<projectDir>/node_modules/<npmPackageName>` — the workspace-live
 *     layout the hmr-server bridges into place (the macro links the
 *     exported library into node_modules alongside its runfiles).
 *
 * The second fallback is what makes the HMR dev server work: the sandbox
 * `<projectDir>/<npmPackageName>` path only exists during a Bazel action,
 * whereas the workspace playground dir has a symlinked node_modules that
 * contains the npm-linked exported library.
 *
 * @param {string} projectDir - absolute path to the project directory
 * @param {string} npmPackageName - npm package directory name
 * @returns {string} resolved path to npm package
 */
function resolveNpmPackage(projectDir, npmPackageName) {
  const localPath = path.resolve(projectDir, npmPackageName);
  if (fs.existsSync(localPath)) return localPath;
  const nodeModulesPath = path.resolve(projectDir, 'node_modules', npmPackageName);
  if (fs.existsSync(nodeModulesPath)) return nodeModulesPath;
  throw new Error(
    `resolveNpmPackage: cannot find ${npmPackageName} under ${projectDir} — ` +
    `checked ${localPath} and ${nodeModulesPath}. Ensure the valdi_web_playground ` +
    `macro linked it into node_modules or that the exported library was built.`,
  );
}

/**
 * Creates a webpack config for a valdi_web playground project.
 *
 * @param {WebpackConfigOptions} options
 * @returns {import('webpack').Configuration}
 */
function createWebpackConfig({ npmPackageName, playgroundDir, entry, npmScope, resolve: extraResolve }) {
  const projectDir = path.resolve(playgroundDir, '..');
  const npmPkgPath = resolveNpmPackage(projectDir, npmPackageName);

  // Fail fast if the declared name of the resolved package doesn't match the
  // alias key we're about to register. The three fields that must agree are:
  //   valdi_exported_library(npm_scope=..., web_package_name=...)   [BUILD.bazel]
  //   valdi_web_playground(npm_package=":<web_package_name>")        [BUILD.bazel]
  //   createWebpackConfig({npmScope, npmPackageName})                [webpack.config.js]
  // A mismatch would otherwise surface as an opaque webpack
  // "Module not found: <scope>/<pkg>" at bundle time.
  const pkgJsonPath = path.join(npmPkgPath, 'package.json');
  const declaredName = JSON.parse(fs.readFileSync(pkgJsonPath, 'utf8')).name;
  const expectedName = npmScope ? `${npmScope}/${npmPackageName}` : npmPackageName;
  if (declaredName !== expectedName) {
    throw new Error(
      `createWebpackConfig: package.json at ${pkgJsonPath} declares name ` +
      `"${declaredName}" but the alias key derived from ` +
      `npmScope="${npmScope || ''}" + npmPackageName="${npmPackageName}" is ` +
      `"${expectedName}". Compiled requires will not resolve. Check that ` +
      `valdi_exported_library(npm_scope=..., web_package_name=...) in your ` +
      `BUILD.bazel matches createWebpackConfig({npmScope, npmPackageName}) here.`,
    );
  }

  // Resolve valdi-compiler-js from the playground's node_modules — same
  // strategy as createHmrConfig. If present, wire its JSX transformer into
  // ts-loader so playground .tsx files compile (Valdi's <view>/<label>
  // intrinsics are statement-form and need rewriting to renderer calls).
  // Falls back to fast transpileOnly: true when valdi-compiler-js isn't
  // available (e.g. playgrounds that only use .ts/.js).
  //
  // ts-loader must resolve from the playground's tree too — the JSX
  // transformer needs the SAME typescript instance that ts-loader uses
  // (typescript is a peer dep of both ts-loader and valdi-compiler-js).
  // Resolving them via the same `paths: [playgroundDir]` chain guarantees
  // it; otherwise ts.isJsxElement checks in the transformer would silently
  // no-op against nodes built by a different typescript module instance.
  const resolveFromPlayground = (id) =>
    require.resolve(id, { paths: [playgroundDir] });

  // Only wire the JSX transformer when the playground actually has .tsx
  // files. Otherwise stay on the fast transpileOnly: true path — switching
  // ts-loader to transpileOnly: false makes it type-check the whole project,
  // surfacing pre-existing TS errors in .ts files that callers never asked
  // to type-check. JSX-bearing playgrounds opt in by having a .tsx anywhere
  // under playgroundDir.
  function playgroundHasTsx(dir) {
    try {
      const entries = fs.readdirSync(dir, { withFileTypes: true });
      for (const e of entries) {
        const full = path.join(dir, e.name);
        if (e.isDirectory()) {
          if (e.name === 'node_modules' || e.name === 'dist') continue;
          if (playgroundHasTsx(full)) return true;
        } else if (e.isFile() && e.name.endsWith('.tsx')) {
          return true;
        }
      }
    } catch (_e) {}
    return false;
  }
  const hasTsx = playgroundHasTsx(playgroundDir);

  let valdiJSXTransformers = null;
  let tsLoaderPath = require.resolve('ts-loader');
  if (hasTsx) {
    try {
      const transformerModulePath = resolveFromPlayground(
        'valdi-compiler-js/src/valdiJSXWebpackTransformer',
      );
      valdiJSXTransformers = require(transformerModulePath).createValdiJSXTransformers;
      tsLoaderPath = resolveFromPlayground('ts-loader');
    } catch (_err) {
      // valdi-compiler-js not reachable from this playground. Fall back to
      // transpileOnly: true; .tsx with Valdi JSX will fail to compile, but
      // that's a config gap not a silent no-op.
    }
  }
  // ts-loader + valdi-compiler-js MUST share a single typescript module
  // instance. They each resolve typescript via their own peer-dep walk-up;
  // if those walk-ups land on different physical files (e.g. 5.3.3 in @valdi's
  // npm tree, 5.9.3 in composer_npm), ts.isJsxElement returns false for nodes
  // emitted by the other instance — transformer silently no-ops, JSX survives,
  // webpack rejects "Module parse failed" with no hint at the cause.
  // Resolve typescript from the transformer's tree and force-pass it to
  // ts-loader via the `compiler` option so they bind the same module.
  let pinnedTypescriptPath = null;
  if (valdiJSXTransformers) {
    const transformerDir = path.dirname(require.resolve(
      'valdi-compiler-js/src/valdiJSXWebpackTransformer',
      { paths: [playgroundDir] },
    ));
    pinnedTypescriptPath = require.resolve('typescript', { paths: [transformerDir] });
  }

  /** @type {import('webpack').WebpackPluginInstance[]} */
  const plugins = [];

  if (process.env.ANALYZE === 'true' || process.env.STATS === 'true') {
    const { BundleAnalyzerPlugin } = require('webpack-bundle-analyzer');
    plugins.push(new BundleAnalyzerPlugin({
      analyzerMode: process.env.STATS === 'true' ? 'json' : 'static',
      reportFilename: path.resolve(projectDir, 'dist', process.env.STATS === 'true' ? 'stats.json' : 'report.html'),
      openAnalyzer: process.env.ANALYZE === 'true',
    }));
  }

  // Bazel sandbox alias: maps `<scope>/<pkg>` requires to the package's src/
  // directory. Compiled code emits scoped requires in standard npm format; in
  // the Bazel sandbox the package isn't in node_modules so this alias bridges
  // the gap. Published consumers don't need this — package.json exports
  // handle resolution. When no npmScope is provided we still expose the
  // unscoped package name so compiled code that doesn't use a scope works too.
  const scopedPackageAliasKey = npmScope
    ? `${npmScope}/${npmPackageName}`
    : npmPackageName;
  const aliases = {
    [scopedPackageAliasKey]: npmPkgPath,
    fs: false,
    path: false,
    mkdirp: false,
    ...(extraResolve?.alias || {}),
  };
  const tslibPath = path.join(npmPkgPath, 'src', 'valdi_core', 'src', 'tslib.js');
  if (fs.existsSync(tslibPath)) {
    aliases['tslib'] = tslibPath;
  }

  return {
    mode: 'production',
    devtool: 'source-map',
    entry,
    output: {
      filename: 'bundle.js',
      chunkFilename: '[name].chunk.js',
      path: path.resolve(projectDir, 'dist'),
      assetModuleFilename: '[hash][ext]',
      publicPath: '/',
    },
    resolve: {
      extensions: ['.ts', '.tsx', '.js'],
      // resolve.modules: Bazel sandbox needs this because import-generated
      // requires use bare paths (e.g. require("coreutils/src/Base64")).
      // Published consumers don't need this — node_modules resolution works.
      modules: [path.join(npmPkgPath, 'src'), 'node_modules'],
      ...(extraResolve || {}),
      alias: aliases,
      // Node built-ins + optional minifiers webpack's own dep tree probes.
      // Setting to `false` makes webpack silently swap in an empty module
      // instead of warning about unresolvable deps that never reach the
      // browser bundle.
      fallback: {
        '@swc/core': false,
        '@swc/core/package.json': false,
        crypto: false,
        esbuild: false,
        'esbuild/package.json': false,
        inspector: false,
        'uglify-js': false,
        'uglify-js/package.json': false,
        url: false,
        worker_threads: false,
        ...(extraResolve?.fallback || {}),
      },
    },
    module: {
      rules: [
        {
          test: /\.tsx?$/,
          use: {
            loader: tsLoaderPath,
            options: valdiJSXTransformers
              ? {
                  transpileOnly: false,
                  compiler: pinnedTypescriptPath,
                  getCustomTransformers: (program) => valdiJSXTransformers(program),
                  compilerOptions: { jsx: 'preserve', noEmit: false },
                }
              : { transpileOnly: true },
          },
          exclude: /node_modules/,
        },
        {
          test: /\.(png|woff|woff2|eot|ttf|svg|jpe?g)$/,
          loader: require.resolve('url-loader'),
          options: { limit: false, fallback: require.resolve('file-loader') },
        },
        { test: /\.protodecl$/, type: 'asset/resource' },
        { test: /\.js$/, type: 'javascript/auto' },
      ],
    },
    optimization: { usedExports: true },
    plugins,
    ignoreWarnings: [
      (warning) => warning.message?.includes('Critical dependency: require function is used'),
    ],
  };
}

/**
 * Creates a webpack-dev-server config extending a base config.
 *
 * @param {import('webpack').Configuration} baseConfig - webpack config from createWebpackConfig
 * @param {DevServerOptions} [options]
 * @returns {import('webpack').Configuration & { devServer: Object }}
 */
function createDevServerConfig(baseConfig, { port = 3030 } = {}) {
  const playgroundDir = typeof baseConfig.entry === 'string'
    ? path.resolve(path.dirname(baseConfig.entry))
    : __dirname;

  return {
    ...baseConfig,
    devServer: {
      static: { directory: playgroundDir },
      hot: true,
      liveReload: true,
      port,
      historyApiFallback: true,
    },
  };
}

/**
 * @typedef {Object} HmrConfigOptions
 * @property {string} npmPackageName - npm package name for the playground.
 * @property {string} playgroundDir - absolute path to playground/ (use __dirname).
 * @property {string} [entry] - entry path (default './playground/main.ts').
 * @property {number} [port] - dev-server port (default 3030).
 * @property {Record<string, any>} [resolve] - extra resolve overrides merged into base.
 * @property {Function[]} [externals] - extra externals functions prepended before defaults.
 */

/**
 * Creates a complete webpack-dev-server config for a `:dev` target.
 *
 * Wires:
 *   - createWebpackConfig with the playground's npm package name.
 *   - resolve.modules → linked npm package's src + node_modules (so bare
 *     imports inside the package keep working in workspace cwd).
 *   - .tsx rule chain: valdi-web-devtools/hmr-loader before ts-loader so the
 *     hmr bootstrap is auto-appended and the JSX transformer fires.
 *   - createDevServerConfig with the chosen port.
 *
 * Per-playground extras (aliases for stubs, externals for res/ paths, etc.)
 * pass through via `resolve` and `externals`.
 *
 * @param {HmrConfigOptions} options
 * @returns {import('webpack').Configuration & { devServer: Object }}
 */
function createHmrConfig(options) {
  const {
    npmPackageName,
    playgroundDir,
    entry = './playground/main.ts',
    port = 3030,
    npmScope,
    resolve: extraResolve,
    externals: extraExternals,
  } = options;

  if (!npmPackageName) throw new Error('createHmrConfig: npmPackageName is required');
  if (!playgroundDir) throw new Error('createHmrConfig: playgroundDir is required');

  // Resolve playground-provided npm deps from the playground's own
  // node_modules rather than valdi-web-devtools's. Required because in Bazel
  // runfiles those packages live next to the playground BUILD target, not
  // next to valdi-web-devtools.
  const resolveFromPlayground = (id) =>
    require.resolve(id, { paths: [playgroundDir] });

  const transformerModulePath = resolveFromPlayground('valdi-compiler-js/src/valdiJSXWebpackTransformer');
  const { createValdiJSXTransformers } = require(transformerModulePath);
  // Pin ts-loader to the same typescript instance the transformer uses
  // (see createWebpackConfig for the silent-killer rationale).
  const pinnedTypescriptPath = require.resolve('typescript', {
    paths: [path.dirname(transformerModulePath)],
  });

  const base = createWebpackConfig({
    npmPackageName,
    playgroundDir,
    entry,
    npmScope,
    resolve: extraResolve,
  });

  // Resolve the linked npm package via node_modules (workspace cwd at
  // runtime). Internal bare imports inside the package (e.g.
  // `coreutils/src/Base64`) resolve through resolve.modules.
  const npmPkgPath = path.dirname(resolveFromPlayground(`${npmPackageName}/package.json`));
  base.resolve.modules = [path.join(npmPkgPath, 'src'), 'node_modules'];

  base.mode = 'development';
  base.devtool = 'source-map';

  // Loaders apply right-to-left: ts-loader transforms TS → JS, then
  // hmr-loader appends the registerModule/accept bootstrap.
  base.module.rules = base.module.rules.map(rule => {
    if (rule.test && rule.test.toString().includes('tsx')) {
      return {
        test: /\.tsx?$/,
        use: [
          { loader: require.resolve('./hmr-loader') },
          {
            loader: resolveFromPlayground('ts-loader'),
            options: {
              transpileOnly: false,
              compiler: pinnedTypescriptPath,
              getCustomTransformers: (program) => createValdiJSXTransformers(program),
              compilerOptions: { jsx: 'preserve', noEmit: false },
            },
          },
        ],
        exclude: /node_modules/,
      };
    }
    return rule;
  });

  if (extraExternals && extraExternals.length) {
    const origExternals = base.externals;
    base.externals = [
      ...extraExternals,
      ...(Array.isArray(origExternals) ? origExternals : origExternals ? [origExternals] : []),
    ];
  }

  const config = createDevServerConfig(base, { port });
  if (config.devServer) {
    // Webpack's own watcher handles .ts/.tsx; static-dir watch would just
    // trigger a full reload in parallel. Disable that.
    config.devServer.static.watch = { ignored: ['**/*.tsx', '**/*.ts'] };
  }
  return config;
}

module.exports = { createWebpackConfig, createDevServerConfig, createHmrConfig };
