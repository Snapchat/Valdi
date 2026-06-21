import { convertColor } from '../styles/ValdiWebStyles';
import { isAttributedText, ParsedAttributedText, renderAttributedText } from '../utils/parseAttributedText';
import { applyFontString, applyTextDecoration, cssLength, textShadowCssValue } from '../utils/textStyle';
import { WebValdiLayout } from './WebValdiLayout';

type TextViewElement = HTMLDivElement & {
  disabled?: boolean;
  select?: () => void;
  selectionStart?: number;
  selectionEnd?: number;
  setSelectionRange?: (selectionStart: number, selectionEnd: number) => void;
  value?: string;
};

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

function backgroundEffectVerticalPadding(paddingPx: number | undefined): string | undefined {
  return paddingPx === undefined ? undefined : `${paddingPx / 2}px`;
}

export class WebValdiTextView extends WebValdiLayout {
  public type = 'textview';
  declare public htmlElement: TextViewElement;

  private _backgroundEffectBorderRadius?: string;
  private _backgroundEffectColor?: string;
  private _backgroundEffectPadding?: string;
  private _backgroundEffectPaddingPx?: number;
  private _lineHeight?: string;
  private _lineHeightMultiple?: number;
  private _value: unknown = '';
  private onEditEndCallback: (event: {
    text: string;
    selectionStart: number;
    selectionEnd: number;
    reason: string;
  }) => void = () => {};
  private onSelectionChangeCallback: (event: { text: string; selectionStart: number; selectionEnd: number }) => void =
    () => {};
  private onChangeCallback: (event: { text: string; selectionStart: number; selectionEnd: number }) => void = () => {};
  private onWillChangeCallback: (event: {
    text: string;
    selectionStart: number;
    selectionEnd: number;
  }) => boolean | void = () => {};
  private onEditBeginCallback: (event: { text: string; selectionStart: number; selectionEnd: number }) => void =
    () => {};
  private onReturnCallback: (event: { text: string; selectionStart: number; selectionEnd: number }) => void = () => {};
  private onWillDeleteCallback: (event: { text: string; selectionStart: number; selectionEnd: number }) => void =
    () => {};
  private selectTextOnFocus = false;
  private returnType = 'linereturn';
  private closesWhenReturnKeyPressed = false;
  private debounceTimer: number | undefined;
  private pendingEditEndReason: string | null = null;
  private selectionChangeHandler!: () => void;

