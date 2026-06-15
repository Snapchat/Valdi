(function () {
  function loadRootModule(jsEvaluator, path) {
    var exports = {};
    var module = {};
    module.exports = exports;

    const requireFunc = path => {
      if (path === 'tslib') {
        return loadRootModule(jsEvaluator, `valdi_core/src/tslib`);
      } else {
        throw new Error(`Invalid root module ${path}`);
      }
    };

    jsEvaluator(path, requireFunc, module, exports);

    return module.exports;
  }

  const jsEvaluator = runtime.loadJsModule;

  const moduleLoader = loadRootModule(jsEvaluator, 'valdi_core/src/ModuleLoader');
  const commonjsModuleLoaderType = runtime.moduleLoaderType === 'commonjs';
  const instance = moduleLoader.create(jsEvaluator, runtime.getSourceMap, runtime.loadModule, commonjsModuleLoaderType);

  const isBrowser = typeof window !== 'undefined' && typeof window.document !== 'undefined';
  if (!isBrowser) {
    // Native runtime: override global require so bare module paths resolve via moduleLoader
    global.require = instance.load.bind(instance);
  } else {
    // Deprecated: valdiRequire remains for backwards compatibility with existing
    // consumers. Use standard require() with package-relative paths instead.
    global.valdiRequire = instance.load.bind(instance);
  }
  global.moduleLoader = instance;
  global.Long = instance.load('valdi_core/src/Long', true);

  instance.load('valdi_core/src/PostInit', true).postInit();
})();
