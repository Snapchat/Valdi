import { AttributesBinder } from '../attributes/AttributesBinder';
import { parseCssLength, parseString } from '../attributes/AttributeApplierHelpers';
import { AttributeApplier, AttributeApplierContext, CompositeAttribute, ElementClass } from '../core/ElementClass';
import {
  isAttributedText,
  ParsedAttributedText,
  registerAttributedTextLayouts,
  renderAttributedText,
  unregisterAttributedTextLayouts,
} from '../utils/parseAttributedText';
import { registerTextAnimationParticipant, unregisterTextAnimationParticipant } from '../utils/TextAnimationRegistry';
import {
  assignStyles,
  AttributeApplierMap,
  getActiveElement,
  replaceEventListener,
  setApplierCleanup,
  SYSTEM_FONT_FAMILY,
} from './ElementClassSupport';
import { TextFieldElementClass } from './TextFieldElementClass';

type TextViewElement = HTMLDivElement & {
  disabled?: boolean;
  select?: () => void;
  selectionStart?: number;
  selectionEnd?: number;
  setSelectionRange?: (selectionStart: number, selectionEnd: number) => void;
  value?: string;
};

const TEXT_VIEW_STATE = '__textViewElementClassState';
const LAYOUT_DEPENDENT = true;

interface TextViewState {
  backgroundEffectBorderRadius?: string;
  backgroundEffectColor?: string;
  backgroundEffectPadding?: string;
  backgroundEffectPaddingPx?: number;
  returnType: string;
  selectionEnd: number;
  selectionStart: number;
  value: unknown;
}

function getTextViewState(element: TextViewElement, context: AttributeApplierContext): TextViewState {
  let state = context.getState<TextViewState>(TEXT_VIEW_STATE);
  if (!state) {
    state = {
      returnType: 'linereturn',
      selectionEnd: 0,
      selectionStart: 0,
      value: element.value ?? '',
    };
    context.setState(TEXT_VIEW_STATE, state);
  }
  return state;
}

function plainTextValue(value: unknown): string {
  if (isAttributedText(value)) {
    return ParsedAttributedText.parse(value).toString();
  }
  return value === undefined || value === null ? '' : String(value);
}

function textNodeLength(node: Node): number {
  if (node.nodeType === 3) {
    return node.textContent?.length ?? 0;
  }
  let length = 0;
  for (let i = 0; i < node.childNodes.length; i++) {
    length += textNodeLength(node.childNodes.item(i)!);
  }
  return length;
}

function textOffsetForNode(root: Node, target: Node, targetOffset: number): number {
  let offset = 0;
  const visit = (node: Node): boolean => {
    if (node === target) {
      if (node.nodeType === 3) {
        offset += Math.min(targetOffset, node.textContent?.length ?? 0);
      } else {
        for (let i = 0; i < Math.min(targetOffset, node.childNodes.length); i++) {
          offset += textNodeLength(node.childNodes.item(i)!);
        }
      }
      return true;
    }
    if (node.nodeType === 3) {
      offset += node.textContent?.length ?? 0;
      return false;
    }
    for (let i = 0; i < node.childNodes.length; i++) {
      if (visit(node.childNodes.item(i)!)) {
        return true;
      }
    }
    return false;
  };
  visit(root);
  return offset;
}

function findTextPosition(root: Node, targetOffset: number): { node: Node; offset: number } {
  let remaining = Math.max(0, targetOffset);
  let lastTextNode: Node | undefined;
  const visit = (node: Node): { node: Node; offset: number } | undefined => {
    if (node.nodeType === 3) {
      lastTextNode = node;
      const length = node.textContent?.length ?? 0;
      if (remaining <= length) {
        return { node, offset: remaining };
      }
      remaining -= length;
      return undefined;
    }
    for (let i = 0; i < node.childNodes.length; i++) {
      const found = visit(node.childNodes.item(i)!);
      if (found) {
        return found;
      }
    }
    return undefined;
  };
  return visit(root) ?? { node: lastTextNode ?? root, offset: lastTextNode ? textNodeLength(lastTextNode) : 0 };
}

