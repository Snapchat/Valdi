import { Component } from 'valdi_core/src/Component';
import { Text } from './Text';

export interface NewlineProps {
  /** Number of empty lines. @default 1 */
  readonly count?: number;
}

/** Vertical whitespace of `count` lines. */
export class Newline extends Component<NewlineProps> {
  onRender(): void {
    const count = Math.max(1, this.viewModel.count ?? 1);
    <Text value={'\n'.repeat(count - 1)} />;
  }
}
