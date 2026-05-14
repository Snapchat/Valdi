import { AttributedText, AttributedTextOnTap } from 'valdi_tsx/src/AttributedText';
import { LabelTextDecoration } from 'valdi_tsx/src/NativeTemplateElements';
import { convertColor } from '../styles/ValdiWebStyles';

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
  PushInlineImage,
  PushAnimationTransform,
}

interface StyleState {
  font?: string;
  color?: string;
  textDecoration?: LabelTextDecoration;
  onTap?: AttributedTextOnTap;
  outlineColor?: string;
  outlineWidth?: number;
}

interface StyleStackEntry {
  type: keyof StyleState;
  value: any;
}

export function isAttributedText(value: any): value is AttributedText {
  return Array.isArray(value) && value.length > 0 && typeof value[0] === 'number';
}

export function renderAttributedText(attributedText: AttributedText): HTMLSpanElement {
  const container = document.createElement('span');
  const styleStack: StyleStackEntry[] = [];

  let i = 0;
  while (i < attributedText.length) {
    const entry = attributedText[i];

    if (typeof entry !== 'number') {
      i++;
      continue;
    }

    switch (entry) {
      case AttributedTextEntryType.Content: {
        const text = attributedText[i + 1] as string;
        i += 2;

        const style: StyleState = {};
        for (let j = styleStack.length - 1; j >= 0; j--) {
          const stackEntry = styleStack[j];
          if (style[stackEntry.type] === undefined) {
            style[stackEntry.type] = stackEntry.value;
          }
        }

        const span = createStyledSpan(text, style);
        container.appendChild(span);
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
      case AttributedTextEntryType.PushOnTap:
        styleStack.push({ type: 'onTap', value: attributedText[i + 1] });
        i += 2;
        break;
      case AttributedTextEntryType.PushOnLayout:
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
      case AttributedTextEntryType.PushOuterOutlineWidth:
      case AttributedTextEntryType.PushInlineImage:
      case AttributedTextEntryType.PushAnimationTransform:
        i += 2;
        break;
      default:
        i++;
        break;
    }
  }

  return container;
}

function createStyledSpan(text: string, style: StyleState): HTMLSpanElement {
  const span = document.createElement('span');
  span.textContent = text;

  if (style.color) {
    span.style.color = convertColor(style.color);
  }

  if (style.font) {
    span.style.fontFamily = style.font;
  }

  if (style.textDecoration === 'underline') {
    span.style.textDecoration = 'underline';
  } else if (style.textDecoration === 'strikethrough') {
    span.style.textDecoration = 'line-through';
  }

  if (style.outlineColor && style.outlineWidth) {
    const w = style.outlineWidth;
    span.style.textShadow = `-${w}px -${w}px 0 ${style.outlineColor}, ${w}px -${w}px 0 ${style.outlineColor}, -${w}px ${w}px 0 ${style.outlineColor}, ${w}px ${w}px 0 ${style.outlineColor}`;
  }

  if (style.onTap) {
    span.style.cursor = 'pointer';
    const onTap = style.onTap;
    span.onclick = (e) => {
      e.stopPropagation();
      onTap();
    };
  }

  return span;
}
