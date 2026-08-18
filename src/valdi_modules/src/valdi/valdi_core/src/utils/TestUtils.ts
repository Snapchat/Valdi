export type ValdiGlobalTestSuiteDoneHandler = {
  __onValdiTestSuiteDone: () => void;
};

const callbacks: Array<() => void> = [];

(globalThis as typeof globalThis & ValdiGlobalTestSuiteDoneHandler).__onValdiTestSuiteDone = () =>
  callbacks.forEach(callback => callback());

/**
 * Registers a callback to be called when a Jasmine test suite is done.
 * Use that to clear any global state that was potentially polluted by test runs.
 *
 * @param callback
 */
export function registerValdiTestSuiteDoneCallback(callback: () => void): void {
  callbacks.push(callback);
}
