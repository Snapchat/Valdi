import { Component } from 'valdi_core/src/Component';
import { AttributedTextBuilder } from 'valdi_core/src/utils/AttributedTextBuilder';
import { AttributedTextInlineViewVerticalAlignment } from 'valdi_tsx/src/AttributedTextInlineViewAttachment';

export class ManagedChildFrames extends Component {
  onRender() {
    <view height={200} width={200}>
      <custom-view height={60} id='managed' iosClass='ManagedChildFrameView' width={100}>
        <view height={20} id='managedChild' left={10} position='absolute' width={30} />
      </custom-view>
    </view>;
  }
}

export class InlineViewTextAttribute extends Component {
  onRender() {
    <view height={200} width={200}>
      <label height={60} id='inlineLabel' value={this.inlineText()} width={100}>
        <view height={12} id='inlineChild' width={18} />
      </label>
    </view>;
  }

  private inlineText() {
    return new AttributedTextBuilder()
      .append('Before ')
      .appendInlineView(0, AttributedTextInlineViewVerticalAlignment.Bottom)
      .append(' after')
      .build();
  }
}

export class InlineViewVerticalAlignmentTextAttribute extends Component {
  onRender() {
    <view height={200} width={200}>
      <label height={80} id='inlineLabel' value={this.inlineText()} width={140}>
        <view height={12} id='inlineTop' width={11} />
        <view height={24} id='inlineCenter' width={22} />
        <view height={36} id='inlineBottom' width={33} />
        <view height={14} id='inlineBaseline' width={44} />
      </label>
    </view>;
  }

  private inlineText() {
    return new AttributedTextBuilder()
      .append('A')
      .appendInlineView(0, AttributedTextInlineViewVerticalAlignment.Top)
      .append('B')
      .appendInlineView(1, AttributedTextInlineViewVerticalAlignment.Center)
      .append('C')
      .appendInlineView(2, AttributedTextInlineViewVerticalAlignment.Bottom)
      .append('D')
      .appendInlineView(3, AttributedTextInlineViewVerticalAlignment.Baseline)
      .append('E')
      .build();
  }
}

export class InlineViewTextViewAttribute extends Component {
  onRender() {
    <view height={200} width={200}>
      <textview height={80} id='inlineTextView' value={this.inlineText()} width={140}>
        <view height={16} id='textViewInlineChild' width={26} />
      </textview>
    </view>;
  }

  private inlineText() {
    return new AttributedTextBuilder()
      .append('Textview ')
      .appendInlineView(0, AttributedTextInlineViewVerticalAlignment.Top)
      .append(' child')
      .build();
  }
}

export class InlineViewInvalidChildIndexAttribute extends Component {
  onRender() {
    <view height={200} width={200}>
      <label height={60} id='inlineLabel' value={this.inlineText()} width={100}>
        <view height={12} id='inlineChild' width={18} />
      </label>
    </view>;
  }

  private inlineText() {
    return new AttributedTextBuilder().append('Before ').appendInlineView(1).append(' after').build();
  }
}

interface InlineViewDynamicSizeViewModel {
  childWidth: number;
  childHeight: number;
}

export class InlineViewDynamicSizeAttribute extends Component<InlineViewDynamicSizeViewModel> {
  onRender() {
    <view height={200} width={200}>
      <label height={80} id='inlineLabel' value={this.inlineText()} width={140}>
        <view height={this.viewModel.childHeight} id='inlineChild' width={this.viewModel.childWidth} />
      </label>
    </view>;
  }

  private inlineText() {
    return new AttributedTextBuilder()
      .append('Size ')
      .appendInlineView(0, AttributedTextInlineViewVerticalAlignment.Center)
      .append(' child')
      .build();
  }
}
