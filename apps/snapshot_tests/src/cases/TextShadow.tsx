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
    marginBottom: 10,
  },
  darkBg: {
    backgroundColor: '#333333',
    padding: 6,
    borderRadius: 4,
    marginBottom: 10,
  },
};

export class TextShadow extends Component {
  onRender(): void {
    <view {...styles.root}>
      <label value="textShadow tests" font={testFont(10)} color="#999999" />
      <layout height={8} />

      <view {...styles.section}>
        <label value="Subtle drop shadow:" font={testFont(9)} color="#666666" />
        <label
          value="Shadow offset 1,1 blur 2"
          font={testBoldFont(16)}
          color="#000000"
          textShadow="#000000 2 0.4 1 1"
        />
      </view>

      <view {...styles.section}>
        <label value="Strong shadow:" font={testFont(9)} color="#666666" />
        <label
          value="Shadow offset 2,2 blur 4"
          font={testBoldFont(16)}
          color="#000000"
          textShadow="#000000 4 0.67 2 2"
        />
      </view>

      <view {...styles.darkBg}>
        <label value="Light text on dark:" font={testFont(9)} color="#999999" />
        <label
          value="Glow effect on dark bg"
          font={testBoldFont(16)}
          color="#FFFFFF"
          textShadow="#FFFFFF 6 0.67 0 0"
        />
      </view>

      <view {...styles.section}>
        <label value="Colored shadow:" font={testFont(9)} color="#666666" />
        <label
          value="Blue shadow on text"
          font={testBoldFont(16)}
          color="#000000"
          textShadow="#0000FF 3 0.67 1 1"
        />
      </view>

      <view {...styles.section}>
        <label value="No blur shadow:" font={testFont(9)} color="#666666" />
        <label
          value="Hard shadow no blur"
          font={testBoldFont(16)}
          color="#000000"
          textShadow="#000000 0 0.4 2 2"
        />
      </view>
    </view>;
  }
}
