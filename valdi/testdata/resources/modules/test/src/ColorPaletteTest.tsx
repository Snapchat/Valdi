import { Component } from 'valdi_core/src/Component';
import { ValdiRuntime } from 'valdi_core/src/ValdiRuntime';

declare const runtime: ValdiRuntime;

export class ColorPaletteTest extends Component {
  onCreate() {
    runtime.configureColorPalette('light', {
      background: 'rgba(0, 0, 255, 1)',
      foreground: 'rgba(0, 128, 0, 1)',
    });
    runtime.configureColorPalette('dark', {
      background: 'rgba(255, 0, 0, 1)',
      foreground: 'rgba(255, 255, 0, 1)',
    });
    runtime.setActiveColorPalette('light');
  }

  onRender() {
    <view border='1 solid background'>
      <view background='foreground' />
    </view>;
  }

  setDarkColorPalette() {
    runtime.setActiveColorPalette('dark');
  }

  setLegacyColorPalette() {
    runtime.setColorPalette({
      background: 'rgba(0, 0, 0, 1)',
      foreground: 'rgba(255, 255, 255, 1)',
    });
  }

  updateDarkColorPalette() {
    runtime.configureColorPalette('dark', {
      background: 'rgba(0, 0, 0, 1)',
      foreground: 'rgba(255, 255, 255, 1)',
    });
  }

  updateLightColorPalette() {
    runtime.configureColorPalette('light', {
      background: 'rgba(255, 0, 0, 1)',
      foreground: 'rgba(255, 255, 0, 1)',
    });
  }
}
