const { ValdiWebRenderer } = require('@VALDI_PACKAGE_NAME@/src/web_renderer/src/ValdiWebRenderer');

const { @VALDI_COMPONENT_NAME@ } = require('@VALDI_PACKAGE_NAME@/src/@VALDI_ROOT_MODULE_PATH@');

const root = document.getElementById('root');
if (!root) {
  throw new Error('Missing #root');
}

const renderer = new ValdiWebRenderer(root);
renderer.renderRootComponent(@VALDI_COMPONENT_NAME@, {}, {}, {});
