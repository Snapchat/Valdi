import { Component } from 'valdi_core/src/Component';
import { Style } from 'valdi_core/src/Style';
import { View } from 'valdi_tsx/src/NativeTemplateElements';

const baseStyle = new Style<View>({ backgroundColor: 'red', scaleX: 0.5 });
const overrideStyle = new Style<View>({ backgroundColor: 'blue', opacity: 0.25, scaleY: 0.75 });

interface ViewModel {
  includeOverride: boolean;
  reverseStyles: boolean;
}

export class StyleTest extends Component<ViewModel> {
  onRender() {
    const style = this.viewModel.includeOverride
      ? this.viewModel.reverseStyles
        ? [overrideStyle, baseStyle]
        : [baseStyle, overrideStyle]
      : [baseStyle];

    <view style={style} />;
  }
}
