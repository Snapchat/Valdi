import { StringSet } from 'coreutils/src/StringSet';
import { ISourceMap } from 'source_map/src/ISourceMap';
import { IModuleLoader, OnHotReloadCallback, OnModuleRegisteredCallback, RequireFunc } from './IModuleLoader';
import { ValdiRuntime } from './ValdiRuntime';
export { RequireFunc } from './IModuleLoader';

declare const runtime: ValdiRuntime;

type JsEvalRetryableError = string;
export type JSEvalResult = JsEvalRetryableError | undefined;
export type JsEvaluator = (path: string, require: any, module: any, exports: any) => JSEvalResult;
export type SourceMapResolver = (path: string) => string | undefined;
export type ValdiModulePreloader = (moduleName: string, completion?: (error?: string) => void) => void;
export type SourceMapFactory = (
  sourceMapContent: string | undefined,
  sourceMapLineOffset: number | undefined,
) => ISourceMap | undefined;

interface ResolvedPath {
  absolutePath: string;
  directoryPaths: string[];
}

function normalizePath(pathEntries: string[]): string[] {
  const out: string[] = [];
  for (const pathEntry of pathEntries) {
    if (pathEntry === '..') {
      if (out.length) {
        // Remove last item
        out.pop();
      } else {
        // Keep the '..' we went outside our root
        out.push(pathEntry);
      }
    } else if (pathEntry === '.' && runtime.getCurrentPlatform() != 3)  {
      // Nothing to do
      continue;
    } else {
      out.push(pathEntry);
    }
  }
  return out;
}

function resolveAbsoluteImport(normalizedPathEntries: string[]): ResolvedPath {
  if (normalizedPathEntries.length === 1) {
    // Modules at the root are considered to be from valdi_core
    normalizedPathEntries.unshift('valdi_core', 'src');
  }

  if (normalizedPathEntries.length > 0 && normalizedPathEntries[0].startsWith('@')) {
    // Postfix the module name with its scope to resolve name space conflicts
    // for scoped imports
    const scope = normalizedPathEntries.shift();
    const moduleName = normalizedPathEntries[0];
    normalizedPathEntries[0] = moduleName + scope;
  }

  const absolutePath = normalizedPathEntries.join('/');
  normalizedPathEntries.pop();

  return {
    directoryPaths: normalizedPathEntries,
    absolutePath: absolutePath,
  };
}

function resolveAbsoluteImportFromPath(path: string): ResolvedPath {
  return resolveAbsoluteImport(normalizePath(path.split('/')));
}

function resolvePath(path: string, fromResolvedPath: ResolvedPath): ResolvedPath {
  const importPathEntries = path.split('/');
  if (importPathEntries[0] === '.' || importPathEntries[0] === '..') {
    // Relative import

    const combinedPath = fromResolvedPath.directoryPaths.slice();
    combinedPath.push(...importPathEntries);
    const normalized = normalizePath(combinedPath);
    return resolveAbsoluteImport(normalized);
  } else {
    // Absolute import
    const normalized = normalizePath(importPathEntries);
    return resolveAbsoluteImport(normalized);
  }
}

const enum ModuleStatus {
  UNLOADED = 0,
  LOADING = 1,
  LOADED = 2,
}

interface Module {
  resolvedPath: ResolvedPath;
  error: Error | undefined;
  exports: any | undefined;
  lazyExports: (() => any) | undefined;
  status: ModuleStatus;
  dependents: StringSet;
  dependencies: StringSet;
  sourceMap: ISourceMap | undefined | null;
  sourceMapLineOffset?: number;
  disposables?: (() => void)[];
}

function isModuleUsed(jsModule: Module) {
  for (const key in jsModule.dependents) {
    // A module is considered used if it has dependents (other modules depend on it).
    return true;
  }
  return false;
}

function rethrow(error: Error) {
  throw new (error as any).constructor(error.message);
}

class RetryableError extends Error {}

export class ModuleLoader implements IModuleLoader {
  private jsEvaluator: JsEvaluator;
  private sourceMapResolver: SourceMapResolver | undefined;
  private valdiModulePreloader: ValdiModulePreloader;
  private modules: { [path: string]: Module } = {};
  private moduleFactory: { [path: string]: () => any } = {};
  private moduleDenylistedForLazyLoading?: StringSet;
  private hotReloadCallbacks: { [path: string]: OnHotReloadCallback[] } = {};
  private moduleRegisteredCallbacks: { [path: string]: OnModuleRegisteredCallback[] } = {};
  private preloadedValdiModules: StringSet = {};
  private commonJsCompatible: boolean;

