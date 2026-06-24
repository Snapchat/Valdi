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
  card: {
    backgroundColor: '#F8F8F8',
    padding: 8,
    borderRadius: 4,
    marginBottom: 6,
  },
};

export class TextCombined extends Component {
  onRender(): void {
    <view {...styles.root}>
      <label value="Combined text property tests" font={testFont(10)} color="#999999" />
      <layout height={6} />

      <view {...styles.card}>
        <label
          value="lineHeight + letterSpacing"
          font={testFont(9)}
          color="#666666"
        />
        <label
          value="Wide spacing with double line height text that wraps around."
          font={testFont(12)}
          color="#000000"
          lineHeight={2}
          letterSpacing={1.5}
          numberOfLines={0}
        />
      </view>

      <view {...styles.card}>
        <label
          value="underline + center + multiline"
          font={testFont(9)}
          color="#666666"
        />
        <label
          value="Centered underlined multiline text wrapping in a card."
          font={testFont(12)}
          color="#0000CC"
          textDecoration="underline"
          textAlign="center"
          numberOfLines={0}
        />
      </view>

      <view {...styles.card}>
        <label
          value="truncation + decoration"
          font={testFont(9)}
          color="#666666"
        />
        <label
          value="This strikethrough text is truncated to two lines which tests the combination of decoration and line clamping."
          font={testFont(12)}
          color="#CC0000"
          textDecoration="strikethrough"
          numberOfLines={2}
        />
      </view>

      <view {...styles.card}>
        <label
          value="tight spacing + right align"
          font={testFont(9)}
          color="#666666"
        />
        <label
          value="Tight letter spacing right aligned multiline."
          font={testFont(12)}
          color="#000000"
          letterSpacing={-0.5}
          textAlign="right"
          lineHeight={0.9}
          numberOfLines={0}
        />
      </view>

      <view {...styles.card}>
        <label
          value="shadow + bold + wide"
          font={testFont(9)}
          color="#666666"
        />
        <label
          value="BOLD WIDE SHADOW"
          font={testBoldFont(18)}
          color="#333333"
          letterSpacing={3}
          textShadow="#000000 2 0.27 1 1"
        />
      </view>
    </view>;
  }
}
