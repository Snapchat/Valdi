import type { ValdiRuntime } from './ValdiRuntime';

declare const require: (id: string) => unknown;

/**
 * Returns the runtime installed on `globalThis.runtime` by the host.
 *
 * Call this when the runtime is needed rather than caching the result at
 * module evaluation: on web, valdi_core modules can be evaluated before the
 * host installs the runtime.
 */
export function getValdiRuntime(): ValdiRuntime {
  return (globalThis as typeof globalThis & { runtime: ValdiRuntime }).runtime;
}

function hasInstalledRuntime(): boolean {
  const installed = (globalThis as typeof globalThis & { runtime?: Partial<ValdiRuntime> }).runtime;
  return typeof installed?.loadModule === 'function';
}

// Browser hosts do not create the Valdi runtime before importing Valdi
// components. Importing the web runtime here makes normal Valdi module imports
// bootstrap web automatically. `location` alone is not a browser signal: native
// JavaScriptWorker installs a WorkerLocation too, so also require that no full
// runtime (native or an already-bootstrapped web one) is installed yet. The
// valdi-web-devtools pre-bootstrap stub only carries logging/asset shims, so it
// does not count as installed.
if (typeof globalThis.location !== 'undefined' && !hasInstalledRuntime()) {
  require('web_renderer/src/ValdiWebRuntime');
}
