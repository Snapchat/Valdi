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
  // Include flexbox properties that might not be in VALID_STYLE_KEYS
  const flexboxProperties = ['justifyContent', 'alignItems', 'alignContent', 'alignSelf', 'flexDirection', 'flexWrap', 'flex', 'flexGrow', 'flexShrink', 'flexBasis'];
  // Include zIndex, boxShadow, and borderRadius explicitly since they're critical for styling
  const additionalProperties = ['zIndex', 'boxShadow', 'borderRadius'];
  const isValid = attribute in VALID_STYLE_KEYS || attribute === "style" || flexboxProperties.includes(attribute) || additionalProperties.includes(attribute);
  if (attribute === 'boxShadow' || attribute === 'borderRadius') {
    console.log('[isAttributeValidStyle]', { attribute, isValid, inValidKeys: attribute in VALID_STYLE_KEYS, inAdditional: additionalProperties.includes(attribute) });
  }
  return isValid;
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
    const result = {
      boxShadow: `${x}px ${y}px ${blur}px ${convertColor(color)}`,
    };
    console.log('[generateStyles] boxShadow:', { attribute, value, result });
    return result;
  }

  if (attribute === 'borderRadius') {
    // Handle borderRadius values - can be numbers (px), percentages, or other CSS values
    if (typeof value === 'string' && value.includes('%')) {
      // Percentage value like '50%' - pass through as-is
      const result = { borderRadius: value };
      console.log('[generateStyles] borderRadius (percentage):', { attribute, value, result });
      return result;
    }
    if (isNumber(value)) {
      // Single numeric value - add px unit
      return { borderRadius: `${value}px` };
    }
    if (typeof value === 'string') {
      // Handle space-separated numeric values (e.g., "50 25 10 5")
      const parts = value.trim().split(/\s+/);
      const processedParts = parts.map(part => {
        // If the part is a number, add 'px'; otherwise keep it as-is (already has units or is a keyword)
        if (!Number.isNaN(Number(part)) && part !== '') {
          return `${part}px`;
        }
        return part;
      });
      // Only modify if we actually changed something
      if (processedParts.some((part, i) => part !== parts[i])) {
        return { borderRadius: processedParts.join(' ') };
      }
    }
    // Other string values (like 'inherit', 'initial', or strings with units) - pass through as-is
    return { borderRadius: value };
  }

  // Handle zIndex explicitly to ensure it's properly applied
  if (attribute === 'zIndex') {
    // zIndex is a unitless number, but ensure we handle both number and string values
    if (value === undefined || value === null) {
      return { zIndex: '' };
    }
    const zIndexValue = typeof value === 'number' ? value : parseInt(String(value), 10) || 0;
    return { zIndex: String(zIndexValue) as any };
  }

  // Handle flexbox properties BEFORE the isAttributeValidStyle check
  // JavaScript style object uses camelCase
  if (attribute === 'justifyContent' || attribute === 'alignItems' || attribute === 'alignContent' || attribute === 'alignSelf' || 
      attribute === 'flexDirection' || attribute === 'flexWrap' || attribute === 'flex' || 
      attribute === 'flexGrow' || attribute === 'flexShrink' || attribute === 'flexBasis') {
    // Keep camelCase for JavaScript style object assignment
    return { [attribute]: value };
  }

  if (!isAttributeValidStyle(attribute)) {
    console.log('Unimplemented web style:', attribute, '=', value);
    return {};
  }

  if (isStyleKeyColor(attribute)) {
    return { [attribute]: convertColor(value) };
  }

  if (attribute === 'margin' || attribute === 'padding') {
    return handleMarginPadding(attribute, value);
  }

  // Handle undefined values - remove the CSS property
  if (value === undefined || value === null) {
    return { [attribute]: '' };
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