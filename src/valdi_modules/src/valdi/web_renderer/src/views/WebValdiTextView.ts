import { WebValdiLayout } from './WebValdiLayout';
import { convertColor } from '../styles/ValdiWebStyles';

export class WebValdiTextView extends WebValdiLayout {
  public type = 'textview';
  public declare htmlElement: HTMLTextAreaElement;
  
  private onEditEndCallback: (event: { text: string, selectionStart: number, selectionEnd: number, reason: string }) => void = () => {};
  private onSelectionChangeCallback: (event: { text: string, selectionStart: number, selectionEnd: number }) => void = () => {};
  private onChangeCallback: (event: { text: string, selectionStart: number, selectionEnd: number }) => void = () => {};
  private onWillChangeCallback: (event: { text: string, selectionStart: number, selectionEnd: number }) => boolean | void = () => {};
  private onEditBeginCallback: (event: { text: string, selectionStart: number, selectionEnd: number }) => void = () => {};
  private onReturnCallback: (event: { text: string, selectionStart: number, selectionEnd: number }) => void = () => {};
  private onWillDeleteCallback: (event: { text: string, selectionStart: number, selectionEnd: number }) => void = () => {};
  private selectTextOnFocus: boolean = false;
  private returnType: string = 'linereturn';
  private closesWhenReturnKeyPressed: boolean = false;
  private debounceTimer: number | undefined;
  private pendingEditEndReason: string | null = null;
  private selectionChangeHandler!: () => void;

  createHtmlElement() {
    const textarea = document.createElement('textarea');
    
    Object.assign(textarea.style, {
      width: '100%',
      height: '100%',
      border: 'none',
      outline: 'none',
      resize: 'none',
      backgroundColor: 'transparent',
      padding: '0',
      margin: '0',
      boxSizing: 'border-box',
      overflow: 'auto',
      whiteSpace: 'pre-wrap',
      wordWrap: 'break-word',
      fontFamily: 'sans-serif',
      fontSize: '14px',
      pointerEvents: 'auto',
    });

    textarea.addEventListener('mousedown', (e) => e.stopPropagation());
    textarea.addEventListener('touchstart', (e) => e.stopPropagation());
    textarea.addEventListener('click', (e) => e.stopPropagation());

    textarea.addEventListener('input', () => {
      this.attributeDelegate?.updateAttribute(this.id, "value", textarea.value);
      this.onChangeCallback({
        text: textarea.value,
        selectionStart: textarea.selectionStart,
        selectionEnd: textarea.selectionEnd,
      });
    });

    textarea.addEventListener('beforeinput', (event: Event) => {
      const result = this.onWillChangeCallback({
        text: textarea.value,
        selectionStart: textarea.selectionStart,
        selectionEnd: textarea.selectionEnd,
      });
      if (result === false) {
        event.preventDefault();
      }
    });

    textarea.addEventListener('focus', () => {
      this.onEditBeginCallback({
        text: textarea.value,
        selectionStart: textarea.selectionStart,
        selectionEnd: textarea.selectionEnd,
      });
      if (this.selectTextOnFocus) {
        textarea.select();
      }
    });

    textarea.addEventListener('keydown', (event: KeyboardEvent) => {
      if (event.key === 'Enter') {
        this.onReturnCallback({
          text: textarea.value,
          selectionStart: textarea.selectionStart,
          selectionEnd: textarea.selectionEnd,
        });
      }
      if (event.key === 'Backspace' || event.key === 'Delete') {
        this.onWillDeleteCallback({
          text: textarea.value,
          selectionStart: textarea.selectionStart,
          selectionEnd: textarea.selectionEnd,
        });
      }
    });

    const handleEditEnd = (reason: string) => {
      if (this.pendingEditEndReason) {
        reason = this.pendingEditEndReason;
        this.pendingEditEndReason = null;
      }
      if (this.debounceTimer) clearTimeout(this.debounceTimer);
      this.debounceTimer = window.setTimeout(() => {
        this.attributeDelegate?.updateAttribute(this.id, "value", textarea.value);
        this.onEditEndCallback({
          text: textarea.value,
          selectionStart: textarea.selectionStart,
          selectionEnd: textarea.selectionEnd,
          reason,
        });
      }, 300);
    };

    textarea.addEventListener('blur', () => handleEditEnd('blur'));
    textarea.addEventListener('keydown', (event: KeyboardEvent) => {
      if (event.key === 'Enter') {
        if (this.returnType !== 'linereturn') {
          event.preventDefault();
          if (this.closesWhenReturnKeyPressed) {
            this.pendingEditEndReason = 'return';
            textarea.blur();
          } else {
            handleEditEnd('return');
          }
        } else if (this.closesWhenReturnKeyPressed) {
          event.preventDefault();
          this.pendingEditEndReason = 'return';
          textarea.blur();
        }
      }
    });

    this.selectionChangeHandler = () => {
      if (document.activeElement === textarea) {
        this.onSelectionChangeCallback({
          text: textarea.value,
          selectionStart: textarea.selectionStart,
          selectionEnd: textarea.selectionEnd,
        });
      }
    };
    document.addEventListener('selectionchange', this.selectionChangeHandler);

    return textarea;
  }

