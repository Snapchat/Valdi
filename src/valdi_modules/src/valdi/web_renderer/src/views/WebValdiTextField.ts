import { UpdateAttributeDelegate } from '../ValdiWebRendererDelegate';
import { WebValdiLayout } from './WebValdiLayout';
import { convertColor } from '../styles/ValdiWebStyles';

class ValdiInput extends HTMLElement {
  private input: HTMLInputElement;
  private onEditEnd: (event: { value: string, reason: string }) => void = () => {};
  private onSelectionChange: (event: { value: string, selection: { start: number | null, end: number | null } }) => void = () => {};
  private attributeDelegate?: UpdateAttributeDelegate;

  constructor() {
    super();

    const shadow = this.attachShadow({ mode: 'open' });

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

    const handleInput = this.debounce((event: Event, reason: string) => {
      const value = this.input.value;
      this.attributeDelegate?.updateAttribute(Number(this.input.getAttribute("id")), "value", value);
      this.onEditEnd({ value, reason });
    }, 300).bind(this);

    this.input.addEventListener('blur', (event) => handleInput(event, 'blur'));
    this.input.addEventListener('keydown', (event: KeyboardEvent) => {
      if (event.key === 'Enter') {
        handleInput(event, 'return');
      }
    });
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
            selection: { start: this.input.selectionStart, end: this.input.selectionEnd }
        });
    }
  };
  connectedCallback() {
    for (const { name, value } of Array.from(this.attributes)) {
      this.forwardAttribute(name, value);
    }

    let observer = new MutationObserver((mutations) => {
      for (const mutation of mutations) {
        if (mutation.type === 'attributes' && mutation.attributeName) {
          const name = mutation.attributeName;
          const value = this.getAttribute(name);
          this.forwardAttribute(name, value);
        }
      }
    })

    observer.observe(this, { attributes: true, childList: true, subtree: true});
    document.addEventListener('selectionchange', this._handleSelectionChange);
  }

  disconnectedCallback() {
    document.removeEventListener('selectionchange', this._handleSelectionChange);
  }

  // Can't be passed through the attribute system, it'll get turned into a string?
  setOnEditEnd(value: (event: { value: string, reason: string }) => void) {
    this.onEditEnd = value;
  }

  setOnSelectionChange(callback: (event: { value: string, selection: { start: number | null, end: number | null } }) => void) {
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
      this.attributeDelegate?.updateAttribute(Number(this.input.getAttribute("id")), name, value);
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
  public declare htmlElement: ValdiInput;

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

  changeAttribute(attributeName: string, attributeValue: any): void {
    switch (attributeName) {
      // Callbacks
      case 'onWillChange':
        // Note: This only supports preventing the change, not modifying the incoming text.
        this.htmlElement.addEventListener('beforeinput', (event: InputEvent) => {
          const result = attributeValue({ value: (event.target as HTMLInputElement).value });
          if (result === undefined) {
            event.preventDefault();
          }
        });
        return;
      case 'onChange': // Replaces onChangeText
        this.htmlElement.addEventListener('input', (event) => attributeValue({ value: (event.target as HTMLInputElement).value }));
        return;
      case 'onEditBegin': // Replaces onFocus
        this.htmlElement.addEventListener('focus', (event) => attributeValue({ value: (event.target as HTMLInputElement).value }));
        return;
      case 'onEditEnd': // Replaces onBlur
        this.htmlElement.setOnEditEnd(attributeValue);
        return;
      case 'onReturn':
        this.htmlElement.addEventListener('keydown', (event: KeyboardEvent) => {
            if (event.key === 'Enter') {
                attributeValue({ value: (event.target as HTMLInputElement).value });
            }
        });
        return;
      case 'onWillDelete':
        this.htmlElement.addEventListener('keydown', (event: KeyboardEvent) => {
            if (event.key === 'Backspace' || event.key === 'Delete') {
                attributeValue({ value: (event.target as HTMLInputElement).value });
            }
        });
        return;
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
        this.htmlElement.style.textAlign = attributeValue;
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
        this.htmlElement.style.color = convertColor(attributeValue);
        return;
      case 'textGradient':
        this.htmlElement.style.backgroundImage = attributeValue;
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
        this.htmlElement.setAttribute(attributeName, attributeValue);
        return;
      case 'value':
        this.htmlElement.setAttribute(attributeName, attributeValue);
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
        if (attributeValue) {
          this.htmlElement.removeAttribute('disabled');
        } else {
          this.htmlElement.setAttribute('disabled', 'true');
        }
        return;
      case 'selectTextOnFocus':
        if (attributeValue) {
          this.htmlElement.addEventListener('focus', () => this.htmlElement.select());
        }
        // Note: This event listener is not removed if the property is later set to false.
        return;
      case 'closesWhenReturnKeyPressed':
        if (attributeValue !== false) { // default true
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

    if (color.startsWith('rgb')) { // rgb(r, g, b)
        return color.replace('rgb', 'rgba').replace(')', `, ${opacity})`);
    }

    if (color.startsWith('#')) {
        let r = 0, g = 0, b = 0;
        if (color.length === 4) { // #RGB
            r = parseInt(color[1] + color[1], 16);
            g = parseInt(color[2] + color[2], 16);
            b = parseInt(color[3] + color[3], 16);
        } else if (color.length === 7) { // #RRGGBB
            r = parseInt(color.slice(1, 3), 16);
            g = parseInt(color.slice(3, 5), 16);
            b = parseInt(color.slice(5, 7), 16);
        } else {
            return color; // Invalid hex format
        }
        if ([r,g,b].some(isNaN)) { return color; } // check for parsing errors
        return `rgba(${r}, ${g}, ${b}, ${opacity})`;
    }

    // For named colors (e.g. "red"), this is hard without a canvas.
    return color;
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