  constructor(
    jsEvaluator: JsEvaluator,
    sourceMapResolver: SourceMapResolver | undefined,
    valdiModulePreloader: ValdiModulePreloader | undefined,
    commonJsCompatible?: boolean,
  ) {
    this.jsEvaluator = jsEvaluator;
    this.sourceMapResolver = sourceMapResolver;
    // Backward compat with previous runtime version
    this.valdiModulePreloader = valdiModulePreloader ?? (() => {});
    this.commonJsCompatible = commonJsCompatible === true;
  }

  getDependencies(path: string): string[] {
    const resolvedPath = resolveAbsoluteImportFromPath(path);
    const dependencies: string[] = [];

    const module = this.modules[resolvedPath.absolutePath];
    if (module) {
      for (const key in module.dependencies) {
        dependencies.push(key);
      }
    }

    return dependencies;
  }

  getDependents(path: string): string[] {
    const resolvedPath = resolveAbsoluteImportFromPath(path);
    const dependents: string[] = [];

    const module = this.modules[resolvedPath.absolutePath];
    if (module) {
      for (const key in module.dependents) {
        dependents.push(key);
      }
    }

    return dependents;
  }

  isLoaded(path: string): boolean {
    const resolvedPath = resolveAbsoluteImportFromPath(path);

    const module = this.modules[resolvedPath.absolutePath];
    if (!module) {
      return false;
    }
    return module.status !== ModuleStatus.UNLOADED;
  }

  load(path: string, disableProxy?: boolean, disableSyncDependencies?: boolean): any {
    const resolvedPath = resolveAbsoluteImportFromPath(path);
    return this.doImport(resolvedPath, undefined, !!disableProxy, !!disableSyncDependencies);
  }

  preload(path: string, maxDepth: number): void {
    const resolvedPath = resolveAbsoluteImportFromPath(path);
    this.doPreload(resolvedPath.absolutePath, 0, maxDepth, {});
  }

  private doPreload(path: string, currentDepth: number, maxDepth: number, visitedModulePaths: StringSet) {
    if (visitedModulePaths[path]) {
      return;
    }
    visitedModulePaths[path] = true;

    let module = this.modules[path];
    if (!module) {
      this.doImport(resolveAbsoluteImportFromPath(path), undefined, false, true);
      module = this.modules[path]!;
    }

    if (module.lazyExports) {
      module.lazyExports();
    }

    if (currentDepth >= maxDepth) {
      return;
    }

    for (const dependency in module.dependencies) {
      this.doPreload(dependency, currentDepth + 1, maxDepth, visitedModulePaths);
    }
  }

  hasModuleFactory(path: string): boolean {
    const resolvedPath = resolveAbsoluteImportFromPath(path);
    return this.moduleFactory[resolvedPath.absolutePath] !== undefined;
  }

  registerModule(path: string, factory: () => any): void {
    const absolutePath = resolveAbsoluteImportFromPath(path).absolutePath;
    this.moduleFactory[absolutePath] = factory;

    const callbacks = this.moduleRegisteredCallbacks[absolutePath];
    if (callbacks) {
      delete this.moduleRegisteredCallbacks[absolutePath];

      for (const callback of callbacks) {
        callback();
      }
    }
  }

  unregisterModule(path: string) {
    const resolvedPath = resolveAbsoluteImportFromPath(path);
    const absolutePath = resolvedPath.absolutePath;
    if (this.moduleFactory[absolutePath]) {
      delete this.moduleFactory[absolutePath];
    }
  }

  onModuleRegistered(path: string, callback: () => void): void {
    const resolvedPath = resolveAbsoluteImportFromPath(path);
    if (this.moduleFactory[resolvedPath.absolutePath]) {
      callback();
    } else {
      let callbacks = this.moduleRegisteredCallbacks[resolvedPath.absolutePath];
      if (!callbacks) {
        callbacks = [];
        this.moduleRegisteredCallbacks[resolvedPath.absolutePath] = callbacks;
      }
      callbacks.push(callback);
    }
  }

  unloadAllUnused(modulesToKeep: string[]): string[] {
    const unloadedModules: StringSet = {};
    const indexedModulesToKeep: StringSet = {};
    for (const path of modulesToKeep) {
      const absolutePath = resolveAbsoluteImportFromPath(path).absolutePath;
      indexedModulesToKeep[absolutePath] = true;
    }

    // eslint-disable-next-line no-empty
    while (this.unloadNextModule(indexedModulesToKeep, unloadedModules)) {}

    const out: string[] = [];
    for (const jsModule in unloadedModules) {
      out.push(jsModule);
    }

    return out;
  }

  private registerModuleDisposable(path: string, disposable: () => void) {
    const module = this.modules[path];
    if (!module) {
      return;
    }
    let disposables = module.disposables;
    if (!disposables) {
      disposables = [];
      module.disposables = disposables;
    }
    disposables.push(disposable);
  }

