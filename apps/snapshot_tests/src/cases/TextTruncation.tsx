import { Component } from 'valdi_core/src/Component';
import { testFont } from './TestFont';

const longText = 'This is a long piece of text that should be truncated based on the numberOfLines property setting.';

const styles = {
  root: {
    width: '100%' as const,
    height: '100%' as const,
    backgroundColor: '#FFFFFF',
    flexDirection: 'column' as const,
    padding: 8,
  },
  textBox: {
    backgroundColor: '#F0F0F0',
    padding: 4,
    marginBottom: 6,
    borderRadius: 2,
  },
};

export class TextTruncation extends Component {
  onRender(): void {
    <view {...styles.root}>
      <label value="numberOfLines tests" font={testFont(10)} color="#999999" />
      <layout height={6} />

      <label value="numberOfLines=1 (default):" font={testFont(9)} color="#666666" />
      <view {...styles.textBox}>
        <label
          value={longText}
          font={testFont(12)}
          color="#000000"
          numberOfLines={1}
        />
      </view>

      <label value="numberOfLines=2:" font={testFont(9)} color="#666666" />
      <view {...styles.textBox}>
        <label
          value={longText}
          font={testFont(12)}
          color="#000000"
          numberOfLines={2}
        />
      </view>

      <label value="numberOfLines=3:" font={testFont(9)} color="#666666" />
      <view {...styles.textBox}>
        <label
          value={longText}
          font={testFont(12)}
          color="#000000"
          numberOfLines={3}
        />
      </view>

      <label value="numberOfLines=0 (unlimited):" font={testFont(9)} color="#666666" />
      <view {...styles.textBox}>
        <label
          value={longText}
          font={testFont(12)}
          color="#000000"
          numberOfLines={0}
        />
      </view>
    </view>;
  }
}
