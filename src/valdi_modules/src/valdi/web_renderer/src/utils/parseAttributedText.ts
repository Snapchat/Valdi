import type { AttributedText, AttributedTextOnLayout, AttributedTextOnTap } from 'valdi_tsx/src/AttributedText';
import type { AttributedTextInlineImageAttachment } from 'valdi_tsx/src/AttributedTextInlineImageAttachment';
import type { AttributedTextInlineViewAttachment } from 'valdi_tsx/src/AttributedTextInlineViewAttachment';
import type { LabelTextDecoration } from 'valdi_tsx/src/NativeTemplateElements';
import { convertColor } from '../styles/ValdiWebStyles';
import { applyFontString, applyTextDecoration, cssLength } from './textStyle';

const enum AttributedTextEntryType {
  Content = 1,
  Pop,
  PushFont,
  PushTextDecoration,
  PushColor,
  PushOnTap,
  PushOnLayout,
  PushOutlineColor,
  PushOutlineWidth,
  PushOuterOutlineColor,
  PushOuterOutlineWidth,
  InlineImage,
  PushAnimationTransform,
  PushBackgroundColor,
  PushBackgroundPadding,
  PushBackgroundBorderRadius,
  InlineView,
}

export interface StyleState {
  font?: string;
  color?: string;
  backgroundColor?: string;
  backgroundPadding?: number | { left?: number; top?: number; right?: number; bottom?: number };
  backgroundBorderRadius?: number | string;
  textDecoration?: LabelTextDecoration;
  onTap?: AttributedTextOnTap;
  onLayout?: AttributedTextOnLayout;
  outlineColor?: string;
  outlineWidth?: number;
  outerOutlineColor?: string;
  outerOutlineWidth?: number;
  inlineImage?: AttributedTextInlineImageAttachment;
  inlineView?: AttributedTextInlineViewAttachment;
}

interface StyleStackEntry {
  type: keyof StyleState;
  value: any;
}

export interface AttributedTextPart {
  content: string;
  style: StyleState;
}

export interface RenderAttributedTextOptions {
  getInlineChild?: (index: number) => HTMLElement | undefined;
}

const ATTRIBUTED_ON_LAYOUT_KEY = '__valdiAttributedOnLayout';
const ATTRIBUTED_OUTLINE_WIDTH_KEY = '__valdiAttributedOutlineWidth';
type AttributedLayoutSpan = HTMLSpanElement & {
  [ATTRIBUTED_ON_LAYOUT_KEY]?: AttributedTextOnLayout;
  [ATTRIBUTED_OUTLINE_WIDTH_KEY]?: number;
};

export function isAttributedText(value: any): value is AttributedText {
  return Array.isArray(value) && value.length > 0 && typeof value[0] === 'number';
}

