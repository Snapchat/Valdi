import { Component } from 'valdi_core/src/Component';
import { ElementRef } from 'valdi_core/src/ElementRef';
import { Style } from 'valdi_core/src/Style';
import { TextField } from 'valdi_tsx/src/NativeTemplateElements';

export interface InputBarViewModel {
  onSubmit?: (text: string) => void;
}

export class InputBar extends Component<InputBarViewModel> {
  textInput = new ElementRef<TextField>();

  onRender(): void {
    <textfield
      ref={this.textInput}
      width="80%"
      placeholder="Ask anything..."
      style={styles.textfield}
      onEditEnd={this.onSubmit}
    />;
  }

  onSubmit: () => void = async () => {
    const ref = this.textInput.single()!;
    const text = ref.getAttribute('value')?.toString() ?? '';
    if (text.length === 0) {
      return;
    }

    ref.setAttribute('value', '');
    this.viewModel.onSubmit?.call(undefined, text);
  };
}

const styles = {
  textfield: new Style<TextField>({
    font: 'Montserrat-SemiBold 20',
  }),
};
