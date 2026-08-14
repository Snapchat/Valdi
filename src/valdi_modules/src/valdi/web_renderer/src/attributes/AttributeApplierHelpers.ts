import { parseCssFunction } from '../utils/cssFunction';
import { consumeCssNumber, isAsciiAlphaCode, isAsciiDigitCode, isCssWhitespaceCode } from '../utils/cssScanner';

const CHAR_HASH = 35;
const CHAR_PERCENT = 37;
const CHAR_DOT = 46;
const CHAR_MINUS = 45;
const CHAR_UNDERSCORE = 95;

export type StyleStringName = {
  [K in keyof CSSStyleDeclaration]: CSSStyleDeclaration[K] extends string ? K : never;
}[keyof CSSStyleDeclaration] &
  string;

function isNullOrUndefined(value: unknown): value is null | undefined {
  return value === null || value === undefined;
}

function blocksUnitlessNumberPrefix(code: number): boolean {
  return (
    isAsciiAlphaCode(code) ||
    isAsciiDigitCode(code) ||
    code === CHAR_UNDERSCORE ||
    code === CHAR_DOT ||
    code === CHAR_PERCENT ||
    code === CHAR_HASH ||
    code === CHAR_MINUS
  );
}

function blocksUnitlessNumberSuffix(code: number): boolean {
  return isAsciiAlphaCode(code) || isAsciiDigitCode(code) || code === CHAR_DOT || code === CHAR_PERCENT;
}

export function parseNumber(value: unknown, attributeName: string): number {
  if (typeof value !== 'number' || !Number.isFinite(value)) {
    throw new Error(`Expected '${attributeName}' to be a finite number`);
  }
  return value;
}

export function parseBoolean(value: unknown, attributeName: string): boolean {
  if (typeof value !== 'boolean') {
    throw new Error(`Expected '${attributeName}' to be a boolean`);
  }
  return value;
}

export function parseString(value: unknown, attributeName: string): string {
  if (typeof value !== 'string') {
    throw new Error(`Expected '${attributeName}' to be a string`);
  }
  return value;
}

function isRepeatFunctionCount(value: string, numberStart: number): boolean {
  let index = numberStart - 1;
  while (index >= 0 && isCssWhitespaceCode(value.charCodeAt(index))) {
    index--;
  }
  if (index < 6 || value.charCodeAt(index) !== 40) {
    return false;
  }
  return value.slice(index - 6, index) === 'repeat';
}

function shouldAppendPxToCssNumber(
  value: string,
  numberStart: number,
  numberEnd: number,
  skipRepeatCount: boolean,
): boolean {
  if (numberStart > 0 && blocksUnitlessNumberPrefix(value.charCodeAt(numberStart - 1))) {
    return false;
  }
  if (numberEnd < value.length && blocksUnitlessNumberSuffix(value.charCodeAt(numberEnd))) {
    return false;
  }
  return !skipRepeatCount || !isRepeatFunctionCount(value, numberStart);
}

function appendPxToUnitlessCssNumbers(value: string, skipRepeatCount: boolean): string {
  let output = '';
  let copiedUntil = 0;
  let index = 0;
  while (index < value.length) {
    const end = consumeCssNumber(value, index);
    if (end < 0) {
      index++;
      continue;
    }

    if (shouldAppendPxToCssNumber(value, index, end, skipRepeatCount)) {
      output += value.slice(copiedUntil, end);
      output += 'px';
      copiedUntil = end;
    }
    index = end;
  }

  return output ? output + value.slice(copiedUntil) : value;
}

export function parseCssLength(value: unknown, attributeName: string): string {
  if (typeof value === 'number') {
    if (!Number.isFinite(value)) {
      throw new Error(`Expected '${attributeName}' to be a finite CSS length`);
    }
    return `${value}px`;
  }
  if (typeof value === 'string') {
    return appendPxToUnitlessCssNumbers(value, false);
  }
  throw new Error(`Expected '${attributeName}' to be a number or string CSS length`);
}

export function parseCssTrackList(value: unknown, attributeName: string): string {
  if (typeof value === 'number') {
    if (!Number.isFinite(value)) {
      throw new Error(`Expected '${attributeName}' to be a finite CSS track size`);
    }
    return `${value}px`;
  }
  if (typeof value === 'string') {
    return appendPxToUnitlessCssNumbers(value, true);
  }
  throw new Error(`Expected '${attributeName}' to be a number or string CSS track list`);
}

export function parseOptionalCssLength(value: unknown, attributeName: string): string | undefined {
  return isNullOrUndefined(value) ? undefined : parseCssLength(value, attributeName);
}

const LINEAR_GRADIENT_PREFIX = 'linear-gradient(';
const VALDI_GRADIENT_ANGLE_DEGREES = [180, 225, 270, 315, 0, 45, 90, 135];

function valdiGradientAngleToCssDegrees(angle: number, unit: string): number {
  const angleRad = unit === 'rad' ? angle : (angle * Math.PI) / 180;
  const valdiAngleIndex = Math.max(0, Math.min(7, Math.floor(angleRad / (Math.PI / 4))));
  return VALDI_GRADIENT_ANGLE_DEGREES[valdiAngleIndex];
}

function parseValdiGradientAngle(value: string): { angle: number; unit: 'deg' | 'rad' } | undefined {
  let unit: 'deg' | 'rad';
  let numberEnd: number;
  if (value.endsWith('deg')) {
    unit = 'deg';
    numberEnd = value.length - 3;
  } else if (value.endsWith('rad')) {
    unit = 'rad';
    numberEnd = value.length - 3;
  } else {
    return undefined;
  }

  const numberText = value.slice(0, numberEnd);
  if (numberText.length === 0) {
    return undefined;
  }
  const consumed = consumeCssNumber(numberText, 0);
  if (consumed !== numberText.length) {
    return undefined;
  }

  const angle = Number(numberText);
  return Number.isFinite(angle) ? { angle, unit } : undefined;
}

export function resolveValdiGradientAngles(value: string): string {
  const parsed = parseCssFunction(value);
  if (!parsed || parsed.name !== 'linear-gradient' || parsed.parameters.length === 0) {
    return value;
  }
  const angle = parseValdiGradientAngle(parsed.parameters[0]);
  if (!angle) {
    return value;
  }

  const cssAngle = valdiGradientAngleToCssDegrees(angle.angle, angle.unit);
  return `${LINEAR_GRADIENT_PREFIX}${[`${cssAngle}deg`, ...parsed.parameters.slice(1)].join(', ')})`;
}
