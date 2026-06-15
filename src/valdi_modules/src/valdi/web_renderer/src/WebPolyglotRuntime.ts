import type { GlobalWithModulePath, WebPolyglotRegistryModule } from './WebPolyglotTypes';

declare const require: (id: string) => any;

/**
 * Registers a web polyglot custom-view class with the web renderer registry.
 * Throws when runtime bridging is unavailable or malformed.
 */
export function registerWebPolyglotViewClassOrThrow(
  fallbackModulePath: string,
  className: string,
  factory: (container: HTMLElement) => void,
): void {
  const registryModule = require('web_renderer/src/WebPolyglotRegistry') as WebPolyglotRegistryModule;
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
