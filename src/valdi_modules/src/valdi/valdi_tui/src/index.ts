/**
 * valdi_tui: render Valdi components to a terminal, in the spirit of Ink for React.
 *
 * Build with `valdi_cli_application` and call {@link render} with a root component made of
 * {@link Box} and {@link Text}. See docs/docs/workflow-terminal-ui.md.
 */
export { render } from './TerminalRenderer';
export type { RenderInstance, RenderOptions } from './TerminalRenderer';
export { Box } from './Box';
export type { AlignItems, AlignSelf, BoxProps, FlexDirection, FlexWrap, JustifyContent } from './Box';
export { Text, textRunsFromProps } from './Text';
export type { TextProps, TextSpan } from './Text';
export { Newline } from './Newline';
export type { NewlineProps } from './Newline';
export { Spacer } from './Spacer';
export { Static } from './Static';
export type { StaticProps } from './Static';
export { Transform } from './Transform';
export type { TransformProps } from './Transform';
export { FocusManager, TerminalApp, getTerminalApp, useFocus, useInput, useResize } from './App';
export type { DisposableOwner, Focusable, FocusOptions, InputHandler, ResizeHandler, UseFocusOptions } from './App';
export { MemoryTerminal, Terminal } from './Terminal';
export type { Disposable, ITerminal, MemoryTerminalOptions, TerminalOptions } from './Terminal';
export { parseInput } from './keys';
export type { Key, KeyPress } from './keys';
export type { TextStyle } from './TextStyle';
export type { BorderCharacters, BorderStyle, BorderStyleName } from './borders';
export type { StyledLine, StyledSegment, TextRun, WrapMode } from './wrap';
export { layoutText, measureLines } from './wrap';
export { stringWidth } from './textWidth';
export { ColorLevel, detectColorLevel, parseColor, stripAnsi } from './ansi';