  private unloadNextModule(modulesToKeep: StringSet, unloadedModules: StringSet): boolean {
    for (const modulePath in this.modules) {
      if (modulesToKeep[modulePath]) {
        continue;
      }
      const jsModule = this.modules[modulePath];
      if (jsModule && !isModuleUsed(jsModule)) {
        this.doUnload(modulePath, unloadedModules);
        return true;
      }
    }
    return false;
  }

  unload(paths: string[], isHotReloading: boolean, disableHotReloadDenyList: boolean): string[] {
    const unloadedModules: StringSet = {};

    for (const path of paths) {
      const resolvedPath = resolveAbsoluteImportFromPath(path);

      this.doUnload(resolvedPath.absolutePath, unloadedModules);

      if (isHotReloading && !this.commonJsCompatible) {
        if (!this.moduleDenylistedForLazyLoading) {
          this.moduleDenylistedForLazyLoading = {};
        }
        const shouldDeny = !disableHotReloadDenyList;
        this.moduleDenylistedForLazyLoading[resolvedPath.absolutePath] = shouldDeny;
      }
    }

    const out: string[] = [];
    for (const jsModule in unloadedModules) {
      out.push(jsModule);

      if (isHotReloading) {
        const callbacks = this.hotReloadCallbacks[jsModule];
        if (callbacks) {
          for (const callback of callbacks) {
            callback();
          }
        }
      }
    }

    return out;
  }

  onHotReload(observingModule: { path: string }, path: string, callback: OnHotReloadCallback): () => void {
    const resolvedPath = resolveAbsoluteImportFromPath(path);
    let callbacks = this.hotReloadCallbacks[resolvedPath.absolutePath];
    if (!callbacks) {
      callbacks = [];
      this.hotReloadCallbacks[resolvedPath.absolutePath] = callbacks;
    }

    callbacks.push(callback);

    const disposable = () => {
      const index = callbacks.indexOf(callback);
      if (index >= 0) {
        callbacks.splice(index, 1);
      }
    };

    if (observingModule.path !== path) {
      this.registerModuleDisposable(observingModule.path, disposable);
    } else {
      // Module is observing itself, in this case we dispose of the callback whenever the module is hot reloaded.
      callbacks.push(disposable);
    }

    return disposable;
  }

  resolveRequire(path: string): RequireFunc {
    const resolvedPath = resolveAbsoluteImportFromPath(path);

    let module = this.modules[resolvedPath.absolutePath];
    if (!module) {
      this.doImport(resolvedPath, undefined, false, true);
      module = this.modules[resolvedPath.absolutePath]!;
    }

    return this.makeRequire(module);
  }

  getOrCreateSourceMap = (path: string, sourceMapFactory: SourceMapFactory): ISourceMap | undefined => {
    const resolvedPath = resolveAbsoluteImportFromPath(path);
    let module = this.modules[resolvedPath.absolutePath];
    if (!module || !module.sourceMap) {
      // Attempt to load the module automatically if it's not in the map
      try {
        this.doImport(resolvedPath, undefined, true, true);
      } catch (err: any) {
        console.log(`Could not load module ${path}: ${err.message}`);
      }

      module = this.modules[resolvedPath.absolutePath];

      if (!module) {
        return undefined;
      }
    }

    let sourceMap = module.sourceMap;

    if (sourceMap === undefined) {
      let resolvedSourceMapContent: string | undefined;
      if (this.sourceMapResolver) {
        resolvedSourceMapContent = this.sourceMapResolver(resolvedPath.absolutePath);
      }

      const resolvedSourceMap = sourceMapFactory(resolvedSourceMapContent, module.sourceMapLineOffset);
      if (resolvedSourceMap) {
        module.sourceMap = resolvedSourceMap;
        sourceMap = resolvedSourceMap;
      } else {
        // Use null to mark a fetched source map that did not resolve to anything
        module.sourceMap = null;
      }
    }

    if (!sourceMap) {
      return undefined;
    }
    return sourceMap;
  };

  private doUnload(path: string, unloadedModules: StringSet) {
    const jsModule = this.modules[path];
    if (!jsModule) {
      return;
    }

    delete this.modules[path];

    unloadedModules[path] = true;

    for (const dependency in jsModule.dependencies) {
      const importedModule = this.modules[dependency];
      if (!importedModule) {
        continue;
      }

      // Remove this module from the imported module's dependents
      delete importedModule.dependents[path];
    }

    const disposables = jsModule.disposables;
    if (disposables) {
      for (const disposable of disposables) {
        disposable();
      }
    }

    for (const dependent in jsModule.dependents) {
      this.doUnload(dependent, unloadedModules);
    }
  }

