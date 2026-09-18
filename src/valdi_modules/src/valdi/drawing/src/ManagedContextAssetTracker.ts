interface AssetState {
  next: AssetState | undefined;
  prev: AssetState | undefined;
  loaded: boolean;
  error: string | undefined;
}

export class ManagedContextAssetTracker {
  get assetsCount(): number {
    return this._assetCount;
  }

  /** True when every tracked asset has settled, counting an errored asset as settled. */
  get allAssetsLoaded(): boolean {
    return this.areAllAssetsLoaded();
  }

  private assetStateById: { [key: number]: AssetState } = {};
  private _assetCount = 0;
  private onAllAssetsLoadedCallbacks: (() => void)[] = [];
  private assetStateRoot: AssetState | undefined = undefined;

  collectErrors(): string[] | undefined {
    let errors: string[] | undefined;
    let current = this.assetStateRoot;
    while (current) {
      if (current.error) {
        if (!errors) {
          errors = [];
        }
        errors.push(current.error);
      }
      current = current.next;
    }

    return errors;
  }

  onAllAssetsLoaded(cb: () => void): void {
    this.onAllAssetsLoadedCallbacks.push(cb);
    this.flushAllAssetsLoadedIfNeeded();
  }

  private removeAssetState(assetState: AssetState) {
    const prev = assetState.prev;
    const next = assetState.next;

    if (prev) {
      prev.next = next;
    }
    if (next) {
      next.prev = prev;
    }

    this._assetCount--;

    if (assetState === this.assetStateRoot) {
      this.assetStateRoot = assetState.next;
    }

    assetState.prev = undefined;
    assetState.next = undefined;
  }

  private appendAssetState(assetState: AssetState) {
    this._assetCount++;

    if (this.assetStateRoot) {
      this.assetStateRoot.prev = assetState;
      assetState.next = this.assetStateRoot;
    }

    this.assetStateRoot = assetState;
  }

  private areAllAssetsLoaded(): boolean {
    let current = this.assetStateRoot;
    while (current) {
      if (!current.loaded && !current.error) {
        return false;
      }
      current = current.next;
    }

    return true;
  }

  private flushAllAssetsLoadedIfNeeded() {
    if (this.areAllAssetsLoaded() && this.onAllAssetsLoadedCallbacks.length > 0) {
      const callbacks = this.onAllAssetsLoadedCallbacks;
      this.onAllAssetsLoadedCallbacks = [];
      for (const callback of callbacks) {
        callback();
      }
    }
  }

  onBeganRequestingLoadedAsset(viewNode: number): void {
    let existingState = this.assetStateById[viewNode];
    if (existingState) {
      this.removeAssetState(existingState);
    }
    const newAssetState: AssetState = { prev: undefined, next: undefined, loaded: false, error: undefined };
    this.assetStateById[viewNode] = newAssetState;

    this.appendAssetState(newAssetState);

    this.flushAllAssetsLoadedIfNeeded();
  }

  onEndRequestingLoadedAsset(viewNode: number): void {
    let existingState = this.assetStateById[viewNode];
    delete this.assetStateById[viewNode];

    if (existingState) {
      this.removeAssetState(existingState);
    }

    this.flushAllAssetsLoadedIfNeeded();
  }

  onLoadedAssetChanged(viewNode: number, error: string | undefined): void {
    let existingState = this.assetStateById[viewNode];

    if (!existingState) {
      return;
    }

    existingState.loaded = !error;
    existingState.error = error;

    this.flushAllAssetsLoadedIfNeeded();
  }
}
