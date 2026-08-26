import type { DaemonConnection } from '../utils/daemonClient';

export const DEBUGGER_INPUT_IDENTIFIER = 'ValdiDebuggerInput';
const DEBUGGER_INPUT_TIMEOUT_MS = 5000;
const DEBUGGER_INPUT_NAMED_KEYS: ReadonlySet<string> = new Set(['Enter', 'Return', 'Escape', 'Backspace', 'Delete']);

interface SegmenterPart {
  index: number;
}

interface SegmenterLike {
  segment(value: string): Iterable<SegmenterPart>;
}

interface SegmenterConstructor {
  new (locales: undefined, options: { granularity: string }): SegmenterLike;
}

export enum DebuggerInputType {
  Capabilities = 'capabilities',
  Query = 'query',
  Tap = 'tap',
  Focus = 'focus',
  Text = 'text',
  Key = 'key',
  Scroll = 'scroll',
}

const DEBUGGER_INPUT_RETURN_KEY_ACTIONS: ReadonlySet<string> = new Set(['onReturn', 'onChange', 'focused', 'return']);
const DEBUGGER_INPUT_ESCAPE_KEY_ACTIONS: ReadonlySet<string> = new Set(['focused']);
const DEBUGGER_INPUT_EDIT_KEY_ACTIONS: ReadonlySet<string> = new Set(['onChange']);
const DEBUGGER_INPUT_TAP_ACTIONS: ReadonlySet<string> = new Set(['onTap']);
const DEBUGGER_INPUT_FOCUS_ACTIONS: ReadonlySet<string> = new Set(['focused']);
const DEBUGGER_INPUT_TEXT_ACTIONS: ReadonlySet<string> = new Set(['onChange']);
const DEBUGGER_INPUT_SCROLL_ACTIONS: ReadonlySet<string> = new Set(['contentOffset']);
export const SUPPORTED_DEBUGGER_INPUT_TYPES: ReadonlySet<string> = new Set(Object.values(DebuggerInputType));
const DEBUGGER_INPUT_SELECTOR_FIELDS: ReadonlySet<string> = new Set(['elementId', 'accessibilityId', 'tag']);
const DEBUGGER_INPUT_COMMON_FIELDS: ReadonlyArray<string> = [
  'type',
  'contextId',
  'elementId',
  'accessibilityId',
  'selector',
];
const DEBUGGER_INPUT_FIELDS_BY_TYPE: ReadonlyMap<DebuggerInputType, ReadonlySet<string>> = new Map([
  [DebuggerInputType.Capabilities, new Set(['type', 'contextId'])],
  [DebuggerInputType.Query, new Set(DEBUGGER_INPUT_COMMON_FIELDS)],
  [DebuggerInputType.Tap, new Set([...DEBUGGER_INPUT_COMMON_FIELDS, 'x', 'y'])],
  [DebuggerInputType.Focus, new Set([...DEBUGGER_INPUT_COMMON_FIELDS, 'focused'])],
  [
    DebuggerInputType.Text,
    new Set([...DEBUGGER_INPUT_COMMON_FIELDS, 'text', 'value', 'selectionStart', 'selectionEnd']),
  ],
  [DebuggerInputType.Key, new Set([...DEBUGGER_INPUT_COMMON_FIELDS, 'key', 'selectionStart', 'selectionEnd'])],
  [DebuggerInputType.Scroll, new Set([...DEBUGGER_INPUT_COMMON_FIELDS, 'x', 'y', 'deltaX', 'deltaY'])],
]);

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function containsLoneSurrogate(value: string): boolean {
  for (let index = 0; index < value.length; index += 1) {
    const codeUnit = value.slice(index, index + 1).codePointAt(0) ?? 0;
    if (codeUnit >= 0xd8_00 && codeUnit <= 0xdb_ff) {
      if (index + 1 >= value.length) return true;
      const nextCodeUnit = value.slice(index + 1, index + 2).codePointAt(0) ?? 0;
      if (nextCodeUnit < 0xdc_00 || nextCodeUnit > 0xdf_ff) return true;
      index += 1;
    } else if (codeUnit >= 0xdc_00 && codeUnit <= 0xdf_ff) {
      return true;
    }
  }
  return false;
}

