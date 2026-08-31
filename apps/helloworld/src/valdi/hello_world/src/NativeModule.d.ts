/**
 * @ExportModule
 */

// Returns the name of the app suffixed with the platform
// Used to show case how a native module can be implemented and
// used within TypeScript.
export const APP_NAME: string;

// Debug-only: demonstrates the bridge-function resolution teardown behavior. iOS runs the native
// harness (SCValdiTeardownRepro); android and web are intentional no-ops (this is iOS-specific).

// Degrade ON (shipped default): resolves a bridge function after teardown; returns a no-op and does
// NOT crash.
export function reproduceTeardownDegraded(): void;

// Degrade OFF (kill switch disabled): resolves a bridge function after teardown; aborts the process
// (SIGABRT) — the original teardown crash.
export function reproduceTeardownCrash(): void;
