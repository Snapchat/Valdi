import { Component } from 'valdi_core/src/Component';
import { testFont, testBoldFont } from './TestFont';

const styles = {
  root: {
    width: '100%' as const,
    height: '100%' as const,
    backgroundColor: '#FFFFFF',
    flexDirection: 'column' as const,
    padding: 8,
  },
  section: {
    marginBottom: 12,
  },
};

export class TextDecoration extends Component {
  onRender(): void {
    <view {...styles.root}>
      <label value="textDecoration tests" font={testFont(10)} color="#999999" />
      <layout height={8} />

      <view {...styles.section}>
        <label value="none (default):" font={testFont(9)} color="#666666" />
        <label
          value="Normal text without decoration"
          font={testFont(14)}
          color="#000000"
          textDecoration="none"
        />
      </view>

      <view {...styles.section}>
        <label value="underline:" font={testFont(9)} color="#666666" />
        <label
          value="Underlined text decoration"
          font={testFont(14)}
          color="#000000"
          textDecoration="underline"
        />
      </view>

      <view {...styles.section}>
        <label value="strikethrough:" font={testFont(9)} color="#666666" />
        <label
          value="Strikethrough text decoration"
          font={testFont(14)}
          color="#000000"
          textDecoration="strikethrough"
        />
      </view>

      <view {...styles.section}>
        <label value="underline + bold:" font={testFont(9)} color="#666666" />
        <label
          value="Bold underlined text"
          font={testBoldFont(14)}
          color="#000000"
          textDecoration="underline"
        />
      </view>

      <view {...styles.section}>
        <label value="strikethrough + colored:" font={testFont(9)} color="#666666" />
        <label
          value="Red strikethrough text"
          font={testFont(14)}
          color="#FF0000"
          textDecoration="strikethrough"
        />
      </view>
    </view>;
  }
}