function applyTextViewSelection(element: TextViewElement, selectionStart: number, selectionEnd: number): void {
  element.selectionStart = selectionStart;
  element.selectionEnd = selectionEnd;
  if (typeof document.createRange !== 'function' || typeof document.getSelection !== 'function') {
    return;
  }
  if (!element.firstChild && typeof document.createTextNode === 'function') {
    element.appendChild(document.createTextNode(''));
  }
  const start = findTextPosition(element, selectionStart);
  const end = findTextPosition(element, selectionEnd);
  const range = document.createRange();
  range.setStart(start.node, start.offset);
  range.setEnd(end.node, end.offset);
  const selection = document.getSelection();
  selection?.removeAllRanges();
  selection?.addRange(range);
}

function syncTextViewSelectionFromDom(element: TextViewElement, context: AttributeApplierContext): void {
  if (typeof document.getSelection !== 'function') {
    return;
  }
  const selection = document.getSelection();
  if (!selection || !selection.anchorNode || !selection.focusNode) {
    return;
  }
  if (!element.contains(selection.anchorNode) || !element.contains(selection.focusNode)) {
    return;
  }
  const anchorOffset = textOffsetForNode(element, selection.anchorNode, selection.anchorOffset);
  const focusOffset = textOffsetForNode(element, selection.focusNode, selection.focusOffset);
  const state = getTextViewState(element, context);
  state.selectionStart = Math.min(anchorOffset, focusOffset);
  state.selectionEnd = Math.max(anchorOffset, focusOffset);
  element.selectionStart = state.selectionStart;
  element.selectionEnd = state.selectionEnd;
}

function syncTextViewValueFromDom(element: TextViewElement, context: AttributeApplierContext): string {
  const text = element.textContent ?? '';
  const state = getTextViewState(element, context);
  state.value = text;
  element.value = text;
  updateTextViewPlaceholderVisibility(element, text);
  syncTextViewSelectionFromDom(element, context);
  return text;
}

function normalizeTextViewLineBreakInput(
  element: TextViewElement,
  context: AttributeApplierContext,
  inputEvent: InputEvent,
): void {
  if (inputEvent.inputType !== 'insertLineBreak') {
    return;
  }

  const state = getTextViewState(element, context);
  const previousText = plainTextValue(state.value);
  if (element.textContent !== `${previousText}\n\n`) {
    return;
  }

  const selection = typeof document.getSelection === 'function' ? document.getSelection() : null;
  const preserveSelection =
    selection !== null &&
    selection.anchorNode !== null &&
    selection.focusNode !== null &&
    element.contains(selection.anchorNode) &&
    element.contains(selection.focusNode);
  if (preserveSelection) {
    syncTextViewSelectionFromDom(element, context);
  }
  const selectionStart = state.selectionStart;
  const selectionEnd = state.selectionEnd;
  const trailingNode = element.childNodes.item(element.childNodes.length - 1);

  if (trailingNode?.nodeType === 3 && trailingNode.textContent === '\n') {
    element.insertBefore(document.createElement('br'), trailingNode);
    element.removeChild(trailingNode);
  } else {
    element.textContent = `${previousText}\n`;
  }

  if (preserveSelection) {
    const textLength = previousText.length + 1;
    applyTextViewSelection(element, Math.min(selectionStart, textLength), Math.min(selectionEnd, textLength));
  }
}

function updateTextViewPlaceholderVisibility(element: TextViewElement, text: string): void {
  if (element.getAttribute('placeholder') === null) {
    return;
  }
  if (text.length === 0) {
    element.setAttribute('data-valdi-empty', 'true');
    return;
  }
  element.removeAttribute('data-valdi-empty');
}

function setTextViewPlaceholder(
  element: TextViewElement,
  context: AttributeApplierContext,
  placeholder: string | undefined,
): void {
  if (!placeholder) {
    element.removeAttribute('aria-placeholder');
    element.removeAttribute('data-valdi-empty');
    element.removeAttribute('placeholder');
    return;
  }

  element.setAttribute('aria-placeholder', placeholder);
  element.setAttribute('placeholder', placeholder);
  updateTextViewPlaceholderVisibility(element, plainTextValue(getTextViewState(element, context).value));
}

