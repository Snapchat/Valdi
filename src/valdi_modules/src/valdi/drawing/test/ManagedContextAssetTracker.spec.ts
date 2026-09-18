import { ManagedContextAssetTracker } from 'drawing/src/ManagedContextAssetTracker';

import 'jasmine/src/jasmine';

describe('ManagedContextAssetTracker', () => {
  it('can collect errors', () => {
    const assetTracker = new ManagedContextAssetTracker();

    assetTracker.onBeganRequestingLoadedAsset(1);
    assetTracker.onBeganRequestingLoadedAsset(2);

    expect(assetTracker.collectErrors()).toBeUndefined();

    assetTracker.onLoadedAssetChanged(1, 'It failed');

    expect(assetTracker.collectErrors()).toEqual(['It failed']);

    assetTracker.onLoadedAssetChanged(1, 'It failed again');

    expect(assetTracker.collectErrors()).toEqual(['It failed again']);

    assetTracker.onLoadedAssetChanged(2, 'This one also failed');

    expect(assetTracker.collectErrors()).toEqual(['This one also failed', 'It failed again']);
  });

  it('notifies when all assets are loaded when empty', () => {
    const assetTracker = new ManagedContextAssetTracker();
    let called = false;

    assetTracker.onAllAssetsLoaded(() => {
      called = true;
    });

    expect(called).toBeTrue();
  });

  it('reports allAssetsLoaded synchronously', () => {
    const assetTracker = new ManagedContextAssetTracker();

    expect(assetTracker.allAssetsLoaded).toBeTrue();

    assetTracker.onBeganRequestingLoadedAsset(1);
    expect(assetTracker.allAssetsLoaded).toBeFalse();

    assetTracker.onLoadedAssetChanged(1, undefined);
    expect(assetTracker.allAssetsLoaded).toBeTrue();
  });

  it('counts an errored asset as settled for allAssetsLoaded', () => {
    const assetTracker = new ManagedContextAssetTracker();

    assetTracker.onBeganRequestingLoadedAsset(1);
    expect(assetTracker.allAssetsLoaded).toBeFalse();

    assetTracker.onLoadedAssetChanged(1, 'It failed');
    expect(assetTracker.allAssetsLoaded).toBeTrue();
  });

  it('notifies when all assets are loaded', () => {
    const assetTracker = new ManagedContextAssetTracker();

    let called = false;

    assetTracker.onBeganRequestingLoadedAsset(1);
    assetTracker.onAllAssetsLoaded(() => {
      called = true;
    });

    expect(called).toBeFalse();

    assetTracker.onBeganRequestingLoadedAsset(2);
    assetTracker.onBeganRequestingLoadedAsset(3);

    expect(called).toBeFalse();

    assetTracker.onLoadedAssetChanged(3, undefined);

    expect(called).toBeFalse();

    assetTracker.onLoadedAssetChanged(1, undefined);

    expect(called).toBeFalse();

    assetTracker.onLoadedAssetChanged(2, undefined);

    expect(called).toBeTrue();
  });

  it('notifies when all assets are loaded after cancelation', () => {
    const assetTracker = new ManagedContextAssetTracker();
    let called = false;

    assetTracker.onBeganRequestingLoadedAsset(1);
    assetTracker.onBeganRequestingLoadedAsset(2);
    assetTracker.onBeganRequestingLoadedAsset(3);

    assetTracker.onAllAssetsLoaded(() => {
      called = true;
    });
    expect(called).toBeFalse();

    assetTracker.onLoadedAssetChanged(3, undefined);
    assetTracker.onLoadedAssetChanged(1, undefined);

    expect(called).toBeFalse();

    assetTracker.onEndRequestingLoadedAsset(2);

    expect(called).toBeTrue();

    assetTracker.onEndRequestingLoadedAsset(3);
    assetTracker.onEndRequestingLoadedAsset(1);

    called = false;
    assetTracker.onAllAssetsLoaded(() => {
      called = true;
    });

    expect(called).toBeTrue();

    assetTracker.onBeganRequestingLoadedAsset(1);

    called = false;
    assetTracker.onAllAssetsLoaded(() => {
      called = true;
    });

    expect(called).toBeFalse();

    assetTracker.onEndRequestingLoadedAsset(1);
  });
});
