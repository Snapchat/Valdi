import { View } from 'valdi_tsx/src/NativeTemplateElements';
import { Component } from '../Component';
import { Style } from '../Style';

export interface ButtonViewModel {
  text: string;
  onTap: () => void;
}

const buttonStyle = new Style<View>({
  backgroundColor: 'white',
  padding: 8,
  borderRadius: 12,
  justifyContent: 'center',
  boxShadow: '0 0 3 rgba(0, 0, 0, 0.15)',
});

export class DebugButton extends Component<ButtonViewModel> {
  onRender() {
    <view onTap={this.viewModel.onTap} style={buttonStyle}>
      <label value={this.viewModel.text} font='Montserrat-SemiBold 12' />
    </view>;
  }
}
