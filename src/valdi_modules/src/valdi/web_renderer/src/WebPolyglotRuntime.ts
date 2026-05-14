import type { GlobalWithModulePath, WebPolyglotModuleLoader, WebPolyglotRegistryModule } from './WebPolyglotTypes';
declare const moduleLoader: WebPolyglotModuleLoader;

/**
 * Registers a web polyglot custom-view class with the web renderer registry.
 * Throws when runtime bridging is unavailable or malformed.
 */
export function registerWebPolyglotViewClassOrThrow(
  fallbackModulePath: string,
  className: string,
  factory: (container: HTMLElement) => void,
): void {
  const runtimeGlobal = globalThis as GlobalWithModulePath;
  const modulePath = runtimeGlobal.module?.path ?? fallbackModulePath;
  const customRequire = moduleLoader.resolveRequire(modulePath);
  const registryModule = customRequire('web_renderer/src/WebPolyglotRegistry') as WebPolyglotRegistryModule;
  if (!registryModule.registerWebPolyglotViewClass) {
    throw new Error('web_renderer/src/WebPolyglotRegistry.registerWebPolyglotViewClass is unavailable');
  }
  registryModule.registerWebPolyglotViewClass(className, factory);
}

/**
 * Backward-compatible wrapper that swallows registration failures.
 */
export function tryRegisterWebPolyglotViewClass(
  fallbackModulePath: string,
  className: string,
  factory: (container: HTMLElement) => void,
): boolean {
  try {
    registerWebPolyglotViewClassOrThrow(fallbackModulePath, className, factory);
    return true;
  } catch (_e) {
    return false;
  }
}
