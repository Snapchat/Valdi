import { Component } from 'valdi_core/src/Component';
import { testFont } from './TestFont';

const caption = 'A caption that is taller than the box it sits in, so its lower lines must be cut off.';

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
    height: 40,
    backgroundColor: '#FFE9C7',
    borderWidth: 1,
    borderColor: '#000000',
    marginBottom: 6,
  },
};

export class ClippedCaption extends Component {
  onRender(): void {
    <view {...styles.root}>
      <label value="clipped caption tests" font={testFont(10)} color="#999999" />
      <layout height={6} />

      <label value="Clipped to 40pt, label is 80pt:" font={testFont(9)} color="#666666" />
      <view {...styles.box} slowClipping={true}>
        <label value={caption} font={testFont(12)} color="#000000" numberOfLines={0} height={80} />
      </view>

      <label value="Clipped with rounded corners:" font={testFont(9)} color="#666666" />
      <view {...styles.box} slowClipping={true} borderRadius={16}>
        <label value={caption} font={testFont(12)} color="#000000" numberOfLines={0} height={80} />
      </view>

      <label value="Two-line limit, ellipsis:" font={testFont(9)} color="#666666" />
      <view {...styles.box} height={undefined} slowClipping={true}>
        <label value={caption} font={testFont(12)} color="#000000" numberOfLines={2} textOverflow="ellipsis" />
      </view>
    </view>;
  }
}
