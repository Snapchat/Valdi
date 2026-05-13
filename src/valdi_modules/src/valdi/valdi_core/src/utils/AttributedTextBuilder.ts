import {
  AttributedText,
  AttributedTextAttributes,
  AttributedTextAnimationTransform,
  AttributedTextChunk,
  AttributedTextEntryType,
  AttributedTextOnTap,
  AttributedTextOnLayout,
} from 'valdi_tsx/src/AttributedText';
import { AttributedTextInlineImageAttachment } from 'valdi_tsx/src/AttributedTextInlineImageAttachment';
import { LabelTextDecoration } from 'valdi_tsx/src/NativeTemplateElements';
import { AnyFunction } from './Callback';

export type { AttributedText };

/**
 * Association of a string content and optional a set of attributes.
 */
export interface AttributedTextStyled {
  content: string;
  attributes?: AttributedTextAttributes;
}

/**
 * Builder for creating AttributedText.
 * The value returned by build() can be used as the value
 * of label elements.
 */
export class AttributedTextBuilder {
  private components: AttributedTextChunk[] = [];

  /**
   * Append a string to the AttributedText.
   */
  appendText(text: string): AttributedTextBuilder {
    this.appendEntry(AttributedTextEntryType.Content, text);
    return this;
  }

  /**
   * Convenience method to append a single string associated with a set of text attributes.
   */
  append(text: string, attributes?: AttributedTextAttributes): AttributedTextBuilder {
    if (attributes) {
      const popCount = this.pushStyle(attributes);

      this.appendText(text);

      this.popCount(popCount);
    } else {
      this.appendText(text);
    }

    return this;
  }

  /**
   * Convenience method to append a single string associated with a set of text attributes.
   */
  appendStyled(styled: AttributedTextStyled): AttributedTextBuilder {
    this.append(styled.content, styled.attributes);
    return this;
  }

  /**
   * Push the given font attributes, evaluate the callback, and pop all the evaluated attributes.
   */
  withStyle(attributes: AttributedTextAttributes, cb: (builder: AttributedTextBuilder) => void): AttributedTextBuilder {
    const popCount = this.pushStyle(attributes);

    cb(this);

    this.popCount(popCount);
    return this;
  }

  /**
   * Push a Font on the Style stack, which will make all subsequent added strings
   * use this font, until pop() is called
   */
  pushFont(font: string): AttributedTextBuilder {
    this.components.push(AttributedTextEntryType.PushFont, font);
    return this;
  }

  /**
   * Push a Color on the Style stack, which will make all subsequent added strings
   * use this color, until pop() is called
   */
  pushColor(color: string): AttributedTextBuilder {
    this.components.push(AttributedTextEntryType.PushColor, color);
    return this;
  }

  /**
   * Push a TextDecoration on the Style stack, which will make all subsequent added strings
   * use this color, until pop() is called
   */
  pushTextDecoration(textDecoration: LabelTextDecoration): AttributedTextBuilder {
    this.components.push(AttributedTextEntryType.PushTextDecoration, textDecoration);
    return this;
  }

  /**
   * Push an onTap on the Style stack, which will make all subsequent added strings
   * be tappable.
   */
  pushOnTap(onTap: AttributedTextOnTap): AttributedTextBuilder {
    this.components.push(AttributedTextEntryType.PushOnTap, onTap);
    return this;
  }

  /**
   * Push an onLayout callback on the Style stack.
   */
  pushOnLayout(onLayout: AttributedTextOnLayout): AttributedTextBuilder {
    this.components.push(AttributedTextEntryType.PushOnLayout, onLayout);
    return this;
  }

  /**
   * Push an outline color on the Style stack.
   */
  pushOutlineColor(outlineColor: string): AttributedTextBuilder {
    this.components.push(AttributedTextEntryType.PushOutlineColor, outlineColor);
    return this;
  }

