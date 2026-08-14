import { parseCssFunction } from './cssFunction';

export type ParsedCssColor = { r: number; g: number; b: number; a: number };

function clampColorChannel(value: number): number {
  return Math.max(0, Math.min(255, Math.round(value)));
}

function clampUnit(value: number): number {
  return Math.max(0, Math.min(1, value));
}

function hexNibble(value: string, index: number): number {
  const code = value.charCodeAt(index);
  if (code >= 48 && code <= 57) {
    return code - 48;
  }
  const lowerCode = code | 32;
  if (lowerCode >= 97 && lowerCode <= 102) {
    return lowerCode - 87;
  }
  return -1;
}

function parseHexPair(value: string, index: number): number {
  const high = hexNibble(value, index);
  const low = hexNibble(value, index + 1);
  return high < 0 || low < 0 ? -1 : high * 16 + low;
}

function parseHexColor(value: string): ParsedCssColor | undefined {
  const start = value.charCodeAt(0) === 35 ? 1 : 0;
  const length = value.length - start;
  if (length === 3) {
    const r = hexNibble(value, start);
    const g = hexNibble(value, start + 1);
    const b = hexNibble(value, start + 2);
    if (r < 0 || g < 0 || b < 0) {
      return undefined;
    }
    return {
      r: r * 17,
      g: g * 17,
      b: b * 17,
      a: 1,
    };
  }
  if (length !== 6) {
    return undefined;
  }
  const r = parseHexPair(value, start);
  const g = parseHexPair(value, start + 2);
  const b = parseHexPair(value, start + 4);
  if (r < 0 || g < 0 || b < 0) {
    return undefined;
  }
  return {
    r,
    g,
    b,
    a: 1,
  };
}

function parseRgbFunctionColor(value: string): ParsedCssColor | undefined {
  const cssFunction = parseCssFunction(value);
  if (!cssFunction || (cssFunction.name !== 'rgb' && cssFunction.name !== 'rgba')) {
    return undefined;
  }
  const parameters = cssFunction.parameters;
  if (parameters.length !== 3 && parameters.length !== 4) {
    return undefined;
  }
  const r = parseFiniteNumber(parameters[0]);
  const g = parseFiniteNumber(parameters[1]);
  const b = parseFiniteNumber(parameters[2]);
  const a = parameters.length === 4 ? parseFiniteNumber(parameters[3]) : 1;
  if (r === undefined || g === undefined || b === undefined || a === undefined) {
    return undefined;
  }
  return {
    r: clampColorChannel(r),
    g: clampColorChannel(g),
    b: clampColorChannel(b),
    a: clampUnit(a),
  };
}

function parseFiniteNumber(value: string): number | undefined {
  if (value.length === 0) {
    return undefined;
  }
  const numberValue = Number(value);
  return Number.isFinite(numberValue) ? numberValue : undefined;
}

export function parseCssColor(value: string): ParsedCssColor | undefined {
  const trimmed = value.trim();
  return parseHexColor(trimmed) ?? parseRgbFunctionColor(trimmed);
}

export function applyCssColorOpacity(color: string, opacityValue: string): string {
  const opacity = parseFloat(opacityValue);
  if (Number.isNaN(opacity) || opacity < 0 || opacity > 1) {
    return color;
  }

  if (color.startsWith('hsla')) {
    return color;
  }

  const parsedColor = parseCssColor(color);
  if (!parsedColor) {
    return color;
  }
  const alpha = clampUnit(parsedColor.a * opacity);
  return `rgba(${parsedColor.r}, ${parsedColor.g}, ${parsedColor.b}, ${alpha})`;
}
