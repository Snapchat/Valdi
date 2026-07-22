// @ts-check
/**
 * Common Valdi web runtime initialization.
 *
 * Sets up the runtime stub needed before any compiled module evaluates.
 * ConsoleLogTransformer emits globalThis.runtime?.isLoggingEnabled guards.
 * getAssets stub enables res.js catalog loading before ValdiWebRuntime bootstraps.
 */
globalThis.runtime = globalThis.runtime || {
  isLoggingEnabled: true,
  getAssets(catalogPath) {
    const ctx = globalThis.__valdiImageContext;
    if (!ctx) return [];
    return ctx.keys()
      .filter(key => key.startsWith('./' + catalogPath + '/'))
      .map(key => {
        const mod = ctx(key);
        const filename = key.split('/').pop() || '';
        const path = filename.split('.').slice(0, -1).join('.');
        return { path, src: mod.default || mod };
      });
  },
};
if (typeof global !== 'undefined') global.runtime = globalThis.runtime;