function textViewEditEvent(
  element: TextViewElement,
  context: AttributeApplierContext,
): { text: string; selectionStart: number; selectionEnd: number } {
  const text = syncTextViewValueFromDom(element, context);
  const state = getTextViewState(element, context);
  return {
    text,
    selectionStart: state.selectionStart,
    selectionEnd: state.selectionEnd,
  };
}

function applyBackgroundEffect(span: HTMLSpanElement, state: TextViewState): void {
  if (!state.backgroundEffectColor) {
    return;
  }
  const verticalPadding = backgroundEffectVerticalPadding(state);
  const horizontalPadding = state.backgroundEffectPadding;
  span.style.backgroundColor = state.backgroundEffectColor;
  span.style.setProperty('box-decoration-break', 'clone');
  span.style.setProperty('-webkit-box-decoration-break', 'clone');
  if (verticalPadding || horizontalPadding) {
    span.style.padding = `${verticalPadding ?? '0'} ${horizontalPadding ?? '0'}`;
  }
  if (horizontalPadding) {
    span.style.marginLeft = `-${horizontalPadding}`;
    span.style.marginRight = `-${horizontalPadding}`;
  }
  span.style.position = 'relative';
  if (state.backgroundEffectBorderRadius) {
    span.style.borderRadius = state.backgroundEffectBorderRadius;
  }
}

function backgroundEffectVerticalPadding(state: TextViewState): string | undefined {
  if (state.backgroundEffectPaddingPx === undefined) {
    return undefined;
  }
  return `${state.backgroundEffectPaddingPx / 2}px`;
}

function wrapBackgroundEffectContent(content: HTMLElement, state: TextViewState): HTMLElement {
  const verticalPadding = backgroundEffectVerticalPadding(state);
  const wrapper = document.createElement('span');
  wrapper.style.display = 'block';
  wrapper.style.position = 'relative';
  wrapper.style.whiteSpace = 'inherit';
  wrapper.style.width = '100%';
  if (state.backgroundEffectPadding || verticalPadding) {
    wrapper.style.padding = `${verticalPadding ?? '0'} ${state.backgroundEffectPadding ?? '0'}`;
  }
  wrapper.appendChild(content);
  return wrapper;
}

function renderTextViewContent(
  element: TextViewElement,
  context: AttributeApplierContext,
  attributeName: string,
): void {
  const state = getTextViewState(element, context);
  const parsedAttributedText = isAttributedText(state.value) ? ParsedAttributedText.parse(state.value) : undefined;
  const text = parsedAttributedText ? parsedAttributedText.toString() : plainTextValue(state.value);
  element.value = text;
  updateTextViewPlaceholderVisibility(element, text);
  element.replaceChildren();

  if (parsedAttributedText) {
    const container = renderAttributedText(parsedAttributedText, context);
    applyBackgroundEffect(container, state);
    const renderedContent = state.backgroundEffectColor ? wrapBackgroundEffectContent(container, state) : container;
    element.appendChild(renderedContent);
    registerTextAnimationParticipant(element, container, context);
    registerAttributedTextLayouts(context, attributeName, parsedAttributedText, container, renderedContent);
    return;
  }

  unregisterTextAnimationParticipant(context);
  unregisterAttributedTextLayouts(context, attributeName);
  if (state.backgroundEffectColor) {
    const span = document.createElement('span');
    span.textContent = text;
    applyBackgroundEffect(span, state);
    element.appendChild(wrapBackgroundEffectContent(span, state));
    return;
  }

  element.textContent = text;
}

