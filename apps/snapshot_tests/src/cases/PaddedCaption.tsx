import { Component } from 'valdi_core/src/Component';
import { testFont } from './TestFont';

const caption = 'A caption that wraps onto several lines inside a narrow box.';

const styles = {
  root: {
    width: '100%' as const,
    height: '100%' as const,
    backgroundColor: '#FFFFFF',
    flexDirection: 'column' as const,
    padding: 8,
  },
  box: {
    width: 150,
    marginBottom: 6,
  },
  padded: {
    backgroundColor: '#CFE8FF',
  },
};

function renderCaption(numberOfLines: number): void {
  <label value={caption} font={testFont(12)} color="#000000" numberOfLines={numberOfLines} />;
}

export class PaddedCaption extends Component {
  onRender(): void {
    <view {...styles.root}>
      <label value="padded caption tests" font={testFont(10)} color="#999999" />
      <layout height={6} />

      <label value="No padding (control):" font={testFont(9)} color="#666666" />
      <view {...styles.box} {...styles.padded}>
        {renderCaption(0)}
      </view>

      <label value="padding=10:" font={testFont(9)} color="#666666" />
      <view {...styles.box} {...styles.padded} padding={10}>
        {renderCaption(0)}
      </view>

      <label value="Asymmetric padding:" font={testFont(9)} color="#666666" />
      <view {...styles.box} {...styles.padded} paddingLeft={20} paddingRight={4} paddingTop={2} paddingBottom={12}>
        {renderCaption(0)}
      </view>

      <label value="padding=10, one line:" font={testFont(9)} color="#666666" />
      <view {...styles.box} {...styles.padded} padding={10}>
        {renderCaption(1)}
      </view>
    </view>;
  }
}
