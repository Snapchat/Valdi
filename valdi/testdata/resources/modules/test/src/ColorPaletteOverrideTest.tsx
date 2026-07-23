import { Component } from 'valdi_core/src/Component';
import { ValdiRuntime } from 'valdi_core/src/ValdiRuntime';

declare const runtime: ValdiRuntime;

export class ColorPaletteOverrideTest extends Component {
  onCreate() {
    runtime.configureColorPalette('light', {
      background: 'rgba(0, 0, 255, 1)',
      foreground: 'rgba(0, 128, 0, 1)',
    });
    runtime.configureColorPalette('dark', {
      background: 'rgba(255, 0, 0, 1)',
      foreground: 'rgba(255, 255, 0, 1)',
    });
    runtime.configureColorPalette('unused', {
      background: 'rgba(0, 0, 0, 1)',
      foreground: 'rgba(255, 255, 255, 1)',
    });
    runtime.setActiveColorPalette('light');
  }

  onRender() {
    <view background='background'>
      <view colorPaletteName='dark' background='background'>
        <view background='foreground' />
      </view>
      <view background='foreground' />
    </view>;
  }

  setDarkActiveColorPalette() {
    runtime.setActiveColorPalette('dark');
  }

  updateDarkColorPalette() {
    runtime.configureColorPalette('dark', {
      background: 'rgba(0, 0, 0, 1)',
      foreground: 'rgba(255, 255, 255, 1)',
    });
  }

  updateUnusedColorPalette() {
    runtime.configureColorPalette('unused', {
      background: 'rgba(255, 255, 255, 1)',
      foreground: 'rgba(0, 0, 0, 1)',
    });
  }
}
