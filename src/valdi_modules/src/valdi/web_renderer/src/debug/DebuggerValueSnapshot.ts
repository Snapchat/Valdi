export interface DebuggerValueSnapshotLimits {
  readonly maximumDepth: number;
  readonly maximumEntries: number;
  readonly maximumPropertyNameCharacters: number;
  readonly maximumStringBytes: number;
}

export interface DebuggerValueSnapshotCapture<T> {
  readonly serializedBytes: number;
  readonly value: T;
}

interface DebuggerValueSnapshotBudget {
  remainingBytes: number;
}

const DEBUG_ACCESSOR_OMISSION_MARKER = 'accessors or unsupported fields omitted';
const DEBUG_CIRCULAR_MARKER = '<circular object/>';
const DEBUG_EMPTY_ARRAY_ITEM_MARKER = '<empty/>';
const DEBUG_BIGINT_MARKER = '<bigint/>';
const DEBUG_SYMBOL_MARKER = '<symbol/>';
const DEBUG_TRUNCATION_MARKER = '... <truncated>';
const DEBUG_UNAVAILABLE_MARKER = '<unavailable/>';

/**
 * Produces a detached JSON value without reading property values through normal
 * JavaScript property access. Only own, enumerable data descriptors are copied.
 */
export function captureDebuggerPropertiesSnapshot(
  source: unknown,
  maximumSerializedBytes: number,
  limits: DebuggerValueSnapshotLimits,
): DebuggerValueSnapshotCapture<Record<string, unknown>> | undefined {
  if (typeof source !== 'object' || source === null || !validMaximum(maximumSerializedBytes) || !validLimits(limits)) {
    return undefined;
  }

  const budget: DebuggerValueSnapshotBudget = { remainingBytes: maximumSerializedBytes };
  try {
    if (Array.isArray(source)) {
      return undefined;
    }
    const activePath = new Set<object>([source]);
    const value = captureObject(source, 0, activePath, budget, limits);
    const serialized = JSON.stringify(value);
    const serializedBytes = utf8ByteLength(serialized);
    return serializedBytes <= maximumSerializedBytes ? { serializedBytes, value } : undefined;
  } catch (_error) {
    // Throwing or revoked Proxy reflection is an expected trust-boundary
    // outcome. The caller treats undefined as a properties-only omission.
    return undefined;
  }
}

function validMaximum(value: number): boolean {
  return Number.isSafeInteger(value) && value > 0;
}

function validLimits(limits: DebuggerValueSnapshotLimits): boolean {
  return (
    validMaximum(limits.maximumDepth) &&
    validMaximum(limits.maximumEntries) &&
    validMaximum(limits.maximumPropertyNameCharacters) &&
    validMaximum(limits.maximumStringBytes)
  );
}

function captureValue(
  value: unknown,
  depth: number,
  activePath: Set<object>,
  budget: DebuggerValueSnapshotBudget,
  limits: DebuggerValueSnapshotLimits,
): unknown {
  if (depth >= limits.maximumDepth) {
    return captureString(DEBUG_TRUNCATION_MARKER, budget, limits.maximumStringBytes);
  }
  if (value === undefined) {
    consumeBudget(budget, 4);
    return null;
  }
  if (value === null || typeof value === 'boolean') {
    consumeBudget(budget, value === null ? 4 : value ? 4 : 5);
    return value;
  }
  if (typeof value === 'number') {
    const serialized = JSON.stringify(value) ?? 'null';
    consumeBudget(budget, serialized.length);
    return value;
  }
  if (typeof value === 'string') {
    return captureString(value, budget, limits.maximumStringBytes);
  }
  if (typeof value === 'function') {
    return captureString('[function]', budget, limits.maximumStringBytes);
  }
  if (typeof value === 'bigint') {
    return captureString(DEBUG_BIGINT_MARKER, budget, limits.maximumStringBytes);
  }
  if (typeof value === 'symbol') {
    return captureString(DEBUG_SYMBOL_MARKER, budget, limits.maximumStringBytes);
  }
  if (typeof value !== 'object') {
    return captureString(DEBUG_UNAVAILABLE_MARKER, budget, limits.maximumStringBytes);
  }
  if (typeof ArrayBuffer !== 'undefined' && ArrayBuffer.isView(value)) {
    return captureString('<array buffer view/>', budget, limits.maximumStringBytes);
  }
  if (activePath.has(value)) {
    return captureString(DEBUG_CIRCULAR_MARKER, budget, limits.maximumStringBytes);
  }

  activePath.add(value);
  try {
    return Array.isArray(value)
      ? captureArray(value, depth, activePath, budget, limits)
      : captureObject(value, depth, activePath, budget, limits);
  } catch (_error) {
    // A nested throwing Proxy is represented without discarding safe sibling
    // fields; its enclosing top-level capture remains detached and bounded.
    return captureString(DEBUG_UNAVAILABLE_MARKER, budget, limits.maximumStringBytes);
  } finally {
    activePath.delete(value);
  }
}