function isCombiningCodePoint(codePoint: number): boolean {
  return (
    (codePoint >= 0x03_00 && codePoint <= 0x03_6f) ||
    (codePoint >= 0x04_83 && codePoint <= 0x04_89) ||
    (codePoint >= 0x05_91 && codePoint <= 0x05_bd) ||
    codePoint === 0x05_bf ||
    (codePoint >= 0x05_c1 && codePoint <= 0x05_c2) ||
    (codePoint >= 0x06_10 && codePoint <= 0x06_1a) ||
    (codePoint >= 0x06_4b && codePoint <= 0x06_5f) ||
    codePoint === 0x06_70 ||
    (codePoint >= 0x06_d6 && codePoint <= 0x06_ed) ||
    (codePoint >= 0x1a_b0 && codePoint <= 0x1a_ff) ||
    (codePoint >= 0x1d_c0 && codePoint <= 0x1d_ff) ||
    (codePoint >= 0x20_d0 && codePoint <= 0x20_ff) ||
    (codePoint >= 0xfe_20 && codePoint <= 0xfe_2f)
  );
}

function isGraphemeExtension(codePoint: number): boolean {
  return (
    isCombiningCodePoint(codePoint) ||
    (codePoint >= 0xfe_00 && codePoint <= 0xfe_0f) ||
    (codePoint >= 0xe_01_00 && codePoint <= 0xe_01_ef) ||
    (codePoint >= 0x1_f3_fb && codePoint <= 0x1_f3_ff) ||
    codePoint === 0x20_e3
  );
}

function isRegionalIndicator(codePoint: number): boolean {
  return codePoint >= 0x1_f1_e6 && codePoint <= 0x1_f1_ff;
}

function fallbackGraphemeCount(value: string): number {
  if (value.length === 0) return 0;
  let count = 1;
  let previousCodePoint = value.codePointAt(0) ?? 0;
  let currentIndex = previousCodePoint > 0xff_ff ? 2 : 1;
  let regionalIndicatorCount = isRegionalIndicator(previousCodePoint) ? 1 : 0;
  while (currentIndex < value.length) {
    const currentCodePoint = value.codePointAt(currentIndex) ?? 0;
    const joinsPrevious =
      (previousCodePoint === 0x00_0d && currentCodePoint === 0x00_0a) ||
      isGraphemeExtension(currentCodePoint) ||
      previousCodePoint === 0x20_0d ||
      currentCodePoint === 0x20_0d ||
      (isRegionalIndicator(previousCodePoint) &&
        isRegionalIndicator(currentCodePoint) &&
        regionalIndicatorCount % 2 === 1);
    if (!joinsPrevious) {
      count += 1;
      if (count > 1) return count;
    }
    if (isRegionalIndicator(currentCodePoint)) {
      regionalIndicatorCount = isRegionalIndicator(previousCodePoint) ? regionalIndicatorCount + 1 : 1;
    } else if (!isGraphemeExtension(currentCodePoint)) {
      regionalIndicatorCount = 0;
    }
    previousCodePoint = currentCodePoint;
    currentIndex += currentCodePoint > 0xff_ff ? 2 : 1;
  }
  return count;
}

function isSingleGrapheme(value: string): boolean {
  const segmenterConstructor =
    typeof Intl === 'undefined' ? undefined : (Intl as unknown as { Segmenter?: SegmenterConstructor }).Segmenter;
  if (segmenterConstructor) {
    try {
      const segmenter = new segmenterConstructor(undefined, { granularity: 'grapheme' });
      const iterator = segmenter.segment(value)[Symbol.iterator]();
      return !iterator.next().done && Boolean(iterator.next().done);
    } catch {
      // Fall through for Node runtimes without grapheme segmentation data.
    }
  }
  return fallbackGraphemeCount(value) === 1;
}

function isSupportedDebuggerInputKey(value: string): boolean {
  if (DEBUGGER_INPUT_NAMED_KEYS.has(value)) return true;
  if (value.length === 0 || containsLoneSurrogate(value) || !isSingleGrapheme(value)) return false;
  let index = 0;
  while (index < value.length) {
    const codePoint = value.codePointAt(index) ?? 0;
    if (codePoint < 0x20 || (codePoint >= 0x7f && codePoint <= 0x9f)) return false;
    index += codePoint > 0xff_ff ? 2 : 1;
  }
  return true;
}

function validateOptionalString(
  request: Record<string, unknown>,
  fieldName: string,
  requireNonEmpty: boolean,
): string | undefined {
  const value = request[fieldName];
  if (value === undefined) return undefined;
  if (typeof value !== 'string') return `${fieldName} must be a string.`;
  if (requireNonEmpty && value.length === 0) return `${fieldName} must not be empty.`;
  if (containsLoneSurrogate(value)) return `${fieldName} must contain valid Unicode.`;
  return undefined;
}

