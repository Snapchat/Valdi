const path = require('path');
const valdiApiVersion = require('./src/valdi_api_version.json');

if (!Number.isSafeInteger(valdiApiVersion) || valdiApiVersion < 0 || valdiApiVersion > 0x7fffffff) {
  throw new Error(
    `Invalid Valdi native API version "${String(valdiApiVersion)}"; expected a non-negative signed 32-bit integer.`,
  );
}

function resolveBazelBinDir() {
  const configured = process.env.BAZEL_BINDIR;
  if (!configured) {
    return path.resolve(__dirname, '..');
  }
  if (path.isAbsolute(configured)) {
    return configured;
  }

  const normalized = path.normalize(configured);
  const marker = `${path.sep}${normalized}${path.sep}`;
  const markerIndex = __dirname.indexOf(marker);
  if (markerIndex >= 0) {
    return path.join(__dirname.substring(0, markerIndex), normalized);
  }
  return path.resolve(configured);
}

const bazelBinDir = resolveBazelBinDir();
const packagePath = path.resolve(__dirname, '@VALDI_PACKAGE_PATH@');

module.exports = {
  mode: 'development',
  devtool: 'source-map',
  entry: path.resolve(__dirname, 'src/index.js'),
  output: {
    path: path.resolve(__dirname, 'dist'),
    filename: 'bundle.js',
    clean: true,
  },
  plugins: [
    {
      apply(compiler) {
        new compiler.webpack.DefinePlugin({
          __VALDI_API_VERSION__: JSON.stringify(valdiApiVersion),
        }).apply(compiler);
      },
    },
  ],
  resolve: {
    alias: {
      '@VALDI_PACKAGE_NAME@': packagePath,
      'path-browserify': path.resolve(__dirname, 'src/path-browserify-shim.js'),
    },
    extensions: ['.js', '.json'],
    modules: [
      path.resolve(__dirname, 'node_modules'),
      path.resolve(bazelBinDir, 'bzl/valdi/npm/node_modules'),
      path.resolve(bazelBinDir, 'external/valdi/bzl/valdi/npm/node_modules'),
      path.resolve(bazelBinDir, 'external/valdi+/bzl/valdi/npm/node_modules'),
      'node_modules',
    ],
  },
  module: {
    rules: [
      {
        // The generated entry uses ESM imports even when the workspace package is CommonJS.
        test: /\.js$/i,
        type: 'javascript/auto',
      },
      {
        test: /\.(bin|protodecl)$/i,
        type: 'asset/bytes',
      },
      {
        test: /\.(png|jpe?g|svg|webp)$/i,
        oneOf: [
          {
            resourceQuery: /(?:^|[?&])no-inline(?:[&=]|$)/,
            type: 'asset/resource',
          },
          {
            type: 'asset/inline',
          },
        ],
      },
    ],
  },
};
