import { AttributesBinder } from '../attributes/AttributesBinder';
import { AttributeApplier, AttributeApplierContext, ElementClass, UnknownAttributeApplier } from '../core/ElementClass';
import { getWebViewClassFactory, WebViewClassAttributeHandler } from '../WebViewClassRegistry';
import { assignStyles, createBaseElement } from './ElementClassSupport';
import { ViewElementClass } from './ViewElementClass';

interface CustomViewState {
  webClassApplied: boolean;
  attributeHandler?: WebViewClassAttributeHandler;
  pendingAttributes: Array<[string, unknown]>;
}

const CUSTOM_VIEW_STATE = '__customViewElementClassState';
const LAYOUT_DEPENDENT = true;

function getCustomViewState(context: AttributeApplierContext): CustomViewState {
  const existing = context.getState<CustomViewState>(CUSTOM_VIEW_STATE);
  if (existing) {
    return existing;
  }
  const state: CustomViewState = {
    webClassApplied: false,
    pendingAttributes: [],
  };
  context.setState(CUSTOM_VIEW_STATE, state);
  return state;
}

function appendPlaceholder(element: HTMLElement, message: string): void {
  element.style.position = 'relative';
  const label = document.createElement('span');
  label.textContent = message;
  Object.assign(label.style, {
    alignItems: 'center',
    color: 'inherit',
    display: 'flex',
    fontSize: '14px',
    inset: '0',
    justifyContent: 'center',
    pointerEvents: 'none',
    position: 'absolute',
  });
  element.appendChild(label);
}

function forwardCustomViewAttribute(context: AttributeApplierContext, attributeName: string, value: unknown): void {
  const state = getCustomViewState(context);
  if (state.attributeHandler) {
    state.attributeHandler.changeAttribute(attributeName, value);
  } else if (!state.webClassApplied) {
    state.pendingAttributes.push([attributeName, value]);
  }
}

const customViewUnknownAttributeApplier: UnknownAttributeApplier = {
  layoutDependent: true,
  apply(_element, value, attributeName, context) {
    forwardCustomViewAttribute(context, attributeName, value);
  },
  reset(_element, attributeName, context) {
    forwardCustomViewAttribute(context, attributeName, undefined);
  },
};

function buildCustomViewAttributeAppliers(viewElementClass: ViewElementClass): Record<string, AttributeApplier> {
  const binder = new AttributesBinder<HTMLElement>();
  binder.bindNoOpAttribute('androidClass');
  binder.bindNoOpAttribute('iosClass');
  binder.bindNoOpAttribute('macosClass');
  binder.bindStringAttribute(
    'webClass',
    (element, value, context) => {
      if (value.length === 0) {
        throw new Error("Expected 'webClass' to be a non-empty string");
      }
      const state = getCustomViewState(context);
      if (state.webClassApplied) {
        return;
      }
      state.webClassApplied = true;
      const factory = getWebViewClassFactory(value);
      if (factory) {
        element.replaceChildren();
        const result = factory(element);
        if (result) {
          state.attributeHandler = result;
          const destroy = result.destroy;
          if (destroy) {
            let destroyed = false;
            context.addCleanup(() => {
              if (destroyed) {
                return;
              }
              destroyed = true;
              state.attributeHandler = undefined;
              destroy.call(result);
            });
          }
        }
        for (let i = 0; i < state.pendingAttributes.length; i++) {
          const [name, pendingValue] = state.pendingAttributes[i];
          state.attributeHandler?.changeAttribute(name, pendingValue);
        }
        state.pendingAttributes.length = 0;
      } else {
        appendPlaceholder(element, value);
      }
      if (!element.style.height && !element.style.minHeight) {
        element.style.minHeight = '80px';
      }
    },
    () => {},
    LAYOUT_DEPENDENT,
  );
  return { ...viewElementClass.attributeAppliers, ...binder.attributeAppliers };
}

export class CustomViewElementClass extends ElementClass {
  constructor(viewElementClass: ViewElementClass) {
    super(
      'custom-view',
      buildCustomViewAttributeAppliers(viewElementClass),
      viewElementClass.compositeAttributes,
      customViewUnknownAttributeApplier,
    );
  }

  protected onCreateElement(): HTMLElement {
    const element = createBaseElement('div');
    assignStyles(element, { pointerEvents: 'auto' });
    return element;
  }
}