  /**
   * Push an outline width on the Style stack.
   */
  pushOutlineWidth(outlineWidth: number): AttributedTextBuilder {
    this.components.push(AttributedTextEntryType.PushOutlineWidth, outlineWidth);
    return this;
  }

  /**
   * Push an outer outline color on the Style stack.
   */
  pushOuterOutlineColor(outerOutlineColor: string): AttributedTextBuilder {
    this.components.push(AttributedTextEntryType.PushOuterOutlineColor, outerOutlineColor);
    return this;
  }

  /**
   * Push an outer outline width on the Style stack.
   */
  pushOuterOutlineWidth(outerOutlineWidth: number): AttributedTextBuilder {
    this.components.push(AttributedTextEntryType.PushOuterOutlineWidth, outerOutlineWidth);
    return this;
  }

  /**
   * Push an inline image attachment on the Style stack.
   * Used for embedding images (like rendered LaTeX) inline with text.
   */
  pushInlineImage(attachment: AttributedTextInlineImageAttachment): AttributedTextBuilder {
    this.components.push(AttributedTextEntryType.PushInlineImage, attachment);
    return this;
  }

  /**
   * Push a per-part animation transform on the Style stack.
   */
  pushAnimationTransform(animationTransform: AttributedTextAnimationTransform): AttributedTextBuilder {
    this.components.push(AttributedTextEntryType.PushAnimationTransform, animationTransform);
    return this;
  }

  /**
   * Append an inline image as an attributed text attachment.
   * The image will be rendered inline with surrounding text using native text layout
   * (NSTextAttachment on iOS, ReplacementSpan on Android).
   *
   * @param attachment The image attachment configuration including dimensions and image data
   * @param placeholderChar Character to use as placeholder in the text (default: U+FFFC Object Replacement Character)
   */
  appendInlineImage(attachment: AttributedTextInlineImageAttachment, placeholderChar: string = '\uFFFC'): AttributedTextBuilder {
    this.pushInlineImage(attachment);
    this.appendText(placeholderChar);
    this.pop();
    return this;
  }

  /**
   * Pop the Style that is on the top of the Style stack. Every pushFont/pushColor/pushTextDecoration
   * needs to have a matching pop() call.
   */
  pop(): AttributedTextBuilder {
    this.components.push(AttributedTextEntryType.Pop);
    return this;
  }

  private appendEntry(type: AttributedTextEntryType, value: string | AnyFunction | number) {
    this.components.push(type, value);
  }

  private pushStyle(attributes: AttributedTextAttributes): number {
    let popCount = 0;
    if (attributes.color) {
      this.pushColor(attributes.color);
      popCount++;
    }

    if (attributes.font) {
      this.pushFont(attributes.font);
      popCount++;
    }

    if (attributes.textDecoration) {
      this.pushTextDecoration(attributes.textDecoration);
      popCount++;
    }

    if (attributes.onTap) {
      this.pushOnTap(attributes.onTap);
      popCount++;
    }

    if (attributes.onLayout) {
      this.pushOnLayout(attributes.onLayout);
      popCount++;
    }

    if (attributes.outlineColor) {
      this.pushOutlineColor(attributes.outlineColor);
      popCount++;
    }

    if (attributes.outlineWidth) {
      this.pushOutlineWidth(attributes.outlineWidth);
      popCount++;
    }

    if (attributes.outerOutlineColor) {
      this.pushOuterOutlineColor(attributes.outerOutlineColor);
      popCount++;
    }

    if (attributes.outerOutlineWidth) {
      this.pushOuterOutlineWidth(attributes.outerOutlineWidth);
      popCount++;
    }

    if (attributes.animationTransform) {
      this.pushAnimationTransform(attributes.animationTransform);
      popCount++;
    }

    return popCount;
  }

  private popCount(count: number) {
    for (let i = 0; i < count; i++) {
      this.pop();
    }
  }

  build(): AttributedText {
    return [...this.components];
  }
}
