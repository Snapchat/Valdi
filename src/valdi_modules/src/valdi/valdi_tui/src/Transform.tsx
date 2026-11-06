import { Component } from 'valdi_core/src/Component';
import { TUI_BOX_CLASS } from './Painter';

export interface TransformProps {
  /**
   * Receives each output line of the children (plain text, one terminal row) and returns
   * the replacement. Cell styles are kept per column, so recoloring is not possible here;
   * use it for effects like uppercasing or gradients of characters.
   */
  readonly transform: (line: string, index: number) => string;
}

/** Post-processes the rendered text of its children, like Ink's `<Transform>`. */
export class Transform extends Component<TransformProps> {
  onRender(): void {
    <custom-view
      iosClass={TUI_BOX_CLASS}
      androidClass={TUI_BOX_CLASS}
      flexDirection='column'
      tuiTransform={this.viewModel.transform}
    >
      <slot />
    </custom-view>;
  }
}
