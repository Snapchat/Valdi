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

function decodeWebLocationQueryComponent(component: string): string | undefined {
  try {
    return decodeURIComponent(component.replace(/\+/g, ' '));
  } catch {
    return undefined;
  }
}

/** Match one literal development flag, rejecting encoded aliases, malformed escapes, and duplicates. */
export function hasSingleWebLocationQueryParameter(
  locationSearch: string | undefined,
  parameterName: string,
  expectedValue: string,
): boolean {
  if (!locationSearch) {
    return false;
  }

  const query = locationSearch.charAt(0) === '?' ? locationSearch.slice(1) : locationSearch;
  let matchCount = 0;
  for (const parameter of query.split('&')) {
    const separatorIndex = parameter.indexOf('=');
    const rawName = separatorIndex < 0 ? parameter : parameter.slice(0, separatorIndex);
    const rawValue = separatorIndex < 0 ? undefined : parameter.slice(separatorIndex + 1);
    const decodedName = decodeWebLocationQueryComponent(rawName);
    const decodedValue = rawValue === undefined ? undefined : decodeWebLocationQueryComponent(rawValue);
    if (decodedName === undefined || (rawValue !== undefined && decodedValue === undefined)) {
      return false;
    }
    if (decodedName !== parameterName) {
      continue;
    }
    matchCount++;
    if (rawName !== parameterName || rawValue !== expectedValue) {
      return false;
    }
  }

  return matchCount === 1;
}
