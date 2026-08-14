import { AttributesBinder } from '../attributes/AttributesBinder';
import { parseCssLength, parseNumber } from '../attributes/AttributeApplierHelpers';
import {
  AttributeApplier,
  AttributeApplierContext,
  ElementClass,
  LayoutAnimationSizeApplier,
} from '../core/ElementClass';
import {
  isAttributedText,
  ParsedAttributedText,
  registerAttributedTextLayouts,
  renderAttributedText,
  unregisterAttributedTextLayouts,
} from '../utils/parseAttributedText';
import { registerTextAnimationParticipant, unregisterTextAnimationParticipant } from '../utils/TextAnimationRegistry';
import { textShadowCssValue } from '../utils/textStyle';
import {
  assignStyles,
  AttributeApplierMap,
  createBaseLayoutItemElement,
  setFont,
  SYSTEM_FONT_FAMILY,
} from './ElementClassSupport';
import { ViewElementClass } from './ViewElementClass';

const TEXT_LINE_HEIGHT_STATE = '__labelElementClassLineHeightState';
const TEXT_CONTENT_ELEMENT_STATE = '__labelElementClassTextContentElementState';
const LAYOUT_DEPENDENT = true;

interface TextLineHeightState {
  lineHeight?: string;
  lineHeightMultiple?: number;
}

function getTextContentElement(element: HTMLElement, context: AttributeApplierContext): HTMLElement {
  return context.getState<HTMLElement>(TEXT_CONTENT_ELEMENT_STATE) ?? element;
}

function getTextLineHeightState(context: AttributeApplierContext): TextLineHeightState {
  let state = context.getState<TextLineHeightState>(TEXT_LINE_HEIGHT_STATE);
  if (!state) {
    state = {};
    context.setState(TEXT_LINE_HEIGHT_STATE, state);
  }
  return state;
}

function updateTextLineHeight(element: HTMLElement, context: AttributeApplierContext): void {
  const state = getTextLineHeightState(context);
  if (state.lineHeight !== undefined) {
    element.style.lineHeight = state.lineHeight;
  } else if (state.lineHeightMultiple !== undefined) {
    element.style.lineHeight = String(state.lineHeightMultiple);
  } else {
    element.style.lineHeight = '';
  }
}

function lineHeightAttributeApplier(): AttributeApplier {
  return {
    layoutDependent: true,
    apply(element, value, attributeName, context) {
      const state = getTextLineHeightState(context);
      state.lineHeight = parseCssLength(value, attributeName);
      updateTextLineHeight(element, context);
    },
    reset(element, _attributeName, context) {
      const state = getTextLineHeightState(context);
      state.lineHeight = undefined;
      updateTextLineHeight(element, context);
    },
  };
}

function lineHeightMultipleAttributeApplier(): AttributeApplier {
  return {
    layoutDependent: true,
    apply(element, value, attributeName, context) {
      const state = getTextLineHeightState(context);
      state.lineHeightMultiple = parseNumber(value, attributeName);
      updateTextLineHeight(element, context);
    },
    reset(element, _attributeName, context) {
      const state = getTextLineHeightState(context);
      state.lineHeightMultiple = undefined;
      updateTextLineHeight(element, context);
    },
  };
}

function applyTextShadow(element: HTMLElement, value: string, context: AttributeApplierContext): void {
  element.style.textShadow = textShadowCssValue(value, context) ?? '';
}

function labelValueAttributeApplier(): AttributeApplier {
  return {
    colorDependent: true,
    layoutDependent: true,
    apply(element, value, attributeName, context) {
      const textContentElement = getTextContentElement(element, context);
      if (isAttributedText(value)) {
        const parsedAttributedText = ParsedAttributedText.parse(value);
        const container = renderAttributedText(parsedAttributedText, context);
        textContentElement.replaceChildren(container);
        registerTextAnimationParticipant(element, container, context);
        registerAttributedTextLayouts(context, attributeName, parsedAttributedText, container);
        return;
      }
      unregisterTextAnimationParticipant(context);
      unregisterAttributedTextLayouts(context, attributeName);
      textContentElement.textContent = String(value);
    },
    reset(element, attributeName, context) {
      unregisterTextAnimationParticipant(context);
      unregisterAttributedTextLayouts(context, attributeName);
      getTextContentElement(element, context).textContent = '';
    },
  };
}

function textDecorationAttributeApplier(): AttributeApplier {
  return {
    apply(element, value, attributeName) {
      if (typeof value !== 'string') {
        throw new Error(`Expected '${attributeName}' to be a string`);
      }
      element.style.textDecorationLine = '';
      element.style.textDecorationStyle = '';
      switch (value) {
        case 'underline':
          element.style.textDecorationLine = 'underline';
          return;
        case 'dashed-underline':
          element.style.textDecorationLine = 'underline';
          element.style.textDecorationStyle = 'dashed';
          return;
        case 'dotted-underline':
          element.style.textDecorationLine = 'underline';
          element.style.textDecorationStyle = 'dotted';
          return;
        case 'strikethrough':
          element.style.textDecorationLine = 'line-through';
          return;
        case 'none':
        case '':
        default:
          element.style.textDecorationLine = 'none';
      }
    },
    reset(element) {
      element.style.textDecorationLine = '';
      element.style.textDecorationStyle = '';
    },
  };
}

