import { StackFrame } from 'source_map/src/StackSymbolicator';

// Frames from these bundles describe how work was delivered (rx pipelines, native references,
// timers, module loading, shared utilities), not whose code was running.
const FRAMEWORK_BUNDLES = new Set([
  'bridge_observables',
  'coreutils',
  'foundation',
  'source_map',
  'valdi_core',
  'valdi_rxjs',
  'valdi_tsx',
]);

// Frame file names are `<bundle>/<path>` by construction (see JavaScriptPathResolver), so a leading
// segment that is not a bundle name, such as `[native_code]` or `<anonymous>`, is not a module frame.
const BUNDLE_NAME = /^[A-Za-z0-9_]+$/;

/**
 * Bundle of the innermost frame that belongs to a feature module, or undefined when the stack never
 * leaves framework code.
 */
export function bundleNameFromStackFrames(frames: StackFrame[]): string | undefined {
  for (const frame of frames) {
    const slash = frame.fileName.indexOf('/');
    if (slash <= 0) {
      continue;
    }
    const bundle = frame.fileName.substring(0, slash);
    if (BUNDLE_NAME.test(bundle) && !FRAMEWORK_BUNDLES.has(bundle)) {
      return bundle;
    }
  }
  return undefined;
}
