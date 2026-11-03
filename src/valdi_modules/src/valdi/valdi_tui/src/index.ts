/**
 * valdi_tui: render Valdi components to a terminal, in the spirit of Ink for React.
 *
 * Components render through the native terminal backend (`//valdi:valdi_terminal`): `Box` is a
 * `<view>`, `Text` a `<label>`, and layout, text wrapping and painting happen natively. Call
 * {@link render} with a root component made of {@link Box} and {@link Text}. See
 * docs/docs/workflow-terminal-ui.md.
 */
export { render } from './TerminalRenderer';
export type { RenderInstance, RenderOptions } from './TerminalRenderer';
export { Box } from './Box';
export type { AlignItems, AlignSelf, BorderStyle, BoxProps, FlexDirection, FlexWrap, JustifyContent } from './Box';
export { Text, textRunsFromProps } from './Text';
export type { TextProps, TextRun, TextSpan, WrapMode } from './Text';
export { Newline } from './Newline';
export type { NewlineProps } from './Newline';
export { Spacer } from './Spacer';
export { Static } from './Static';
export type { StaticProps } from './Static';
export { Transform } from './Transform';
export type { TransformProps } from './Transform';
export { FocusManager, TerminalApp, getTerminalApp, useFocus, useInput, useResize } from './App';
export type {
  Disposable,
  DisposableOwner,
  Focusable,
  FocusOptions,
  InputHandler,
  ResizeHandler,
  TerminalInfo,
  UseFocusOptions,
} from './App';
export { getTerminalBackend } from './TerminalBackend';
export type { TerminalBackend, TerminalFrame, TerminalSinkKind } from './TerminalBackend';
export { parseInput } from './keys';
export type { Key, KeyPress } from './keys';
export type { TextStyle } from './TextStyle';
export { ColorLevel, parseColor, stripAnsi } from './ansi';
