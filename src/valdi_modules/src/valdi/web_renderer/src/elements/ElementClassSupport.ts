import { parseCssLength } from '../attributes/AttributeApplierHelpers';
import { AttributeApplier, AttributeApplierContext } from '../core/ElementClass';
import { resolveRenderableAssetSource } from '../utils/assetSource';
import { isPlainCssNumber, readWhitespaceSeparatedToken, skipCssWhitespace } from '../utils/cssScanner';

export interface AttributeApplierMap<TElement extends HTMLElement = HTMLElement> {
  [name: string]: AttributeApplier<TElement>;
}

const BASE_LAYOUT_ITEM_STYLES: Record<string, string | number> = {
  flexShrink: 0,
  minHeight: 0,
  minWidth: 0,
  position: 'relative',
};

const BASE_ELEMENT_STYLES: Record<string, string | number> = {
  ...BASE_LAYOUT_ITEM_STYLES,
  display: 'flex',
  flexDirection: 'column',
};

export function assignStyles(element: HTMLElement, styles: Record<string, string | number>): void {
  Object.assign(element.style, styles);
}

export function getActiveElement(element: HTMLElement): Element | null {
  const root = element.getRootNode();
  if (typeof ShadowRoot !== 'undefined' && root instanceof ShadowRoot) {
    return root.activeElement;
  }
  return document.activeElement;
}

export function createBaseElement<K extends keyof HTMLElementTagNameMap>(tagName: K): HTMLElementTagNameMap[K] {
  const element = document.createElement(tagName);
  assignStyles(element, BASE_ELEMENT_STYLES);
  return element;
}

export function createBaseLayoutItemElement<K extends keyof HTMLElementTagNameMap>(
  tagName: K,
): HTMLElementTagNameMap[K] {
  const element = document.createElement(tagName);
  assignStyles(element, BASE_LAYOUT_ITEM_STYLES);
  return element;
}

export const SYSTEM_FONT_FAMILY = '-apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif';

function applyFontDescriptor(element: HTMLElement, descriptor: string): void {
  element.style.fontWeight = '';
  element.style.fontStyle = '';

  if (descriptor === 'system' || descriptor === 'title' || descriptor.startsWith('system-')) {
    element.style.fontFamily = SYSTEM_FONT_FAMILY;
    if (descriptor.includes('bold') || descriptor === 'title') {
      element.style.fontWeight = '700';
    }
    if (descriptor.includes('italic')) {
      element.style.fontStyle = 'italic';
    }
    return;
  }

  if (descriptor === 'bold') {
    element.style.fontFamily = SYSTEM_FONT_FAMILY;
    element.style.fontWeight = '700';
    return;
  }

  if (descriptor === 'italic') {
    element.style.fontFamily = SYSTEM_FONT_FAMILY;
    element.style.fontStyle = 'italic';
    return;
  }

  element.style.fontFamily = descriptor;
}

export function applyFontString(element: HTMLElement, font: string, attributeName: string): void {
  if (!font) {
    throw new Error(`Expected '${attributeName}' to be a non-empty font string`);
  }
  const parts = font.split(' ');
  applyFontDescriptor(element, parts[0]);
  if (parts.length > 1) {
    element.style.fontSize = parseCssLength(Number(parts[1]), attributeName);
  }
}

export function setApplierCleanup(
  context: AttributeApplierContext,
  key: string,
  cleanup: (() => void) | undefined,
): void {
  const cleanupByKeyKey = '__elementClassCleanupByKey';
  let cleanupByKey = context.getState<Record<string, (() => void) | undefined>>(cleanupByKeyKey);
  if (!cleanupByKey) {
    if (!cleanup) {
      return;
    }
    cleanupByKey = {};
    context.setState(cleanupByKeyKey, cleanupByKey);
  }
  cleanupByKey[key]?.();
  cleanupByKey[key] = cleanup;
  if (cleanup) {
    const cleanupMap = cleanupByKey;
    context.addCleanup(() => {
      if (cleanupMap[key] === cleanup) {
        cleanupMap[key] = undefined;
        cleanup();
      }
    });
  }
}

export function replaceEventListener(
  element: HTMLElement,
  context: AttributeApplierContext,
  key: string,
  eventName: string,
  listener: ((event: any) => void) | undefined,
): void {
  if (!listener) {
    setApplierCleanup(context, key, undefined);
    return;
  }
  element.addEventListener(eventName, listener);
  setApplierCleanup(context, key, () => {
    element.removeEventListener(eventName, listener);
  });
}

export function setFont(): AttributeApplier {
  return {
    layoutDependent: true,
    apply(element, value, attributeName) {
      applyFontString(element, String(value), attributeName);
    },
    reset(element) {
      element.style.fontFamily = '';
      element.style.fontSize = '';
      element.style.fontStyle = '';
      element.style.fontWeight = '';
    },
  };
}

export function borderAttributeApplier(): AttributeApplier {
  return {
    colorDependent: true,
    apply(_element, value, _attributeName, context) {
      const element = context.getViewAttributeElement();
      const border = String(value);
      if (!border) {
        element.style.border = '';
        return;
      }
      const widthToken = readWhitespaceSeparatedToken(border, 0);
      const styleToken = widthToken ? readWhitespaceSeparatedToken(border, widthToken.nextIndex) : undefined;
      if (!widthToken || !styleToken) {
        element.style.border = border;
        return;
      }
      const colorStartIndex = skipCssWhitespace(border, styleToken.nextIndex);
      if (colorStartIndex >= border.length) {
        element.style.border = border;
        return;
      }
      const width = isPlainCssNumber(widthToken.token) ? `${widthToken.token}px` : widthToken.token;
      const color = context.resolveColor(border.slice(colorStartIndex));
      element.style.border = `${width} ${styleToken.token} ${color}`;
    },
    reset(_element, _attributeName, context) {
      const element = context.getViewAttributeElement();
      element.style.border = '0 solid transparent';
    },
  };
}

export function resolveRenderableSrc(src: unknown): string | undefined {
  return resolveRenderableAssetSource(src);
}

export function srcAttributeApplier<
  TElement extends HTMLImageElement | HTMLVideoElement,
>(): AttributeApplier<TElement> {
  return {
    layoutDependent: true,
    apply(element, value) {
      const src = resolveRenderableSrc(value);
      if (src === undefined) {
        element.removeAttribute('src');
      } else {
        element.src = src;
      }
    },
    reset(element) {
      element.removeAttribute('src');
    },
  };
}
