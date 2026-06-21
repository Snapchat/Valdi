import { LabelTextDecoration } from './NativeTemplateElements';
import { AttributedTextInlineImageAttachment } from './AttributedTextInlineImageAttachment';

export type AttributedTextOnTap = () => void;
export type AttributedTextOnLayout = (x: number, y: number, width: number, height: number) => void;

export interface AttributedTextAnimationTransform {
  /**
   * Stable animation identity for parts using this transform.
   *
   * When provided, native renderers use this key together with each part index
   * to preserve animation state across attributed text updates. When omitted,
   * the part index alone is used.
   */
  key?: string;

  /**
   * Initial vertical offset, in logical pixels, applied to each animated part.
   *
   * The part animates from this offset back to its normal text position.
   */
  translationY?: number;

  /**
   * Initial scale applied to each animated part.
   *
   * The part animates from this scale back to 1.
   */
  scale?: number;

  /**
   * Initial opacity applied to each animated part.
   *
   * The part animates from this opacity back to 1.
   */
  opacity?: number;

  /**
   * Duration, in seconds, for each part to animate from the initial transform
   * back to its normal text appearance.
   */
  duration?: number;

  /**
   * Delay, in seconds, between the start of each attributed text part in
   * the same animation transaction.
   *
   * A value of 0 starts all parts at the same time. A value of 0.1 starts the
   * first part immediately, the second after 0.1s, the third after 0.2s, and
   * so on.
   */
  timeOffsetBetweenParts?: number;
}

export interface AttributedTextBackgroundPadding {
  left?: number;
  top?: number;
  right?: number;
  bottom?: number;
}

export type AttributedTextBackgroundPaddingValue = number | AttributedTextBackgroundPadding;

export interface AttributedTextAttributes {
  font?: string;
  color?: string;
  backgroundColor?: string;
  backgroundPadding?: AttributedTextBackgroundPaddingValue;
  backgroundBorderRadius?: number | string;
  textDecoration?: LabelTextDecoration;
  onTap?: AttributedTextOnTap;
  // onLayout currently supports single line text only.
  onLayout?: AttributedTextOnLayout;

  // both outlineColor and outlineWidth must be set for outline
  outlineColor?: string;
  outlineWidth?: number;

  // both outerOutlineColor and outerOutlineWidth must be set for outer outline
  // outer outline is only supported on textview
  outerOutlineColor?: string;
  outerOutlineWidth?: number;

  // Applies a per-part animation transform during native text rendering.
  animationTransform?: AttributedTextAnimationTransform;
}

export const enum AttributedTextEntryType {
  /**
   * Appends a string content, which will be rendered using
   * the style at the top of the stack
   */
  Content = 1,
  /**
   * Pops a previously pushed style from the stack
   */
  Pop,
  /**
   * Pushes a font at the top of the style stack.
   */
  PushFont,
  /**
   * Pushes a text decoration at the top of the style stack.
   */
  PushTextDecoration,
  /**
   * Pushes a color at the top of the style stack.
   */
  PushColor,
  /**
   * Pushes an onTap callback on at the top of the style stack.
   */
  PushOnTap,
  /**
   * Pushes an onLayout callback at the top of the style stack.
   */
  PushOnLayout,
  /**
   * Pushes an outline color at the top of the style stack.
   */
  PushOutlineColor,
  /**
   * Pushes an outline width at the top of the style stack.
   */
  PushOutlineWidth,
  /**
   * Pushes an outer outline color at the top of the style stack.
   */
  PushOuterOutlineColor,
  /**
   * Pushes an outer outline width at the top of the style stack.
   */
  PushOuterOutlineWidth,
  /**
   * Pushes an inline image attachment at the top of the style stack.
   * The value should be an AttributedTextInlineImageAttachment object.
   * Content should be the Unicode Object Replacement Character (U+FFFC).
   */
  PushInlineImage,
  /**
   * Pushes a per-part animation transform at the top of the style stack.
   */
  PushAnimationTransform,
  /**
   * Pushes a background color at the top of the style stack.
   */
  PushBackgroundColor,
  /**
   * Pushes background padding at the top of the style stack.
   */
  PushBackgroundPadding,
  /**
   * Pushes background border radius at the top of the style stack.
   */
  PushBackgroundBorderRadius,
}

export type AttributedTextChunk =
  | AttributedTextEntryType
  | string
  | number
  | AttributedTextOnTap
  | AttributedTextOnLayout
  | AttributedTextInlineImageAttachment
  | AttributedTextBackgroundPaddingValue
  | AttributedTextAnimationTransform;

export type AttributedText = AttributedTextChunk[];
