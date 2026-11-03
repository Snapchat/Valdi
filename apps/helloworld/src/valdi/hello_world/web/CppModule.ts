// Web override for the C++ native module. No-op on the browser; the
// original C++ implementation logs the root component ID via native
// bindings that don't exist here. Kept as a stub so the app boots.

// Keeps the parameter so the signature matches the native declaration in src/CppModule.d.ts.
// eslint-disable-next-line @typescript-eslint/no-unused-vars
export function onRootComponentCreated(_contextId: string): void {
  // no-op on web
}
