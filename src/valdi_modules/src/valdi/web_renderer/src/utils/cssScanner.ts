const CHAR_TAB = 9;
const CHAR_LINE_FEED = 10;
const CHAR_FORM_FEED = 12;
const CHAR_CARRIAGE_RETURN = 13;
const CHAR_SPACE = 32;
const CHAR_OPEN_PAREN = 40;
const CHAR_CLOSE_PAREN = 41;
const CHAR_COMMA = 44;
const CHAR_DOT = 46;
const CHAR_MINUS = 45;
const CHAR_SINGLE_QUOTE = 39;
const CHAR_DOUBLE_QUOTE = 34;
const CHAR_BACKSLASH = 92;
const CHAR_UNDERSCORE = 95;
const CHAR_ZERO = 48;
const CHAR_NINE = 57;
const CHAR_UPPER_A = 65;
const CHAR_UPPER_Z = 90;
const CHAR_LOWER_A = 97;
const CHAR_LOWER_Z = 122;

export type CssNumberToken = {
  nextIndex: number;
  startIndex: number;
  text: string;
  value: number;
};

export type CssToken = {
  nextIndex: number;
  startIndex: number;
  token: string;
};

export type ParsedCssFunction = {
  name: string;
  parameters: string[];
};

export type ParsedCssFunctionCall = ParsedCssFunction & {
  nextIndex: number;
  startIndex: number;
};

export function isAsciiDigitCode(code: number): boolean {
  return code >= CHAR_ZERO && code <= CHAR_NINE;
}

export function isAsciiAlphaCode(code: number): boolean {
  return (code >= CHAR_UPPER_A && code <= CHAR_UPPER_Z) || (code >= CHAR_LOWER_A && code <= CHAR_LOWER_Z);
}

export function isCssWhitespaceCode(code: number): boolean {
  return (
    code === CHAR_TAB ||
    code === CHAR_LINE_FEED ||
    code === CHAR_FORM_FEED ||
    code === CHAR_CARRIAGE_RETURN ||
    code === CHAR_SPACE
  );
}

export function skipCssWhitespace(value: string, index: number): number {
  let nextIndex = index;
  while (nextIndex < value.length && isCssWhitespaceCode(value.charCodeAt(nextIndex))) {
    nextIndex++;
  }
  return nextIndex;
}

export function skipTrailingCssWhitespace(value: string, endIndex: number): number {
  let nextIndex = endIndex;
  while (nextIndex > 0 && isCssWhitespaceCode(value.charCodeAt(nextIndex - 1))) {
    nextIndex--;
  }
  return nextIndex;
}

export function trimCssStartIndex(value: string, start: number, end: number): number {
  let nextStart = start;
  while (nextStart < end && isCssWhitespaceCode(value.charCodeAt(nextStart))) {
    nextStart++;
  }
  return nextStart;
}

export function trimCssEndIndex(value: string, start: number, end: number): number {
  let nextEnd = end;
  while (nextEnd > start && isCssWhitespaceCode(value.charCodeAt(nextEnd - 1))) {
    nextEnd--;
  }
  return nextEnd;
}

export function consumeCssNumber(value: string, start: number): number {
  const length = value.length;
  let index = start;
  if (value.charCodeAt(index) === CHAR_MINUS) {
    index++;
  }

  const digitStart = index;
  while (index < length && isAsciiDigitCode(value.charCodeAt(index))) {
    index++;
  }
  const hasIntegerDigits = index > digitStart;
  if (index < length && value.charCodeAt(index) === CHAR_DOT) {
    const fractionStart = index + 1;
    let fractionEnd = fractionStart;
    while (fractionEnd < length && isAsciiDigitCode(value.charCodeAt(fractionEnd))) {
      fractionEnd++;
    }
    if (fractionEnd > fractionStart) {
      return fractionEnd;
    }
  }

  return hasIntegerDigits ? index : -1;
}

export function readCssNumber(value: string, index: number): CssNumberToken | undefined {
  const nextIndex = consumeCssNumber(value, index);
  if (nextIndex < 0) {
    return undefined;
  }
  const text = value.slice(index, nextIndex);
  return {
    nextIndex,
    startIndex: index,
    text,
    value: Number(text),
  };
}

export function isPlainCssNumber(value: string): boolean {
  if (value.length === 0) {
    return false;
  }
  const number = readCssNumber(value, 0);
  return number !== undefined && number.nextIndex === value.length;
}

