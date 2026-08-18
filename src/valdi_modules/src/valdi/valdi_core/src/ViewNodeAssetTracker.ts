import { IViewNodeAssetTracker, ViewNodeAssetTrackerEventType } from './IViewNodeAssetTracker';

interface AssetState {
  next: AssetState | undefined;
  prev: AssetState | undefined;
  loaded: boolean;
  error: string | undefined;
}

export class ViewNodeAssetTracker implements IViewNodeAssetTracker {
  get assetsCount(): number {
    return this._assetCount;
  }

  private assetStateById: { [key: number]: AssetState } = {};
  private _assetCount = 0;
  private onAllAssetsLoadedCallbacks: (() => void)[] = [];
  private assetStateRoot: AssetState | undefined = undefined;

  onAssetEvent(eventType: ViewNodeAssetTrackerEventType, nodeId: number, error: string | undefined): void {
    switch (eventType) {
      case ViewNodeAssetTrackerEventType.beganRequestingLoadedAsset:
        this.onBeganRequestingLoadedAsset(nodeId);
        break;
      case ViewNodeAssetTrackerEventType.endRequestingLoadedAsset:
        this.onEndRequestingLoadedAsset(nodeId);
        break;
      case ViewNodeAssetTrackerEventType.loadedAssetChange:
        this.onLoadedAssetChanged(nodeId, error);
        break;
    }
  }

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

  onAllAssetsLoaded(callback: () => void): void {
    this.onAllAssetsLoadedCallbacks.push(callback);
    this.flushAllAssetsLoadedIfNeeded();
  }

  private removeAssetState(assetState: AssetState): void {
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

  private appendAssetState(assetState: AssetState): void {
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

  private flushAllAssetsLoadedIfNeeded(): void {
    if (this.areAllAssetsLoaded() && this.onAllAssetsLoadedCallbacks.length > 0) {
      const callbacks = this.onAllAssetsLoadedCallbacks;
      this.onAllAssetsLoadedCallbacks = [];
      for (const callback of callbacks) {
        callback();
      }
    }
  }

  onBeganRequestingLoadedAsset(viewNode: number): void {
    const existingState = this.assetStateById[viewNode];
    if (existingState) {
      this.removeAssetState(existingState);
    }
    const newAssetState: AssetState = { prev: undefined, next: undefined, loaded: false, error: undefined };
    this.assetStateById[viewNode] = newAssetState;

    this.appendAssetState(newAssetState);
    this.flushAllAssetsLoadedIfNeeded();
  }

  onEndRequestingLoadedAsset(viewNode: number): void {
    const existingState = this.assetStateById[viewNode];
    delete this.assetStateById[viewNode];

    if (existingState) {
      this.removeAssetState(existingState);
    }

    this.flushAllAssetsLoadedIfNeeded();
  }

  onLoadedAssetChanged(viewNode: number, error: string | undefined): void {
    const existingState = this.assetStateById[viewNode];
    if (!existingState) {
      return;
    }

    existingState.loaded = !error;
    existingState.error = error;
    this.flushAllAssetsLoadedIfNeeded();
  }
}