  private syncDependencies(jsModule: Module, parent?: Module) {
    if (!parent) {
      return;
    }

    const parentPath = parent.resolvedPath.absolutePath;

    jsModule.dependents[parentPath] = true;
    parent.dependencies[jsModule.resolvedPath.absolutePath] = true;
  }

  private doImport(
    resolvedModulePath: ResolvedPath,
    parent: Module | undefined,
    disableProxy: boolean,
    disableSyncDependencies: boolean,
  ): any {
    const absolutePath = resolvedModulePath.absolutePath;

    let jsModule = this.modules[absolutePath];
    if (jsModule) {
      if (!disableSyncDependencies) {
        this.syncDependencies(jsModule, parent);
      }

      if (jsModule.lazyExports && jsModule.status !== ModuleStatus.LOADING) {
        if (disableProxy || this.commonJsCompatible) {
          return jsModule.lazyExports();
        }

        if (this.moduleDenylistedForLazyLoading && this.moduleDenylistedForLazyLoading[absolutePath]) {
          // This module was blacklisted for lazy loading, we immediately load it instead of waiting
          // until the proxy triggers the load.
          jsModule.lazyExports();
        }
      }

      return jsModule.exports;
    }

    jsModule = {
      resolvedPath: resolvedModulePath,
      error: undefined,
      exports: undefined,
      lazyExports: undefined,
      status: ModuleStatus.UNLOADED,
      dependents: {},
      dependencies: {},
      sourceMap: undefined,
    };
    this.modules[absolutePath] = jsModule;

    try {
      if (!disableSyncDependencies) {
        this.syncDependencies(jsModule, parent);
      }
      jsModule.exports = this.makeModuleExports(jsModule);

      if (jsModule.lazyExports) {
        if (disableProxy || this.commonJsCompatible) {
          return jsModule.lazyExports();
        }

        if (this.moduleDenylistedForLazyLoading && this.moduleDenylistedForLazyLoading[absolutePath]) {
          // This module was blacklisted for lazy loading, we immediately load it instead of waiting
          // until the proxy triggers the load.
          jsModule.lazyExports();
        }
      }

      const valdiModuleName = resolvedModulePath.directoryPaths[0];
      if (valdiModuleName && !this.preloadedValdiModules[valdiModuleName]) {
        this.preloadedValdiModules[valdiModuleName] = true;
        this.valdiModulePreloader(valdiModuleName, undefined);
      }
    } catch (err: any) {
      jsModule.error = err;
      throw err;
    }

    return jsModule.exports;
  }

  private makeModuleExports(jsModule: Module): any {
    const modulePath = jsModule.resolvedPath.absolutePath;
    const moduleFactory = this.moduleFactory[modulePath];
    if (moduleFactory) {
      return moduleFactory();
    }

    const requireFunc = this.makeRequire(jsModule);
    const jsExports = {};
    const jsModuleObj = {
      exports: jsExports,
      path: modulePath,
      sourceMapLineOffset: 0,
    };

    let exportsObj: any;
    let didEval = false;

    const lazyExports = () => {
      if (!didEval) {
        if (jsModule.error) {
          rethrow(jsModule.error);
        }

        jsModule.status = ModuleStatus.LOADING;
        try {
          const result = this.jsEvaluator(modulePath, requireFunc, jsModuleObj, jsExports);
          if (result) {
            throw new RetryableError(result);
          }
          jsModule.status = ModuleStatus.LOADED;
        } catch (err: any) {
          if (!(err instanceof RetryableError)) {
            jsModule.error = err;
          }
          jsModule.status = ModuleStatus.UNLOADED;
          throw err;
        } finally {
          if (jsModuleObj.sourceMapLineOffset) {
            jsModule.sourceMapLineOffset = jsModuleObj.sourceMapLineOffset;
          }
        }
        didEval = true;
        exportsObj = jsModuleObj.exports;
      }
      return exportsObj;
    };

    jsModule.lazyExports = lazyExports;

    if (this.commonJsCompatible) {
      return jsExports;
    } else {
      return new Proxy(jsExports, {
        get(target, key) {
          if (!didEval) {
            lazyExports();
          }

          return exportsObj[key];
        },
      });
    }
  }

  private makeRequire(jsModule: Module): RequireFunc {
    return (path: string, disableProxy?: boolean, disableSyncDependencies?: boolean) => {
      const resolve = resolvePath(path, jsModule.resolvedPath);
      return this.doImport(resolve, jsModule, !!disableProxy, !!disableSyncDependencies);
    };
  }
}

export function create(
  jsEvaluator: JsEvaluator,
  sourceMapResolver: SourceMapResolver | undefined,
  valdiModulePreloader: ValdiModulePreloader | undefined,
  commonJsCompatible: boolean,
): IModuleLoader {
  return new ModuleLoader(jsEvaluator, sourceMapResolver, valdiModulePreloader, commonJsCompatible);
}