  createHtmlElement() {
    const element = document.createElement('div') as TextViewElement;

    Object.assign(element.style, {
      width: '100%',
      height: '100%',
      border: 'none',
      outline: 'none',
      resize: 'none',
      backgroundColor: 'transparent',
      padding: '0',
      margin: '0',
      boxSizing: 'border-box',
      overflow: 'hidden',
      whiteSpace: 'pre-wrap',
      wordBreak: 'normal',
      wordWrap: 'break-word',
      fontFamily: 'sans-serif',
      fontSize: '14px',
      pointerEvents: 'auto',
    });

    element.tabIndex = -1;
    element.contentEditable = 'false';
    element.value = '';
    element.selectionStart = 0;
    element.selectionEnd = 0;
    element.setSelectionRange = (selectionStart: number, selectionEnd: number) => {
      applyTextViewSelection(element, selectionStart, selectionEnd);
    };
    element.select = () => {
      applyTextViewSelection(element, 0, element.textContent?.length ?? 0);
    };

    element.addEventListener('mousedown', e => e.stopPropagation());
    element.addEventListener('touchstart', e => e.stopPropagation());
    element.addEventListener('click', e => e.stopPropagation());

    element.addEventListener('input', () => {
      const event = this.textViewEditEvent();
      this.attributeDelegate?.updateAttribute(this.id, 'value', event.text);
      this.onChangeCallback(event);
    });

    element.addEventListener('beforeinput', (event: Event) => {
      const result = this.onWillChangeCallback(this.textViewEditEvent());
      if (result === false) {
        event.preventDefault();
      }
    });

    element.addEventListener('focus', () => {
      this.onEditBeginCallback(this.textViewEditEvent());
      if (this.selectTextOnFocus) {
        element.select?.();
      }
    });

    element.addEventListener('keydown', (event: KeyboardEvent) => {
      if (event.key === 'Enter') {
        this.onReturnCallback(this.textViewEditEvent());
      }
      if (event.key === 'Backspace' || event.key === 'Delete') {
        this.onWillDeleteCallback(this.textViewEditEvent());
      }
    });

    const handleEditEnd = (reason: string) => {
      if (this.pendingEditEndReason) {
        reason = this.pendingEditEndReason;
        this.pendingEditEndReason = null;
      }
      if (this.debounceTimer) {
        clearTimeout(this.debounceTimer);
      }
      this.debounceTimer = window.setTimeout(() => {
        const event = this.textViewEditEvent();
        this.attributeDelegate?.updateAttribute(this.id, 'value', event.text);
        this.onEditEndCallback({ ...event, reason });
      }, 300);
    };

    element.addEventListener('blur', () => handleEditEnd('blur'));
    element.addEventListener('keydown', (event: KeyboardEvent) => {
      if (event.key !== 'Enter') {
        return;
      }
      if (this.returnType !== 'linereturn') {
        event.preventDefault();
        if (this.closesWhenReturnKeyPressed) {
          this.pendingEditEndReason = 'return';
          element.blur();
        } else {
          handleEditEnd('return');
        }
      } else if (this.closesWhenReturnKeyPressed) {
        event.preventDefault();
        this.pendingEditEndReason = 'return';
        element.blur();
      }
    });

    this.selectionChangeHandler = () => {
      if (document.activeElement === element) {
        this.syncTextViewSelectionFromDom();
        this.onSelectionChangeCallback(this.textViewEditEvent());
      }
    };
    document.addEventListener('selectionchange', this.selectionChangeHandler);

    return element;
  }

  override destroy() {
    document.removeEventListener('selectionchange', this.selectionChangeHandler);
    if (this.debounceTimer) {
      clearTimeout(this.debounceTimer);
    }
    super.destroy();
  }

  private syncTextViewSelectionFromDom(): void {
    if (typeof document.getSelection !== 'function') {
      return;
    }
    const selection = document.getSelection();
    if (!selection || !selection.anchorNode || !selection.focusNode) {
      return;
    }
    if (!this.htmlElement.contains(selection.anchorNode) || !this.htmlElement.contains(selection.focusNode)) {
      return;
    }
    const anchorOffset = textOffsetForNode(this.htmlElement, selection.anchorNode, selection.anchorOffset);
    const focusOffset = textOffsetForNode(this.htmlElement, selection.focusNode, selection.focusOffset);
    this.htmlElement.selectionStart = Math.min(anchorOffset, focusOffset);
    this.htmlElement.selectionEnd = Math.max(anchorOffset, focusOffset);
  }

  private syncTextViewValueFromDom(): string {
    const text = this.htmlElement.textContent ?? '';
    this._value = text;
    this.htmlElement.value = text;
    this.syncTextViewSelectionFromDom();
    return text;
  }

  private textViewEditEvent(): { text: string; selectionStart: number; selectionEnd: number } {
    const text = this.syncTextViewValueFromDom();
    return {
      text,
      selectionStart: this.htmlElement.selectionStart ?? 0,
      selectionEnd: this.htmlElement.selectionEnd ?? 0,
    };
  }

  private applyBackgroundEffect(span: HTMLSpanElement): void {
    if (!this._backgroundEffectColor) {
      return;
    }
    const verticalPadding = backgroundEffectVerticalPadding(this._backgroundEffectPaddingPx);
    span.style.backgroundColor = this._backgroundEffectColor;
    span.style.setProperty('box-decoration-break', 'clone');
    span.style.setProperty('-webkit-box-decoration-break', 'clone');
    if (verticalPadding || this._backgroundEffectPadding) {
      span.style.padding = `${verticalPadding ?? '0'} ${this._backgroundEffectPadding ?? '0'}`;
    }
    if (this._backgroundEffectPadding) {
      span.style.marginLeft = `-${this._backgroundEffectPadding}`;
      span.style.marginRight = `-${this._backgroundEffectPadding}`;
    }
    span.style.position = 'relative';
    if (this._backgroundEffectBorderRadius) {
      span.style.borderRadius = this._backgroundEffectBorderRadius;
    }
  }

