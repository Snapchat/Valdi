import { WebViewClassFactory, addRegistrationCallback } from './WebViewClassRegistry';

/**
 * Shared helper for web polyglot modules.
 * Registers a web custom-view class name with the renderer registry when available.
 */
export function registerWebPolyglotViewClass(className: string, factory: WebViewClassFactory): void {
  addRegistrationCallback((registry) => {
    registry.set(className, factory);
  });
}
