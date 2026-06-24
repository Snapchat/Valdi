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
  textBox: {
    backgroundColor: '#F0F0F0',
    padding: 4,
    marginBottom: 6,
    borderRadius: 2,
  },
};

export class TextMultiline extends Component {
  onRender(): void {
    <view {...styles.root}>
      <label value="Multiline wrapping tests" font={testFont(10)} color="#999999" />
      <layout height={6} />

      <label value="Short text (no wrap):" font={testFont(9)} color="#666666" />
      <view {...styles.textBox}>
        <label value="Hello" font={testFont(12)} color="#000000" numberOfLines={0} />
      </view>

      <label value="Wrapping at container edge:" font={testFont(9)} color="#666666" />
      <view {...styles.textBox}>
        <label
          value="This text is long enough to wrap across multiple lines in the container."
          font={testFont(12)}
          color="#000000"
          numberOfLines={0}
        />
      </view>

      <label value="Mixed sizes in column:" font={testFont(9)} color="#666666" />
      <view {...styles.textBox}>
        <label value="Title" font={testBoldFont(18)} color="#000000" />
        <label
          value="Subtitle text that may wrap to a second line depending on width."
          font={testFont(12)}
          color="#666666"
          numberOfLines={0}
        />
        <label value="Caption" font={testFont(10)} color="#999999" />
      </view>

      <label value="Centered multiline:" font={testFont(9)} color="#666666" />
      <view {...styles.textBox}>
        <label
          value="Centered multiline text should align each line to the center of the label."
          font={testFont(12)}
          color="#000000"
          textAlign="center"
          numberOfLines={0}
        />
      </view>

      <label value="Right-aligned multiline:" font={testFont(9)} color="#666666" />
      <view {...styles.textBox}>
        <label
          value="Right-aligned multiline text should push each line to the right edge."
          font={testFont(12)}
          color="#000000"
          textAlign="right"
          numberOfLines={0}
        />
      </view>
    </view>;
  }
}