  private wrapBackgroundEffectContent(content: HTMLElement): HTMLElement {
    const verticalPadding = backgroundEffectVerticalPadding(this._backgroundEffectPaddingPx);
    const wrapper = document.createElement('span');
    wrapper.style.boxSizing = 'border-box';
    wrapper.style.display = 'block';
    wrapper.style.position = 'relative';
    wrapper.style.whiteSpace = 'inherit';
    wrapper.style.width = '100%';
    if (this._backgroundEffectPadding || verticalPadding) {
      wrapper.style.padding = `${verticalPadding ?? '0'} ${this._backgroundEffectPadding ?? '0'}`;
    }
    wrapper.appendChild(content);
    return wrapper;
  }

  private renderTextViewContent(): void {
    const parsedAttributedText = isAttributedText(this._value) ? ParsedAttributedText.parse(this._value) : undefined;
    const text = parsedAttributedText ? parsedAttributedText.toString() : plainTextValue(this._value);
    this.htmlElement.value = text;
    this.htmlElement.replaceChildren();

    if (parsedAttributedText) {
      const container = renderAttributedText(parsedAttributedText, {
        getInlineChild: index => this.children[index]?.htmlElement,
      });
      this.applyBackgroundEffect(container);
      this.htmlElement.appendChild(
        this._backgroundEffectColor ? this.wrapBackgroundEffectContent(container) : container,
      );
      return;
    }

    if (this._backgroundEffectColor) {
      const span = document.createElement('span');
      span.textContent = text;
      this.applyBackgroundEffect(span);
      this.htmlElement.appendChild(this.wrapBackgroundEffectContent(span));
      return;
    }

    this.htmlElement.textContent = text;
  }

  private updateLineHeight() {
    if (this._lineHeight !== undefined) {
      this.htmlElement.style.lineHeight = this._lineHeight;
    } else if (this._lineHeightMultiple !== undefined) {
      this.htmlElement.style.lineHeight = String(this._lineHeightMultiple);
    } else {
      this.htmlElement.style.lineHeight = '';
    }
  }

