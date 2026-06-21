import { UpdateAttributeDelegate } from '../ValdiWebRendererDelegate';
import { WebValdiLayout } from './WebValdiLayout';
import { convertColor } from '../styles/ValdiWebStyles';
import { isAttributedText, ParsedAttributedText } from '../utils/parseAttributedText';
import { applyFontString, cssLength, textShadowCssValue } from '../utils/textStyle';

class ValdiInput extends HTMLElement {
  private input: HTMLInputElement;
  private onEditEnd: (event: { text: string; selectionStart: number; selectionEnd: number; reason: string }) => void =
    () => {};
  private onSelectionChange: (event: {
    value: string;
    selection: { start: number | null; end: number | null };
  }) => void = () => {};
  private attributeDelegate?: UpdateAttributeDelegate;

  constructor() {
    super();

    const shadow = this.attachShadow({ mode: 'open', delegatesFocus: true });

    this.input = document.createElement('input');

    // Style the inner input to remove default browser borders and outlines
    Object.assign(this.input.style, {
      width: '100%',
      height: '100%',
      border: 'none',
      outline: 'none',
      backgroundColor: 'transparent',
      padding: 0,
      margin: 0,
      boxSizing: 'border-box',
      MozAppearance: 'textfield',
      WebkitAppearance: 'none',
    });

    // Add a style element to ensure no focus outlines
    const style = document.createElement('style');
    style.textContent = `
      input {
        border: none !important;
        outline: none !important;
        box-shadow: none !important;
      }
      input:focus {
        outline: none !important;
        box-shadow: none !important;
      }
      input:focus-visible {
        outline: none !important;
        box-shadow: none !important;
      }
    `;
    shadow.appendChild(style);

    const handleInput = this.debounce((_event: Event, reason: string) => {
      const text = this.input.value;
      this.attributeDelegate?.updateAttribute(Number(this.input.getAttribute('id')), 'value', text);
      this.onEditEnd({
        text,
        selectionStart: this.input.selectionStart ?? 0,
        selectionEnd: this.input.selectionEnd ?? 0,
        reason,
      });
    }, 300).bind(this);

    this.input.addEventListener('blur', event => handleInput(event, 'blur'));
    this.input.addEventListener('keydown', (event: KeyboardEvent) => {
      if (event.key === 'Enter') {
        handleInput(event, 'return');
      }
    });

    // Stop propagation on interaction events to prevent parent onTap handlers
    // (e.g. scroll's closeKeyboard) from stealing focus.
    // This matches the pattern used by WebValdiTextView (textarea) which works correctly.
    this.input.addEventListener('mousedown', e => e.stopPropagation());
    this.input.addEventListener('touchstart', e => e.stopPropagation());
    this.input.addEventListener('click', e => e.stopPropagation());

    shadow.appendChild(this.input);
  }

  static get observedAttributes(): string[] {
    return ['onEditEnd', 'type', 'name'];
  }

  debounce<T extends (...args: any[]) => void>(fn: T, delay: number): T {
    let timer: number | undefined;
    return function (...args: any[]) {
      if (timer) clearTimeout(timer);
      timer = window.setTimeout(() => fn(...args), delay);
    } as T;
  }

  private _handleSelectionChange = () => {
    if (this.shadowRoot?.activeElement === this.input) {
      this.onSelectionChange({
        value: this.input.value,
        selection: { start: this.input.selectionStart, end: this.input.selectionEnd },
      });
    }
  };
  connectedCallback() {
    for (const { name, value } of Array.from(this.attributes)) {
      this.forwardAttribute(name, value);
    }

    let observer = new MutationObserver(mutations => {
      for (const mutation of mutations) {
        if (mutation.type === 'attributes' && mutation.attributeName) {
          const name = mutation.attributeName;
          const value = this.getAttribute(name);
          this.forwardAttribute(name, value);
        }
      }
    });

    observer.observe(this, { attributes: true, childList: true, subtree: true });
    document.addEventListener('selectionchange', this._handleSelectionChange);
  }

  disconnectedCallback() {
    document.removeEventListener('selectionchange', this._handleSelectionChange);
  }

  // Can't be passed through the attribute system, it'll get turned into a string?
  setOnEditEnd(value: (event: { text: string; selectionStart: number; selectionEnd: number; reason: string }) => void) {
    this.onEditEnd = value;
  }

  setOnSelectionChange(
    callback: (event: { value: string; selection: { start: number | null; end: number | null } }) => void,
  ) {
    this.onSelectionChange = callback;
  }

  setAttributeDelegate(attributeDelegate?: UpdateAttributeDelegate) {
    this.attributeDelegate = attributeDelegate;
  }

  private forwardAttribute(name: string, value: any | null) {
    if (value === null) {
      this.input.removeAttribute(name);
    } else {
      this.input.setAttribute(name, value);
      this.attributeDelegate?.updateAttribute(Number(this.input.getAttribute('id')), name, value);
    }

    // Try to also assign as property if it's valid
    if (name in this.input) {
      try {
        (this.input as any)[name] = value;
      } catch (_) {
        // ignore invalid property assignment
      }
    }
  }

