import { UpdateAttributeDelegate } from '../ValdiWebRendererDelegate';
import { convertColor } from '../styles/ValdiWebStyles';
import { WebValdiLayout } from './WebValdiLayout';

type TextFieldEvent = { text: string; selectionStart: number; selectionEnd: number };

class ValdiInput extends HTMLElement {
  private input: HTMLInputElement;
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
      /* eslint-disable @typescript-eslint/naming-convention -- vendor-prefixed CSSOM property names */
      MozAppearance: 'textfield',
      WebkitAppearance: 'none',
      /* eslint-enable @typescript-eslint/naming-convention */
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

  // Expose input properties so event handlers can access them
  // after shadow DOM retargets event.target to this host element.
  get value(): string {
    return this.input.value;
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

  set value(v: string) {
    if (this.input.value !== v) {
      this.input.value = v;
    }
  }

  set disabled(v: boolean) {
    this.input.disabled = v;
  }

  debounce<A extends unknown[]>(fn: (...args: A) => void, delay: number): (...args: A) => void {
    let timer: number | undefined;
    return function (...args: A) {
      if (timer) clearTimeout(timer);
      timer = window.setTimeout(() => fn(...args), delay);
    };
  }

  connectedCallback(): void {
    for (const { name, value } of Array.from(this.attributes)) {
      this.forwardAttribute(name, value);
    }

    const observer = new MutationObserver(mutations => {
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

  disconnectedCallback(): void {
    document.removeEventListener('selectionchange', this._handleSelectionChange);
  }

  // Can't be passed through the attribute system, it'll get turned into a string?
  setOnEditEnd(
    value: (event: { text: string; selectionStart: number; selectionEnd: number; reason: string }) => void,
  ): void {
    this.onEditEnd = value;
  }

  setOnSelectionChange(
    callback: (event: { value: string; selection: { start: number | null; end: number | null } }) => void,
  ): void {
    this.onSelectionChange = callback;
  }

  setAttributeDelegate(attributeDelegate?: UpdateAttributeDelegate): void {
    this.attributeDelegate = attributeDelegate;
  }

  // Pass through
  getAttribute(name: string): string | null {
    return this.input.getAttribute(name);
  }

  // Public methods to control the inner input
  select(): void {
    this.input.select();
  }

  blur(): void {
    this.input.blur();
  }

  focus(options?: FocusOptions): void {
    this.input.focus(options);
  }

  setSelectionRange(start: number, end: number): void {
    this.input.setSelectionRange(start, end);
  }

  private onEditEnd: (event: { text: string; selectionStart: number; selectionEnd: number; reason: string }) => void =
    () => {};
  private onSelectionChange: (event: {
    value: string;
    selection: { start: number | null; end: number | null };
  }) => void = () => {};
  private _handleSelectionChange = (): void => {
    if (this.shadowRoot?.activeElement === this.input) {
      this.onSelectionChange({
        value: this.input.value,
        selection: { start: this.input.selectionStart, end: this.input.selectionEnd },
      });
    }
  };

  private forwardAttribute(name: string, value: string | null): void {
    if (value === null) {
      this.input.removeAttribute(name);
    } else {
      this.input.setAttribute(name, value);
      this.attributeDelegate?.updateAttribute(Number(this.input.getAttribute('id')), name, value);
    }

    // Try to also assign as property if it's valid
    if (name in this.input) {
      try {
        (this.input as unknown as Record<string, unknown>)[name] = value;
      } catch (_) {
        // ignore invalid property assignment
      }
    }
  }
}

export function registerTextFieldElements(): void {
  if (!customElements.get('valdi-input')) {
    customElements.define('valdi-input', ValdiInput);
  }
}

export class WebValdiTextField extends WebValdiLayout {
  public type = 'textfield';
  public declare htmlElement: ValdiInput;
  private _callbackListeners: { key: string; eventType: string; handler: EventListener }[] = [];

  createHtmlElement(): ValdiInput {
    const element = document.createElement('valdi-input') as ValdiInput;
    element.setAttribute('type', 'text');
    element.setAttribute('name', 'input');
    element.setAttribute('id', String(this.id));

    element.setAttributeDelegate(this.attributeDelegate);

    Object.assign(element.style, {
      /* eslint-disable @typescript-eslint/naming-convention -- vendor-prefixed CSSOM property names */
      MozAppearance: 'textfield',
      WebkitAppearance: 'none',
      /* eslint-enable @typescript-eslint/naming-convention */
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

  changeAttribute(attributeName: string, attributeValue: unknown): void {
    switch (attributeName) {
      case 'onWillChange': {
        const cb = attributeValue as ((event: TextFieldEvent) => boolean | void) | undefined;
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
        const cb = attributeValue as ((event: TextFieldEvent) => void) | undefined;
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
        const cb = attributeValue as ((event: TextFieldEvent) => void) | undefined;
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
        this.htmlElement.setOnEditEnd(attributeValue as Parameters<ValdiInput['setOnEditEnd']>[0]);
        return;
      case 'onReturn': {
        const cb = attributeValue as ((event: TextFieldEvent) => void) | undefined;
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
        const cb = attributeValue as ((event: TextFieldEvent) => void) | undefined;
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
        this.htmlElement.setOnSelectionChange(attributeValue as Parameters<ValdiInput['setOnSelectionChange']>[0]);
        return;

      // Style & Appearance
      case 'tintColor':
        this.htmlElement.style.caretColor = convertColor(attributeValue as string);
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
        const color = convertColor(attributeValue as string);
        // We inject a style tag into the shadow DOM to style the placeholder.
        // `opacity: 1` is for Firefox, which applies a default opacity.
        styleElement.textContent = `input::placeholder { color: ${color}; opacity: 1; }`;
        return;
      }
      case 'textAlign':
        this.htmlElement.style.textAlign = attributeValue as string;
        return;
      case 'font': {
        const parts = String(attributeValue).split(' ');
        if (parts.length >= 2) {
          // Format: 'font-family size ...'
          // We'll assume size is in px for web.
          this.htmlElement.style.fontFamily = parts[0];
          this.htmlElement.style.fontSize = `${parts[1]}px`;
        }
        return;
      }
      case 'color':
        this.htmlElement.style.color = convertColor(attributeValue as string);
        return;
      case 'textGradient':
        this.htmlElement.style.backgroundImage = attributeValue as string;
        this.htmlElement.style.backgroundClip = 'text';
        this.htmlElement.style.webkitBackgroundClip = 'text'; // For Safari/Chrome
        this.htmlElement.style.color = 'transparent';
        return;
      case 'textShadow': {
        // Format: '{color} {radius} {opacity} {offsetX} {offsetY}'
        // CSS: text-shadow: offsetX offsetY blur-radius color
        const match = String(attributeValue).match(/(.+?)\s+([0-9.-]+)\s+([0-9.-]+)\s+([0-9.-]+)\s+([0-9.-]+)$/);
        if (match) {
          const [, color, radius, opacity, offsetX, offsetY] = match;
          const finalColor = applyOpacity(convertColor(color), opacity);
          this.htmlElement.style.textShadow = `${offsetX}px ${offsetY}px ${radius}px ${finalColor}`;
        }
        return;
      }

      // Content
      case 'placeholder':
        this.htmlElement.setAttribute(attributeName, attributeValue as string);
        return;
      case 'value':
        // Must set the .value property (not attribute) to update the current input value.
        // setAttribute('value') only sets the default/initial value for <input> elements.
        this.htmlElement.value = (attributeValue as string | null | undefined) ?? '';
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
        // Map Valdi's contentType to HTML <input> attributes. Mirrors the
        // native binders (iOS SCValdiTextInputSetContentTypeValues, Android
        // EditTextAttributesBinder.applyContentType) which treat contentType
        // as authoritative over keyboard, masking, and suggestion behavior —
        // so this helper sets all the related attrs together.
        applyContentType(this.htmlElement, attributeValue as string);
        return;
      case 'keyboardType':
        this.htmlElement.setAttribute('inputmode', attributeValue as string);
        return;
      case 'keyboardAppearance': // [iOS-Only]
        this.htmlElement.style.colorScheme = attributeValue as string;
        return;
      case 'returnKeyType':
        this.htmlElement.setAttribute('enterkeyhint', attributeValue as string);
        return;
      case 'returnKeyText':
        this.htmlElement.setAttribute('enterkeyhint', attributeValue as string);
        return;
      case 'autocapitalization': // Replaces autoCapitalize
        this.htmlElement.setAttribute('autocapitalize', attributeValue as string);
        return;
      case 'autocorrection': // Replaces autoCorrect
        this.htmlElement.setAttribute('autocorrect', attributeValue ? 'on' : 'off');
        return;
      case 'characterLimit': // Replaces maxLength
        this.htmlElement.setAttribute('maxlength', attributeValue as string);
        return;
      case 'enableInlinePredictions': // [iOS-Only]
        this.htmlElement.setAttribute('autocomplete', attributeValue ? 'on' : 'off');
        return;
      default:
      // Do nothing
    }

    super.changeAttribute(attributeName, attributeValue);
  }

  private replaceListener(key: string, eventType: string, handler: EventListener | undefined): void {
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
}

function applyOpacity(color: string, opacityValue: string): string {
  const opacity = parseFloat(opacityValue);
  // Opacity must be between 0 and 1.
  if (isNaN(opacity) || opacity < 0 || opacity > 1) {
    return color;
  }

  // If color already has alpha, we assume it's the one we want.
  // This handles the 'rgba(0, 0, 0, 0.1) 1 1 1 1' case where opacity is 1 but we want 0.1.
  if (color.startsWith('rgba') || color.startsWith('hsla')) {
    return color;
  }

  if (color.startsWith('rgb')) {
    // rgb(r, g, b)
    return color.replace('rgb', 'rgba').replace(')', `, ${opacity})`);
  }

  if (color.startsWith('#')) {
    let r = 0,
      g = 0,
      b = 0;
    if (color.length === 4) {
      // #RGB
      r = parseInt(color[1] + color[1], 16);
      g = parseInt(color[2] + color[2], 16);
      b = parseInt(color[3] + color[3], 16);
    } else if (color.length === 7) {
      // #RRGGBB
      r = parseInt(color.slice(1, 3), 16);
      g = parseInt(color.slice(3, 5), 16);
      b = parseInt(color.slice(5, 7), 16);
    } else {
      return color; // Invalid hex format
    }
    if ([r, g, b].some(isNaN)) {
      return color;
    } // check for parsing errors
    return `rgba(${r}, ${g}, ${b}, ${opacity})`;
  }

  // For named colors (e.g. "red"), this is hard without a canvas.
  return color;
}

type ContentTypeAttrs = {
  type: string;
  inputmode?: string;
  pattern?: string;
  autocomplete?: string;
  autocorrect?: string;
  spellcheck?: string;
};

// Mirrors the cross-platform contentType → input traits mapping used by the
// iOS (SCValdiTextInputSetContentTypeValues) and Android
// (EditTextAttributesBinder.applyContentType) binders. The native binders
// rewrite the full set of related traits on every change; we follow suit so
// stale state from a prior contentType can't leak through.
const CONTENT_TYPE_ATTRS: Record<string, ContentTypeAttrs> = {
  default: { type: 'text' },
  phoneNumber: { type: 'tel' },
  email: { type: 'email' },
  password: { type: 'password' },
  passwordNumber: { type: 'password', inputmode: 'numeric', pattern: '[0-9]*' },
  passwordVisible: { type: 'text', autocomplete: 'off' },
  url: { type: 'url' },
  number: { type: 'text', inputmode: 'numeric', pattern: '[0-9]*' },
  numberDecimal: { type: 'text', inputmode: 'decimal' },
  // Mirrors iOS UIKeyboardTypeNumbersAndPunctuation — full keyboard so the
  // minus key is reachable. inputmode=decimal hides minus on iOS.
  numberDecimalSigned: { type: 'text', inputmode: 'text', pattern: '-?[0-9]*[.,]?[0-9]*' },
  noSuggestions: { type: 'text', autocomplete: 'off', autocorrect: 'off', spellcheck: 'false' },
};

const CONTENT_TYPE_SECONDARY_ATTRS = ['inputmode', 'pattern', 'autocomplete', 'autocorrect', 'spellcheck'] as const;

function setOrRemoveAttr(element: HTMLElement, name: string, value: string | undefined): void {
  if (value === undefined) {
    if (element.hasAttribute(name)) {
      element.removeAttribute(name);
    }
  } else if (element.getAttribute(name) !== value) {
    element.setAttribute(name, value);
  }
}

function applyContentType(element: HTMLElement, type: string): void {
  const attrs = CONTENT_TYPE_ATTRS[type] ?? CONTENT_TYPE_ATTRS.default;
  setOrRemoveAttr(element, 'type', attrs.type);
  for (const name of CONTENT_TYPE_SECONDARY_ATTRS) {
    setOrRemoveAttr(element, name, attrs[name]);
  }
}