function textAlignAttributeApplier(): AttributeApplier {
  const binder = new AttributesBinder<HTMLElement>();
  binder.bindStringAttribute(
    'textAlign',
    (element, value) => {
      element.style.textAlign = value === 'justified' ? 'justify' : value;
    },
    element => {
      element.style.textAlign = '';
    },
  );
  return binder.attributeAppliers.textAlign;
}

function buildTextAttributeAppliers(viewElementClass: ViewElementClass): AttributeApplierMap {
  const binder = new AttributesBinder<HTMLElement>();
  binder.bindAttribute('value', labelValueAttributeApplier());
  binder.bindAttribute('font', setFont());
  binder.bindAttribute('textAlign', textAlignAttributeApplier());
  binder.bindAttribute('textDecoration', textDecorationAttributeApplier());
  binder.bindStringAttribute(
    'textShadow',
    (element, value, context) => {
      applyTextShadow(element, value, context);
    },
    element => {
      element.style.textShadow = '';
    },
  );
  binder.bindAttribute('lineHeight', lineHeightAttributeApplier());
  binder.bindAttribute('lineHeightMultiple', lineHeightMultipleAttributeApplier());
  binder.bindCssLengthStyleAttribute('letterSpacing', 'letterSpacing', LAYOUT_DEPENDENT);
  binder.bindNumberAttribute(
    'numberOfLines',
    (element, value) => {
      if (value <= 0) {
        element.style.removeProperty('-webkit-line-clamp');
        element.style.overflow = '';
        return;
      }
      element.style.display = '-webkit-box';
      element.style.overflow = 'hidden';
      element.style.setProperty('-webkit-line-clamp', String(value));
      element.style.setProperty('-webkit-box-orient', 'vertical');
    },
    element => {
      element.style.removeProperty('-webkit-line-clamp');
      element.style.removeProperty('-webkit-box-orient');
      element.style.display = 'inline';
      element.style.overflow = '';
    },
    LAYOUT_DEPENDENT,
  );
  return { ...viewElementClass.attributeAppliers, ...binder.attributeAppliers };
}

export class LabelElementClass extends ElementClass {
  private textContentElementTemplate: HTMLElement | undefined;

  constructor(private readonly viewElementClass: ViewElementClass) {
    super('label', buildTextAttributeAppliers(viewElementClass), viewElementClass.compositeAttributes);
  }

  override getViewAttributeElement(element: HTMLElement, context: AttributeApplierContext): HTMLElement {
    this.getOrCreateTextContentElement(element, context);
    return this.viewElementClass.getViewAttributeElement(element, context);
  }

  override makeLayoutAnimationSizeApplier(
    element: HTMLElement,
    context: AttributeApplierContext,
    finalWidth: number,
    finalHeight: number,
  ): LayoutAnimationSizeApplier | undefined {
    return this.viewElementClass.makeLayoutAnimationSizeApplier(element, context, finalWidth, finalHeight);
  }

  protected onCreateElement(): HTMLElement {
    const element = createBaseLayoutItemElement('span');
    assignStyles(element, {
      display: 'inline',
      whiteSpace: 'pre-wrap',
      wordWrap: 'break-word',
      fontFamily: SYSTEM_FONT_FAMILY,
      color: 'black',
    });
    return element;
  }

  private getOrCreateTextContentElement(element: HTMLElement, context: AttributeApplierContext): HTMLElement {
    const existing = context.getState<HTMLElement>(TEXT_CONTENT_ELEMENT_STATE);
    if (existing) {
      return existing;
    }

    const textContentElement = this.getTextContentElementTemplate().cloneNode(false) as HTMLElement;
    const textContent = element.childNodes.length === 0 ? element.textContent : undefined;
    while (element.childNodes.length !== 0) {
      const child = element.childNodes.item(0)!;
      element.removeChild(child);
      textContentElement.appendChild(child);
    }
    if (textContent !== undefined) {
      textContentElement.textContent = textContent;
      element.textContent = '';
    }
    element.appendChild(textContentElement);
    context.setState(TEXT_CONTENT_ELEMENT_STATE, textContentElement);
    return textContentElement;
  }

  private getTextContentElementTemplate(): HTMLElement {
    if (!this.textContentElementTemplate) {
      const textContentElement = document.createElement('span');
      textContentElement.style.display = 'contents';
      this.textContentElementTemplate = textContentElement;
    }
    return this.textContentElementTemplate;
  }
}
