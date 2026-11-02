import { withGlobalNativeRefs } from 'valdi_core/src/NativeReferences';
import Worker from 'worker/src/Worker';

/**
 * A Worker wrapper that automatically tears down its instance
 * once it becomes unused, and will restart the worker instance
 * on demand.
 */
export class ManagedWorker {
  private _worker: Worker | undefined;
  private _useCount = 0;
  private _destroyTimeout: number | undefined;

  constructor(readonly script: string, readonly keepAliveDelayMs: number) {}

  get worker(): Worker {
    if (!this._worker) {
      this._worker = new Worker(this.script);
    }
    return this._worker;
  }

  onUsed(): void {
    this._useCount++;

    if (this._destroyTimeout) {
      clearTimeout(this._destroyTimeout);
      this._destroyTimeout = undefined;
    }
  }

  onUnused(): void {
    this._useCount--;
    if (this._useCount === 0 && !this._destroyTimeout) {
      this._destroyTimeout = withGlobalNativeRefs('worker.ManagedWorker.dispose', () => {
        return setTimeout(() => {
          this.dispose();
        }, this.keepAliveDelayMs);
      });
    }
  }

  private dispose(): void {
    this._worker = undefined;
    this._destroyTimeout = undefined;
  }
}
