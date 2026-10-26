import type { ValdiRuntime } from './ValdiRuntime';

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
