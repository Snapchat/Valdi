import { AttributesBinder } from '../attributes/AttributesBinder';
import { AttributeApplier, AttributeApplierContext, ElementClass } from '../core/ElementClass';
import { isAttributedText, ParsedAttributedText } from '../utils/parseAttributedText';
import { textShadowCssValue } from '../utils/textStyle';
import {
  assignStyles,
  AttributeApplierMap,
  getActiveElement,
  replaceEventListener,
  setApplierCleanup,
  SYSTEM_FONT_FAMILY,
} from './ElementClassSupport';
import { LabelElementClass } from './LabelElementClass';

export type TextInputElement = HTMLInputElement | HTMLTextAreaElement;
const LAYOUT_DEPENDENT = true;

function editEvent(element: TextInputElement): { text: string; selectionStart: number; selectionEnd: number } {
  return {
    text: element.value,
    selectionStart: element.selectionStart ?? 0,
    selectionEnd: element.selectionEnd ?? 0,
  };
}

function textInputValueAttributeApplier(): AttributeApplier<TextInputElement> {
  return {
    layoutDependent: true,
    apply(element, value) {
      if (isAttributedText(value)) {
        element.value = ParsedAttributedText.parse(value).toString();
        return;
      }
      element.value = String(value);
    },
    reset(element) {
      element.value = '';
    },
  };
}

function setPlaceholderColor(
  element: TextInputElement,
  context: AttributeApplierContext,
  color: string | undefined,
): void {
  const className = `valdi-placeholder-${context.id}`;
  element.classList.add(className);
  if (!color) {
    setApplierCleanup(context, 'textfield:placeholderColor', undefined);
    return;
  }
  const style = document.createElement('style');
  style.textContent = `.${className}::placeholder { color: ${color}; opacity: 1; }`;
  const root = element.getRootNode();
  if (typeof ShadowRoot !== 'undefined' && root instanceof ShadowRoot) {
    root.appendChild(style);
  } else {
    document.head.appendChild(style);
  }
  setApplierCleanup(context, 'textfield:placeholderColor', () => {
    style.remove();
  });
}