  // Pass through
  getAttribute(name: string): string | null {
    return this.input.getAttribute(name);
  }

  // Expose input properties so event handlers can access them
  // after shadow DOM retargets event.target to this host element.
  get value(): string {
    return this.input.value;
  }

  set value(v: string) {
    if (this.input.value !== v) {
      this.input.value = v;
    }
  }

  get selectionStart(): number | null {
    return this.input.selectionStart;
  }

  get selectionEnd(): number | null {
    return this.input.selectionEnd;
  }

  get disabled(): boolean {
    return this.input.disabled;
  }

  set disabled(v: boolean) {
    this.input.disabled = v;
  }

  // Public methods to control the inner input
  select() {
    this.input.select();
  }

  blur() {
    this.input.blur();
  }

  focus(options?: FocusOptions) {
    this.input.focus(options);
  }

  setSelectionRange(start: number, end: number) {
    this.input.setSelectionRange(start, end);
  }
}

export function registerTextFieldElements(): void {
  if (!customElements.get('valdi-input')) {
    customElements.define('valdi-input', ValdiInput);
  }
}

export class WebValdiTextField extends WebValdiLayout {
  public type = 'textfield';
  declare public htmlElement: ValdiInput;
  private _callbackListeners: { key: string; eventType: string; handler: EventListener }[] = [];

  createHtmlElement() {
    const element = document.createElement('valdi-input') as ValdiInput;
    element.setAttribute('type', 'text');
    element.setAttribute('name', 'input');
    element.setAttribute('id', String(this.id));

    element.setAttributeDelegate(this.attributeDelegate);

    Object.assign(element.style, {
      MozAppearance: 'textfield',
      WebkitAppearance: 'none',
      backgroundColor: 'transparent',
      border: '0 solid black',
      borderRadius: 0,
      boxSizing: 'border-box',
      margin: 0,
      padding: 0,
      resize: 'none',
      pointerEvents: 'auto',
    });

    return element;
  }

  private replaceListener(key: string, eventType: string, handler: EventListener | undefined) {
    const existing = this._callbackListeners.findIndex(l => l.key === key);
    if (existing >= 0) {
      this.htmlElement.removeEventListener(
        this._callbackListeners[existing].eventType,
        this._callbackListeners[existing].handler,
      );
      this._callbackListeners.splice(existing, 1);
    }
    if (handler) {
      this.htmlElement.addEventListener(eventType, handler);
      this._callbackListeners.push({ key, eventType, handler });
    }
  }