function validateOptionalNumber(
  request: Record<string, unknown>,
  fieldName: string,
  requireInteger: boolean,
): string | undefined {
  const value = request[fieldName];
  if (value === undefined) return undefined;
  if (typeof value !== 'number' || !Number.isFinite(value) || (requireInteger && !Number.isInteger(value))) {
    return `${fieldName} must be a finite ${requireInteger ? 'integer' : 'number'}.`;
  }
  return undefined;
}

function responseValidationError(message: string): never {
  throw new TypeError(`Invalid debugger input response: ${message}`);
}

function validateOptionalResponseString(
  value: Record<string, unknown>,
  fieldName: string,
  requireNonEmpty: boolean,
): void {
  const fieldValue = value[fieldName];
  if (fieldValue === undefined) return;
  if (typeof fieldValue !== 'string') responseValidationError(`${fieldName} must be a string.`);
  if (requireNonEmpty && fieldValue.length === 0) responseValidationError(`${fieldName} must not be empty.`);
  if (containsLoneSurrogate(fieldValue)) responseValidationError(`${fieldName} must contain valid Unicode.`);
}

function validateOptionalResponseNumber(
  value: Record<string, unknown>,
  fieldName: string,
  requireInteger: boolean,
): void {
  const fieldValue = value[fieldName];
  if (fieldValue === undefined) return;
  if (
    typeof fieldValue !== 'number' ||
    !Number.isFinite(fieldValue) ||
    (requireInteger && !Number.isInteger(fieldValue))
  ) {
    responseValidationError(`${fieldName} must be a finite ${requireInteger ? 'integer' : 'number'}.`);
  }
}

function requireResponseString(value: Record<string, unknown>, fieldName: string): string {
  validateOptionalResponseString(value, fieldName, false);
  const fieldValue = value[fieldName];
  if (typeof fieldValue !== 'string') responseValidationError(`${fieldName} must be a string.`);
  return fieldValue;
}

function requireResponseNumber(value: Record<string, unknown>, fieldName: string, requireInteger: boolean): number {
  validateOptionalResponseNumber(value, fieldName, requireInteger);
  const fieldValue = value[fieldName];
  if (typeof fieldValue !== 'number') {
    responseValidationError(`${fieldName} must be a finite ${requireInteger ? 'integer' : 'number'}.`);
  }
  return fieldValue;
}

function requireResponseAction(data: Record<string, unknown>, allowedActions: ReadonlySet<string>): void {
  const action = data['action'];
  if (typeof action !== 'string' || !allowedActions.has(action)) {
    responseValidationError(`action must be one of: ${Array.from(allowedActions).join(', ')}.`);
  }
}

function validateStringArray(value: unknown, fieldName: string): void {
  if (!Array.isArray(value) || value.some(item => typeof item !== 'string')) {
    responseValidationError(`${fieldName} must be an array of strings.`);
  }
}

function validateElementFrame(value: unknown, fieldName: string): void {
  if (!isRecord(value)) responseValidationError(`${fieldName} must be an object.`);
  for (const coordinate of ['x', 'y', 'width', 'height']) {
    const coordinateValue = value[coordinate];
    if (typeof coordinateValue !== 'number' || !Number.isFinite(coordinateValue)) {
      responseValidationError(`${fieldName}.${coordinate} must be a finite number.`);
    }
  }
}

function validateElementDescriptor(value: unknown, index: number): void {
  const fieldName = `elements[${index}]`;
  if (!isRecord(value)) responseValidationError(`${fieldName} must be an object.`);
  if (typeof value['elementId'] !== 'number' || !Number.isInteger(value['elementId'])) {
    responseValidationError(`${fieldName}.elementId must be a finite integer.`);
  }
  validateOptionalResponseNumber(value, 'parentElementId', true);
  if (typeof value['tag'] !== 'string' || value['tag'].length === 0) {
    responseValidationError(`${fieldName}.tag must be a non-empty string.`);
  }
  for (const stringField of [
    'accessibilityId',
    'accessibilityCategory',
    'accessibilityNavigation',
    'accessibilityLabel',
    'accessibilityHint',
    'accessibilityValue',
  ]) {
    validateOptionalResponseString(value, stringField, false);
  }
  for (const booleanField of ['selected', 'enabled', 'focused']) {
    if (typeof value[booleanField] !== 'boolean') {
      responseValidationError(`${fieldName}.${booleanField} must be a boolean.`);
    }
  }
  validateElementFrame(value['frame'], `${fieldName}.frame`);
  validateElementFrame(value['absoluteFrame'], `${fieldName}.absoluteFrame`);
  validateStringArray(value['actions'], `${fieldName}.actions`);
}

