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
    marginBottom: 8,
  },
};

export class TextLetterSpacing extends Component {
  onRender(): void {
    <view {...styles.root}>
      <label value="letterSpacing tests" font={testFont(10)} color="#999999" />
      <layout height={8} />

      <view {...styles.row}>
        <label value="0 (default):" font={testFont(9)} color="#666666" />
        <label
          value="Default letter spacing"
          font={testFont(14)}
          color="#000000"
          letterSpacing={0}
        />
      </view>

      <view {...styles.row}>
        <label value="1pt:" font={testFont(9)} color="#666666" />
        <label
          value="Wider letter spacing"
          font={testFont(14)}
          color="#000000"
          letterSpacing={1}
        />
      </view>

      <view {...styles.row}>
        <label value="3pt:" font={testFont(9)} color="#666666" />
        <label
          value="Very wide spacing"
          font={testFont(14)}
          color="#000000"
          letterSpacing={3}
        />
      </view>

      <view {...styles.row}>
        <label value="-0.5pt:" font={testFont(9)} color="#666666" />
        <label
          value="Tighter letter spacing"
          font={testFont(14)}
          color="#000000"
          letterSpacing={-0.5}
        />
      </view>

      <view {...styles.row}>
        <label value="2pt wrapping:" font={testFont(9)} color="#666666" />
        <label
          value="Letter spacing affects how text wraps to multiple lines"
          font={testFont(14)}
          color="#000000"
          letterSpacing={2}
          numberOfLines={0}
        />
      </view>
    </view>;
  }
}
