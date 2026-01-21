import { getModuleLoader } from '../ModuleLoaderGlobal';

// We rely on a native number formatter to be set in Android to get around `toLocaleString()` being unavailable in quickjs.
let nativeNumberFormatter: ((value: number, fractionalDigits: number) => string) | undefined = undefined;

try {
  nativeNumberFormatter = getModuleLoader().load('NumberFormatting').formatNumber;
} catch (e: any) {
  // no-op; this module is only present on Android
}

// We rely on a native currency formatter to be set in Android to get around `Intl.NumberFormat` being unavailable in quickjs.
let nativeNumberWithCurrencyFormatter: ((value: number, currencyCode: string, minimumFractionDigits?: number, maximumFractionDigits?: number, localeIdentifier?: string) => string) | undefined = undefined;

try {
  nativeNumberWithCurrencyFormatter = getModuleLoader().load('NumberFormatting').formatNumberWithCurrency;
} catch (e: any) {
  // no-op; this module is only present on Android
}

export function formatNumber(value: number, numFractionalDigits: number = -1): string {
  if (nativeNumberFormatter) {
    return nativeNumberFormatter(value, numFractionalDigits);
  }

  let options: any;
  if (numFractionalDigits !== -1) {
    options = {
      minimumFractionDigits: numFractionalDigits,
      maximumFractionDigits: numFractionalDigits,
    };
  }

  return value.toLocaleString(undefined, options);
}

/**
 * Sanitizes a locale identifier to be compatible with Intl.NumberFormat.
 *
 * Handles various input formats:
 * - [language] (e.g., "en")
 * - [language]_[region] (e.g., "en_US")
 * - [language]-[script] (e.g., "zh-Hans")
 * - [language]-[script]_[region] (e.g., "zh-Hans_HK")
 * - Any of the above with @[extension] suffix (e.g., "en_GB@rg=US")
 *
 * Returns a BCP 47 compliant locale string (uses hyphens as separators).
 */
function sanitizeLocaleIdentifier(localeIdentifier: string): string {
  // Remove custom extensions (e.g., "@rg=US")
  const withoutExtension = localeIdentifier.split('@')[0];

  // Convert underscores to hyphens for BCP 47 compatibility
  // Apple uses underscore before region (e.g., "en_US"), but Intl expects hyphens ("en-US")
  return withoutExtension.replace(/_/g, '-');
}

export function formatNumberWithCurrency(
  value: number,
  currencyCode: string,
  options?: { minimumFractionDigits?: number; maximumFractionDigits?: number; localeIdentifier?: string },
): string {
  if (nativeNumberWithCurrencyFormatter) {
    return nativeNumberWithCurrencyFormatter(value, currencyCode, options?.minimumFractionDigits, options?.maximumFractionDigits, options?.localeIdentifier);
  }

  // Use the provided localeIdentifier if available, otherwise fall back to undefined (device locale)
  // This ensures currency formatting uses the correct locale from the price data.
  const locale = options?.localeIdentifier ? sanitizeLocaleIdentifier(options.localeIdentifier) : undefined;

  return new Intl.NumberFormat(locale, {
    style: 'currency',
    currency: currencyCode,
    minimumFractionDigits: options?.minimumFractionDigits,
    maximumFractionDigits: options?.maximumFractionDigits,
  }).format(value);
}

/**
 * Convenience function to have javascript numbers converted to Long in native
 */
export function serializeLong(value: number | Long): Long {
  if (typeof value === 'number') {
    return Long.fromNumber(value);
  }
  return value;
}
