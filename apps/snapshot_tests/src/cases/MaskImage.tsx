import { Component } from 'valdi_core/src/Component';
import { testFont } from './TestFont';

/**
 * Pixel coverage for the `maskImage` attribute.
 *
 * Exercises the shared C++ implementation (LayerClass::apply_maskImage / reset_maskImage) plus the
 * gradient string preprocessing registered for the attribute. That path had no coverage at all,
 * which is the same gap that let the iOS implementation of this attribute be deleted wholesale and
 * ship as a silent no-op.
 *
 * Note this does NOT cover the platform view-layer implementations (UIView+Valdi on iOS,
 * ViewAttributesBinder on Android) — those render through different code and need platform tests.
 */
const styles = {
  root: {
    width: '100%' as const,
    height: '100%' as const,
    backgroundColor: '#FFFFFF',
    flexDirection: 'column' as const,
    padding: 8,
  },
  section: {
    marginBottom: 8,
  },
  // Solid fill so the mask's alpha is the only thing shaping the output.
  swatch: {
    width: 120,
    height: 40,
    backgroundColor: '#0000FF',
  },
};

export class MaskImage extends Component {
  onRender(): void {
    <view {...styles.root}>
      <label value="maskImage tests" font={testFont(10)} color="#999999" />
      <layout height={6} />

      <layout {...styles.section}>
        <label value="No mask (control):" font={testFont(9)} color="#666666" />
        <view {...styles.swatch} />
      </layout>

      <layout {...styles.section}>
        <label value="Linear fade, transparent to opaque:" font={testFont(9)} color="#666666" />
        <view {...styles.swatch} maskImage="linear-gradient(#00000000 0%, #000000 100%)" />
      </layout>

      <layout {...styles.section}>
        <label value="Linear fade, top 20% only:" font={testFont(9)} color="#666666" />
        <view {...styles.swatch} maskImage="linear-gradient(#00000000 0%, #000000 20%)" />
      </layout>

      <layout {...styles.section}>
        <label value="Radial mask:" font={testFont(9)} color="#666666" />
        <view {...styles.swatch} maskImage="radial-gradient(#000000, transparent)" />
      </layout>

      <layout {...styles.section}>
        <label value="Mask plus borderRadius (both shape the layer):" font={testFont(9)} color="#666666" />
        <view {...styles.swatch} borderRadius={12} maskImage="linear-gradient(#000000 0%, #00000000 100%)" />
      </layout>
    </view>;
  }
}
