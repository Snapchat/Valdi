import { Component } from 'valdi_core/src/Component';
import { AttributedTextBuilder } from 'valdi_core/src/utils/AttributedTextBuilder';

export class TextAttribute extends Component {
  onRender() {
    const attributedTextBuilder = new AttributedTextBuilder();

    attributedTextBuilder.append('Hello');
    attributedTextBuilder.withStyle(
      {
        color: 'red',
        backgroundColor: 'yellow',
        backgroundPadding: { left: 1, top: 2, right: 3, bottom: 4 },
        backgroundBorderRadius: '5%',
        font: 'title',
        textDecoration: 'dashed-underline',
      },
      b => {
        b.appendText(' ').appendText('World');
      },
    );

    attributedTextBuilder.append(' ');
    attributedTextBuilder.append('Code', {
      backgroundColor: 'yellow',
      backgroundPadding: 6,
    });

    attributedTextBuilder.pushColor('blue');
    attributedTextBuilder.appendText('!');
    attributedTextBuilder.pushColor('green');
    attributedTextBuilder.appendText('?');
    attributedTextBuilder.pop();
    attributedTextBuilder.appendText('!');
    attributedTextBuilder.pop();

    <view>
      <label value={attributedTextBuilder.build()} />;
    </view>;
  }
}
