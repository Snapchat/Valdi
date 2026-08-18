import { Base64 } from 'coreutils/src/Base64';
import { AttributedText, AttributedTextOnLayout, AttributedTextOnTap } from 'valdi_tsx/src/AttributedText';
import { AttributedTextInlineImageAttachment } from 'valdi_tsx/src/AttributedTextInlineImageAttachment';
import {
  AttributedTextInlineViewAttachment,
  AttributedTextInlineViewVerticalAlignment,
} from 'valdi_tsx/src/AttributedTextInlineViewAttachment';
import { LabelTextDecoration } from 'valdi_tsx/src/NativeTemplateElements';
import type { AttributeApplierContext, ElementLayoutObserver } from '../core/ElementClass';
import { COLOR_PALETTE_MANAGER } from '../core/Palette';
import { applyFontString } from '../elements/ElementClassSupport';
import {
  markTextAnimationAttachmentSpan,
  NormalizedTextAnimationTransform,
  normalizeTextAnimationTransform,
  setTextAnimationTransform,
} from './TextAnimationTypes';

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
  animationTransform?: NormalizedTextAnimationTransform;
}

interface StyleStackEntry {
  type?: keyof StyleState;
  value?: any;
}

export interface AttributedTextPart {
  content: string;
  style: StyleState;
}

interface AttributedLayoutMeasurement {
  onLayout: AttributedTextOnLayout;
  span: Element;
  outlineWidth: number;
  x: number;
  y: number;
  width: number;
  height: number;
}

type ScheduleAttributedTextLayoutNotification = (callback: () => void) => void;

const ATTRIBUTED_TEXT_LAYOUT_OBSERVER_STATE_KEY = '__attributedTextLayoutObserver';

export function isAttributedText(value: any): value is AttributedText {
  return Array.isArray(value) && value.length > 0 && typeof value[0] === 'number';
}

export class ParsedAttributedText {
  static parse(attributedText: AttributedText): ParsedAttributedText {
    const parts: AttributedTextPart[] = [];
    const styleStack: StyleStackEntry[] = [];
    const animationPartCounts: number[] = [];
    let hasOnLayout = false;

    let i = 0;
    while (i < attributedText.length) {
      const entry = attributedText[i];

      if (typeof entry !== 'number') {
        i++;
        continue;
      }

      switch (entry) {
        case AttributedTextEntryType.Content: {
          const style = styleStateForPart(styleStack, animationPartCounts);
          hasOnLayout = hasOnLayout || !!style.onLayout;
          parts.push({ content: String(attributedText[i + 1] ?? ''), style });
          i += 2;
          break;
        }
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
        case AttributedTextEntryType.InlineImage: {
          const style = styleStateForPart(styleStack, animationPartCounts);
          hasOnLayout = hasOnLayout || !!style.onLayout;
          style.inlineImage = attributedText[i + 1] as AttributedTextInlineImageAttachment;
          parts.push({ content: '', style });
          i += 2;
          break;
        }
        case AttributedTextEntryType.InlineView: {
          const style = styleStateForPart(styleStack, animationPartCounts);
          hasOnLayout = hasOnLayout || !!style.onLayout;
          style.inlineView = attributedText[i + 1] as AttributedTextInlineViewAttachment;
          parts.push({ content: '', style });
          i += 2;
          break;
        }
        case AttributedTextEntryType.PushAnimationTransform:
          {
            const entryValue = attributedText[i + 1];
            const animationTransform = normalizeTextAnimationTransform(entryValue, animationPartCounts.length);
            if (animationTransform) {
              animationPartCounts.push(0);
              styleStack.push({ type: 'animationTransform', value: animationTransform });
            } else {
              logInvalidTextAnimationTransform(entryValue);
              styleStack.push({});
            }
          }
          i += 2;
          break;
        default:
          i++;
          break;
      }
    }

    return new ParsedAttributedText(parts, hasOnLayout);
  }

  constructor(
    readonly parts: AttributedTextPart[],
    readonly hasOnLayout: boolean,
  ) {}

  toString(): string {
    let out = '';
    for (const part of this.parts) {
      out += part.content;
    }
    return out;
  }
}

export function renderAttributedText(
  attributedText: ParsedAttributedText,
  context?: AttributeApplierContext,
): HTMLSpanElement {
  const container = document.createElement('span');
  for (const part of attributedText.parts) {
    container.appendChild(createStyledSpan(part.content, part.style, context));
  }

  return container;
}

function styleStateFromStack(styleStack: StyleStackEntry[]): StyleState {
  const style: StyleState = {};
  for (let i = styleStack.length - 1; i >= 0; i--) {
    const stackEntry = styleStack[i];
    if (!stackEntry.type) {
      continue;
    }
    if (style[stackEntry.type] === undefined) {
      style[stackEntry.type] = stackEntry.value;
    }
  }
  return style;
}

