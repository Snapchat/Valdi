import { Component } from 'valdi_core/src/Component';
import { testFont } from './TestFont';

/**
 * Pins how the `border` shorthand renders each style keyword.
 *
 * `preprocessBorder` parses the style identifier and discards it — "Ignoring border style since we can
 * only support solid" — so `dashed` and `dotted` are accepted without complaint and render solid. This
 * is upstream behaviour, present on master and on github.com/Snapchat/Valdi main, not something this
 * change introduced.
 *
 * Two things this baseline is here to catch:
 *
 *  1. If someone makes the parser pass the style through, or teaches snap_drawing a dash path effect
 *     (it has none today — Layer::drawForeground only strokes solid), these rows stop matching and the
 *     change has to be looked at deliberately rather than landing as a silent visual diff.
 *  2. If the shorthand's token order handling changes, the widths/colours here shift. Order matters
 *     more than it looks: `'4 #F44336 dashed'` parses the colour as the style identifier and then
 *     fails on the colour, producing no border at all.
 *
 * Scope worth being explicit about: this renders through the headless C++ runtime, which instantiates
 * no UIView and no android.View. It therefore covers the shared parse and the snap_drawing stroke, and
 * says nothing about the platform border paths — the iOS CAShapeLayer dash/dotted branches in
 * UIView+Valdi.m are unreachable from TSX and untested by this.
 */
const styles = {
  root: {
    width: '100%' as const,
    height: '100%' as const,
    backgroundColor: '#FFFFFF',
    flexDirection: 'column' as const,
    padding: 8,
  },
  sectionLabel: { color: '#666666' },
  box: { height: 34, marginBottom: 6, justifyContent: 'center' as const, paddingLeft: 6 },
};

export class BorderStyles extends Component {
  onRender(): void {
    <view {...styles.root}>
      <label value="solid / dashed / dotted, all 4px" font={testFont(9)} {...styles.sectionLabel} />
      <view {...styles.box} border="4 solid #F44336">
        <label value="solid" font={testFont(9)} />
      </view>
      <view {...styles.box} border="4 dashed #2196F3">
        <label value="dashed" font={testFont(9)} />
      </view>
      <view {...styles.box} border="4 dotted #4CAF50">
        <label value="dotted" font={testFont(9)} />
      </view>

      <label value="width variants, solid" font={testFont(9)} {...styles.sectionLabel} />
      <view {...styles.box} border="1 solid #9C27B0">
        <label value="1px" font={testFont(9)} />
      </view>
      <view {...styles.box} border="8 solid #FF9800">
        <label value="8px" font={testFont(9)} />
      </view>

      <label value="with corner radius" font={testFont(9)} {...styles.sectionLabel} />
      <view {...styles.box} border="4 dashed #00BCD4" borderRadius={10}>
        <label value="dashed + radius" font={testFont(9)} />
      </view>
    </view>;
  }
}