function captureArray(
  value: unknown[],
  depth: number,
  activePath: Set<object>,
  budget: DebuggerValueSnapshotBudget,
  limits: DebuggerValueSnapshotLimits,
): unknown[] {
  const output: unknown[] = [];
  if (!tryConsumeBudget(budget, 2)) {
    return output;
  }
  const lengthDescriptor = Object.getOwnPropertyDescriptor(value, 'length');
  const length =
    typeof lengthDescriptor?.value === 'number' && Number.isSafeInteger(lengthDescriptor.value)
      ? Math.max(0, lengthDescriptor.value)
      : 0;
  const itemCount = Math.min(length, limits.maximumEntries);
  let inspectedItemCount = 0;
  for (; inspectedItemCount < itemCount; inspectedItemCount++) {
    if (!tryConsumeArrayItemPrefix(output, budget, 2)) {
      break;
    }
    const descriptor = Object.getOwnPropertyDescriptor(value, String(inspectedItemCount));
    if (
      descriptor === undefined ||
      !descriptor.enumerable ||
      !Object.prototype.hasOwnProperty.call(descriptor, 'value')
    ) {
      output.push(captureString(DEBUG_EMPTY_ARRAY_ITEM_MARKER, budget, limits.maximumStringBytes));
      continue;
    }
    output.push(captureValue(descriptor.value, depth + 1, activePath, budget, limits));
  }
  if (
    length > inspectedItemCount &&
    output.length < limits.maximumEntries &&
    tryConsumeArrayItemPrefix(output, budget, 2)
  ) {
    output.push(captureString(`${length - inspectedItemCount} more items`, budget, limits.maximumStringBytes));
  }
  return output;
}

function captureObject(
  value: object,
  depth: number,
  activePath: Set<object>,
  budget: DebuggerValueSnapshotBudget,
  limits: DebuggerValueSnapshotLimits,
): Record<string, unknown> {
  const output = Object.create(null) as Record<string, unknown>;
  if (!tryConsumeBudget(budget, 2)) {
    return output;
  }
  let inspectedEntryCount = 0;
  let omitted = false;
  // JavaScript has no resumable own-key iterator. This may materialize a
  // Proxy's complete own-key result, but it never traverses the prototype.
  // Property values are still read only from data descriptors.
  const propertyNames = Object.getOwnPropertyNames(value);
  for (const propertyName of propertyNames) {
    const descriptor = Object.getOwnPropertyDescriptor(value, propertyName);
    if (descriptor === undefined || !descriptor.enumerable) {
      continue;
    }
    if (inspectedEntryCount >= limits.maximumEntries) {
      omitted = true;
      break;
    }
    inspectedEntryCount++;
    if (
      propertyName.length > limits.maximumPropertyNameCharacters ||
      !Object.prototype.hasOwnProperty.call(descriptor, 'value')
    ) {
      omitted = true;
      continue;
    }
    if (!tryConsumePropertyPrefix(output, propertyName, budget, 2)) {
      omitted = true;
      break;
    }
    setDataProperty(output, propertyName, captureValue(descriptor.value, depth + 1, activePath, budget, limits));
  }
  if (omitted) {
    addTruncationProperty(output, DEBUG_ACCESSOR_OMISSION_MARKER, budget, limits);
  }
  return output;
}

function addTruncationProperty(
  target: Record<string, unknown>,
  message: string,
  budget: DebuggerValueSnapshotBudget,
  limits: DebuggerValueSnapshotLimits,
): void {
  if (
    Object.keys(target).length >= limits.maximumEntries ||
    Object.prototype.hasOwnProperty.call(target, '__truncated__') ||
    !tryConsumePropertyPrefix(target, '__truncated__', budget, 2)
  ) {
    return;
  }
  setDataProperty(target, '__truncated__', captureString(message, budget, limits.maximumStringBytes));
}

function setDataProperty(target: Record<string, unknown>, propertyName: string, value: unknown): void {
  Object.defineProperty(target, propertyName, {
    configurable: true,
    enumerable: true,
    value,
    writable: true,
  });
}

function captureString(value: string, budget: DebuggerValueSnapshotBudget, maximumStringBytes: number): string {
  const availableContentBytes = Math.max(0, budget.remainingBytes - 2);
  const completePrefix = boundedStringPrefix(value, maximumStringBytes, availableContentBytes);
  if (completePrefix.end === value.length) {
    consumeBudget(budget, completePrefix.jsonBytes + 2);
    return value;
  }

  const markerRawBytes = utf8ByteLength(DEBUG_TRUNCATION_MARKER);
  const markerJsonBytes = jsonStringUtf8ByteLength(DEBUG_TRUNCATION_MARKER) - 2;
  const markerFits = markerRawBytes <= maximumStringBytes && markerJsonBytes <= availableContentBytes;
  const marker = markerFits ? DEBUG_TRUNCATION_MARKER : '';
  const truncatedPrefix = boundedStringPrefix(
    value,
    maximumStringBytes - (markerFits ? markerRawBytes : 0),
    availableContentBytes - (markerFits ? markerJsonBytes : 0),
  );
  const captured = `${value.slice(0, truncatedPrefix.end)}${marker}`;
  consumeBudget(budget, truncatedPrefix.jsonBytes + (markerFits ? markerJsonBytes : 0) + 2);
  return captured;
}

