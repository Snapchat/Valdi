import { Component } from 'valdi_core/src/Component';
import { Box } from './Box';

/** A flexible gap that pushes its siblings apart along the parent's main axis. */
export class Spacer extends Component {
  onRender(): void {
    <Box flexGrow={1} />;
  }
}
