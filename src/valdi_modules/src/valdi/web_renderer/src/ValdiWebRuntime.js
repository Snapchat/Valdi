
const path = require('path-browserify');

// Valdi runtime assumes global instead of globalThis
globalThis.global = globalThis;

// To make tests happy
globalThis.describe = function(name, func) {};

// Load up all of the modules
const context = require.context('../../', true, /\.js$/);

function loadPath(path) {
  const module = context(path);
  return module;
}

class Runtime {
  // This is essentially the require() function that the runtime is using.
  // relativePath is not the contents of require, it is preprocessed by the runtime.
  loadJsModule(relativePath, requireFunc, module, exports) {
    relativePath = path.normalize(relativePath);

    // There are a few different ways that imports can be resolved
    // 1. Relative path
    var resolvedImportPath = './' + relativePath + '.js';
    
    if (context.keys().includes(resolvedImportPath)) {
      module.exports = loadPath(resolvedImportPath);
      return;
    } 

    // 2. Legacy vue paths
    var vueImportPath = './' + relativePath + 'vue.js';
    
    if (context.keys().includes(vueImportPath)) {
      module.exports = loadPath(vueImportPath);
      return;
    } 

    // 3. Some modules try to import from nested res folders but they are flattened into the top
    // level res
    const segments = relativePath.split('/');
    if (segments[1] === "res") {
      var resPath = './' + path.join(segments[0], 'res.js');
      if (context.keys().includes(resPath)) {
        module.exports = loadPath(resPath);
        return;
      }
    }

    // 3. A catch all looking for the exact path in the available options
    if (!context.keys().includes(relativePath)) {
      var err = new Error(`Module not found: ${relativePath}`);
      err.code = 'MODULE_NOT_FOUND';
      throw err;
    }
    module.exports = loadPath(relativePath);
  }

  componentPaths = new Map();

  // For navigation loading.
  requireByComponent(componentName) {
    if (this.componentPaths.has(componentName)) {
      // console.log("found component in cache");
      return this.componentPaths.get(componentName);
    }

    for (const key of context.keys()) {
      var module = context(key);
      for (const exported of Object.keys(module)) {
        var component = module[exported];
        // Component path is set by the NavigationPage annotation.
        if (component != null && component.hasOwnProperty('componentPath')) {
          if (!this.componentPaths.has(component.componentPath)) {
            // console.log('adding ', component.componentPath);
            this.componentPaths.set(component.componentPath, component);
          }

          if (component.componentPath === componentName) {
            return component;
          }
        }
      }
    }

    console.error("could not find", componentName);
  }

  setColorPalette(palette) {
    global.currentPalette = palette;
  }

  getColorPalette() {
    return global.currentPalette;
  }

  getCurrentPlatform() {
    // 1 = Android
    // 2 = iOS
    // 3 = web 
    return 3;
  }

  submitRawRenderRequest(renderRequest) {
    // console.log("submitRawRenderRequest", renderRequest);
  }

  createContext(manager) {
    // console.log("createContext", manager);
    return "contextId";
  }

  setLayoutSpecs(contextId, width, height, rtl) {
    // console.log("setLayoutSpecs", contextId, width, height, rtl);
  }

  postMessage(contextId, command, params) {
    // console.log("postMessage", contextId, command, params);
  }

  getAssets(catalogPath) {
    // Get all images in the monolith
    const imageContext = require.context("../../", true, /\.(png|jpe?g|svg)$/);

    // Get just the images in the requested module
    const filteredImages = imageContext.keys().filter((key) =>
      key.startsWith(`./${catalogPath}/`)
    );

    // Get all image modules
    const images = filteredImages.map((key) => ({
      path: path.basename(key).split('.').slice(0, -1).join('.'),
      //width: 
      src: imageContext(key).default, // Webpack will replace with URL
    }));

    return images;
  }

  makeAssetFromUrl(url) {
    return {
      path: url,
      width: 100,
      height: 100,
    };
  }

  pushCurrentContext(contextId) {
    // console.log("pushCurrentContext", contextId);
  }

  popCurrentContext() {}

  getFrameForElementId(contextId, elementId, callback) {
    callback(undefined);
  }

  getNativeViewForElementId(contextId, elementId, callback) {
    callback(undefined);
  }

