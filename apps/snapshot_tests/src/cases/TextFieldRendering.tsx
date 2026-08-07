import { Component } from 'valdi_core/src/Component';
import { testFont } from './TestFont';

/**
 * Pixel coverage for `textfield` and `textview` rendering.
 *
 * The existing text cases all cover `label`. Editable text renders through a separate class
 * (EditableTextLayerClass) with its own value/placeholder handling, and had no coverage.
 *
 * Rendered headless, so there is no focus, caret or keyboard here — this covers the resting
 * appearance: value text, placeholder text, colors and multi-line layout.
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
    marginBottom: 10,
  },
  field: {
    width: '100%' as const,
    height: 28,
    backgroundColor: '#F0F0F0',
    borderRadius: 4,
    paddingLeft: 6,
    paddingRight: 6,
  },
  multiline: {
    width: '100%' as const,
    height: 60,
    backgroundColor: '#F0F0F0',
    borderRadius: 4,
    paddingLeft: 6,
    paddingRight: 6,
  },
};

export class TextFieldRendering extends Component {
  onRender(): void {
    <view {...styles.root}>
      <label value="textfield / textview tests" font={testFont(10)} color="#999999" />
      <layout height={8} />

      <layout {...styles.section}>
        <label value="With a value:" font={testFont(9)} color="#666666" />
        <textfield {...styles.field} value="Typed content" font={testFont(14)} color="#000000" />
      </layout>

      <layout {...styles.section}>
        <label value="Empty, showing placeholder:" font={testFont(9)} color="#666666" />
        <textfield
          {...styles.field}
          placeholder="Search places"
          placeholderColor="#999999"
          font={testFont(14)}
          color="#000000"
        />
      </layout>

      <layout {...styles.section}>
        <label value="Value longer than the field (clipping):" font={testFont(9)} color="#666666" />
        <textfield
          {...styles.field}
          value="A value long enough to overflow the available width"
          font={testFont(14)}
          color="#000000"
        />
      </layout>

      <layout {...styles.section}>
        <label value="Multi-line textview:" font={testFont(9)} color="#666666" />
        <textview
          {...styles.multiline}
          value={'First line of a multi-line value\nSecond line wraps below it'}
          font={testFont(13)}
          color="#000000"
        />
      </layout>
    </view>;
  }
}
