import { LabelTextDecoration } from './NativeTemplateElements';
import { AttributedTextInlineImageAttachment } from './AttributedTextInlineImageAttachment';

export type AttributedTextOnTap = () => void;
export type AttributedTextOnLayout = (x: number, y: number, width: number, height: number) => void;

export interface AttributedTextAnimationTransform {
  translationY?: number;
  scale?: number;
  opacity?: number;
}

export interface AttributedTextAttributes {
  font?: string;
  color?: string;
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
}

export type AttributedTextChunk =
  | AttributedTextEntryType
  | string
  | number
  | AttributedTextOnTap
  | AttributedTextOnLayout
  | AttributedTextInlineImageAttachment
  | AttributedTextAnimationTransform;

export type AttributedText = AttributedTextChunk[];
