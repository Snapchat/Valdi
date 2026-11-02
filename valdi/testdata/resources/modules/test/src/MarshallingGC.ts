const PADDING = 'x'.repeat(64);

function expectedElement(slot: number, index: number): string {
  return `v${slot}_${index}_${PADDING}`;
}

function checkArray(slot: number, value: unknown, elementsCount: number): string | undefined {
  if (!Array.isArray(value)) {
    return `slot ${slot}: expected an array, got ${typeof value}`;
  }
  if (value.length !== elementsCount) {
    return `slot ${slot}: expected ${elementsCount} elements, got ${value.length}`;
  }
  for (let i = 0; i < elementsCount; i++) {
    const expected = expectedElement(slot, i);
    if (value[i] !== expected) {
      return `slot ${slot}[${i}]: expected '${expected}', got '${String(value[i])}'`;
    }
  }
  return undefined;
}

export function checkArguments(elementsCount: number, ...args: unknown[]): string {
  for (let i = 0; i < args.length; i++) {
    const error = checkArray(i, args[i], elementsCount);
    if (error) {
      return error;
    }
  }
  return `ok:${args.length}`;
}

export function checkObject(elementsCount: number, propertiesCount: number, object: Record<string, unknown>): string {
  for (let i = 0; i < propertiesCount; i++) {
    const error = checkArray(i, object[`p${i}`], elementsCount);
    if (error) {
      return error;
    }
  }
  return `ok:${propertiesCount}`;
}