function styleStateForPart(styleStack: StyleStackEntry[], animationPartCounts: number[]): StyleState {
  const style = styleStateFromStack(styleStack);
  const animationTransform = style.animationTransform;
  if (animationTransform && animationTransform.groupIndex < animationPartCounts.length) {
    style.animationTransform = {
      ...animationTransform,
      partIndexInGroup: animationPartCounts[animationTransform.groupIndex]++,
    };
  }
  return style;
}

function logInvalidTextAnimationTransform(value: unknown): void {
  console.error('Invalid text animation transform: expected an object', value);
}

class AttributedTextLayoutObserver implements ElementLayoutObserver {
  private readonly measurements: AttributedLayoutMeasurement[] = [];
  private notificationScheduled = false;
  private readonly notifyLayouts = () => {
    this.notificationScheduled = false;
    for (let i = 0; i < this.measurements.length; i++) {
      const measurement = this.measurements[i];
      measurement.onLayout(measurement.x, measurement.y, measurement.width, measurement.height);
    }
  };

  constructor(
    attributedText: ParsedAttributedText,
    private readonly container: HTMLElement,
    private readonly relativeTo: HTMLElement | undefined,
    private readonly scheduleNotification: ScheduleAttributedTextLayoutNotification | undefined,
  ) {
    for (let i = 0; i < attributedText.parts.length; i++) {
      const style = attributedText.parts[i].style;
      const onLayout = style.onLayout;
      if (onLayout) {
        const outlineColor = style.outerOutlineColor ?? style.outlineColor;
        const outlineWidth = style.outerOutlineWidth ?? style.outlineWidth;
        this.measurements.push({
          onLayout,
          span: container.childNodes.item(i) as HTMLSpanElement,
          outlineWidth: outlineColor && outlineWidth ? outlineWidth : 0,
          x: 0,
          y: 0,
          width: 0,
          height: 0,
        });
      }
    }
  }

  onSizeChanged(_width: number, _height: number): void {
    const parentRect = (this.relativeTo ?? this.container).getBoundingClientRect();
    for (let i = 0; i < this.measurements.length; i++) {
      const measurement = this.measurements[i];
      const rect = measurement.span.getBoundingClientRect();
      measurement.x = rect.left - parentRect.left;
      measurement.y = rect.top - parentRect.top;
      measurement.width = Math.max(0, rect.width - measurement.outlineWidth * 2);
      measurement.height = rect.height;
    }
  }

  onCommit(_element: HTMLElement): void {
    if (!this.scheduleNotification) {
      this.notifyLayouts();
      return;
    }
    if (!this.notificationScheduled) {
      this.notificationScheduled = true;
      this.scheduleNotification(this.notifyLayouts);
    }
  }
}

export function dispatchAttributedTextLayouts(
  attributedText: ParsedAttributedText,
  container: HTMLElement,
  relativeTo?: HTMLElement,
): void {
  if (attributedText.hasOnLayout) {
    const observer = new AttributedTextLayoutObserver(attributedText, container, relativeTo, undefined);
    observer.onSizeChanged(0, 0);
    observer.onCommit(container);
  }
}

export function unregisterAttributedTextLayouts(context: AttributeApplierContext, attributeName: string): void {
  context.setState(ATTRIBUTED_TEXT_LAYOUT_OBSERVER_STATE_KEY, undefined);
  context.setLayoutObserver(attributeName, undefined);
}

export function registerAttributedTextLayouts(
  context: AttributeApplierContext,
  attributeName: string,
  attributedText: ParsedAttributedText,
  container: HTMLElement,
  relativeTo?: HTMLElement,
): void {
  if (!attributedText.hasOnLayout) {
    unregisterAttributedTextLayouts(context, attributeName);
    return;
  }

  let observer: AttributedTextLayoutObserver;
  observer = new AttributedTextLayoutObserver(attributedText, container, relativeTo, callback => {
    context.enqueuePostLayoutCallback(() => {
      if (context.getState<AttributedTextLayoutObserver>(ATTRIBUTED_TEXT_LAYOUT_OBSERVER_STATE_KEY) !== observer) {
        return;
      }
      try {
        callback();
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        console.error(`Valdi web renderer failed to notify attributed text layout on node ${context.id}: ${message}`);
      }
    });
  });
  context.setState(ATTRIBUTED_TEXT_LAYOUT_OBSERVER_STATE_KEY, observer);
  context.setLayoutObserver(attributeName, observer);
}

function backgroundPaddingToCss(
  padding: number | { left?: number; top?: number; right?: number; bottom?: number },
): string {
  if (typeof padding === 'number') {
    return `${padding}px`;
  }
  return `${padding.top ?? 0}px ${padding.right ?? 0}px ${padding.bottom ?? 0}px ${padding.left ?? 0}px`;
}

