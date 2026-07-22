const { createHmrConfig } = require('valdi-web-devtools/webpack');

module.exports = createHmrConfig({
  npmPackageName: 'hello_experiment_npm',
  playgroundDir: __dirname,
});
