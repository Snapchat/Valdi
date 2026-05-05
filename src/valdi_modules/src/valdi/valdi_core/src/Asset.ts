import { Asset } from 'valdi_tsx/src/Asset';
import { BackendRenderingType, LoadedAsset, LoadedAssetMetadata, ValdiRuntime } from './ValdiRuntime';
import { Device } from './Device';

export { Asset };

/**
 * Specifies the desired output type of a given asset.
 * This will impact the underlying asset loader used to load
 * an asset.
 */
export const enum AssetOutputType {
  BYTES = 0,
  IMAGE_IOS = 1,
  IMAGE_ANDROID = 2,
  IMAGE_SNAP_DRAWING = 3,
  VIDEO_IOS = 4,
  VIDEO_ANDROID = 5,
  ANIMATED_IMAGE = 6,
  DUMMY = 7,
}

/**
 * This enum is similar to AssetOutputType, but is abstracted away from
 * the underlying platform used. This makes it possible to have cross-platform
 * code which expects a specific type of asset, like video, to request to load
 * a video without having to specify whether it's for iOS and Android.
 */
export const enum AssetType {
  BYTES,
  IMAGE,
  VIDEO,
  ANIMATED_IMAGE,
  DUMMY,
}

declare const runtime: ValdiRuntime;

/**
 * Make an Asset (to be set on <image/>) from an arbitrary URL
 */
export function makeAssetFromUrl(url: string): Asset {
  return runtime.makeAssetFromUrl(url);
}

/**
 * Create an Asset from a bytes source.
 * The returned asset instance will retain the bytes, and can be set
 * on the src attribute of an <image>. When the src is set, the runtime
 * will asynchronously convert the bytes into a suitable representation
 * to display inside the image element.
 */
export function makeAssetFromBytes(bytes: ArrayBuffer | Uint8Array): Asset {
  return runtime.makeAssetFromBytes(bytes);
}

/**
 * Make a directional Asset from a given left to right (LTR) and
 * right to left (RTL) asset. The LTR asset will be used when the
 * asset is used in a node that is in a LTR layout direction, whereas
 * the RTL asset will be used in a RTL layout direction.
 * @param ltrAsset the asset to use in a LTR layout direction
 * @param rtlAsset the asset to use in a RTL layout direction
 * @returns an Asset that can be used as a src attribute, and will
 * use either the ltr or rtl asset depending the layout direction.
 */
export function makeDirectionalAsset(ltrAsset: string | Asset, rtlAsset: string | Asset): Asset {
  return runtime.makeDirectionalAsset(ltrAsset, rtlAsset);
}

export type PlatformAssetOverrides = {
  ios?: string | Asset;
  android?: string | Asset;
};

/**
 * Make a platform specific Asset from a default asset `defaultAsset` and
 * iOS and/or Android assets in `platformAssetOverrides`. The iOS asset will be
 * rendered on iOS, whereas the Android asset will be used in a node rendered
 * on Android.
 * Omitting iOS or Android in `platformAssetOverrides` will mean that platform falls back
 * to the `defaultAsset`. An empty `platformAssetOverrides`, is programmer error and will
 * throw an error.
 * Unmentioned platforms (web / desktop) will use the `defaultAsset`
 * until support is added to allow overrides for those platforms.
 * @param defaultAsset the asset to use in absence of a platform override
 * matching your current rendering platform (ie. iOS, Android)
 * @param platformAssetOverrides an object containing asset overrides for specific rendering platforms
 * @returns an Asset that can be used as a src attribute, and will
 * use either the `defaultAsset` or platform specific asset depending the rendering platform,
 * and the contents of `platformAssetOverrides`.
 */
export function makePlatformSpecificAsset(
  defaultAsset: string | Asset,
  platformAssetOverrides: PlatformAssetOverrides,
): Asset {
  return runtime.makePlatformSpecificAsset(defaultAsset, platformAssetOverrides);
}

/**
 * Callback called whenever an asset has finished loading.
 */
export type AssetLoadObserver = (loadedAsset: LoadedAsset | Uint8Array | undefined, error: string | undefined) => void;

export interface AssetSubscription {
  unsubscribe(): void;
}

/**
 * Resolve the AssetOutpuType for a given contextId and AssetType. This will query
 * the underlying render backend used within the context to resolve the AssetOutputType
 * that should be used to load a given asset.
 */
export function resolveAssetOutputType(contextId: number | string, assetType: AssetType): AssetOutputType {
  switch (assetType) {
    case AssetType.BYTES:
      return AssetOutputType.BYTES;
    case AssetType.IMAGE:
      switch (runtime.getBackendRenderingTypeForContextId(contextId)) {
        case BackendRenderingType.ANDROID:
          return AssetOutputType.IMAGE_ANDROID;
        case BackendRenderingType.IOS:
          return AssetOutputType.IMAGE_IOS;
        case BackendRenderingType.SNAP_DRAWING:
          return AssetOutputType.IMAGE_SNAP_DRAWING;
      }
      break;
    case AssetType.VIDEO:
      switch (runtime.getBackendRenderingTypeForContextId(contextId)) {
        case BackendRenderingType.ANDROID:
          return AssetOutputType.VIDEO_ANDROID;
        case BackendRenderingType.IOS:
          return AssetOutputType.VIDEO_IOS;
        case BackendRenderingType.SNAP_DRAWING:
          if (Device.isIOS()) {
            return AssetOutputType.VIDEO_IOS;
          } else if (Device.isAndroid()) {
            return AssetOutputType.VIDEO_ANDROID;
          } else {
            return AssetOutputType.DUMMY;
          }
      }
      break;
    case AssetType.ANIMATED_IMAGE:
      return AssetOutputType.ANIMATED_IMAGE;
    case AssetType.DUMMY:
      return AssetOutputType.DUMMY;
  }
}

/**
 * Append an asset load observer into a given asset.
 * This will trigger a load of the asset and populate the cache.
 * The asset will remain loaded for as long as the observer is registered.
 * The observer can be removed by calling unsubscribe() on the returned object
 * from this function. Every call of addAssetLoadObserver must have an unsubscribe()
 * call otherwise the asset will leak.
 */
export function addAssetLoadObserver(
  asset: string | Asset,
  onLoad: AssetLoadObserver,
  outputType: AssetOutputType,
  preferredWidth?: number,
  preferredHeight?: number,
): AssetSubscription {
  const unsubscribe = runtime.addAssetLoadObserver(asset, onLoad, outputType, preferredWidth, preferredHeight);
  return { unsubscribe };
}

/**
 * Returns the metadata reported by the LoadedAsset.
 */
export function getLoadedAssetMetadata(loadedAsset: LoadedAsset): LoadedAssetMetadata | undefined {
  return runtime.getLoadedAssetMetadata(loadedAsset);
}
