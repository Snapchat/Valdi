import { requiresUnitlessNumber } from './requiresUnitlessNumber';
import { isNumber } from './isNumber';
import { handleMarginPadding } from './handleMarginPadding';
const VALID_STYLE_KEYS = document.createElement('div').style;

declare const global: {
  currentPalette: any;
  darkModeObserver: object;
  theme: string;
};

export function isAttributeValidStyle(attribute: string): boolean {
  return attribute in VALID_STYLE_KEYS || attribute === "style";
}

export function isStyleKeyColor(attribute: string): boolean {
  return attribute.toLowerCase().includes('color');
}

export function convertColor(color: string): string {
  return global.currentPalette?.[color] ?? color;
}

export type RGBColor = { r: number; g: number; b: number };

export function hexToRGBColor(hex: string): RGBColor {
  // Remove leading #
  hex = hex.replace(/^#/, '');

  // Support short form (#f36 → #ff3366)
  if (hex.length === 3) {
    hex = hex.split('').map(c => c + c).join('');
  }

  if (hex.length !== 6) {
    throw new Error(`Invalid hex color: "${hex}"`);
  }

  const r = parseInt(hex.substring(0, 2), 16);
  const g = parseInt(hex.substring(2, 4), 16);
  const b = parseInt(hex.substring(4, 6), 16);

  return { r, g, b };
}

export function generateStyles(attribute: string, value: any): Partial<CSSStyleDeclaration> {
  if (attribute === 'font') {
    const [fontFamily, fontSize, fontWeight] = value.split(' ');
    return {
      fontFamily,
      fontSize: `${fontSize}px`,
      fontWeight,
    };
  }

  if (attribute === 'boxShadow') {
    const [x, y, blur, color] = value.split(' ');
    return {
      boxShadow: `${x}px ${y}px ${blur}px ${convertColor(color)}`,
    };
  }

  if (!isAttributeValidStyle(attribute)) {
    return {};
  }

  if (isStyleKeyColor(attribute)) {
    return { [attribute]: convertColor(value) };
  }

  if (attribute === 'margin' || attribute === 'padding') {
    return handleMarginPadding(attribute, value);
  }

  if (isNumber(value)) {
    if (requiresUnitlessNumber(attribute)) {
      return { [attribute]: value };
    } else {
      return { [attribute]: `${value}px` };
    }
  }

  return {
    [attribute]: value,
  };
}