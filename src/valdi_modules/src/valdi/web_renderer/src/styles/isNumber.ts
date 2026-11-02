export function isNumber(value: unknown): boolean {
  if (typeof value === 'number') {
    return !Number.isNaN(value);
  }

  if (typeof value !== 'string') {
    return false;
  }

  return !Number.isNaN(Number(value));
}