  changeAttribute(attributeName: string, attributeValue: any): void {
    switch (attributeName) {
      case 'onWillChange': {
        const cb = attributeValue;
        const handler = cb
          ? (((event: InputEvent) => {
              const el = this.htmlElement;
              const result = cb({
                text: el.value,
                selectionStart: el.selectionStart ?? 0,
                selectionEnd: el.selectionEnd ?? 0,
              });
              if (result === false) {
                event.preventDefault();
              }
            }) as EventListener)
          : undefined;
        this.replaceListener('onWillChange', 'beforeinput', handler);
        return;
      }
      case 'onChange': {
        const cb = attributeValue;
        const handler = cb
          ? ((() => {
              const el = this.htmlElement;
              this.attributeDelegate?.updateAttribute(this.id, 'value', el.value);
              cb({
                text: el.value,
                selectionStart: el.selectionStart ?? 0,
                selectionEnd: el.selectionEnd ?? 0,
              });
            }) as EventListener)
          : undefined;
        this.replaceListener('onChange', 'input', handler);
        return;
      }
      case 'onEditBegin': {
        const cb = attributeValue;
        const handler = cb
          ? ((() => {
              const el = this.htmlElement;
              cb({
                text: el.value,
                selectionStart: el.selectionStart ?? 0,
                selectionEnd: el.selectionEnd ?? 0,
              });
            }) as EventListener)
          : undefined;
        this.replaceListener('onEditBegin', 'focus', handler);
        return;
      }
      case 'onEditEnd':
        this.htmlElement.setOnEditEnd(attributeValue);
        return;
      case 'onReturn': {
        const cb = attributeValue;
        const handler = cb
          ? (((event: KeyboardEvent) => {
              if (event.key === 'Enter') {
                const el = this.htmlElement;
                cb({
                  text: el.value,
                  selectionStart: el.selectionStart ?? 0,
                  selectionEnd: el.selectionEnd ?? 0,
                });
              }
            }) as EventListener)
          : undefined;
        this.replaceListener('onReturn', 'keydown', handler);
        return;
      }
      case 'onWillDelete': {
        const cb = attributeValue;
        const handler = cb
          ? (((event: KeyboardEvent) => {
              if (event.key === 'Backspace' || event.key === 'Delete') {
                const el = this.htmlElement;
                cb({
                  text: el.value,
                  selectionStart: el.selectionStart ?? 0,
                  selectionEnd: el.selectionEnd ?? 0,
                });
              }
            }) as EventListener)
          : undefined;
        this.replaceListener('onWillDelete', 'keydown', handler);
        return;
      }
      case 'onSelectionChange':
        this.htmlElement.setOnSelectionChange(attributeValue);
        return;

      // Style & Appearance
      case 'tintColor':
        this.htmlElement.style.caretColor = convertColor(attributeValue);
        return;
      case 'placeholderColor': {
        const styleId = 'placeholder-style';
        const shadowRoot = this.htmlElement.shadowRoot;
        if (!shadowRoot) return;

        let styleElement = shadowRoot.querySelector('#' + styleId) as HTMLStyleElement;
        if (!styleElement) {
          styleElement = document.createElement('style');
          styleElement.id = styleId;
          shadowRoot.appendChild(styleElement);
        }
        const color = convertColor(attributeValue);
        // We inject a style tag into the shadow DOM to style the placeholder.
        // `opacity: 1` is for Firefox, which applies a default opacity.
        styleElement.textContent = `input::placeholder { color: ${color}; opacity: 1; }`;
        return;
      }
      case 'textAlign':
        this.htmlElement.style.textAlign = attributeValue === 'justified' ? 'justify' : attributeValue;
        return;
      case 'font':
        applyFontString(this.htmlElement, String(attributeValue));
        return;
      case 'lineHeight':
        this.htmlElement.style.lineHeight =
          attributeValue === undefined || attributeValue === null ? '' : cssLength(attributeValue);
        return;
      case 'lineHeightMultiple':
        this.htmlElement.style.lineHeight =
          attributeValue === undefined || attributeValue === null ? '' : String(attributeValue);
        return;
      case 'color':
        this.htmlElement.style.color = convertColor(attributeValue);
        return;
      case 'textGradient':
        this.htmlElement.style.backgroundImage = attributeValue;
        this.htmlElement.style.backgroundClip = 'text';
        this.htmlElement.style.webkitBackgroundClip = 'text'; // For Safari/Chrome
        this.htmlElement.style.color = 'transparent';
        return;
      case 'textShadow': {
        this.htmlElement.style.textShadow = textShadowCssValue(attributeValue) ?? '';
        return;
      }

      // Content
      case 'placeholder':
        this.htmlElement.setAttribute(attributeName, attributeValue);
        return;
      case 'value':
        // Must set the .value property (not attribute) to update the current input value.
        // setAttribute('value') only sets the default/initial value for <input> elements.
        this.htmlElement.value = isAttributedText(attributeValue)
          ? ParsedAttributedText.parse(attributeValue).toString()
          : (attributeValue ?? '');
        return;
      case 'selection':
        if (Array.isArray(attributeValue) && attributeValue.length === 2) {
          this.htmlElement.setSelectionRange(attributeValue[0], attributeValue[1]);
        }
        return;

      // Behavior
      case 'focused':
        if (attributeValue) {
          this.htmlElement.focus();
        } else {
          this.htmlElement.blur();
        }
        return;
      case 'enabled':
        this.htmlElement.disabled = !attributeValue;
        return;
      case 'selectTextOnFocus':
        if (attributeValue) {
          this.htmlElement.addEventListener('focus', () => this.htmlElement.select());
        }
        // Note: This event listener is not removed if the property is later set to false.
        return;
      case 'closesWhenReturnKeyPressed':
        if (attributeValue !== false) {
          // default true
          this.htmlElement.addEventListener('keydown', (event: KeyboardEvent) => {
            if (event.key === 'Enter') {
              this.htmlElement.blur();
            }
          });
        }
        // Note: This event listener is not removed if the property is later set to false.
        return;

      // Keyboard & Input
      case 'contentType':
        this.htmlElement.setAttribute(attributeName, getContentType(attributeValue));
        return;
      case 'keyboardType':
        this.htmlElement.setAttribute('inputmode', attributeValue);
        return;
      case 'keyboardAppearance': // [iOS-Only]
        this.htmlElement.style.colorScheme = attributeValue;
        return;
      case 'returnKeyType':
        this.htmlElement.setAttribute('enterkeyhint', attributeValue);
        return;
      case 'returnKeyText':
        this.htmlElement.setAttribute('enterkeyhint', attributeValue);
        return;
      case 'autocapitalization': // Replaces autoCapitalize
        this.htmlElement.setAttribute('autocapitalize', attributeValue);
        return;
      case 'autocorrection': // Replaces autoCorrect
        this.htmlElement.setAttribute('autocorrect', attributeValue ? 'on' : 'off');
        return;
      case 'characterLimit': // Replaces maxLength
        this.htmlElement.setAttribute('maxlength', attributeValue);
        return;
      case 'enableInlinePredictions': // [iOS-Only]
        this.htmlElement.setAttribute('autocomplete', attributeValue ? 'on' : 'off');
        return;
      default:
      // Do nothing
    }

    super.changeAttribute(attributeName, attributeValue);
  }
}

function getContentType(type: string) {
  switch (type) {
    case 'phoneNumber':
      return 'tel';
    case 'email':
      return 'email';
    case 'password':
      return 'password';
    case 'url':
      return 'url';
    default:
      return 'text';
  }
}
