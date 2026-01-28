// Declare webpack require.context
declare const require: {
  (id: string): any;
  context(directory: string, useSubdirectories: boolean, regExp: RegExp): any;
};

// Declare global for Node-like environment
declare const global: any;

const path = require('path-browserify');

// Valdi runtime assumes global instead of globalThis
(globalThis as any).global = globalThis;

// To make tests happy
(globalThis as any).describe = function(name: string, func: Function) {};

// Load up all of the modules
const context = require.context('../../', true, /\.js$/);

function loadPath(pathStr: string) {
  const module = context(pathStr);
  return module;
}

class Runtime {
  componentPaths = new Map();
  jsonContext = require.context('../../', true, /\.json$/);
  isDebugEnabled = true;
  buildType = "debug";

  // This is essentially the require() function that the runtime is using.
  // relativePath is not the contents of require, it is preprocessed by the runtime.
  loadJsModule(relativePath: string, requireFunc: any, module: any, exports: any) {
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
      (err as any).code = 'MODULE_NOT_FOUND';
      throw err;
    }
    module.exports = loadPath(relativePath);
  }

  // For navigation loading.
  requireByComponent(componentName: string) {
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

  setColorPalette(palette: any) {
    (global as any).currentPalette = palette;
  }

  getColorPalette() {
    return (global as any).currentPalette;
  }

  getCurrentPlatform() {
    // 1 = Android
    // 2 = iOS
    // 3 = web 
    return 3;
  }

  submitRawRenderRequest(renderRequest: any) {
    // console.log("submitRawRenderRequest", renderRequest);
  }

  createContext(manager: any) {
    // console.log("createContext", manager);
    return "contextId";
  }

  setLayoutSpecs(contextId: string, width: number, height: number, rtl: boolean) {
    // console.log("setLayoutSpecs", contextId, width, height, rtl);
  }

  postMessage(contextId: string, command: string, params: any) {
    // console.log("postMessage", contextId, command, params);
  }

  getAssets(catalogPath: string) {
    // Get all images in the monolith
    const imageContext = require.context("../../", true, /\.(png|jpe?g|svg)$/);

    // Get just the images in the requested module
    const filteredImages = imageContext.keys().filter((key: string) =>
      key.startsWith(`./${catalogPath}/`)
    );

    // Get all image modules
    const images = filteredImages.map((key: string) => ({
      path: path.basename(key).split('.').slice(0, -1).join('.'),
      //width: 
      src: imageContext(key).default, // Webpack will replace with URL
    }));

    return images;
  }

  makeAssetFromUrl(url: string) {
    return {
      path: url,
      width: 100,
      height: 100,
    };
  }

  pushCurrentContext(contextId: string) {
    // console.log("pushCurrentContext", contextId);
  }

  popCurrentContext() {}

  getFrameForElementId(contextId: string, elementId: number, callback: Function) {
    callback(undefined);
  }

  getNativeViewForElementId(contextId: string, elementId: number, callback: Function) {
    callback(undefined);
  }

  getNativeNodeForElementId(contextId: string, elementId: number) {
    return undefined;
  }

  makeOpaque(object: any) {
    return object;
  }

  configureCallback(options: any, func: Function) {}

  getViewNodeDebugInfo(contextId: string, elementId: number, callback: Function) {
    callback(undefined);
  }

  takeElementSnapshot(contextId: string, elementId: number, callback: Function) {
    callback(undefined);
  }

  getLayoutDebugInfo(contextId: string, elementId: number, callback: Function) {
    callback(undefined);
  }

  performSyncWithMainThread(func: Function) {
    func();
  }

  createWorker(url: string) {
    return {
      postMessage(data: any) {},
      setOnMessage(f: Function) {},
      terminate() {},
    };
  }

  destroyContext(contextId: string) {}

  measureContext(contextId: string, maxWidth: number, widthMode: number, maxHeight: number, heightMode: number, rtl: boolean): [number, number] {
    return [0, 0];
  }

  getCSSModule(path: string) {
    return {
      getRule(name: string) {
        return undefined;
      },
    };
  }

  createCSSRule(attributes: any) {
    return 0;
  }

  internString(str: string) {
    return 0;
  }

  getAttributeId(attributeName: string) {
    return 0;
  }

  protectNativeRefs(contextId: string) {
    return () => {};
  }

  getBackendRenderingTypeForContextId(contextId: string) {
    return 1;
  }

  isModuleLoaded(module: string) {
    return true;
  }

  loadModule(module: string, completion?: Function) {
    if (completion) completion();
  }

  getModuleEntry(module: string, pathStr: string, asString: boolean) {
    var filePath = './'+module+'/'+pathStr;
    return JSON.stringify(this.jsonContext(filePath));
  }

  getModuleJsPaths(module: string) {
    return [""];
  }

  trace(tag: string, callback: Function) {
    return callback();
  }

  makeTraceProxy(tag: string, callback: Function) {
    // Return callback directly to avoid adding stack frames
    return callback;
  }

  startTraceRecording() {
    return 0;
  }

  stopTraceRecording(id: number) {
    return [];
  }

  callOnMainThread(method: Function, parameters: any) {
    method(parameters);
  }

  onMainThreadIdle(cb: Function) {
    requestIdleCallback(() => {
      cb();
    });
  }

  makeAssetFromBytes(bytes: ArrayBuffer) {
    return {
      path: "",
      width: 100,
      height: 100,
    };
  }

  makeDirectionalAsset(ltrAsset: any, rtlAsset: any) {
    return {
      path: "",
      width: 100,
      height: 100,
    };
  }

  makePlatformSpecificAsset(defaultAsset: any, platformAssetOverrides: any) {
    return {
      path: "",
      width: 100,
      height: 100,
    };
  }

  addAssetLoadObserver(asset: any, onLoad: Function, outputType: any, preferredWidth?: number, preferredHeight?: number) {
    return () => {};
  }

  outputLog(type: string, content: string) {
    //This should never be called, web is using the browser's console.log
  }

  scheduleWorkItem(cb: Function, delayMs: number, interruptible: boolean) {
    return 0;
  }

  unscheduleWorkItem(taskId: number) {}

  getCurrentContext() {
    return "";
  }

  saveCurrentContext() {
    return 0;
  }

  restoreCurrentContext(contextId: number) {}

  onUncaughtError(message: string, error: any) {
    console.log("uncaught error", message, error);
  }

  setUncaughtExceptionHandler(cb: Function) {}

  setUnhandledRejectionHandler(cb: Function) {}

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

  bytesToString(bytes: ArrayBuffer | Uint8Array) {
    const view = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
    return new TextDecoder().decode(view);
  }

  submitDebugMessage(level: string, message: string) {
    // Unused, should go through console.log
  }
};

const globalAny = globalThis as any;
globalAny.runtime = new Runtime();

// Init is going to try to overwrite console.log, prevent that
Object.freeze((globalThis as any).__originalConsole__);

globalAny.__originalConsole__ = {
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
const initModule = require("../../valdi_core/src/Init.js");

// Restore console
globalAny.console = globalAny.__originalConsole__;

export {};