export class ParsedAttributedText {
  static parse(attributedText: AttributedText): ParsedAttributedText {
    const parts: AttributedTextPart[] = [];
    const styleStack: StyleStackEntry[] = [];

    let i = 0;
    while (i < attributedText.length) {
      const entry = attributedText[i];

      if (typeof entry !== 'number') {
        i++;
        continue;
      }

      switch (entry) {
        case AttributedTextEntryType.Content:
          parts.push({
            content: String(attributedText[i + 1] ?? ''),
            style: styleStateFromStack(styleStack),
          });
          i += 2;
          break;
        case AttributedTextEntryType.Pop:
          styleStack.pop();
          i++;
          break;
        case AttributedTextEntryType.PushFont:
          styleStack.push({ type: 'font', value: attributedText[i + 1] });
          i += 2;
          break;
        case AttributedTextEntryType.PushTextDecoration:
          styleStack.push({ type: 'textDecoration', value: attributedText[i + 1] });
          i += 2;
          break;
        case AttributedTextEntryType.PushColor:
          styleStack.push({ type: 'color', value: attributedText[i + 1] });
          i += 2;
          break;
        case AttributedTextEntryType.PushBackgroundColor:
          styleStack.push({ type: 'backgroundColor', value: attributedText[i + 1] });
          i += 2;
          break;
        case AttributedTextEntryType.PushBackgroundPadding:
          styleStack.push({ type: 'backgroundPadding', value: attributedText[i + 1] });
          i += 2;
          break;
        case AttributedTextEntryType.PushBackgroundBorderRadius:
          styleStack.push({ type: 'backgroundBorderRadius', value: attributedText[i + 1] });
          i += 2;
          break;
        case AttributedTextEntryType.PushOnTap:
          styleStack.push({ type: 'onTap', value: attributedText[i + 1] });
          i += 2;
          break;
        case AttributedTextEntryType.PushOnLayout:
          styleStack.push({ type: 'onLayout', value: attributedText[i + 1] });
          i += 2;
          break;
        case AttributedTextEntryType.PushOutlineColor:
          styleStack.push({ type: 'outlineColor', value: attributedText[i + 1] });
          i += 2;
          break;
        case AttributedTextEntryType.PushOutlineWidth:
          styleStack.push({ type: 'outlineWidth', value: attributedText[i + 1] });
          i += 2;
          break;
        case AttributedTextEntryType.PushOuterOutlineColor:
          styleStack.push({ type: 'outerOutlineColor', value: attributedText[i + 1] });
          i += 2;
          break;
        case AttributedTextEntryType.PushOuterOutlineWidth:
          styleStack.push({ type: 'outerOutlineWidth', value: attributedText[i + 1] });
          i += 2;
          break;
        case AttributedTextEntryType.InlineImage:
          parts.push({
            content: '',
            style: {
              ...styleStateFromStack(styleStack),
              inlineImage: attributedText[i + 1] as AttributedTextInlineImageAttachment,
            },
          });
          i += 2;
          break;
        case AttributedTextEntryType.InlineView:
          parts.push({
            content: '',
            style: {
              ...styleStateFromStack(styleStack),
              inlineView: attributedText[i + 1] as AttributedTextInlineViewAttachment,
            },
          });
          i += 2;
          break;
        case AttributedTextEntryType.PushAnimationTransform:
          i += 2;
          break;
        default:
          i++;
          break;
      }
    }

    return new ParsedAttributedText(parts);
  }

  constructor(readonly parts: AttributedTextPart[]) {}

  toString(): string {
    let out = '';
    for (const part of this.parts) {
      out += part.content;
    }
    return out;
  }
}

export function renderAttributedText(
  attributedText: AttributedText | ParsedAttributedText,
  options: RenderAttributedTextOptions = {},
): HTMLSpanElement {
  const parsed =
    attributedText instanceof ParsedAttributedText ? attributedText : ParsedAttributedText.parse(attributedText);
  const container = document.createElement('span');
  for (const part of parsed.parts) {
    container.appendChild(createStyledSpan(part.content, part.style, options));
  }
  scheduleAttributedTextLayouts(container);
  return container;
}

function styleStateFromStack(styleStack: StyleStackEntry[]): StyleState {
  const style: StyleState = {};
  for (let i = styleStack.length - 1; i >= 0; i--) {
    const stackEntry = styleStack[i];
    if (style[stackEntry.type] === undefined) {
      style[stackEntry.type] = stackEntry.value;
    }
  }
  return style;
}

export function dispatchAttributedTextLayouts(container: HTMLElement, relativeTo?: HTMLElement): void {
  const parentRect = (relativeTo ?? container).getBoundingClientRect();
  const spans = container.querySelectorAll('span');
  for (let i = 0; i < spans.length; i++) {
    const span = spans[i] as AttributedLayoutSpan;
    const onLayout = span[ATTRIBUTED_ON_LAYOUT_KEY];
    if (!onLayout) {
      continue;
    }
    const rect = span.getBoundingClientRect();
    const outlineWidth = span[ATTRIBUTED_OUTLINE_WIDTH_KEY] ?? 0;
    onLayout(
      rect.left - parentRect.left,
      rect.top - parentRect.top,
      Math.max(0, rect.width - outlineWidth * 2),
      rect.height,
    );
  }
}

function scheduleAttributedTextLayouts(container: HTMLElement): void {
  if (typeof requestAnimationFrame !== 'function') {
    return;
  }
  requestAnimationFrame(() => dispatchAttributedTextLayouts(container));
}