export function readWhitespaceSeparatedToken(value: string, index: number): CssToken | undefined {
  const startIndex = skipCssWhitespace(value, index);
  if (startIndex >= value.length) {
    return undefined;
  }
  let nextIndex = startIndex;
  while (nextIndex < value.length && !isCssWhitespaceCode(value.charCodeAt(nextIndex))) {
    nextIndex++;
  }
  return { token: value.slice(startIndex, nextIndex), startIndex, nextIndex };
}

export function readPreviousWhitespaceSeparatedToken(value: string, endIndex: number): CssToken | undefined {
  const tokenEndIndex = skipTrailingCssWhitespace(value, endIndex);
  if (tokenEndIndex === 0) {
    return undefined;
  }

  let tokenStartIndex = tokenEndIndex;
  while (tokenStartIndex > 0 && !isCssWhitespaceCode(value.charCodeAt(tokenStartIndex - 1))) {
    tokenStartIndex--;
  }
  return {
    token: value.slice(tokenStartIndex, tokenEndIndex),
    startIndex: tokenStartIndex,
    nextIndex: tokenEndIndex,
  };
}

export function consumeLiteral(value: string, index: number, literal: string): number {
  return value.startsWith(literal, index) ? index + literal.length : -1;
}

function isCssFunctionNameCode(code: number): boolean {
  return isAsciiAlphaCode(code) || isAsciiDigitCode(code) || code === CHAR_MINUS || code === CHAR_UNDERSCORE;
}

function appendParameter(value: string, parameters: string[], start: number, end: number): void {
  const trimmedStart = trimCssStartIndex(value, start, end);
  const trimmedEnd = trimCssEndIndex(value, trimmedStart, end);
  parameters.push(value.slice(trimmedStart, trimmedEnd));
}

function appendFinalParameter(value: string, parameters: string[], start: number, end: number): void {
  const trimmedStart = trimCssStartIndex(value, start, end);
  const trimmedEnd = trimCssEndIndex(value, trimmedStart, end);
  if (parameters.length > 0 || trimmedStart !== trimmedEnd) {
    parameters.push(value.slice(trimmedStart, trimmedEnd));
  }
}

export function parseCssFunctionCall(value: string, index: number): ParsedCssFunctionCall | undefined {
  const length = value.length;
  const startIndex = skipCssWhitespace(value, index);
  let nextIndex = startIndex;
  const nameStart = nextIndex;
  while (nextIndex < length && isCssFunctionNameCode(value.charCodeAt(nextIndex))) {
    nextIndex++;
  }
  if (nextIndex === nameStart) {
    return undefined;
  }

  const nameEnd = nextIndex;
  nextIndex = skipCssWhitespace(value, nextIndex);
  if (nextIndex >= length || value.charCodeAt(nextIndex) !== CHAR_OPEN_PAREN) {
    return undefined;
  }

  nextIndex++;
  let parameterStart = nextIndex;
  let nestedDepth = 0;
  let quote = 0;
  const parameters: string[] = [];
  for (; nextIndex < length; nextIndex++) {
    const code = value.charCodeAt(nextIndex);
    if (quote !== 0) {
      if (code === CHAR_BACKSLASH) {
        nextIndex++;
      } else if (code === quote) {
        quote = 0;
      }
      continue;
    }

    if (code === CHAR_DOUBLE_QUOTE || code === CHAR_SINGLE_QUOTE) {
      quote = code;
      continue;
    }
    if (code === CHAR_OPEN_PAREN) {
      nestedDepth++;
      continue;
    }
    if (code === CHAR_CLOSE_PAREN) {
      if (nestedDepth > 0) {
        nestedDepth--;
        continue;
      }
      appendFinalParameter(value, parameters, parameterStart, nextIndex);
      return {
        name: value.slice(nameStart, nameEnd).toLowerCase(),
        nextIndex: nextIndex + 1,
        parameters,
        startIndex,
      };
    }
    if (code === CHAR_COMMA && nestedDepth === 0) {
      appendParameter(value, parameters, parameterStart, nextIndex);
      parameterStart = nextIndex + 1;
    }
  }
  return undefined;
}

export function parseCssFunction(value: string): ParsedCssFunction | undefined {
  const parsed = parseCssFunctionCall(value, 0);
  if (!parsed || skipCssWhitespace(value, parsed.nextIndex) !== value.length) {
    return undefined;
  }
  return {
    name: parsed.name,
    parameters: parsed.parameters,
  };
}
