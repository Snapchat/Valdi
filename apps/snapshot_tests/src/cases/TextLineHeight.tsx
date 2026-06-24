import { Component } from 'valdi_core/src/Component';
import { testFont } from './TestFont';

const styles = {
  root: {
    width: '100%' as const,
    height: '100%' as const,
    backgroundColor: '#FFFFFF',
    flexDirection: 'column' as const,
    padding: 8,
  },
  row: {
    flexDirection: 'row' as const,
    alignItems: 'flex-start' as const,
    marginBottom: 4,
  },
  labelCol: {
    width: 80,
  },
};

export class TextLineHeight extends Component {
  onRender(): void {
    <view {...styles.root}>
      <label value="lineHeight tests" font={testFont(10)} color="#999999" />
      <layout height={6} />

      <layout {...styles.row}>
        <label value="1.0x:" font={testFont(10)} color="#666666" {...styles.labelCol} />
        <label
          value="Line height default (1.0). Single line of text."
          font={testFont(12)}
          color="#000000"
          lineHeight={1}
          numberOfLines={0}
          flexShrink={1}
        />
      </layout>

      <layout {...styles.row}>
        <label value="1.5x:" font={testFont(10)} color="#666666" {...styles.labelCol} />
        <label
          value="Line height 1.5x ratio. This text should have extra spacing between lines when it wraps."
          font={testFont(12)}
          color="#000000"
          lineHeight={1.5}
          numberOfLines={0}
          flexShrink={1}
        />
      </layout>

      <layout {...styles.row}>
        <label value="2.0x:" font={testFont(10)} color="#666666" {...styles.labelCol} />
        <label
          value="Line height 2.0x ratio. Double spaced text wrapping to multiple lines."
          font={testFont(12)}
          color="#000000"
          lineHeight={2}
          numberOfLines={0}
          flexShrink={1}
        />
      </layout>

      <layout {...styles.row}>
        <label value="0.8x:" font={testFont(10)} color="#666666" {...styles.labelCol} />
        <label
          value="Tight line height 0.8x. Lines should be closer together than default."
          font={testFont(12)}
          color="#000000"
          lineHeight={0.8}
          numberOfLines={0}
          flexShrink={1}
        />
      </layout>
    </view>;
  }
}
