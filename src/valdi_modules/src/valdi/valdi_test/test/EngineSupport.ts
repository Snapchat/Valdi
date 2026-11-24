import { Device } from 'valdi_core/src/Device';

/**
 * True when the JS engine implements locale-aware casing. Some engines (the one
 * the internal Linux CI runs) return the input of toLocaleUpperCase/LowerCase
 * unchanged, so locale casing specs can't pass there.
 */
export const supportsLocaleCasing = 'A'.toLocaleLowerCase() === 'a' && 'a'.toLocaleUpperCase() === 'A';

/**
 * True when locale casing applies the locale's own rules. Some engines case correctly but
 * ignore the locale argument, so Turkic and Lithuanian specs can't pass there even though
 * supportsLocaleCasing is true.
 */
export const supportsLocaleSpecificCasing =
  supportsLocaleCasing &&
  'i'.toLocaleUpperCase('tr') === '\u0130' &&
  '\u00cc'.toLocaleLowerCase('lt') === 'i\u0307\u0300';

/** True when the host reports at least one device locale (CI hosts may report none). */
export function hasDeviceLocales(): boolean {
  return Device.getDeviceLocales().length > 0;
}
