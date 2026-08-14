import { AttributeApplier, AttributeApplierContext } from '../core/ElementClass';
import {
  parseBoolean,
  parseCssLength,
  parseCssTrackList,
  parseNumber,
  parseString,
  resolveValdiGradientAngles,
  StyleStringName,
} from './AttributeApplierHelpers';

export const MIN_VISIBLE_CHANGE_ALPHA = 0.0039;
export const MIN_VISIBLE_CHANGE_COLOR = 0.0039;
export const MIN_VISIBLE_CHANGE_PIXEL = 0.00016;

type AttributeApply<TElement extends HTMLElement, TValue> = (
  element: TElement,
  value: TValue,
  context: AttributeApplierContext,
  attributeName: string,
) => void;

type AttributeReset<TElement extends HTMLElement> = (
  element: TElement,
  context: AttributeApplierContext,
  attributeName: string,
) => void;

const SUPPORTS_COLOR_MIX =
  typeof CSS === 'undefined' ||
  (typeof CSS.supports === 'function' && CSS.supports('color', 'color-mix(in srgb, black 50%, white 50%)'));

function resolveColorAnimationEndpoint(
  value: unknown,
  resetColor: string,
  attributeName: string,
  context: AttributeApplierContext,
): string {
  return value === undefined || value === null ? resetColor : context.resolveColor(parseString(value, attributeName));
}

export class AttributesBinder<TElement extends HTMLElement> {
  readonly attributeAppliers: Record<string, AttributeApplier<TElement>> = {};

  bindAttribute(name: string, applier: AttributeApplier<TElement>): void {
    this.attributeAppliers[name] = applier;
  }

  bindNoOpAttribute(name: string, layoutDependent?: boolean): void {
    this.bindAttribute(name, {
      layoutDependent,
      apply() {},
      reset() {},
    });
  }

  bindDirectAttribute(name: string, domAttributeName: string, layoutDependent?: boolean): void {
    this.bindAttribute(name, {
      layoutDependent,
      apply(element, value) {
        element.setAttribute(domAttributeName, String(value));
      },
      reset(element) {
        element.removeAttribute(domAttributeName);
      },
    });
  }

  bindAriaBooleanAttribute(name: string, ariaAttributeName: string, layoutDependent?: boolean): void {
    this.bindAttribute(name, {
      layoutDependent,
      apply(element, value) {
        element.setAttribute(ariaAttributeName, String(!!value));
      },
      reset(element) {
        element.removeAttribute(ariaAttributeName);
      },
    });
  }

  bindNumberAttribute(
    name: string,
    apply: AttributeApply<TElement, number>,
    reset: AttributeReset<TElement>,
    layoutDependent?: boolean,
  ): void {
    this.bindAttribute(name, {
      layoutDependent,
      apply(element, value, attributeName, context) {
        apply(element, parseNumber(value, attributeName), context, attributeName);
      },
      reset(element, attributeName, context) {
        reset(element, context, attributeName);
      },
    });
  }

  bindAnimatableNumberAttribute(
    name: string,
    resetValue: number,
    minimumVisibleChange: number,
    apply: AttributeApply<TElement, number>,
    reset: AttributeReset<TElement>,
  ): void {
    const endpoint = (value: unknown): number =>
      value === undefined || value === null ? resetValue : parseNumber(value, name);
    this.bindAttribute(name, {
      animationMinimumVisibleChange: minimumVisibleChange,
      makeAnimationInterpolator(_element, from, to, _context) {
        const start = endpoint(from);
        const end = endpoint(to);
        return progress => start + (end - start) * progress;
      },
      apply(element, value, attributeName, context) {
        apply(element, parseNumber(value, attributeName), context, attributeName);
      },
      reset(element, attributeName, context) {
        reset(element, context, attributeName);
      },
    });
  }

  bindBooleanAttribute(
    name: string,
    apply: AttributeApply<TElement, boolean>,
    reset: AttributeReset<TElement>,
    layoutDependent?: boolean,
  ): void {
    this.bindAttribute(name, {
      layoutDependent,
      apply(element, value, attributeName, context) {
        apply(element, parseBoolean(value, attributeName), context, attributeName);
      },
      reset(element, attributeName, context) {
        reset(element, context, attributeName);
      },
    });
  }

  bindStringAttribute(
    name: string,
    apply: AttributeApply<TElement, string>,
    reset: AttributeReset<TElement>,
    layoutDependent?: boolean,
  ): void {
    this.bindAttribute(name, {
      layoutDependent,
      apply(element, value, attributeName, context) {
        apply(element, parseString(value, attributeName), context, attributeName);
      },
      reset(element, attributeName, context) {
        reset(element, context, attributeName);
      },
    });
  }

