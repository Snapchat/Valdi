const { createWebpackConfig } = require('valdi-web-devtools/webpack');

module.exports = createWebpackConfig({
  npmPackageName: 'hello_experiment_npm',
  playgroundDir: __dirname,
  entry: './playground/main.ts',
});
