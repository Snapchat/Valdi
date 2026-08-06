import { Component } from 'valdi_core/src/Component';
import { AttributedTextBuilder } from 'valdi_core/src/utils/AttributedTextBuilder';
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

export class InlineImageLayout extends Component {
  onRender(): void {
    const textWithImage = new AttributedTextBuilder()
      .append('Before ', { font: testFont(14), color: '#000000' })
      .appendInlineImage({ attachmentId: 'img1', width: 20, height: 20 })
      .append(' After', { font: testFont(14), color: '#000000' })
      .build();

    const textWithLargeImage = new AttributedTextBuilder()
      .append('Tall ', { font: testFont(14), color: '#000000' })
      .appendInlineImage({ attachmentId: 'img2', width: 40, height: 40 })
      .append(' image', { font: testFont(14), color: '#000000' })
      .build();

    const textWithMultipleImages = new AttributedTextBuilder()
      .append('A', { font: testFont(14), color: '#000000' })
      .appendInlineImage({ attachmentId: 'img3', width: 16, height: 16 })
      .append('B', { font: testFont(14), color: '#000000' })
      .appendInlineImage({ attachmentId: 'img4', width: 24, height: 12 })
      .append('C', { font: testFont(14), color: '#000000' })
      .build();

    const wrappingTextWithImage = new AttributedTextBuilder()
      .append('This is a longer sentence with an inline ', { font: testFont(12), color: '#000000' })
      .appendInlineImage({ attachmentId: 'img5', width: 30, height: 14 })
      .append(' image that should cause the text to wrap across lines.', { font: testFont(12), color: '#000000' })
      .build();

    <view {...styles.root}>
      <label value="Inline image layout tests" font={testFont(10)} color="#999999" />
      <layout height={6} />

      <label value="Small inline image:" font={testFont(9)} color="#666666" />
      <view {...styles.textBox}>
        <label value={textWithImage} numberOfLines={0} />
      </view>

      <label value="Tall inline image:" font={testFont(9)} color="#666666" />
      <view {...styles.textBox}>
        <label value={textWithLargeImage} numberOfLines={0} />
      </view>

      <label value="Multiple images:" font={testFont(9)} color="#666666" />
      <view {...styles.textBox}>
        <label value={textWithMultipleImages} numberOfLines={0} />
      </view>

      <label value="Wrapping with image:" font={testFont(9)} color="#666666" />
      <view {...styles.textBox}>
        <label value={wrappingTextWithImage} numberOfLines={0} />
      </view>
    </view>;
  }
}
