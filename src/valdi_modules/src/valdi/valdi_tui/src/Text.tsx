import { Component } from 'valdi_core/src/Component';
import { AttributedTextBuilder } from 'valdi_core/src/utils/AttributedTextBuilder';
import { AttributedText } from 'valdi_tsx/src/AttributedText';
import { LabelTextDecoration } from 'valdi_tsx/src/NativeTemplateElements';
import { TextStyle, isEmptyStyle, mergeStyles } from './TextStyle';
import { parseColor } from './ansi';

/** A styled piece of inline text. Spans flow and wrap together inside one {@link Text}. */
export interface TextSpan extends TextStyle {
  readonly text: string;
  /** Ink's name for {@link TextStyle.dim}. */
  readonly dimColor?: boolean;
}

/**
 * How text that does not fit its width is handled. `wrap` breaks between words (and inside words
 * wider than the width); the other modes keep one line and replace the overflow with an ellipsis.
 */
export type WrapMode =
  | 'wrap'
  | 'end'
  | 'middle'
  | 'start'
  | 'truncate'
  | 'truncate-end'
  | 'truncate-middle'
  | 'truncate-start';

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

const LABEL_IOS_CLASS = 'SCValdiLabel';
const LABEL_ANDROID_CLASS = 'com.snap.valdi.views.ValdiTextView';

/** A run of text with the style it renders in. */
export interface TextRun {
  readonly text: string;
  readonly style?: TextStyle;
}

function baseStyle(vm: TextProps): TextStyle | undefined {
  const style: TextStyle = {
    color: vm.color,
    backgroundColor: vm.backgroundColor,
    bold: vm.bold,
    dim: vm.dim || vm.dimColor ? true : undefined,
    italic: vm.italic,
    underline: vm.underline,
    strikethrough: vm.strikethrough,
    inverse: vm.inverse,
  };
  return isEmptyStyle(style) ? undefined : style;
}

function spanStyle(span: TextSpan, parent: TextStyle | undefined): TextStyle | undefined {
  return mergeStyles(parent, {
    color: span.color,
    backgroundColor: span.backgroundColor,
    bold: span.bold,
    dim: span.dim || span.dimColor ? true : undefined,
    italic: span.italic,
    underline: span.underline,
    strikethrough: span.strikethrough,
    inverse: span.inverse,
  });
}

/** Flattens the `value` of a {@link Text} into runs, with the text's own style as the base of each. */
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

// Only the weight and slant of a font survive in a terminal, as bold, dim and italic.
function fontFor(style: TextStyle | undefined): string | undefined {
  if (!style) {
    return undefined;
  }
  const weight = style.bold ? '-bold' : style.dim ? '-light' : '';
  const slant = style.italic ? '-italic' : '';
  return weight || slant ? `system${weight}${slant} 1` : undefined;
}

function decorationFor(style: TextStyle | undefined): LabelTextDecoration | undefined {
  if (style?.underline) {
    return 'underline';
  }
  if (style?.strikethrough) {
    return 'strikethrough';
  }
  return undefined;
}

function attributedValue(runs: ReadonlyArray<TextRun>): AttributedText {
  const builder = new AttributedTextBuilder();
  for (const run of runs) {
    const style = run.style;
    // Spans carry every style so inline runs differ from their neighbours without relying on the label's own attributes.
    builder.append(run.text, {
      font: fontFor(style),
      color: parseColor(style?.color),
      backgroundColor: parseColor(style?.backgroundColor),
      textDecoration: decorationFor(style),
    });
  }
  return builder.build();
}

function needsAttributedValue(runs: ReadonlyArray<TextRun>): boolean {
  return runs.length > 1 || runs.some(run => run.style?.backgroundColor !== undefined);
}

/**
 * Renders text with a `<label>`, so it participates in flexbox layout like Ink's `<Text>`. The
 * text is measured and wrapped in terminal cells by the native backend.
 *
 * Inverse video applies to the whole text; a span's own `inverse` has no effect because spans only
 * carry font, color, decoration and background.
 *
 * ```tsx
 * <Text color='green' bold value='Done' />
 * <Text value={['Status: ', { text: 'ok', color: 'green' }]} />
 * ```
 */
export class Text extends Component<TextProps> {
  onRender(): void {
    const vm = this.viewModel;
    const runs = textRunsFromProps(vm);
    const style = baseStyle(vm);
    const wrap = vm.wrap ?? 'wrap';
    const wraps = wrap === 'wrap';
    const value = needsAttributedValue(runs) ? attributedValue(runs) : runs[0].text;

    // custom-view with the label classes, because `inverse` and `textWrap` are terminal attributes
    // the <label> typings do not declare.
    <custom-view
      iosClass={LABEL_IOS_CLASS}
      androidClass={LABEL_ANDROID_CLASS}
      value={value}
      color={parseColor(style?.color)}
      font={fontFor(style)}
      textDecoration={decorationFor(style)}
      numberOfLines={wraps ? 0 : 1}
      textOverflow="ellipsis"
      textWrap={wraps ? undefined : wrap}
      inverse={style?.inverse}
      flexShrink={1}
    />;
  }
}