function validateDebuggerInputResult(data: Record<string, unknown>, request: Record<string, unknown>): void {
  const requestType = request['type'] as DebuggerInputType;
  validateOptionalResponseString(data, 'contextId', true);
  validateOptionalResponseString(data, 'accessibilityId', false);
  validateOptionalResponseString(data, 'action', true);
  validateOptionalResponseString(data, 'message', false);
  validateOptionalResponseString(data, 'value', false);
  for (const fieldName of ['elementId', 'actionElementId', 'selectionStart', 'selectionEnd']) {
    validateOptionalResponseNumber(data, fieldName, true);
  }
  for (const fieldName of ['contentOffsetX', 'contentOffsetY']) {
    validateOptionalResponseNumber(data, fieldName, false);
  }

  if (data['elements'] !== undefined) {
    if (!Array.isArray(data['elements'])) responseValidationError('elements must be an array.');
    data['elements'].forEach((element, index) => validateElementDescriptor(element, index));
  }
  if (data['supportedTypes'] !== undefined) validateStringArray(data['supportedTypes'], 'supportedTypes');
  if (data['selectorForms'] !== undefined) validateStringArray(data['selectorForms'], 'selectorForms');

  if (!data['handled']) return;
  if (requestType === DebuggerInputType.Query) {
    if (data['action'] !== DebuggerInputType.Query) responseValidationError('query action must be query.');
    if (!Array.isArray(data['elements'])) responseValidationError('query elements must be an array.');
  }
  if (requestType === DebuggerInputType.Capabilities) {
    if (data['action'] !== DebuggerInputType.Capabilities) {
      responseValidationError('capabilities action must be capabilities.');
    }
    validateStringArray(data['supportedTypes'], 'supportedTypes');
    validateStringArray(data['selectorForms'], 'selectorForms');
    return;
  }
  if (requestType !== DebuggerInputType.Query) {
    requireResponseNumber(data, 'elementId', true);
    requireResponseNumber(data, 'actionElementId', true);
  }

  switch (requestType) {
    case DebuggerInputType.Tap: {
      requireResponseAction(data, DEBUGGER_INPUT_TAP_ACTIONS);
      break;
    }
    case DebuggerInputType.Focus: {
      requireResponseAction(data, DEBUGGER_INPUT_FOCUS_ACTIONS);
      break;
    }
    case DebuggerInputType.Text: {
      requireResponseAction(data, DEBUGGER_INPUT_TEXT_ACTIONS);
      requireResponseString(data, 'value');
      requireResponseNumber(data, 'selectionStart', true);
      requireResponseNumber(data, 'selectionEnd', true);
      break;
    }
    case DebuggerInputType.Key: {
      const key = request['key'];
      if (typeof key !== 'string') responseValidationError('key request must contain a string key.');
      const allowedActions =
        key === 'Enter' || key === 'Return'
          ? DEBUGGER_INPUT_RETURN_KEY_ACTIONS
          : key === 'Escape'
            ? DEBUGGER_INPUT_ESCAPE_KEY_ACTIONS
            : DEBUGGER_INPUT_EDIT_KEY_ACTIONS;
      requireResponseAction(data, allowedActions);
      requireResponseString(data, 'value');
      if (key !== 'Escape') {
        requireResponseNumber(data, 'selectionStart', true);
        requireResponseNumber(data, 'selectionEnd', true);
      }
      break;
    }
    case DebuggerInputType.Scroll: {
      requireResponseAction(data, DEBUGGER_INPUT_SCROLL_ACTIONS);
      requireResponseNumber(data, 'contentOffsetX', false);
      requireResponseNumber(data, 'contentOffsetY', false);
      break;
    }
    default: {
      break;
    }
  }
}

function validateSelector(request: Record<string, unknown>): string | undefined {
  const selectorCount = [request['elementId'], request['accessibilityId'], request['selector']].filter(
    value => value !== undefined,
  ).length;
  if (selectorCount > 1) return 'Use only one of elementId, accessibilityId, or selector.';

  const elementIdError = validateOptionalNumber(request, 'elementId', true);
  if (elementIdError) return elementIdError;
  const accessibilityIdError = validateOptionalString(request, 'accessibilityId', true);
  if (accessibilityIdError) return accessibilityIdError;

  const selector = request['selector'];
  if (selector === undefined) return undefined;
  if (typeof selector === 'string') {
    return validateOptionalString(request, 'selector', true);
  }
  if (!isRecord(selector)) return 'selector must be a string or an object.';
  const unknownField = Object.keys(selector)
    .sort()
    .find(fieldName => !DEBUGGER_INPUT_SELECTOR_FIELDS.has(fieldName));
  if (unknownField) return `Unsupported selector field '${unknownField}'.`;
  if (Object.keys(selector).length === 0) return 'selector object must include elementId, accessibilityId, or tag.';
  return (
    validateOptionalNumber(selector, 'elementId', true) ??
    validateOptionalString(selector, 'accessibilityId', true) ??
    validateOptionalString(selector, 'tag', true)
  );
}