const textViewContentComposite: CompositeAttribute<TextViewElement> = {
  name: 'textViewContent',
  parts: [
    { name: 'value', optional: true, colorDependent: true, layoutDependent: true },
    {
      name: 'backgroundEffectBorderRadius',
      optional: true,
      parse: (_element, value, name) => parseCssLength(value, name),
    },
    {
      name: 'backgroundEffectColor',
      optional: true,
      colorDependent: true,
      parse: (_element, value, name, context) => context.resolveColor(parseString(value, name)),
    },
    {
      name: 'backgroundEffectPadding',
      optional: true,
      layoutDependent: true,
      parse: (_element, value, name) => parseCssLength(value, name),
    },
  ],
  apply(element, values, attributeName, context) {
    const state = getTextViewState(element, context);
    state.value = values[0] ?? '';
    state.backgroundEffectBorderRadius = values[1] as string | undefined;
    state.backgroundEffectColor = values[2] as string | undefined;
    state.backgroundEffectPadding = values[3] as string | undefined;
    const numericPadding = Number.parseFloat(state.backgroundEffectPadding ?? '');
    state.backgroundEffectPaddingPx = Number.isFinite(numericPadding) ? numericPadding : undefined;
    renderTextViewContent(element, context, attributeName);
  },
  reset(element, attributeName, context) {
    const state = getTextViewState(element, context);
    state.value = '';
    state.backgroundEffectBorderRadius = undefined;
    state.backgroundEffectColor = undefined;
    state.backgroundEffectPadding = undefined;
    state.backgroundEffectPaddingPx = undefined;
    renderTextViewContent(element, context, attributeName);
  },
};

function enabledAttributeApplier(): AttributeApplier<TextViewElement> {
  const binder = new AttributesBinder<TextViewElement>();
  binder.bindBooleanAttribute(
    'enabled',
    (element, enabled) => {
      element.disabled = !enabled;
      element.setAttribute('aria-disabled', String(!enabled));
      element.contentEditable = enabled ? 'plaintext-only' : 'false';
    },
    element => {
      element.disabled = false;
      element.removeAttribute('aria-disabled');
      element.contentEditable = 'plaintext-only';
    },
  );
  return binder.attributeAppliers.enabled;
}

function selectableAttributeApplier(): AttributeApplier<TextViewElement> {
  const binder = new AttributesBinder<TextViewElement>();
  binder.bindBooleanAttribute(
    'selectable',
    (element, selectable) => {
      element.style.userSelect = selectable === false ? 'none' : 'text';
    },
    element => {
      element.style.userSelect = 'text';
    },
  );
  return binder.attributeAppliers.selectable;
}

function focusedAttributeApplier(): AttributeApplier<TextViewElement> {
  const binder = new AttributesBinder<TextViewElement>();
  binder.bindBooleanAttribute(
    'focused',
    (element, focused) => {
      if (focused) {
        element.focus();
      } else {
        element.blur();
      }
    },
    element => {
      element.blur();
    },
  );
  return binder.attributeAppliers.focused;
}

function selectTextOnFocusAttributeApplier(): AttributeApplier<TextViewElement> {
  const binder = new AttributesBinder<TextViewElement>();
  binder.bindBooleanAttribute(
    'selectTextOnFocus',
    (element, value, context) => {
      if (!value) {
        replaceEventListener(element, context, 'textview:selectTextOnFocus', 'focus', undefined);
        return;
      }
      replaceEventListener(element, context, 'textview:selectTextOnFocus', 'focus', () => {
        element.select?.();
      });
    },
    (element, context) => replaceEventListener(element, context, 'textview:selectTextOnFocus', 'focus', undefined),
  );
  return binder.attributeAppliers.selectTextOnFocus;
}

function selectionAttributeApplier(): AttributeApplier<TextViewElement> {
  return {
    apply(element, value, _attributeName, context) {
      if (!Array.isArray(value) || value.length !== 2) {
        throw new Error('Expected selection to be a two item array');
      }
      applyTextViewSelection(element, Number(value[0]), Number(value[1]));
      const state = getTextViewState(element, context);
      state.selectionStart = Number(value[0]);
      state.selectionEnd = Number(value[1]);
    },
    reset(element, _attributeName, context) {
      applyTextViewSelection(element, 0, 0);
      const state = getTextViewState(element, context);
      state.selectionStart = 0;
      state.selectionEnd = 0;
    },
  };
}

