const path = require('path');
const webpack = require('webpack');

const DEFAULT_COMMIT_HASH="develop"

const pickFirst = (...options) => {
  for (option of options) {
    try {
      return option();
    } catch (error) {
      continue;
    }
  }
  console.warn('No git hash provided, falling back to', DEFAULT_COMMIT_HASH);
  return DEFAULT_COMMIT_HASH;
}

/**
 * Read in the current git hash. For bazel builds it is expected to read it from
 * the GIT_HASH file since the git state is not aviable to BUILD.bazel
 */
let commitHash = pickFirst(
  // for bzl build expects a COMMIT_HASH file with the commit hash in it
  () => require('child_process').execSync('git rev-parse HEAD').toString().trim(),
  () => require('fs').readFileSync('./GIT_HASH').toString().trim(),
);

module.exports = {
  mode: 'production',
  target: 'node',
  entry: './src/index.ts',
  devtool: 'source-map',
  plugins: [
    new webpack.DefinePlugin({
      __COMMIT_HASH__: JSON.stringify(commitHash),
    }),
  ],
  module: {
    rules: [
      {
        test: /\.([cm]?ts|tsx)$/,
        include: [
          path.resolve(__dirname, './src'),
          path.resolve(__dirname, './remotedebug-ios-webkit-adapter'),
          path.resolve(__dirname, './supported-locales')
        ],
        exclude: /node_modules/,
        loader: 'ts-loader',
        options: {
          configFile: path.resolve(__dirname, 'tsconfig.base.json'),
        },
      },
      {
        test: /\.node$/,
        loader: 'node-loader',
        options: {
          name: '[name].[ext]',
        },
      },
    ],
  },
  resolve: {
    // Add `.ts` and `.tsx` as a resolvable extension.
    extensions: ['.ts', '.tsx', '.js'],
    // Add support for TypeScripts fully qualified ESM imports.
    extensionAlias: {
      '.js': ['.js', '.ts'],
      '.cjs': ['.cjs', '.cts'],
      '.mjs': ['.mjs', '.mts'],
    },
  },
  output: {
    filename: 'bundle.js',
    path: path.resolve(__dirname, 'dist'),
  },
};