export function validateDebuggerInputRequest(request: unknown): string | undefined {
  if (!isRecord(request)) return 'Debugger input request must be an object.';
  const type = request['type'];
  if (typeof type !== 'string' || !SUPPORTED_DEBUGGER_INPUT_TYPES.has(type)) {
    return `Unsupported input type ${String(type)}.`;
  }
  const inputType = type as DebuggerInputType;
  const supportedFields = DEBUGGER_INPUT_FIELDS_BY_TYPE.get(inputType);
  if (!supportedFields) return `Unsupported input type ${type}.`;
  const unknownField = Object.keys(request)
    .sort()
    .find(fieldName => !supportedFields.has(fieldName));
  if (unknownField) return `Field '${unknownField}' is not supported for ${type} input.`;

  const contextError = validateOptionalString(request, 'contextId', true);
  if (contextError) return contextError;
  const selectorError = validateSelector(request);
  if (selectorError) return selectorError;
  const hasSelector =
    request['elementId'] !== undefined || request['accessibilityId'] !== undefined || request['selector'] !== undefined;
  if (inputType !== DebuggerInputType.Capabilities && inputType !== DebuggerInputType.Query && !hasSelector) {
    return 'An elementId, accessibilityId, or selector is required.';
  }

  if (request['focused'] !== undefined && typeof request['focused'] !== 'boolean') {
    return 'focused must be a boolean.';
  }
  for (const fieldName of ['text', 'value', 'key']) {
    const error = validateOptionalString(request, fieldName, fieldName === 'key');
    if (error) return error;
  }
  for (const fieldName of ['selectionStart', 'selectionEnd']) {
    const error = validateOptionalNumber(request, fieldName, true);
    if (error) return error;
  }
  for (const fieldName of ['x', 'y', 'deltaX', 'deltaY']) {
    const error = validateOptionalNumber(request, fieldName, false);
    if (error) return error;
  }

  if (inputType === DebuggerInputType.Text) {
    if (request['text'] === undefined && request['value'] === undefined)
      return 'Text input requires a string text or value.';
    if (request['text'] !== undefined && request['value'] !== undefined) return 'Use only one of text or value.';
  }
  if (inputType === DebuggerInputType.Key) {
    if (request['key'] === undefined) return 'Key input requires a string key.';
    if (!isSupportedDebuggerInputKey(request['key'] as string)) {
      return 'key must be Enter, Return, Escape, Backspace, Delete, or one printable grapheme.';
    }
  }
  return undefined;
}

export function unwrapDebuggerInputResponse(
  customBody: unknown,
  request: Record<string, unknown>,
): Record<string, unknown> {
  if (!isRecord(customBody) || typeof customBody['handled'] !== 'boolean') {
    throw new TypeError('Invalid debugger input response: handled must be a boolean.');
  }
  if (customBody['handled']) {
    const data = customBody['data'];
    if (!isRecord(data)) {
      throw new TypeError('Invalid debugger input response: data must be an object.');
    }
    if (typeof data['handled'] !== 'boolean') {
      throw new TypeError('Invalid debugger input response: data.handled must be a boolean.');
    }
    const contractVersion = data['contractVersion'];
    if (typeof contractVersion !== 'number' || !Number.isInteger(contractVersion) || contractVersion < 1) {
      throw new TypeError('Invalid debugger input response: data.contractVersion must be a positive integer.');
    }
    if (typeof data['type'] !== 'string' || data['type'] !== request['type']) {
      throw new TypeError('Invalid debugger input response: data.type must match the request type.');
    }
    validateDebuggerInputResult(data, request);
    return data;
  }
  return {
    handled: false,
    type: request['type'],
    elementId: request['elementId'],
    message: 'The target app did not register the Valdi debugger input handler.',
  };
}

export async function sendDebuggerInput(
  conn: DaemonConnection,
  clientId: string,
  request: Record<string, unknown>,
): Promise<Record<string, unknown>> {
  const validationError = validateDebuggerInputRequest(request);
  if (validationError) {
    throw new Error(validationError);
  }
  const customBody = await conn.customRequest(clientId, DEBUGGER_INPUT_IDENTIFIER, request, DEBUGGER_INPUT_TIMEOUT_MS);
  return unwrapDebuggerInputResponse(customBody, request);
}
