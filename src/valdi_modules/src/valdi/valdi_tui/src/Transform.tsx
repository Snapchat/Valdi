import { Component } from 'valdi_core/src/Component';
import { Text, TextProps, TextSpan } from './Text';

export interface TransformProps extends TextProps {
  /**
   * Receives a line of text and its index, and returns the replacement. Use it for effects like
   * uppercasing or gradients of characters.
   *
   * It runs on each string of `value` separately: a plain string is transformed line by line, and
   * for an array each piece or span is transformed on its own, with the index counting the lines
   * seen so far. A line that spans several pieces is therefore transformed in parts.
   */
  readonly transform: (line: string, index: number) => string;
}

/**
 * Text whose content is post-processed by a function, like Ink's `<Transform>`. The terminal
 * backend has no hook between layout and painting, so the function runs on the text values before
 * they reach a label rather than on the laid-out rows.
 */
export class Transform extends Component<TransformProps> {
  onRender(): void {
    const { transform, value, ...textProps } = this.viewModel;
    let lineIndex = 0;
    const apply = (text: string): string => {
      const lines = text.split('\n');
      const out = lines.map(line => transform(line, lineIndex++));
      // The last line of a piece continues on the next piece, so it does not consume a line index.
      lineIndex -= 1;
      return out.join('\n');
    };

    let transformed: TextProps['value'];
    if (value === undefined) {
      transformed = undefined;
    } else if (typeof value === 'string' || typeof value === 'number') {
      transformed = apply(String(value));
    } else {
      transformed = value.map((part): string | TextSpan =>
        typeof part === 'string' ? apply(part) : { ...part, text: apply(part.text) },
      );
    }

    <Text {...textProps} value={transformed} />;
  }
}