interface BoundedStringPrefix {
  readonly end: number;
  readonly jsonBytes: number;
}

function boundedStringPrefix(value: string, maximumRawBytes: number, maximumJsonBytes: number): BoundedStringPrefix {
  let rawBytes = 0;
  let jsonBytes = 0;
  let end = 0;
  while (end < value.length) {
    const characterCode = value.charCodeAt(end);
    const nextCharacterCode = value.charCodeAt(end + 1);
    const isSurrogatePair =
      characterCode >= 0xd800 && characterCode <= 0xdbff && nextCharacterCode >= 0xdc00 && nextCharacterCode <= 0xdfff;
    const nextEnd = end + (isSurrogatePair ? 2 : 1);
    const characterRawBytes = characterCode < 0x80 ? 1 : characterCode < 0x800 ? 2 : isSurrogatePair ? 4 : 3;
    let characterJsonBytes: number;
    if (
      characterCode === 0x22 ||
      characterCode === 0x5c ||
      characterCode === 0x08 ||
      characterCode === 0x09 ||
      characterCode === 0x0a ||
      characterCode === 0x0c ||
      characterCode === 0x0d
    ) {
      characterJsonBytes = 2;
    } else if (characterCode < 0x20 || (!isSurrogatePair && characterCode >= 0xd800 && characterCode <= 0xdfff)) {
      characterJsonBytes = 6;
    } else {
      characterJsonBytes = characterRawBytes;
    }
    if (rawBytes + characterRawBytes > maximumRawBytes || jsonBytes + characterJsonBytes > maximumJsonBytes) {
      break;
    }
    rawBytes += characterRawBytes;
    jsonBytes += characterJsonBytes;
    end = nextEnd;
  }
  return { end, jsonBytes };
}

function tryConsumePropertyPrefix(
  target: object,
  propertyName: string,
  budget: DebuggerValueSnapshotBudget,
  minimumValueBytes: number,
): boolean {
  const separatorBytes = Object.keys(target).length === 0 ? 0 : 1;
  const prefixBytes = separatorBytes + jsonStringUtf8ByteLength(propertyName) + 1;
  if (prefixBytes + minimumValueBytes > budget.remainingBytes) {
    return false;
  }
  consumeBudget(budget, prefixBytes);
  return true;
}

function tryConsumeArrayItemPrefix(
  target: unknown[],
  budget: DebuggerValueSnapshotBudget,
  minimumValueBytes: number,
): boolean {
  const prefixBytes = target.length === 0 ? 0 : 1;
  if (prefixBytes + minimumValueBytes > budget.remainingBytes) {
    return false;
  }
  consumeBudget(budget, prefixBytes);
  return true;
}

function tryConsumeBudget(budget: DebuggerValueSnapshotBudget, byteCount: number): boolean {
  if (byteCount > budget.remainingBytes) {
    return false;
  }
  consumeBudget(budget, byteCount);
  return true;
}

function consumeBudget(budget: DebuggerValueSnapshotBudget, byteCount: number): void {
  budget.remainingBytes = Math.max(0, budget.remainingBytes - Math.max(0, byteCount));
}

function jsonStringUtf8ByteLength(value: string): number {
  let byteCount = 2;
  for (let index = 0; index < value.length; index++) {
    const characterCode = value.charCodeAt(index);
    if (
      characterCode === 0x22 ||
      characterCode === 0x5c ||
      characterCode === 0x08 ||
      characterCode === 0x09 ||
      characterCode === 0x0a ||
      characterCode === 0x0c ||
      characterCode === 0x0d
    ) {
      byteCount += 2;
    } else if (characterCode < 0x20) {
      byteCount += 6;
    } else if (characterCode < 0x80) {
      byteCount += 1;
    } else if (characterCode < 0x800) {
      byteCount += 2;
    } else if (characterCode >= 0xd800 && characterCode <= 0xdbff) {
      const nextCharacterCode = value.charCodeAt(index + 1);
      if (nextCharacterCode >= 0xdc00 && nextCharacterCode <= 0xdfff) {
        byteCount += 4;
        index++;
      } else {
        byteCount += 6;
      }
    } else if (characterCode >= 0xdc00 && characterCode <= 0xdfff) {
      byteCount += 6;
    } else {
      byteCount += 3;
    }
  }
  return byteCount;
}

function utf8ByteLength(value: string): number {
  let byteCount = 0;
  for (let index = 0; index < value.length; index++) {
    const characterCode = value.charCodeAt(index);
    if (characterCode < 0x80) {
      byteCount += 1;
    } else if (characterCode < 0x800) {
      byteCount += 2;
    } else if (characterCode >= 0xd800 && characterCode <= 0xdbff) {
      const nextCharacterCode = value.charCodeAt(index + 1);
      if (nextCharacterCode >= 0xdc00 && nextCharacterCode <= 0xdfff) {
        byteCount += 4;
        index++;
      } else {
        byteCount += 3;
      }
    } else {
      byteCount += 3;
    }
  }
  return byteCount;
}
