// Browser-safe entry point for valdi-web-devtools. Kept intentionally lean
// so that `import { mountRoot } from 'valdi-web-devtools'` in playground
// application code does NOT drag Node-only webpack tooling
// (webpack-bundle-analyzer, webpack-cli, etc.) into the browser bundle
// graph.
//
// Consumers who need the webpack config helpers should import them from the
// `valdi-web-devtools/webpack` subpath (see package.json `exports`).

'use strict';

const { mountRoot, attachHmr } = require('./hmr');

module.exports = { mountRoot, attachHmr };
