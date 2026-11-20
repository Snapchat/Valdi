import { Component } from 'valdi_core/src/Component';
import { testFont } from './TestFont';
import { testImageAsset } from './TestImage';

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
    marginBottom: 6,
  },
  cell: {
    marginRight: 8,
  },
  // Outline shows the image view's bounds so the fit mode is visible against them.
  frame: {
    width: 60,
    height: 60,
    backgroundColor: '#EEEEEE',
    borderWidth: 1,
    borderColor: '#000000',
  },
};

export class ImageContentModes extends Component {
  onRender(): void {
    const src = testImageAsset();

    <view {...styles.root}>
      <label value="objectFit tests" font={testFont(10)} color="#999999" />
      <layout height={6} />

      <layout {...styles.row}>
        <layout {...styles.cell}>
          <label value="fill" font={testFont(9)} color="#666666" />
          <image {...styles.frame} src={src} objectFit="fill" />
        </layout>
        <layout {...styles.cell}>
          <label value="contain" font={testFont(9)} color="#666666" />
          <image {...styles.frame} src={src} objectFit="contain" />
        </layout>
      </layout>

      <layout {...styles.row}>
        <layout {...styles.cell}>
          <label value="cover" font={testFont(9)} color="#666666" />
          <image {...styles.frame} src={src} objectFit="cover" />
        </layout>
        <layout {...styles.cell}>
          <label value="none" font={testFont(9)} color="#666666" />
          <image {...styles.frame} src={src} objectFit="none" />
        </layout>
      </layout>
    </view>;
  }
}
