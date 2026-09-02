// Web override for the platform-specific NativeModule. iOS/Android
// return the app name suffixed with the platform; on web we just say
// "Web".

export const APP_NAME: string = 'Valdi Hello World (Web)';

// No-op on web: the teardown crash is iOS-specific.
export function reproduceTeardownDegraded(): void {
  console.log('[TeardownRepro] no-op on web');
}

export function reproduceTeardownCrash(): void {
  console.log('[TeardownRepro] no-op on web');
}

export function reproduceTeardownInvocationCrash(): void {
  console.log('[TeardownRepro] no-op on web');
}
