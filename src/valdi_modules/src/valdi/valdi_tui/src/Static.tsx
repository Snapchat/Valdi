import { Component } from 'valdi_core/src/Component';

const STATIC_CLASS = 'ValdiTerminalStatic';

export interface StaticProps<T> {
  readonly items: ReadonlyArray<T>;
  /** Renders one item. Called inside the render pass, so it must emit JSX like `onRender`. */
  readonly render: (item: T, index: number) => void;
}

/**
 * Output that is written above the live frame and never erased: logs, completed tasks,
 * test results. Items are rendered as a column; rows already written are not written again.
 */
export class Static<T = unknown> extends Component<StaticProps<T>> {
  onRender(): void {
    const { items, render } = this.viewModel;
    // Absolute, so the block reserves no rows in the live frame.
    <custom-view
      iosClass={STATIC_CLASS}
      androidClass={STATIC_CLASS}
      position='absolute'
      flexDirection='column'
      top={0}
      left={0}
      right={0}
    >
      {items.forEach((item, index) => render(item, index))}
    </custom-view>;
  }
}