  getNativeNodeForElementId(contextId, elementId) {
    return undefined;
  }

  makeOpaque(object) {
    return object;
  }

  configureCallback(options, func) {}

  getViewNodeDebugInfo(contextId, elementId, callback) {
    callback(undefined);
  }

  takeElementSnapshot(contextId, elementId, callback) {
    callback(undefined);
  }

  getLayoutDebugInfo(contextId, elementId, callback) {
    callback(undefined);
  }

  performSyncWithMainThread(func) {
    func();
  }

  createWorker(url) {
    return {
      postMessage(data) {},
      setOnMessage(f) {},
      terminate() {},
    };
  }

  destroyContext(contextId) {}

  measureContext(contextId, maxWidth, widthMode, maxHeight, heightMode, rtl) {
    return [0, 0];
  }

  getCSSModule(path) {
    return {
      getRule(name) {
        return undefined;
      },
    };
  }

  createCSSRule(attributes) {
    return 0;
  }

  internString(str) {
    return 0;
  }

  getAttributeId(attributeName) {
    return 0;
  }

  protectNativeRefs(contextId) {
    return () => {};
  }

  getBackendRenderingTypeForContextId(contextId) {
    return 1;
  }

  isModuleLoaded(module) {
    return true;
  }

  loadModule(module, completion) {
    if (completion) completion();
  }

  jsonContext = require.context('../../', true, /\.json$/);
  getModuleEntry(module, path, asString) {
    var filePath = './'+module+'/'+path;
    return JSON.stringify(this.jsonContext(filePath));
  }

  getModuleJsPaths(module) {
    return [""];
  }

  trace(tag, callback) {
    return callback();
  }

  makeTraceProxy(tag, callback) {
    return () => callback();
  }

  startTraceRecording() {
    return 0;
  }

  stopTraceRecording(id) {
    return [];
  }

  callOnMainThread(method, parameters) {
    method(parameters);
  }

  onMainThreadIdle(cb) {
    requestIdleCallback(() => {
      cb();
    });
  }

  makeAssetFromBytes(bytes) {
    return {
      path: "",
      width: 100,
      height: 100,
    };
  }

  makeDirectionalAsset(ltrAsset, rtlAsset) {
    return {
      path: "",
      width: 100,
      height: 100,
    };
  }

  makePlatformSpecificAsset(defaultAsset, platformAssetOverrides) {
    return {
      path: "",
      width: 100,
      height: 100,
    };
  }

  addAssetLoadObserver(asset, onLoad, outputType, preferredWidth, preferredHeight) {
    return () => {};
  }

  outputLog(type, content) {
    //This should never be called, web is using the browser's console.log
  }

  scheduleWorkItem(cb, delayMs, interruptible) {
    return 0;
  }

  unscheduleWorkItem(taskId) {}

  getCurrentContext() {
    return "";
  }

  saveCurrentContext() {
    return 0;
  }

  restoreCurrentContext(contextId) {}

  onUncaughtError(message, error) {
    console.log("uncaught error", message, error);
  }

  setUncaughtExceptionHandler(cb) {}

  setUnhandledRejectionHandler(cb) {}

  dumpMemoryStatistics() {
    return {
      memoryUsageBytes: 0,
      objectsCount: 0,
    };
  }

  performGC() {
    // Not a thing on the web
  }

  dumpHeap() {
    // Not a thing on the web
    return new ArrayBuffer(0);
  }

  bytesToString(bytes) {
    const view = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
    return new TextDecoder().decode(view);
  }

  submitDebugMessage(level, message) {
    // Unused, should go through console.log
  }

  isDebugEnabled = true

  buildType = "debug"
};

globalThis.runtime = new Runtime();

// Init is going to try to overwrite console.log, prevent that
Object.freeze(globalThis.__originalConsole__);

globalThis.__originalConsole__ = {
  log: console.log.bind(console),
  warn: console.warn.bind(console),
  error: console.error.bind(console),
  info: console.info.bind(console),
  debug: console.debug.bind(console),
  dir: console.dir.bind(console),
  trace: console.trace.bind(console),
  assert: console.assert.bind(console),
};

// Run the init function
// Relies on runtime being set so it must happen after
// Assumes relative to the monolithic npm
require("../../valdi_core/src/Init.js");

// Restore console
globalThis.console = globalThis.__originalConsole__;