function textGravityAttributeApplier(): AttributeApplier<TextViewElement> {
  const binder = new AttributesBinder<TextViewElement>();
  binder.bindStringAttribute(
    'textGravity',
    (element, value) => {
      element.style.alignContent = value === 'bottom' ? 'end' : value;
    },
    element => {
      element.style.alignContent = '';
    },
  );
  return binder.attributeAppliers.textGravity;
}

function onSelectionChangeAttributeApplier(): AttributeApplier<TextViewElement> {
  return {
    apply(element, value, attributeName, context) {
      if (typeof value !== 'function') {
        throw new Error(`Expected '${attributeName}' to be a function`);
      }
      const listener = () => {
        if (getActiveElement(element) !== element) {
          return;
        }
        syncTextViewSelectionFromDom(element, context);
        const state = getTextViewState(element, context);
        value({
          selectionEnd: state.selectionEnd,
          selectionStart: state.selectionStart,
          text: plainTextValue(state.value),
        });
      };
      document.addEventListener('selectionchange', listener);
      setApplierCleanup(context, 'textview:onSelectionChange', () => {
        document.removeEventListener('selectionchange', listener);
      });
    },
    reset(_element, _attributeName, context) {
      setApplierCleanup(context, 'textview:onSelectionChange', undefined);
    },
  };
}

function bindTextViewEventAttributes(binder: AttributesBinder<TextViewElement>): void {
  binder.bindFunctionAttribute(
    'onWillChange',
    (element, callback, context) => {
      replaceEventListener(element, context, 'textview:onWillChange', 'beforeinput', event => {
        if (callback(textViewEditEvent(element, context)) === false) {
          event.preventDefault();
        }
      });
    },
    (element, context) => replaceEventListener(element, context, 'textview:onWillChange', 'beforeinput', undefined),
  );
  binder.bindFunctionAttribute(
    'onChange',
    (element, callback, context) => {
      replaceEventListener(element, context, 'textview:onChange', 'input', (inputEvent: InputEvent) => {
        normalizeTextViewLineBreakInput(element, context, inputEvent);
        const event = textViewEditEvent(element, context);
        context.onAttributeUpdatedExternally('value', event.text);
        callback(event);
      });
    },
    (element, context) => replaceEventListener(element, context, 'textview:onChange', 'input', undefined),
  );
  binder.bindFunctionAttribute(
    'onEditBegin',
    (element, callback, context) => {
      replaceEventListener(element, context, 'textview:onEditBegin', 'focus', () =>
        callback(textViewEditEvent(element, context)),
      );
    },
    (element, context) => replaceEventListener(element, context, 'textview:onEditBegin', 'focus', undefined),
  );
  binder.bindFunctionAttribute(
    'onEditEnd',
    (element, callback, context) => {
      replaceEventListener(element, context, 'textview:onEditEnd', 'blur', () =>
        callback({ ...textViewEditEvent(element, context), reason: 'blur' }),
      );
    },
    (element, context) => replaceEventListener(element, context, 'textview:onEditEnd', 'blur', undefined),
  );
  binder.bindFunctionAttribute(
    'onReturn',
    (element, callback, context) => {
      replaceEventListener(element, context, 'textview:onReturn', 'keydown', event => {
        if (event.key === 'Enter') {
          if (getTextViewState(element, context).returnType !== 'linereturn') {
            event.preventDefault();
          }
          callback(textViewEditEvent(element, context));
        }
      });
    },
    (element, context) => replaceEventListener(element, context, 'textview:onReturn', 'keydown', undefined),
  );
  binder.bindFunctionAttribute(
    'onWillDelete',
    (element, callback, context) => {
      replaceEventListener(element, context, 'textview:onWillDelete', 'keydown', event => {
        if (event.key === 'Backspace' || event.key === 'Delete') {
          callback(textViewEditEvent(element, context));
        }
      });
    },
    (element, context) => replaceEventListener(element, context, 'textview:onWillDelete', 'keydown', undefined),
  );
}

