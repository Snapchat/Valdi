import { ApplicationCancelable } from '../src/ApplicationBridge';

const noopCancelable: ApplicationCancelable = { cancel() {} };

// minimal Node env shim so TS stops complaining (no runtime dependency)
declare const process: undefined | { env?: Record<string, string | undefined> };

/**
 * Fires when your app “enters background”.
 * Stub: no-ops; returns a cancelable for API compatibility.
 */
export function observeEnteredBackground(_observe: () => void): ApplicationCancelable {
  // You could hook document.visibilityState === 'hidden' here if desired.
  return noopCancelable;
}

/**
 * Fires when your app “enters foreground”.
 * Stub: no-ops; returns a cancelable for API compatibility.
 */
export function observeEnteredForeground(_observe: () => void): ApplicationCancelable {
  // You could hook document.visibilityState === 'visible' here if desired.
  return noopCancelable;
}

/**
 * Observe keyboard height.
 * Stub: immediately reports 0 and returns a no-op cancelable.
 */
export function observeKeyboardHeight(observe: (height: number) => void): ApplicationCancelable {
  try {
    observe(0);
  } catch {
    /* ignore */
  }
  return noopCancelable;
}

/**
 * Whether the app is foregrounded.
 * Stub: returns true in browsers when visible; otherwise true.
 */
export function isForegrounded(): boolean {
  if (typeof document !== 'undefined' && typeof document.visibilityState === 'string') {
    return document.visibilityState === 'visible';
  }
  return true;
}

/**
 * Whether we’re in an integration-test environment.
 * Stub: checks a couple of common flags, else false.
 */
export function isIntegrationTestEnvironment(): boolean {
  // Jest / Playwright / custom env flags
  if (typeof process !== 'undefined' && process.env) {
    if (process.env.JEST_WORKER_ID || process.env.PLAYWRIGHT === '1' || process.env.INTEGRATION_TEST === '1') {
      return true;
    }
  }
  return false;
}

/**
 * App version string.
 * Stub: tries a few sources, falls back to "0.0.0".
 * - global __APP_VERSION__ (inject via bundler DefinePlugin/Vite define)
 * - npm_package_version (when running via npm scripts in Node)
 */
declare const __APP_VERSION__: string | undefined; // optional build-time define
export function getAppVersion(): string {
  // Build-time injected
  if (typeof __APP_VERSION__ === 'string' && __APP_VERSION__) return __APP_VERSION__;

  // NPM-provided var during scripts (Node)
  if (typeof process !== 'undefined' && process.env?.npm_package_version) {
    return process.env.npm_package_version!;
  }

  return '0.0.0';
}
