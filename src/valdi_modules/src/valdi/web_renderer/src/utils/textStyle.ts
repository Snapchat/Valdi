export const SYSTEM_FONT_FAMILY = '-apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif';

const FONT_WEIGHTS = new Set([
  'normal',
  'bold',
  'lighter',
  'bolder',
  '100',
  '200',
  '300',
  '400',
  '500',
  '600',
  '700',
  '800',
  '900',
]);

export function cssLength(value: number | string): string {
  return typeof value === 'number' ? `${value}px` : value;
}

function applyFontDescriptor(styles: Record<string, string>, descriptor: string): void {
  styles.fontStyle = '';
  styles.fontWeight = '';

  if (descriptor === 'system' || descriptor === 'title' || descriptor.startsWith('system-')) {
    styles.fontFamily = SYSTEM_FONT_FAMILY;
    if (descriptor.includes('bold') || descriptor === 'title') {
      styles.fontWeight = '700';
    }
    if (descriptor.includes('italic')) {
      styles.fontStyle = 'italic';
    }
    return;
  }

  if (descriptor === 'bold') {
    styles.fontFamily = SYSTEM_FONT_FAMILY;
    styles.fontWeight = '700';
    return;
  }

  if (descriptor === 'italic') {
    styles.fontFamily = SYSTEM_FONT_FAMILY;
    styles.fontStyle = 'italic';
    return;
  }

  styles.fontFamily = descriptor;
}

export function fontStylesFromString(font: string): Record<string, string> {
  const parts = String(font).trim().split(/\s+/);
  const styles: Record<string, string> = {};
  if (parts.length === 0 || parts[0] === '') {
    return styles;
  }

  applyFontDescriptor(styles, parts[0]);
  if (parts.length > 1 && !Number.isNaN(Number(parts[1]))) {
    styles.fontSize = `${Number(parts[1])}px`;
  }
  if (parts.length > 2 && FONT_WEIGHTS.has(parts[2].toLowerCase())) {
    styles.fontWeight = parts[2];
  }
  return styles;
}

export function applyFontString(element: HTMLElement, font: string): void {
  Object.assign(element.style, fontStylesFromString(font));
}

export function applyTextDecoration(element: HTMLElement, value: string | undefined): void {
  element.style.textDecorationLine = '';
  element.style.textDecorationStyle = '';

  switch (value) {
    case 'underline':
      element.style.textDecorationLine = 'underline';
      return;
    case 'dashed-underline':
      element.style.textDecorationLine = 'underline';
      element.style.textDecorationStyle = 'dashed';
      return;
    case 'dotted-underline':
      element.style.textDecorationLine = 'underline';
      element.style.textDecorationStyle = 'dotted';
      return;
    case 'strikethrough':
      element.style.textDecorationLine = 'line-through';
      return;
    case 'none':
    case undefined:
    case '':
    default:
      element.style.textDecorationLine = 'none';
  }
}

function resolveColor(color: string): string {
  const globalObject = (globalThis as any).global ?? globalThis;
  return globalObject.currentPalette?.[color] ?? color;
}

function applyOpacity(color: string, opacityValue: string): string {
  const opacity = Number.parseFloat(opacityValue);
  if (Number.isNaN(opacity) || opacity < 0 || opacity > 1) {
    return color;
  }

  if (color.startsWith('rgba') || color.startsWith('hsla')) {
    return color;
  }

  if (color.startsWith('rgb')) {
    return color.replace('rgb', 'rgba').replace(')', `, ${opacity})`);
  }

  if (color.startsWith('#')) {
    let r = 0;
    let g = 0;
    let b = 0;
    if (color.length === 4) {
      r = Number.parseInt(color[1] + color[1], 16);
      g = Number.parseInt(color[2] + color[2], 16);
      b = Number.parseInt(color[3] + color[3], 16);
    } else if (color.length === 7) {
      r = Number.parseInt(color.slice(1, 3), 16);
      g = Number.parseInt(color.slice(3, 5), 16);
      b = Number.parseInt(color.slice(5, 7), 16);
    } else {
      return color;
    }
    if ([r, g, b].some(Number.isNaN)) {
      return color;
    }
    return `rgba(${r}, ${g}, ${b}, ${opacity})`;
  }

  return color;
}

export function textShadowCssValue(value: string): string | undefined {
  const match = String(value).match(/^(.+?)\s+(-?[0-9.]+)\s+(-?[0-9.]+)\s+(-?[0-9.]+)\s+(-?[0-9.]+)$/);
  if (!match) {
    return undefined;
  }
  const [, color, radius, opacity, offsetX, offsetY] = match;
  const resolvedColor = applyOpacity(resolveColor(color.trim()), opacity);
  return `${Number(offsetX)}px ${Number(offsetY)}px ${Number(radius)}px ${resolvedColor}`;
}