  override destroy() {
    document.removeEventListener('selectionchange', this.selectionChangeHandler);
    if (this.debounceTimer) clearTimeout(this.debounceTimer);
    super.destroy();
  }

  changeAttribute(attributeName: string, attributeValue: any): void {
    const textarea = this.htmlElement;
    
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
        textarea.style.caretColor = convertColor(attributeValue);
        return;
      case 'placeholderColor':
        // Can't style placeholder without CSS injection, skip for now
        return;
      case 'textAlign':
        textarea.style.textAlign = attributeValue;
        return;
      case 'font': {
        const parts = String(attributeValue).split(' ');
        if (parts.length >= 2) {
          textarea.style.fontFamily = parts[0];
          textarea.style.fontSize = `${parts[1]}px`;
        }
        return;
      }
      case 'color':
        textarea.style.color = convertColor(attributeValue);
        return;

      case 'placeholder':
        textarea.placeholder = attributeValue ?? '';
        return;
      case 'value':
        textarea.value = attributeValue ?? '';
        return;
      case 'selection':
        if (Array.isArray(attributeValue) && attributeValue.length === 2) {
          textarea.setSelectionRange(attributeValue[0], attributeValue[1]);
        }
        return;

      case 'focused':
        if (attributeValue) {
          textarea.focus();
        } else {
          textarea.blur();
        }
        return;
      case 'enabled':
        textarea.disabled = !attributeValue;
        textarea.style.pointerEvents = attributeValue ? 'auto' : 'none';
        return;
      case 'selectTextOnFocus':
        this.selectTextOnFocus = !!attributeValue;
        return;
      case 'closesWhenReturnKeyPressed':
        this.closesWhenReturnKeyPressed = attributeValue !== false;
        return;
      case 'returnType':
        this.returnType = attributeValue || 'linereturn';
        textarea.setAttribute('enterkeyhint', attributeValue === 'linereturn' ? 'enter' : attributeValue);
        return;

      case 'keyboardAppearance':
        textarea.style.colorScheme = attributeValue;
        return;
      case 'autocapitalization':
        textarea.setAttribute('autocapitalize', attributeValue);
        return;
      case 'autocorrection':
        textarea.setAttribute('autocorrect', attributeValue ? 'on' : 'off');
        return;
      case 'characterLimit':
        textarea.maxLength = attributeValue;
        return;

      case 'textGravity':
      case 'textGradient':
      case 'textShadow':
      case 'enableInlinePredictions':
      case 'backgroundEffectColor':
      case 'backgroundEffectBorderRadius':
      case 'backgroundEffectPadding':
        // Skip complex styling for MVP
        return;

      default:
        // Fall through to parent
    }

    super.changeAttribute(attributeName, attributeValue);
  }
}