function getContentType(type: string): string {
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

function buildEditTextAttributeAppliers(labelElementClass: LabelElementClass): AttributeApplierMap<TextInputElement> {
  const binder = new AttributesBinder<TextInputElement>();
  binder.bindFunctionAttribute(
    'onWillChange',
    (element, callback, context) => {
      replaceEventListener(element, context, 'textfield:onWillChange', 'beforeinput', event => {
        if (callback(editEvent(element)) === false) {
          event.preventDefault();
        }
      });
    },
    (element, context) => replaceEventListener(element, context, 'textfield:onWillChange', 'beforeinput', undefined),
  );
  binder.bindFunctionAttribute(
    'onChange',
    (element, callback, context) => {
      replaceEventListener(element, context, 'textfield:onChange', 'input', () => {
        const event = editEvent(element);
        context.onAttributeUpdatedExternally('value', event.text);
        callback(event);
      });
    },
    (element, context) => replaceEventListener(element, context, 'textfield:onChange', 'input', undefined),
  );
  binder.bindFunctionAttribute(
    'onEditBegin',
    (element, callback, context) => {
      replaceEventListener(element, context, 'textfield:onEditBegin', 'focus', () => callback(editEvent(element)));
    },
    (element, context) => replaceEventListener(element, context, 'textfield:onEditBegin', 'focus', undefined),
  );
  binder.bindFunctionAttribute(
    'onEditEnd',
    (element, callback, context) => {
      replaceEventListener(element, context, 'textfield:onEditEnd', 'blur', () =>
        callback({ ...editEvent(element), reason: 'blur' }),
      );
    },
    (element, context) => replaceEventListener(element, context, 'textfield:onEditEnd', 'blur', undefined),
  );
  binder.bindFunctionAttribute(
    'onReturn',
    (element, callback, context) => {
      replaceEventListener(element, context, 'textfield:onReturn', 'keydown', event => {
        if (event.key === 'Enter') {
          callback(editEvent(element));
        }
      });
    },
    (element, context) => replaceEventListener(element, context, 'textfield:onReturn', 'keydown', undefined),
  );
  binder.bindFunctionAttribute(
    'onWillDelete',
    (element, callback, context) => {
      replaceEventListener(element, context, 'textfield:onWillDelete', 'keydown', event => {
        if (event.key === 'Backspace' || event.key === 'Delete') {
          callback(editEvent(element));
        }
      });
    },
    (element, context) => replaceEventListener(element, context, 'textfield:onWillDelete', 'keydown', undefined),
  );
  binder.bindFunctionAttribute(
    'onSelectionChange',
    (element, callback, context) => {
      const listener = () => {
        if (getActiveElement(element) === element) {
          callback(editEvent(element));
        }
      };
      document.addEventListener('selectionchange', listener);
      setApplierCleanup(context, 'textfield:onSelectionChange', () => {
        document.removeEventListener('selectionchange', listener);
      });
    },
    (_element, context) => setApplierCleanup(context, 'textfield:onSelectionChange', undefined),
  );
  binder.bindColorAttribute(
    'tintColor',
    (element, value) => {
      element.style.caretColor = value;
    },
    element => {
      element.style.caretColor = '';
    },
  );
  binder.bindColorAttribute(
    'placeholderColor',
    (element, value, context) => {
      setPlaceholderColor(element, context, value);
    },
    (element, context) => {
      setPlaceholderColor(element, context, undefined);
    },
  );
  binder.bindStringAttribute(
    'textGradient',
    (element, value) => {
      element.style.backgroundImage = value;
      element.style.backgroundClip = 'text';
      element.style.webkitBackgroundClip = 'text';
      element.style.color = 'transparent';
    },
    element => {
      element.style.backgroundImage = '';
      element.style.backgroundClip = '';
      element.style.webkitBackgroundClip = '';
      element.style.color = '';
    },
  );
  binder.bindStringAttribute(
    'textShadow',
    (element, value, context) => {
      const shadowCssValue = textShadowCssValue(value, context);
      if (shadowCssValue !== undefined) {
        element.style.textShadow = shadowCssValue;
      }
    },
    element => {
      element.style.textShadow = '';
    },
  );
  binder.bindAttribute('value', textInputValueAttributeApplier());
  binder.bindStringAttribute(
    'placeholder',
    (element, value) => {
      element.placeholder = value;
    },
    element => {
      element.placeholder = '';
    },
    LAYOUT_DEPENDENT,
  );
  binder.bindBooleanAttribute(
    'enabled',
    (element, enabled) => {
      element.disabled = !enabled;
    },
    element => {
      element.disabled = false;
    },
  );
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
  binder.bindBooleanAttribute(
    'selectTextOnFocus',
    (element, value, context) => {
      if (!value) {
        replaceEventListener(element, context, 'textfield:selectTextOnFocus', 'focus', undefined);
        return;
      }
      replaceEventListener(element, context, 'textfield:selectTextOnFocus', 'focus', () => {
        element.select();
      });
    },
    (element, context) => replaceEventListener(element, context, 'textfield:selectTextOnFocus', 'focus', undefined),
  );
  binder.bindBooleanAttribute(
    'closesWhenReturnKeyPressed',
    (element, value, context) => {
      if (!value) {
        replaceEventListener(element, context, 'textfield:closesWhenReturnKeyPressed', 'keydown', undefined);
        return;
      }
      replaceEventListener(element, context, 'textfield:closesWhenReturnKeyPressed', 'keydown', event => {
        if (event.key === 'Enter') {
          element.blur();
        }
      });
    },
    (element, context) =>
      replaceEventListener(element, context, 'textfield:closesWhenReturnKeyPressed', 'keydown', undefined),
  );
  binder.bindStringAttribute(
    'contentType',
    (element, value) => {
      element.setAttribute('type', getContentType(value));
    },
    element => {
      element.setAttribute('type', 'text');
    },
  );
  binder.bindStringAttribute(
    'keyboardType',
    (element, value) => {
      element.setAttribute('inputmode', value);
    },
    element => {
      element.removeAttribute('inputmode');
    },
  );
  binder.bindStringAttribute(
    'keyboardAppearance',
    (element, value) => {
      element.style.colorScheme = value;
    },
    element => {
      element.style.colorScheme = '';
    },
  );
  binder.bindStringAttribute(
    'returnKeyType',
    (element, value) => {
      element.setAttribute('enterkeyhint', value);
    },
    element => {
      element.removeAttribute('enterkeyhint');
    },
  );
  binder.bindStringAttribute(
    'returnKeyText',
    (element, value) => {
      element.setAttribute('enterkeyhint', value);
    },
    element => {
      element.removeAttribute('enterkeyhint');
    },
  );
  binder.bindStringAttribute(
    'returnType',
    (element, value) => {
      element.setAttribute('enterkeyhint', value === 'linereturn' ? 'enter' : value);
    },
    element => {
      element.removeAttribute('enterkeyhint');
    },
  );
  binder.bindStringAttribute(
    'autocapitalization',
    (element, value) => {
      element.setAttribute('autocapitalize', value);
    },
    element => {
      element.removeAttribute('autocapitalize');
    },
  );
  binder.bindStringAttribute(
    'autocorrection',
    (element, value) => {
      // HTML also accepts "on", but Valdi exposes only "default" and "none".
      // Preserve the user agent's default behavior by omitting the attribute.
      if (value === 'default') {
        element.removeAttribute('autocorrect');
        return;
      }
      if (value === 'none') {
        element.setAttribute('autocorrect', 'off');
        return;
      }
      throw new Error(`Unsupported autocorrection value '${value}'`);
    },
    element => {
      element.removeAttribute('autocorrect');
    },
  );
  binder.bindNumberAttribute(
    'characterLimit',
    (element, value) => {
      element.maxLength = value;
    },
    element => {
      element.removeAttribute('maxlength');
    },
  );
  binder.bindBooleanAttribute(
    'enableInlinePredictions',
    (element, value) => {
      element.setAttribute('autocomplete', value ? 'on' : 'off');
    },
    element => {
      element.removeAttribute('autocomplete');
    },
  );
  binder.bindBooleanAttribute(
    'selectable',
    (element, value) => {
      element.style.userSelect = value === false ? 'none' : '';
    },
    element => {
      element.style.userSelect = '';
    },
  );
  binder.bindNoOpAttribute('textGravity');
  binder.bindNoOpAttribute('backgroundEffectColor');
  binder.bindNoOpAttribute('backgroundEffectBorderRadius');
  binder.bindCssLengthStyleAttribute('backgroundEffectPadding', 'padding', LAYOUT_DEPENDENT);
  binder.bindAttribute('selection', {
    apply(element, value) {
      if (!Array.isArray(value) || value.length !== 2) {
        throw new Error('Expected selection to be a two item array');
      }
      element.setSelectionRange(Number(value[0]), Number(value[1]));
    },
    reset(element) {
      element.setSelectionRange(0, 0);
    },
  });
  return {
    ...(labelElementClass.attributeAppliers as AttributeApplierMap<TextInputElement>),
    ...binder.attributeAppliers,
  };
}

export class TextFieldElementClass extends ElementClass<TextInputElement> {
  constructor(labelElementClass: LabelElementClass) {
    super('textfield', buildEditTextAttributeAppliers(labelElementClass), labelElementClass.compositeAttributes);
  }

  protected onCreateElement(): TextInputElement {
    const element = document.createElement('input');
    element.setAttribute('type', 'text');
    assignStyles(element, {
      backgroundColor: 'transparent',
      border: '0',
      fontFamily: SYSTEM_FONT_FAMILY,
      margin: 0,
      outline: 'none',
      padding: 0,
      pointerEvents: 'auto',
    });
    return element;
  }
}