  changeAttribute(attributeName: string, attributeValue: any): void {
    switch (attributeName) {
      case 'onWillChange':
        this.onWillChangeCallback = attributeValue;
        return;
      case 'onChange':
        this.onChangeCallback = attributeValue;
        return;
      case 'onEditBegin':
        this.onEditBeginCallback = attributeValue;
        return;
      case 'onEditEnd':
        this.onEditEndCallback = attributeValue;
        return;
      case 'onReturn':
        this.onReturnCallback = attributeValue;
        return;
      case 'onWillDelete':
        this.onWillDeleteCallback = attributeValue;
        return;
      case 'onSelectionChange':
        this.onSelectionChangeCallback = attributeValue;
        return;

      case 'tintColor':
        this.htmlElement.style.caretColor = convertColor(attributeValue);
        return;
      case 'placeholderColor':
        return;
      case 'textAlign':
        this.htmlElement.style.textAlign = attributeValue === 'justified' ? 'justify' : attributeValue;
        return;
      case 'textDecoration':
        applyTextDecoration(this.htmlElement, attributeValue);
        return;
      case 'font':
        applyFontString(this.htmlElement, String(attributeValue));
        return;
      case 'color':
        this.htmlElement.style.color = convertColor(attributeValue);
        return;
      case 'textGradient':
        this.htmlElement.style.backgroundImage = attributeValue;
        this.htmlElement.style.backgroundClip = 'text';
        this.htmlElement.style.webkitBackgroundClip = 'text';
        this.htmlElement.style.color = 'transparent';
        return;
      case 'textShadow':
        this.htmlElement.style.textShadow = textShadowCssValue(attributeValue) ?? '';
        return;
      case 'lineHeight':
        this._lineHeight =
          attributeValue === undefined || attributeValue === null ? undefined : cssLength(attributeValue);
        this.updateLineHeight();
        return;
      case 'lineHeightMultiple':
        this._lineHeightMultiple =
          attributeValue === undefined || attributeValue === null ? undefined : Number(attributeValue);
        this.updateLineHeight();
        return;

      case 'placeholder':
        return;
      case 'value':
        this._value = attributeValue ?? '';
        this.renderTextViewContent();
        return;
      case 'selection':
        if (Array.isArray(attributeValue) && attributeValue.length === 2) {
          this.htmlElement.setSelectionRange?.(Number(attributeValue[0]), Number(attributeValue[1]));
        }
        return;

      case 'focused':
        if (attributeValue) {
          this.htmlElement.focus();
        } else {
          this.htmlElement.blur();
        }
        return;
      case 'enabled':
        this.htmlElement.disabled = !attributeValue;
        this.htmlElement.setAttribute('aria-disabled', String(!attributeValue));
        this.htmlElement.contentEditable = attributeValue ? 'plaintext-only' : 'false';
        this.htmlElement.style.pointerEvents = attributeValue ? 'auto' : 'none';
        return;
      case 'selectable':
        this.htmlElement.style.userSelect = attributeValue === false ? 'none' : 'text';
        return;
      case 'selectTextOnFocus':
        this.selectTextOnFocus = !!attributeValue;
        return;
      case 'closesWhenReturnKeyPressed':
        this.closesWhenReturnKeyPressed = attributeValue !== false;
        return;
      case 'returnType':
        this.returnType = attributeValue || 'linereturn';
        this.htmlElement.setAttribute('enterkeyhint', attributeValue === 'linereturn' ? 'enter' : attributeValue);
        return;

      case 'keyboardAppearance':
        this.htmlElement.style.colorScheme = attributeValue;
        return;
      case 'autocapitalization':
        this.htmlElement.setAttribute('autocapitalize', attributeValue);
        return;
      case 'autocorrection':
        this.htmlElement.setAttribute('autocorrect', attributeValue ? 'on' : 'off');
        return;
      case 'characterLimit':
        return;
      case 'contentType':
      case 'keyboardType':
      case 'enableInlinePredictions':
      case 'returnKeyText':
      case 'returnKeyType':
        return;

      case 'backgroundEffectColor':
        this._backgroundEffectColor =
          attributeValue === undefined || attributeValue === null ? undefined : convertColor(attributeValue);
        this.renderTextViewContent();
        return;
      case 'backgroundEffectBorderRadius':
        this._backgroundEffectBorderRadius =
          attributeValue === undefined || attributeValue === null ? undefined : cssLength(attributeValue);
        this.renderTextViewContent();
        return;
      case 'backgroundEffectPadding': {
        if (attributeValue === undefined || attributeValue === null) {
          this._backgroundEffectPadding = undefined;
          this._backgroundEffectPaddingPx = undefined;
        } else {
          this._backgroundEffectPadding = cssLength(attributeValue);
          const padding =
            typeof attributeValue === 'number' ? attributeValue : Number.parseFloat(this._backgroundEffectPadding);
          this._backgroundEffectPaddingPx = Number.isFinite(padding) ? padding : undefined;
        }
        this.renderTextViewContent();
        return;
      }
      case 'numberOfLines':
        if (attributeValue && attributeValue > 0) {
          this.htmlElement.style.display = '-webkit-box';
          this.htmlElement.style.overflow = 'hidden';
          this.htmlElement.style.setProperty('-webkit-line-clamp', String(attributeValue));
          this.htmlElement.style.setProperty('-webkit-box-orient', 'vertical');
        } else {
          this.htmlElement.style.removeProperty('-webkit-line-clamp');
          this.htmlElement.style.removeProperty('-webkit-box-orient');
          this.htmlElement.style.display = '';
          this.htmlElement.style.overflow = 'hidden';
        }
        return;
      case 'textGravity':
        this.htmlElement.style.alignContent = attributeValue === 'bottom' ? 'end' : attributeValue;
        return;

      default:
    }

    super.changeAttribute(attributeName, attributeValue);
  }
}
