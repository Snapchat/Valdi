import { Asset, ThemableAssetMap, makeDirectionalAsset, makeThemableAsset } from 'valdi_core/src/Asset';
import { Component } from 'valdi_core/src/Component';
import { ValdiRuntime } from 'valdi_core/src/ValdiRuntime';

declare const runtime: ValdiRuntime;

interface ViewModel {
  lightAsset: string | Asset;
  darkAsset: string | Asset;
  includeDarkAsset: boolean;
}

export class ThemableAsset extends Component<ViewModel> {
  private asset?: Asset;
  private nestedAsset?: Asset;

  onCreate() {
    runtime.configureColorPalette('light', {
      background: 'rgba(0, 0, 255, 1)',
    });
    runtime.configureColorPalette('dark', {
      background: 'rgba(255, 0, 0, 1)',
    });
    runtime.setActiveColorPalette('light');
  }

  onViewModelUpdate(): void {
    const assetsByColorPalette: ThemableAssetMap = {
      light: this.viewModel.lightAsset,
    };

    if (this.viewModel.includeDarkAsset) {
      assetsByColorPalette.dark = this.viewModel.darkAsset;
    }

    this.asset = makeThemableAsset(assetsByColorPalette);
    this.nestedAsset = makeDirectionalAsset(this.asset, this.viewModel.lightAsset);
  }

  onRender() {
    <view>
      <image src={this.asset} />
      <view colorPaletteName='dark'>
        <image src={this.asset} />
      </view>
      <image src={this.nestedAsset} />
    </view>;
  }

  setDarkColorPalette() {
    runtime.setActiveColorPalette('dark');
  }
}
