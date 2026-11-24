export function continueAfterNativePromise(load: () => Promise<string>, report: () => void): void {
  load().then(() => report());
}
