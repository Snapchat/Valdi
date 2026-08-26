// Browser hosts do not create the Valdi runtime before importing Valdi
// components. Importing the web runtime here makes normal Valdi module imports
// bootstrap web automatically. Native/mobile runtimes already install
// globalThis.runtime, and they never evaluate this browser-gated require.
import type { ValdiRuntime } from './ValdiRuntime';

declare const require: (id: string) => unknown;

export function getValdiRuntime(): ValdiRuntime {
  return (globalThis as typeof globalThis & { runtime: ValdiRuntime }).runtime;
}

if (typeof globalThis.location !== 'undefined') {
  require('web_renderer/src/ValdiWebRuntime');
}
