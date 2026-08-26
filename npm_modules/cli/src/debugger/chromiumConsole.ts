import type { ChromiumDevToolsEvent } from '../utils/chromiumDevToolsClient';

export const MAX_CHROMIUM_CONSOLE_MESSAGE_LENGTH = 16_384;
const MAX_CONSOLE_ARGUMENTS = 64;
const MAX_CONSOLE_FRAGMENT_LENGTH = MAX_CHROMIUM_CONSOLE_MESSAGE_LENGTH * 2;
const MAX_PREVIEW_PROPERTIES = 12;
const MISSING_PROPERTY = Symbol('missing-property');
const UNAVAILABLE_PROPERTY = Symbol('unavailable-property');
const SENSITIVE_CONSOLE_KEY =
  /authorization|cookie|(?:access|refresh|id)[_-]?token|api[_-]?key|password|passwd|credential|secret|private[_-]?input/i;

export enum ChromiumConsoleLevel {
  Debug = 'debug',
  Error = 'error',
  Info = 'info',
  Log = 'log',
  Warning = 'warn',
}

export enum ChromiumConsoleSource {
  Browser = 'browser',
  Console = 'console',
  Exception = 'exception',
}

export interface ChromiumConsoleEntry {
  level: ChromiumConsoleLevel;
  message: string;
  source: ChromiumConsoleSource;
  timestamp: number;
}

type OwnDataProperty = unknown | typeof MISSING_PROPERTY | typeof UNAVAILABLE_PROPERTY;

function readOwnDataProperty(value: object, key: PropertyKey): OwnDataProperty {
  try {
    const descriptor = Object.getOwnPropertyDescriptor(value, key);
    if (descriptor === undefined) return MISSING_PROPERTY;
    return Object.prototype.hasOwnProperty.call(descriptor, 'value') ? descriptor.value : UNAVAILABLE_PROPERTY;
  } catch {
    return UNAVAILABLE_PROPERTY;
  }
}

function readOwnString(value: object, key: PropertyKey): string | undefined {
  const property = readOwnDataProperty(value, key);
  return typeof property === 'string' ? property : undefined;
}

function safeArrayLength(value: unknown): number | null {
  try {
    if (!Array.isArray(value)) return null;
  } catch {
    return null;
  }
  const length = readOwnDataProperty(value, 'length');
  return typeof length === 'number' && Number.isSafeInteger(length) && length >= 0 ? length : null;
}

function boundedFragment(value: string): string {
  const redacted = redactConsoleText(value);
  return redacted.length > MAX_CONSOLE_FRAGMENT_LENGTH
    ? `${redacted.slice(0, MAX_CONSOLE_FRAGMENT_LENGTH)}…`
    : redacted;
}

function formatPrimitive(value: unknown): string | null {
  switch (typeof value) {
    case 'string': {
      return boundedFragment(value);
    }
    case 'number':
    case 'bigint':
    case 'boolean':
    case 'undefined': {
      return String(value);
    }
    case 'symbol': {
      return value.description === undefined ? 'Symbol()' : `Symbol(${boundedFragment(value.description)})`;
    }
    default: {
      return value === null ? 'null' : null;
    }
  }
}

function formatPreviewProperty(value: unknown, isArray: boolean): string {
  if (typeof value !== 'object' || value === null) return '[Unavailable]';
  const name = readOwnString(value, 'name') ?? '';
  const rawValue = readOwnString(value, 'value') ?? readOwnString(value, 'type') ?? 'undefined';
  const formattedValue = SENSITIVE_CONSOLE_KEY.test(name) ? '[REDACTED]' : boundedFragment(rawValue);
  return isArray ? formattedValue : `${boundedFragment(name)}: ${formattedValue}`;
}