function backgroundPaddingToCss(
  padding: number | { left?: number; top?: number; right?: number; bottom?: number },
): string {
  if (typeof padding === 'number') {
    return `${padding}px`;
  }
  return `${padding.top ?? 0}px ${padding.right ?? 0}px ${padding.bottom ?? 0}px ${padding.left ?? 0}px`;
}

function bytesToBase64(bytes: Uint8Array): string {
  const BufferCtor = (globalThis as any).Buffer;
  if (BufferCtor) {
    return BufferCtor.from(bytes).toString('base64');
  }
  let binary = '';
  for (let i = 0; i < bytes.length; i++) {
    binary += String.fromCharCode(bytes[i]);
  }
  return btoa(binary);
}

function applyOutline(span: HTMLSpanElement, color: string | undefined, width: number | undefined): void {
  if (!color || !width) {
    return;
  }
  span.style.webkitTextStroke = `${width}px ${convertColor(color)}`;
  span.style.paintOrder = 'stroke fill';
  (span as AttributedLayoutSpan)[ATTRIBUTED_OUTLINE_WIDTH_KEY] = width;
}

function applyInlineImage(span: HTMLSpanElement, attachment: AttributedTextInlineImageAttachment): void {
  span.textContent = '';
  const image = document.createElement('img');
  image.alt = attachment.attachmentId;
  image.style.display = 'inline-block';
  image.style.height = `${attachment.height}px`;
  image.style.verticalAlign = 'middle';
  image.style.width = `${attachment.width}px`;
  if (attachment.imageData) {
    image.src = `data:image/png;base64,${bytesToBase64(attachment.imageData)}`;
  }
  span.appendChild(image);
}

function verticalAlignForInlineView(attachment: AttributedTextInlineViewAttachment): string {
  switch (attachment.verticalAlignment) {
    case 1:
      return 'top';
    case 2:
      return 'bottom';
    case 3:
      return 'baseline';
    case 0:
    default:
      return 'middle';
  }
}

function applyInlineView(
  span: HTMLSpanElement,
  attachment: AttributedTextInlineViewAttachment,
  options: RenderAttributedTextOptions,
): void {
  span.textContent = '';
  span.style.alignItems = 'center';
  span.style.display = 'inline-flex';
  span.style.verticalAlign = verticalAlignForInlineView(attachment);
  const child = options.getInlineChild?.(attachment.childIndex);
  if (child) {
    span.appendChild(child);
  }
}

function createStyledSpan(text: string, style: StyleState, options: RenderAttributedTextOptions): HTMLSpanElement {
  const span = document.createElement('span') as AttributedLayoutSpan;
  span.textContent = text;

  if (style.inlineImage) {
    applyInlineImage(span, style.inlineImage);
  }

  if (style.inlineView) {
    applyInlineView(span, style.inlineView, options);
  }

  if (style.color) {
    span.style.color = convertColor(style.color);
  }

  if (style.font) {
    applyFontString(span, style.font);
  }

  if (style.backgroundColor) {
    span.style.backgroundColor = convertColor(style.backgroundColor);
    span.style.setProperty('box-decoration-break', 'clone');
    span.style.setProperty('-webkit-box-decoration-break', 'clone');
  }

  if (style.backgroundPadding !== undefined) {
    span.style.padding = backgroundPaddingToCss(style.backgroundPadding);
  }

  if (style.backgroundBorderRadius !== undefined) {
    span.style.borderRadius = cssLength(style.backgroundBorderRadius);
  }

  applyTextDecoration(span, style.textDecoration);
  applyOutline(span, style.outerOutlineColor ?? style.outlineColor, style.outerOutlineWidth ?? style.outlineWidth);

  if (style.onTap) {
    span.style.cursor = 'pointer';
    const onTap = style.onTap;
    span.onclick = e => {
      e.stopPropagation();
      onTap();
    };
  }

  if (style.onLayout) {
    span[ATTRIBUTED_ON_LAYOUT_KEY] = style.onLayout;
  }

  return span;
}
