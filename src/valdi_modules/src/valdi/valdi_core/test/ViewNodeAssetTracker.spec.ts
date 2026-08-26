import { ViewNodeAssetTrackerEventType } from 'valdi_core/src/IViewNodeAssetTracker';
import { ViewNodeAssetTracker } from 'valdi_core/src/ViewNodeAssetTracker';

import 'jasmine/src/jasmine';

describe('ViewNodeAssetTracker', () => {
  it('collects errors from tracked assets', () => {
    const assetTracker = new ViewNodeAssetTracker();

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

  it('notifies immediately when no assets are tracked', () => {
    const assetTracker = new ViewNodeAssetTracker();
    let called = false;

    assetTracker.onAllAssetsLoaded(() => {
      called = true;
    });

    expect(called).toBeTrue();
  });

  it('notifies when all assets have loaded', () => {
    const assetTracker = new ViewNodeAssetTracker();
    let called = false;

    assetTracker.onBeganRequestingLoadedAsset(1);
    assetTracker.onAllAssetsLoaded(() => {
      called = true;
    });

    expect(called).toBeFalse();

    assetTracker.onBeganRequestingLoadedAsset(2);
    assetTracker.onBeganRequestingLoadedAsset(3);
    assetTracker.onLoadedAssetChanged(3, undefined);
    assetTracker.onLoadedAssetChanged(1, undefined);

    expect(called).toBeFalse();

    assetTracker.onLoadedAssetChanged(2, undefined);

    expect(called).toBeTrue();
  });

  it('notifies when an outstanding asset request is canceled', () => {
    const assetTracker = new ViewNodeAssetTracker();
    let called = false;

    assetTracker.onBeganRequestingLoadedAsset(1);
    assetTracker.onBeganRequestingLoadedAsset(2);
    assetTracker.onBeganRequestingLoadedAsset(3);
    assetTracker.onAllAssetsLoaded(() => {
      called = true;
    });

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
    expect(called).toBeTrue();
  });

  it('processes the shared native asset event contract', () => {
    const assetTracker = new ViewNodeAssetTracker();
    let called = false;

    assetTracker.onAssetEvent(ViewNodeAssetTrackerEventType.beganRequestingLoadedAsset, 4, undefined);
    assetTracker.onAllAssetsLoaded(() => {
      called = true;
    });
    expect(called).toBeFalse();

    assetTracker.onAssetEvent(ViewNodeAssetTrackerEventType.loadedAssetChange, 4, undefined);
    expect(called).toBeTrue();

    assetTracker.onAssetEvent(ViewNodeAssetTrackerEventType.endRequestingLoadedAsset, 4, undefined);
    expect(assetTracker.assetsCount).toBe(0);
  });
});
