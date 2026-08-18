import { Component } from 'valdi_core/src/Component';
import { ValdiRuntime } from 'valdi_core/src/ValdiRuntime';

declare const runtime: ValdiRuntime;

/**
 * Pins how the color-palette / theme API resolves named colors at render time.
 *
 * Exercises the surface added by the hierarchical-color-palette change: `configureColorPalette`
 * registers named palettes, the per-node `colorPaletteName` attribute pins a subtree to one, and
 * attribute values that are palette color names (`background='panelBg'`, and the color token in the
 * `border` shorthand) resolve against the pinned palette.
 *
 * The palettes are only registered, never made active via `setActiveColorPalette` — the harness
 * renders every case in one runtime, so flipping the global active palette would bleed into other
 * cases. Each panel pins its own palette with `colorPaletteName`, which is the per-node override path.
 *
 * Scope: this renders through the headless C++ runtime (no UIView / android.View), so it covers the
 * shared palette resolution and the snap_drawing fill, not any platform-specific theming.
 */

const SNAP_LIGHT = 'snapLight';
const SNAP_DARK = 'snapDark';

function configureThemePalettes(): void {
  runtime.configureColorPalette(SNAP_LIGHT, {
    panelBg: 'rgba(242, 242, 247, 1)',
    panelText: 'rgba(20, 20, 20, 1)',
    accent: 'rgba(0, 122, 255, 1)',
  });
  runtime.configureColorPalette(SNAP_DARK, {
    panelBg: 'rgba(28, 28, 30, 1)',
    panelText: 'rgba(245, 245, 245, 1)',
    accent: 'rgba(255, 45, 85, 1)',
  });
}

const styles = {
  root: {
    width: '100%' as const,
    height: '100%' as const,
    backgroundColor: '#FFFFFF',
    flexDirection: 'row' as const,
    padding: 16,
  },
  panel: {
    width: 132,
    height: 132,
    flexDirection: 'column' as const,
    padding: 12,
    borderRadius: 12,
  },
  bar: { width: '100%' as const, height: 12, borderRadius: 6 },
  lineWide: { width: '70%' as const, height: 8, borderRadius: 4, marginTop: 14 },
  lineNarrow: { width: '45%' as const, height: 8, borderRadius: 4, marginTop: 8 },
};

// Two panels resolving the same named colors against two pinned palettes, side by side.
export class ColorPaletteThemes extends Component {
  onCreate(): void {
    configureThemePalettes();
  }

  onRender(): void {
    <view {...styles.root}>
      <view {...styles.panel} colorPaletteName={SNAP_LIGHT} background="panelBg" border="2 solid accent" marginRight={12}>
        <view {...styles.bar} background="accent" />
        <view {...styles.lineWide} background="panelText" />
        <view {...styles.lineNarrow} background="panelText" />
      </view>
      <view {...styles.panel} colorPaletteName={SNAP_DARK} background="panelBg" border="2 solid accent">
        <view {...styles.bar} background="accent" />
        <view {...styles.lineWide} background="panelText" />
        <view {...styles.lineNarrow} background="panelText" />
      </view>
    </view>;
  }
}

const nestedStyles = {
  root: {
    width: '100%' as const,
    height: '100%' as const,
    flexDirection: 'column' as const,
    padding: 16,
  },
  inner: {
    width: '100%' as const,
    flexDirection: 'column' as const,
    padding: 12,
    borderRadius: 10,
    marginTop: 14,
  },
};

// A light-pinned panel wrapping a dark-pinned subtree: palette inheritance down the tree plus a
// nested `colorPaletteName` override taking precedence over the inherited palette.
export class ColorPaletteNestedOverride extends Component {
  onCreate(): void {
    configureThemePalettes();
  }

  onRender(): void {
    <view {...nestedStyles.root} colorPaletteName={SNAP_LIGHT} background="panelBg">
      <view {...styles.bar} background="accent" />
      <view width="60%" height={8} borderRadius={4} marginTop={10} background="panelText" />
      <view {...nestedStyles.inner} colorPaletteName={SNAP_DARK} background="panelBg" border="2 solid accent">
        <view width="50%" height={10} borderRadius={5} background="accent" />
        <view width="80%" height={8} borderRadius={4} marginTop={8} background="panelText" />
      </view>
    </view>;
  }
}