  bindFunctionAttribute(
    name: string,
    apply: AttributeApply<TElement, Function>,
    reset: AttributeReset<TElement>,
    layoutDependent?: boolean,
  ): void {
    this.bindAttribute(name, {
      layoutDependent,
      apply(element, value, attributeName, context) {
        if (typeof value !== 'function') {
          throw new Error(`Expected '${attributeName}' to be a function`);
        }
        apply(element, value, context, attributeName);
      },
      reset(element, attributeName, context) {
        reset(element, context, attributeName);
      },
    });
  }

  bindEnumAttribute<TValue extends string>(
    name: string,
    allowedValues: ReadonlyArray<TValue>,
    apply: AttributeApply<TElement, TValue>,
    reset: AttributeReset<TElement>,
    layoutDependent?: boolean,
  ): void {
    const allowed = new Set<string>(allowedValues);
    this.bindAttribute(name, {
      layoutDependent,
      apply(element, value, attributeName, context) {
        const parsed = parseString(value, attributeName);
        if (!allowed.has(parsed)) {
          throw new Error(`Invalid '${attributeName}' value '${parsed}'`);
        }
        apply(element, parsed as TValue, context, attributeName);
      },
      reset(element, attributeName, context) {
        reset(element, context, attributeName);
      },
    });
  }

  bindCssLengthStyleAttribute(name: string, styleName: StyleStringName, layoutDependent?: boolean): void {
    this.bindAttribute(name, {
      layoutDependent,
      apply(element, value, attributeName) {
        element.style[styleName] = parseCssLength(value, attributeName);
      },
      reset(element) {
        element.style[styleName] = '';
      },
    });
  }

  bindCssTrackListStyleAttribute(name: string, styleName: StyleStringName, layoutDependent?: boolean): void {
    this.bindAttribute(name, {
      layoutDependent,
      apply(element, value, attributeName) {
        element.style[styleName] = parseCssTrackList(value, attributeName);
      },
      reset(element) {
        element.style[styleName] = '';
      },
    });
  }

  bindStyleValueAttribute(name: string, styleName: StyleStringName, layoutDependent?: boolean): void {
    this.bindAttribute(name, {
      layoutDependent,
      apply(element, value) {
        element.style[styleName] = String(value);
      },
      reset(element) {
        element.style[styleName] = '';
      },
    });
  }

  bindColorAttribute(
    name: string,
    apply: AttributeApply<TElement, string>,
    reset: AttributeReset<TElement>,
    layoutDependent?: boolean,
  ): void {
    this.bindAttribute(name, {
      colorDependent: true,
      layoutDependent,
      apply(element, value, attributeName, context) {
        apply(element, context.resolveColor(parseString(value, attributeName)), context, attributeName);
      },
      reset(element, attributeName, context) {
        reset(element, context, attributeName);
      },
    });
  }

  bindAnimatableColorAttribute(
    name: string,
    resetColor: string,
    minimumVisibleChange: number,
    apply: AttributeApply<TElement, string>,
    reset: AttributeReset<TElement>,
  ): void {
    const applier: AttributeApplier<TElement> = {
      colorDependent: true,
      apply(element, value, attributeName, context) {
        apply(element, context.resolveColor(parseString(value, attributeName)), context, attributeName);
      },
      reset(element, attributeName, context) {
        reset(element, context, attributeName);
      },
    };
    if (SUPPORTS_COLOR_MIX) {
      applier.animationMinimumVisibleChange = minimumVisibleChange;
      applier.makeAnimationInterpolator = (_element, from, to, context) => {
        const start = resolveColorAnimationEndpoint(from, resetColor, name, context);
        const end = resolveColorAnimationEndpoint(to, resetColor, name, context);
        return progress => {
          const clamped = Math.max(0, Math.min(1, progress));
          if (clamped <= 0) {
            return start;
          }
          if (clamped >= 1) {
            return end;
          }
          const endWeight = clamped * 100;
          return `color-mix(in srgb, ${start} ${100 - endWeight}%, ${end} ${endWeight}%)`;
        };
      };
    }
    this.bindAttribute(name, applier);
  }

  bindColorStyleAttribute(
    name: string,
    styleName: StyleStringName,
    resetValue: string,
    layoutDependent?: boolean,
  ): void {
    this.bindColorAttribute(
      name,
      (element, value) => {
        element.style[styleName] = resolveValdiGradientAngles(value);
      },
      element => {
        element.style[styleName] = resetValue;
      },
      layoutDependent,
    );
  }
}