function backgroundBorderRadiusToCss(radius: number | string): string {
  return typeof radius === 'number' ? `${radius}px` : radius;
}

function applyTextDecoration(span: HTMLSpanElement, decoration: LabelTextDecoration | undefined): void {
  switch (decoration) {
    case 'underline':
      span.style.textDecorationLine = 'underline';
      break;
    case 'dashed-underline':
      span.style.textDecorationLine = 'underline';
      span.style.textDecorationStyle = 'dashed';
      break;
    case 'dotted-underline':
      span.style.textDecorationLine = 'underline';
      span.style.textDecorationStyle = 'dotted';
      break;
    case 'strikethrough':
      span.style.textDecorationLine = 'line-through';
      break;
    default:
      break;
  }
}

function applyOutline(
  span: HTMLSpanElement,
  color: string | undefined,
  width: number | undefined,
  context: AttributeApplierContext | undefined,
): void {
  if (!color || !width) {
    return;
  }
  span.style.webkitTextStroke = `${width}px ${convertColor(color, context)}`;
  span.style.paintOrder = 'stroke fill';
}

function applyInlineImage(span: HTMLSpanElement, attachment: AttributedTextInlineImageAttachment): void {
  markTextAnimationAttachmentSpan(span);
  span.textContent = '';
  const image = document.createElement('img');
  image.alt = attachment.attachmentId;
  image.style.display = 'inline-block';
  image.style.height = `${attachment.height}px`;
  image.style.verticalAlign = 'middle';
  image.style.width = `${attachment.width}px`;
  if (attachment.imageData) {
    image.src = `data:image/png;base64,${Base64.fromByteArray(attachment.imageData)}`;
  }
  span.appendChild(image);
}

function verticalAlignForInlineView(attachment: AttributedTextInlineViewAttachment): string {
  switch (attachment.verticalAlignment) {
    case AttributedTextInlineViewVerticalAlignment.Top:
      return 'top';
    case AttributedTextInlineViewVerticalAlignment.Bottom:
      return 'bottom';
    case AttributedTextInlineViewVerticalAlignment.Baseline:
      return 'baseline';
    case AttributedTextInlineViewVerticalAlignment.Center:
    default:
      return 'middle';
  }
}

function applyInlineView(
  span: HTMLSpanElement,
  attachment: AttributedTextInlineViewAttachment,
  context: AttributeApplierContext | undefined,
): void {
  markTextAnimationAttachmentSpan(span);
  span.textContent = '';
  span.style.alignItems = 'center';
  span.style.display = 'inline-flex';
  span.style.verticalAlign = verticalAlignForInlineView(attachment);
  const child = context?.getChildHtmlElement(attachment.childIndex);
  if (child) {
    span.appendChild(child);
  }
}

function createStyledSpan(
  text: string,
  style: StyleState,
  context: AttributeApplierContext | undefined,
): HTMLSpanElement {
  const span = document.createElement('span');
  span.textContent = text;

  if (style.inlineImage) {
    applyInlineImage(span, style.inlineImage);
  }

  if (style.inlineView) {
    applyInlineView(span, style.inlineView, context);
  }

  if (style.color) {
    span.style.color = convertColor(style.color, context);
  }

  if (style.font) {
    applyFontString(span, style.font, 'font');
  }

  if (style.backgroundColor) {
    span.style.backgroundColor = convertColor(style.backgroundColor, context);
    span.style.setProperty('box-decoration-break', 'clone');
    span.style.setProperty('-webkit-box-decoration-break', 'clone');
  }

  if (style.backgroundPadding !== undefined) {
    span.style.padding = backgroundPaddingToCss(style.backgroundPadding);
  }

  if (style.backgroundBorderRadius !== undefined) {
    span.style.borderRadius = backgroundBorderRadiusToCss(style.backgroundBorderRadius);
  }

  applyTextDecoration(span, style.textDecoration);
  applyOutline(
    span,
    style.outerOutlineColor ?? style.outlineColor,
    style.outerOutlineWidth ?? style.outlineWidth,
    context,
  );

  if (style.onTap) {
    span.style.cursor = 'pointer';
    const onTap = style.onTap;
    span.onclick = e => {
      e.stopPropagation();
      onTap();
    };
  }

  if (style.animationTransform) {
    setTextAnimationTransform(span, style.animationTransform);
  }

  return span;
}

function convertColor(color: string, context: AttributeApplierContext | undefined): string {
  return context
    ? context.resolveColor(color)
    : COLOR_PALETTE_MANAGER.resolveColor(COLOR_PALETTE_MANAGER.getActiveColorPaletteName(), color);
}
