export function waitForShutdownSignal(): Promise<void> {
  return new Promise(resolve => {
    const resolveOnce = () => {
      process.off('SIGINT', resolveOnce);
      process.off('SIGTERM', resolveOnce);
      resolve();
    };

    process.once('SIGINT', resolveOnce);
    process.once('SIGTERM', resolveOnce);
  });
}
