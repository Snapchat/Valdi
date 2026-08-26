/** Match development query flags without requiring browser-only URL globals in the Valdi runtime. */
export function hasWebLocationQueryParameter(
  locationSearch: string | undefined,
  parameterName: string,
  expectedValue: string,
): boolean {
  if (!locationSearch) {
    return false;
  }

  const query = locationSearch.charAt(0) === '?' ? locationSearch.slice(1) : locationSearch;
  for (const parameter of query.split('&')) {
    const separatorIndex = parameter.indexOf('=');
    if (separatorIndex < 0 || parameter.slice(0, separatorIndex) !== parameterName) {
      continue;
    }
    return parameter.slice(separatorIndex + 1) === expectedValue;
  }

  return false;
}
