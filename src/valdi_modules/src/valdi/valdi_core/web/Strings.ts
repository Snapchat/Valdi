export function getLocalizedString(bundle: string, key: string): string | undefined {
  console.log({ getLocalizedString: 'getLocalizedString', bundle, key });
  return `${bundle}.${key}`;
}
