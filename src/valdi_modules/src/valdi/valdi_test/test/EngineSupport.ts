import { Device } from 'valdi_core/src/Device';

/**
 * True when the JS engine implements locale-aware casing. Some engines (the one
 * the internal Linux CI runs) return the input of toLocaleUpperCase/LowerCase
 * unchanged, so locale casing specs can't pass there.
 */
export const supportsLocaleCasing = 'A'.toLocaleLowerCase() === 'a' && 'a'.toLocaleUpperCase() === 'A';

/** True when the host reports at least one device locale (CI hosts may report none). */
export function hasDeviceLocales(): boolean {
  return Device.getDeviceLocales().length > 0;
}