function formatRemotePreview(remoteObject: object): string {
  const previewValue = readOwnDataProperty(remoteObject, 'preview');
  const preview = typeof previewValue === 'object' && previewValue !== null ? previewValue : null;
  const propertiesValue = preview ? readOwnDataProperty(preview, 'properties') : MISSING_PROPERTY;
  const propertyCount = safeArrayLength(propertiesValue);
  const description =
    readOwnString(remoteObject, 'description') ??
    (preview ? readOwnString(preview, 'description') : undefined) ??
    readOwnString(remoteObject, 'type') ??
    '[Unavailable]';
  if (propertyCount === null || propertyCount === 0) return boundedFragment(description);

  const subtype = (preview ? readOwnString(preview, 'subtype') : undefined) ?? readOwnString(remoteObject, 'subtype');
  const isArray = subtype === 'array';
  const formatted: string[] = [];
  const count = Math.min(propertyCount, MAX_PREVIEW_PROPERTIES);
  for (let index = 0; index < count; index += 1) {
    const property = readOwnDataProperty(propertiesValue as object, String(index));
    formatted.push(formatPreviewProperty(property, isArray));
  }
  const overflow = preview ? readOwnDataProperty(preview, 'overflow') === true : false;
  if (overflow || propertyCount > MAX_PREVIEW_PROPERTIES) formatted.push('…');
  return isArray ? `[${formatted.join(', ')}]` : `{${formatted.join(', ')}}`;
}

function formatRemoteObject(value: unknown): string {
  if (value === MISSING_PROPERTY || value === UNAVAILABLE_PROPERTY) return '[Unavailable]';
  const primitive = formatPrimitive(value);
  if (primitive !== null) return primitive;
  if (typeof value !== 'object' || value === null) return '[Unavailable]';

  const type = readOwnString(value, 'type');
  const subtype = readOwnString(value, 'subtype');
  if (type === 'undefined') return 'undefined';
  if (subtype === 'null') return 'null';

  const unserializableValue = readOwnString(value, 'unserializableValue');
  if (unserializableValue !== undefined) return boundedFragment(unserializableValue);

  const remoteValue = readOwnDataProperty(value, 'value');
  if (remoteValue !== MISSING_PROPERTY && remoteValue !== UNAVAILABLE_PROPERTY) {
    const formattedValue = formatPrimitive(remoteValue);
    if (formattedValue !== null) return formattedValue;
  }
  if (subtype === 'error') {
    const errorDescription = readOwnString(value, 'description');
    if (errorDescription !== undefined) return boundedFragment(errorDescription);
  }
  return formatRemotePreview(value);
}

function formatConsoleArguments(value: unknown): string {
  const length = safeArrayLength(value);
  if (length === null || length === 0) return '';
  const count = Math.min(length, MAX_CONSOLE_ARGUMENTS);
  const arguments_: unknown[] = [];
  const formatted: string[] = [];
  let formattedLength = 0;
  let inputTruncated = false;
  for (let index = 0; index < count; index += 1) {
    const argument = readOwnDataProperty(value as object, String(index));
    const formattedArgument = formatRemoteObject(argument);
    const separatorLength = formatted.length === 0 ? 0 : 1;
    const remainingLength = MAX_CONSOLE_FRAGMENT_LENGTH - formattedLength - separatorLength;
    if (remainingLength <= 0) {
      formatted.push('…');
      inputTruncated = true;
      break;
    }
    arguments_.push(argument);
    if (formattedArgument.length > remainingLength) {
      formatted.push(`${formattedArgument.slice(0, Math.max(0, remainingLength - 1))}…`);
      inputTruncated = true;
      break;
    }
    formatted.push(formattedArgument);
    formattedLength += separatorLength + formattedArgument.length;
  }
  if (!inputTruncated && length > arguments_.length && formattedLength < MAX_CONSOLE_FRAGMENT_LENGTH) {
    formatted.push('…');
  }

  const firstValue =
    typeof arguments_[0] === 'object' && arguments_[0] !== null
      ? readOwnDataProperty(arguments_[0], 'value')
      : MISSING_PROPERTY;
  if (typeof firstValue !== 'string' || !/%[%Ocdfios]/.test(formatted[0] ?? '')) {
    return formatted.join(' ');
  }

  let nextArgument = 1;
  const substituted = (formatted[0] ?? '').replaceAll(/%([%Ocdfios])/g, (match: string, specifier: string) => {
    if (specifier === '%') return '%';
    if (nextArgument >= arguments_.length) return match;
    const argument = formatted[nextArgument++] ?? '';
    if (specifier === 'c') return '';
    if (specifier === 'd' || specifier === 'i') return String(Number.parseInt(argument, 10));
    if (specifier === 'f') return String(Number.parseFloat(argument));
    return argument;
  });
  return [substituted, ...formatted.slice(nextArgument)].join(' ');
}

