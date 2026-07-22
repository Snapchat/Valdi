/**
 * Minimal setup. Runtime stubs + HMR attach.
 */

require('valdi-web-devtools/setup');

require('hello_experiment_npm/src/_navigation_registry');
require('hello_experiment_npm/src/_worker_registry');
require('hello_experiment_npm/src/_image_registry');

const { ValdiWebRenderer } = require('hello_experiment_npm/src/web_renderer/src/ValdiWebRenderer');
const { ComponentPrototype } = require('hello_experiment_npm/src/valdi_core/src/ComponentPrototype');

declare const module: { hot?: unknown };
if (module.hot) {
  const { attachHmr } = require('valdi-web-devtools');
  attachHmr(ValdiWebRenderer, {
    newPrototype: () => ComponentPrototype.instanceWithNewId(),
  });
}

export { ValdiWebRenderer };
