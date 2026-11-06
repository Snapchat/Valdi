import { Component } from 'valdi_core/src/Component';
import { TUI_TEXT_CLASS } from './Painter';
import { TextStyle } from './TextStyle';
import { TextRun, WrapMode, layoutText, measureLines } from './wrap';

/** A styled piece of inline text. Spans flow and wrap together inside one {@link Text}. */
export interface TextSpan extends TextStyle {
  readonly text: string;
  /** Ink's name for {@link TextStyle.dim}. */
  readonly dimColor?: boolean;
}

export interface TextProps extends TextStyle {
  /**
   * The text to display. An array mixes plain strings with {@link TextSpan}s so parts of one
   * paragraph can carry their own style, the way nested `<Text>` does in Ink.
   */
  readonly value?: string | number | ReadonlyArray<string | TextSpan>;
  /** Ink's name for {@link TextStyle.dim}. */
  readonly dimColor?: boolean;
  /** @default 'wrap' */
  readonly wrap?: WrapMode;
}

const MEASURE_MODE_UNSPECIFIED = 0;

function baseStyle(vm: TextProps): TextStyle | undefined {
  const dim = vm.dim || vm.dimColor ? true : undefined;
  if (
    vm.color === undefined &&
    vm.backgroundColor === undefined &&
    !vm.bold &&
    !dim &&
    !vm.italic &&
    !vm.underline &&
    !vm.strikethrough &&
    !vm.inverse
  ) {
    return undefined;
  }
  return {
    color: vm.color,
    backgroundColor: vm.backgroundColor,
    bold: vm.bold,
    dim,
    italic: vm.italic,
    underline: vm.underline,
    strikethrough: vm.strikethrough,
    inverse: vm.inverse,
  };
}

function spanStyle(span: TextSpan, parent: TextStyle | undefined): TextStyle | undefined {
  const own: TextStyle = {
    color: span.color,
    backgroundColor: span.backgroundColor,
    bold: span.bold,
    dim: span.dim || span.dimColor ? true : undefined,
    italic: span.italic,
    underline: span.underline,
    strikethrough: span.strikethrough,
    inverse: span.inverse,
  };
  if (!parent) {
    return own;
  }
  const merged: { -readonly [K in keyof TextStyle]: TextStyle[K] } = { ...parent };
  for (const key of Object.keys(own) as (keyof TextStyle)[]) {
    if (own[key] !== undefined) {
      (merged as any)[key] = own[key];
    }
  }
  return merged;
}

export function textRunsFromProps(vm: TextProps): TextRun[] {
  const style = baseStyle(vm);
  const value = vm.value;
  if (value === undefined) {
    return [{ text: '', style }];
  }
  if (typeof value === 'string' || typeof value === 'number') {
    return [{ text: String(value), style }];
  }
  const runs: TextRun[] = [];
  for (const part of value) {
    if (typeof part === 'string') {
      runs.push({ text: part, style });
    } else {
      runs.push({ text: part.text, style: spanStyle(part, style) });
    }
  }
  return runs.length > 0 ? runs : [{ text: '', style }];
}

/**
 * Renders text. Unlike `<label>`, the text is measured in terminal cells and wrapped by the
 * renderer, so it participates in flexbox layout like Ink's `<Text>`.
 *
 * ```tsx
 * <Text color='green' bold value='Done' />
 * <Text value={['Status: ', { text: 'ok', color: 'green' }]} />
 * ```
 */
export class Text extends Component<TextProps> {
  private runs: TextRun[] = [];
  private wrapMode: WrapMode = 'wrap';
  private measureKey = '';
  private measure: (width: number, widthMode: number, height: number, heightMode: number) => [number, number] = () => [
    0, 0,
  ];

  onRender(): void {
    const runs = textRunsFromProps(this.viewModel);
    const wrapMode = this.viewModel.wrap ?? 'wrap';
    const key = `${wrapMode}|${JSON.stringify(runs)}`;
    if (key !== this.measureKey) {
      // A new measure function invalidates the node's layout, which a changed attribute alone would not.
      this.measureKey = key;
      this.runs = runs;
      this.wrapMode = wrapMode;
      this.measure = (width, widthMode) => {
        // Yoga distributes fractional widths; text lives on whole cells, so wrap against the
        // rounded width (the painter rounds the same way).
        const maxWidth = widthMode === MEASURE_MODE_UNSPECIFIED ? undefined : Math.round(width);
        return measureLines(layoutText(this.runs, maxWidth, this.wrapMode));
      };
    }

    <custom-view
      iosClass={TUI_TEXT_CLASS}
      androidClass={TUI_TEXT_CLASS}
      flexShrink={1}
      onMeasure={this.measure}
      tuiRuns={this.runs}
      tuiWrap={this.wrapMode}
    />;
  }
}
