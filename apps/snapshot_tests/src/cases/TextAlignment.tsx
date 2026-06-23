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
  textBox: {
    backgroundColor: '#F0F0F0',
    padding: 4,
    marginBottom: 6,
    borderRadius: 2,
  },
};

export class TextAlignment extends Component {
  onRender(): void {
    <view {...styles.root}>
      <label value="textAlign tests" font={testFont(10)} color="#999999" />
      <layout height={6} />

      <label value="Left (default):" font={testFont(9)} color="#666666" />
      <view {...styles.textBox}>
        <label
          value="Left-aligned text content that wraps to show alignment behavior."
          font={testFont(12)}
          color="#000000"
          textAlign="left"
          numberOfLines={0}
        />
      </view>

      <label value="Center:" font={testFont(9)} color="#666666" />
      <view {...styles.textBox}>
        <label
          value="Center-aligned text content that wraps to show alignment behavior."
          font={testFont(12)}
          color="#000000"
          textAlign="center"
          numberOfLines={0}
        />
      </view>

      <label value="Right:" font={testFont(9)} color="#666666" />
      <view {...styles.textBox}>
        <label
          value="Right-aligned text content that wraps to show alignment behavior."
          font={testFont(12)}
          color="#000000"
          textAlign="right"
          numberOfLines={0}
        />
      </view>

      <label value="Justified:" font={testFont(9)} color="#666666" />
      <view {...styles.textBox}>
        <label
          value="Justified text stretches each line to fill the full width of the container evenly."
          font={testFont(12)}
          color="#000000"
          textAlign="justified"
          numberOfLines={0}
        />
      </view>
    </view>;
  }
}
