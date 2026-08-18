/** Asset-request lifecycle events emitted by a native or web view-node tree. */
export const enum ViewNodeAssetTrackerEventType {
  /** A view node started requesting its current asset. */
  beganRequestingLoadedAsset = 1,
  /** The node stopped requesting the asset because it was removed, replaced, or canceled. */
  endRequestingLoadedAsset = 2,
  /** The requested asset finished loading successfully or failed. */
  loadedAssetChange = 3,
}

/** Runtime callback for an asset event; `error` is defined only when loading fails. */
export type ViewNodeAssetTrackerCallback = (
  eventType: ViewNodeAssetTrackerEventType,
  nodeId: number,
  error: string | undefined,
) => void;

/**
 * Observes asset requests owned by one view-node tree.
 *
 * Install the tracker before creating asset-backed elements. A node first emits
 * `onBeganRequestingLoadedAsset`, followed by `onLoadedAssetChanged` when its
 * request succeeds or fails, and `onEndRequestingLoadedAsset` when that request
 * is canceled, replaced, or the node is removed. A canceled request can end
 * without ever reporting a loaded-asset change.
 *
 * Node IDs identify requests within their tree. A later begin event for the same
 * node replaces its previous tracked request. Successful loads pass `undefined`
 * as their error; failed loads pass a descriptive error string.
 */
export interface IViewNodeAssetTracker {
  /** Dispatches the raw native/web runtime event to its lifecycle handler. */
  onAssetEvent(eventType: ViewNodeAssetTrackerEventType, nodeId: number, error: string | undefined): void;

  /** Starts tracking the current asset request for `viewNode`. */
  onBeganRequestingLoadedAsset(viewNode: number): void;

  /** Stops tracking the request after cancellation, replacement, or node removal. */
  onEndRequestingLoadedAsset(viewNode: number): void;

  /** Reports completion; `error` is undefined on success and descriptive on failure. */
  onLoadedAssetChanged(viewNode: number, error: string | undefined): void;
}