function redactConsoleText(text: string): string {
  return text
    .replaceAll(/\b(?:sk-(?:proj-|svcacct-)?[\w-]{12,}|sess-[\w-]{12,})\b/g, '[REDACTED]')
    .replaceAll(/\bbearer\s+[\w+./~-]+=*/gi, 'Bearer [REDACTED]')
    .replaceAll(
      /(["']?\b(?:authorization|proxy-authorization|cookie|set-cookie)["']?\s*[:=]\s*)(?:"[^"]*"|'[^']*'|[^\n\r,]+)/gi,
      '$1[REDACTED]',
    )
    .replaceAll(
      /(["']?\b(?:access[_-]?token|refresh[_-]?token|id[_-]?token|api[_-]?key|password|passwd|credential|secret|private[_-]?input)["']?\s*[:=]\s*)(?:"[^"]*"|'[^']*'|[^\n\r&,;}]+)/gi,
      '$1[REDACTED]',
    )
    .replaceAll(
      /([&?](?:access_token|refresh_token|id_token|api_key|token|code|password|key|session)=)[^\s#&]+/gi,
      '$1[REDACTED]',
    );
}

function normalizeConsoleLevel(level: unknown): ChromiumConsoleLevel {
  switch (level) {
    case 'error':
    case 'assert': {
      return ChromiumConsoleLevel.Error;
    }
    case 'warn':
    case 'warning': {
      return ChromiumConsoleLevel.Warning;
    }
    case 'info': {
      return ChromiumConsoleLevel.Info;
    }
    case 'debug':
    case 'verbose':
    case 'trace': {
      return ChromiumConsoleLevel.Debug;
    }
    default: {
      return ChromiumConsoleLevel.Log;
    }
  }
}

function makeConsoleEntry(
  level: unknown,
  message: string,
  source: ChromiumConsoleSource,
  timestamp: unknown,
): ChromiumConsoleEntry {
  const redacted = boundedFragment(message);
  return {
    level: normalizeConsoleLevel(level),
    message:
      redacted.length > MAX_CHROMIUM_CONSOLE_MESSAGE_LENGTH
        ? `${redacted.slice(0, MAX_CHROMIUM_CONSOLE_MESSAGE_LENGTH)}…`
        : redacted,
    source,
    timestamp: typeof timestamp === 'number' && Number.isFinite(timestamp) ? timestamp : Date.now(),
  };
}

export function formatChromiumConsoleEvent(event: ChromiumDevToolsEvent): ChromiumConsoleEntry | null {
  if (typeof event !== 'object' || event === null) return null;
  const method = readOwnString(event, 'method');
  const paramsValue = readOwnDataProperty(event, 'params');
  if (typeof paramsValue !== 'object' || paramsValue === null) return null;

  if (method === 'Runtime.consoleAPICalled') {
    const message = formatConsoleArguments(readOwnDataProperty(paramsValue, 'args'));
    if (!message) return null;
    return makeConsoleEntry(
      readOwnDataProperty(paramsValue, 'type'),
      message,
      ChromiumConsoleSource.Console,
      readOwnDataProperty(paramsValue, 'timestamp'),
    );
  }

  if (method === 'Log.entryAdded') {
    const entry = readOwnDataProperty(paramsValue, 'entry');
    if (typeof entry !== 'object' || entry === null) return null;
    const message = readOwnString(entry, 'text');
    if (!message) return null;
    return makeConsoleEntry(
      readOwnDataProperty(entry, 'level'),
      message,
      ChromiumConsoleSource.Browser,
      readOwnDataProperty(entry, 'timestamp'),
    );
  }

  if (method === 'Runtime.exceptionThrown') {
    const details = readOwnDataProperty(paramsValue, 'exceptionDetails');
    if (typeof details !== 'object' || details === null) return null;
    const exception = readOwnDataProperty(details, 'exception');
    const description =
      typeof exception === 'object' && exception !== null ? readOwnString(exception, 'description') : undefined;
    const message = description ?? readOwnString(details, 'text') ?? 'Uncaught exception';
    return makeConsoleEntry(
      ChromiumConsoleLevel.Error,
      message,
      ChromiumConsoleSource.Exception,
      readOwnDataProperty(paramsValue, 'timestamp'),
    );
  }

  return null;
}
