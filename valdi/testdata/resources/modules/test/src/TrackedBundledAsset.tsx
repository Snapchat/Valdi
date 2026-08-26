import res from 'test/res';
import { Component } from 'valdi_core/src/Component';
import { ViewNodeAssetTrackerEventType } from 'valdi_core/src/IViewNodeAssetTracker';
import { getValdiRuntime } from 'valdi_core/src/ValdiRuntimeProvider';
import { ViewNodeAssetTracker } from 'valdi_core/src/ViewNodeAssetTracker';

interface ViewModel {
  showImage: boolean;
  onAssetEvent(eventType: ViewNodeAssetTrackerEventType, nodeId: number, error: string | undefined): void;
  onAllAssetsLoaded(): void;
}

class ReportingViewNodeAssetTracker extends ViewNodeAssetTracker {
  constructor(private readonly viewModel: ViewModel) {
    super();
  }

  onBeganRequestingLoadedAsset(viewNode: number): void {
    super.onBeganRequestingLoadedAsset(viewNode);
    this.viewModel.onAssetEvent(ViewNodeAssetTrackerEventType.beganRequestingLoadedAsset, viewNode, undefined);
    this.onAllAssetsLoaded(() => this.viewModel.onAllAssetsLoaded());
  }

  onEndRequestingLoadedAsset(viewNode: number): void {
    super.onEndRequestingLoadedAsset(viewNode);
    this.viewModel.onAssetEvent(ViewNodeAssetTrackerEventType.endRequestingLoadedAsset, viewNode, undefined);
  }

  onLoadedAssetChanged(viewNode: number, error: string | undefined): void {
    this.viewModel.onAssetEvent(ViewNodeAssetTrackerEventType.loadedAssetChange, viewNode, error);
    super.onLoadedAssetChanged(viewNode, error);
  }
}

export class TrackedBundledAsset extends Component<ViewModel> {
  onCreate(): void {
    const assetTracker = new ReportingViewNodeAssetTracker(this.viewModel);
    getValdiRuntime().setViewNodeAssetTracker(this.renderer.contextId, assetTracker.onAssetEvent.bind(assetTracker));
  }

  onRender(): void {
    <view>{this.viewModel.showImage ? <image id="imageView" src={res.emoji} /> : null}</view>;
  }

  clearAssetTracker(): void {
    getValdiRuntime().setViewNodeAssetTracker(this.renderer.contextId, undefined);
  }
}