function numberOfLinesAttributeApplier(): AttributeApplier<TextViewElement> {
  const binder = new AttributesBinder<TextViewElement>();
  binder.bindNumberAttribute(
    'numberOfLines',
    (element, value) => {
      if (value <= 0) {
        element.style.removeProperty('-webkit-line-clamp');
        element.style.removeProperty('-webkit-box-orient');
        element.style.display = '';
        element.style.overflow = 'hidden';
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
      element.style.display = '';
      element.style.overflow = 'hidden';
    },
    LAYOUT_DEPENDENT,
  );
  return binder.attributeAppliers.numberOfLines;
}

function buildTextViewAttributeAppliers(
  textFieldElementClass: TextFieldElementClass,
): AttributeApplierMap<TextViewElement> {
  const binder = new AttributesBinder<TextViewElement>();
  bindTextViewEventAttributes(binder);
  binder.bindNoOpAttribute('contentType');
  binder.bindAttribute('enabled', enabledAttributeApplier());
  binder.bindAttribute('focused', focusedAttributeApplier());
  binder.bindNoOpAttribute('keyboardAppearance');
  binder.bindNoOpAttribute('keyboardType');
  binder.bindAttribute('numberOfLines', numberOfLinesAttributeApplier());
  binder.bindAttribute('onSelectionChange', onSelectionChangeAttributeApplier());
  binder.bindStringAttribute(
    'placeholder',
    (element, value, context) => setTextViewPlaceholder(element, context, value),
    (element, context) => setTextViewPlaceholder(element, context, undefined),
    LAYOUT_DEPENDENT,
  );
  binder.bindColorAttribute(
    'placeholderColor',
    (element, value) => element.style.setProperty('--valdi-textview-placeholder-color', value),
    element => element.style.removeProperty('--valdi-textview-placeholder-color'),
  );
  binder.bindNoOpAttribute('returnKeyText');
  binder.bindNoOpAttribute('returnKeyType');
  binder.bindStringAttribute(
    'returnType',
    (element, value, context) => {
      getTextViewState(element, context).returnType = value;
      if (value === 'linereturn') {
        element.removeAttribute('enterkeyhint');
      } else {
        element.setAttribute('enterkeyhint', value === 'continue' || value === 'join' ? 'enter' : value);
      }
    },
    (element, context) => {
      getTextViewState(element, context).returnType = 'linereturn';
      element.removeAttribute('enterkeyhint');
    },
  );
  binder.bindAttribute('selectable', selectableAttributeApplier());
  binder.bindAttribute('selectTextOnFocus', selectTextOnFocusAttributeApplier());
  binder.bindAttribute('selection', selectionAttributeApplier());
  binder.bindAttribute('textGravity', textGravityAttributeApplier());
  return {
    ...(textFieldElementClass.attributeAppliers as AttributeApplierMap<TextViewElement>),
    ...binder.attributeAppliers,
  };
}

export class TextViewElementClass extends ElementClass<TextViewElement> {
  constructor(textFieldElementClass: TextFieldElementClass) {
    super('textview', buildTextViewAttributeAppliers(textFieldElementClass), {
      ...textFieldElementClass.compositeAttributes,
      [textViewContentComposite.name]: textViewContentComposite,
    } as any);
  }

  createElement(_id: number, _viewClass: string): TextViewElement {
    return this.onCreateElement();
  }

  protected onCreateElement(): TextViewElement {
    const element = document.createElement('div') as TextViewElement;
    assignStyles(element, {
      fontFamily: SYSTEM_FONT_FAMILY,
      outline: 'none',
      overflow: 'hidden',
      pointerEvents: 'auto',
      whiteSpace: 'pre-wrap',
      wordBreak: 'normal',
      wordWrap: 'break-word',
    });
    element.tabIndex = -1;
    element.contentEditable = 'plaintext-only';
    element.value = '';
    element.selectionStart = 0;
    element.selectionEnd = 0;
    element.setSelectionRange = (selectionStart: number, selectionEnd: number) => {
      applyTextViewSelection(element, selectionStart, selectionEnd);
    };
    element.select = () => {
      applyTextViewSelection(element, 0, element.textContent?.length ?? 0);
    };
    return element;
  }
}
