import {
  DeviceCancelable,
  DeviceHapticFeedbackType,
  DeviceSystemType,
  getDeviceLocales,
  getDisplayBottomInset,
  getDisplayHeight,
  getDisplayLeftInset,
  getDisplayRightInset,
  getDisplayScale,
  getDisplayTopInset,
  getDisplayWidth,
  getLocaleUsesMetricSystem,
  getModel,
  getSystemType,
  getUptimeMs as nativeGetUptimeMs,
  getTimeZoneRawSecondsFromGMT as nativeGetTimeZoneRawSecondsFromGMT,
  getSystemVersion,
  getTimeZoneName,
  getWindowHeight,
  getWindowWidth,
  getTimeZoneDstSecondsFromGMT as nativeGetTimeZoneDstSecondsFromGMT,
  isDesktop,
  isWeb,
  observeDarkMode as nativeObserveDarkMode,
  observeDisplaySizeChange as nativeObserveDisplaySizeChange,
  observeDisplayInsetChange as nativeObserveDisplayInsetChange,
  copyToClipBoard as nativeCopyToClipBoard,
  performHapticFeedback as nativePerformHapticFeedback,
  setBackButtonObserver as nativeSetBackButtonObserver
} from 'valdi_core/src/DeviceBridge';

/**
 * This lazy cache can be invalidated
 */
class DeviceCache<T> {
  private value?: T;
  constructor(private maker: () => T) {}
  get(): T {
    if (this.value === undefined) {
      this.value = this.maker();
    }
    return this.value;
  }
  invalidate() {
    this.value = undefined;
  }
}

/**
 * Caching values, will be invalidate by observables
 */
const cacheSystemType = new DeviceCache<DeviceSystemType>(getSystemType);
const cacheSystemVersion = new DeviceCache<string>(getSystemVersion);
const cacheModel = new DeviceCache<string>(getModel);
const cacheDisplayWidth = new DeviceCache<number>(getDisplayWidth);
const cacheDisplayHeight = new DeviceCache<number>(getDisplayHeight);
const cacheDisplayScale = new DeviceCache<number>(getDisplayScale);
const cacheWindowWidth = new DeviceCache<number>(getWindowWidth);
const cacheWindowHeight = new DeviceCache<number>(getWindowHeight);
const cacheDisplayLeftInset = new DeviceCache<number>(getDisplayLeftInset);
const cacheDisplayRightInset = new DeviceCache<number>(getDisplayRightInset);
const cacheDisplayTopInset = new DeviceCache<number>(getDisplayTopInset);
const cacheDisplayBottomInset = new DeviceCache<number>(getDisplayBottomInset);
const cacheDeviceLocales = new DeviceCache<string[]>(getDeviceLocales ?? (() => []));
const cacheLocaleUsesMetricSystem = new DeviceCache<boolean>(getLocaleUsesMetricSystem ?? (() => false));
const cacheTimeZoneName = new DeviceCache<string>(getTimeZoneName ?? (() => 'unknown'));
const cacheIsDesktop = new DeviceCache<boolean>(isDesktop ?? (() => false));
const cacheIsWeb = new DeviceCache<boolean>(isWeb ?? (() => false));
const cacheIsRTL = new DeviceCache<boolean>(computeIsRTL);

/**
 * ISO 639-1 language codes for right-to-left languages
 */
const RTL_LANGUAGES = ['ar', 'he', 'fa', 'ur', 'yi', 'ps', 'sd', 'ckb'];

/**
 * Computes whether the device's primary locale uses RTL layout
 */
function computeIsRTL(): boolean {
  const locales = getDeviceLocales?.() ?? [];
  if (locales.length === 0) return false;
  const primaryLanguage = locales[0].split('-')[0].toLowerCase();
  return RTL_LANGUAGES.includes(primaryLanguage);
}

/**
 * Dark mode last cached value
 */
let darkMode = false;

/**
 * TimeZone to seconds cache map
 */
const cacheTimeZoneRawSecondsFromGMT: { [timezone: string]: number | undefined } = {};
const cacheTimeZoneDstSecondsFromGMT: { [timezone: string]: number | undefined } = {};

/**
 * Observables to invalidate caches
 */
const observingInsets = nativeObserveDisplayInsetChange(() => {
  cacheDisplayWidth.invalidate();
  cacheDisplayHeight.invalidate();
  cacheWindowWidth.invalidate();
  cacheWindowHeight.invalidate();
  cacheDisplayScale.invalidate();
  cacheDisplayLeftInset.invalidate();
  cacheDisplayRightInset.invalidate();
  cacheDisplayTopInset.invalidate();
  cacheDisplayBottomInset.invalidate();
});

const observingDarkMode = nativeObserveDarkMode((isDark: boolean) => {
  darkMode = isDark;
});

/**
 * Returns a DeviceCancelable that can be only called once.
 */
function toSafeDeviceCancelable(cancelable: DeviceCancelable): DeviceCancelable {
  return {
    cancel: () => {
      if (cancelable.cancel) {
        cancelable.cancel();
      }
      cancelable.cancel = undefined as unknown as any;
    },
  };
}

/**
 * Public API for device calls
 */
export namespace Device {
  /**
   * Get whether the system is Android
   */
  export function isAndroid(): boolean {
    return cacheSystemType.get() === DeviceSystemType.ANDROID;
  }
  /**
   * Get whether the system is iOS
   */
  export function isIOS(): boolean {
    return cacheSystemType.get() === DeviceSystemType.IOS;
  }

  /**
   * Check whether the Device is running on a Desktop platform.
   */
  export function isDesktop() {
    return cacheIsDesktop.get();
  }

  /**
   * Check whether the Device is running on the Web platform.
   */
  export function isWeb(): boolean {
    return cacheIsWeb.get();
  }

  /**
   * Get whether the system is Android or iOS
   */
  export function getSystemType(): DeviceSystemType {
    return cacheSystemType.get();
  }

  /**
   * Copies text to clipboard
   */
  export function copyToClipBoard(text: string): void {
    if (nativeCopyToClipBoard) {
      nativeCopyToClipBoard(text);
    }
  }
  /**
   * Get the system version
   * e.g.: iOS10
   */
  export function getSystemVersion(): string {
    return cacheSystemVersion.get();
  }
  /**
   * Get the model of the device
   * e.g: iPhone7,1
   */
  export function getModel(): string {
    return cacheModel.get();
  }
  /**
   * Get the display width in density dependent pixels.
   * To get the actual size in pixel, multiply that value
   * by the display scale.
   */
  export function getDisplayWidth(): number {
    return cacheDisplayWidth.get();
  }
  /**
   * Get the display height in density dependent pixels.
   * To get the actual size in pixel, multiply that value
   * by the display scale.
   */
  export function getDisplayHeight(): number {
    return cacheDisplayHeight.get();
  }
  /**
   * Get the display scale ratio. Used to convert
   * between dp and pixels.
   */
  export function getDisplayScale(): number {
    return cacheDisplayScale.get();
  }

  /**
   * Get the window width in density dependent pixels.
   * To get the actual size in pixel, multiply that value
   * by the display scale. All side bars are included in this value.
   * In multitasking/splitscreen mode, this reports the dimensions of the
   * window, not the dimensions of the physical screen.
   */
  export function getWindowWidth(): number {
    return cacheWindowWidth.get();
  }
  /**
   * Get the real screen height in density dependent pixels.
   * To get the actual size in pixel, multiply that value
   * by the display scale. All status/navigation bars are included in this value.
   * In multitasking/splitscreen mode, this reports the dimensions of the
   * window, not the dimensions of the physical screen.
   */
  export function getWindowHeight(): number {
    return cacheWindowHeight.get();
  }

  /**
   * Get the inset of the display. Some systems
   * will render the app under some system provided
   * transluscent elements or even hardware elements
   * (like the iPhone X).
   *
   * WARNING:
   *   Consider using <WithInsets/> or Device.observeDisplayInsetChange() instead
   *   To ensure the inset size is correct at all times as this is a dynamic value
   */
  export function getDisplayLeftInset(): number {
    return cacheDisplayLeftInset.get();
  }
  /**
   * Get the inset of the display. Some systems
   * will render the app under some system provided
   * transluscent elements or even hardware elements
   * (like the iPhone X).
   *
   * WARNING:
   *   Consider using <WithInsets/> or Device.observeDisplayInsetChange() instead
   *   To ensure the inset size is correct at all times as this is a dynamic value
   */
  export function getDisplayRightInset(): number {
    return cacheDisplayRightInset.get();
  }
  /**
   * Get the inset of the display. Some systems
   * will render the app under some system provided
   * transluscent elements or even hardware elements
   * (like the iPhone X).
   *
   * WARNING:
   *   Consider using <WithInsets/> or Device.observeDisplayInsetChange() instead
   *   To ensure the inset size is correct at all times as this is a dynamic value
   */
  export function getDisplayTopInset(): number {
    return cacheDisplayTopInset.get();
  }
  /**
   * Get the inset of the display. Some systems
   * will render the app under some system provided
   * transluscent elements or even hardware elements
   * (like the iPhone X).
   *
   * WARNING:
   *   Consider using <WithInsets/> or Device.observeDisplayInsetChange() instead
   *   To ensure the inset size is correct at all times as this is a dynamic value
   */
  export function getDisplayBottomInset(): number {
    return cacheDisplayBottomInset.get();
  }

  /**
   * List of preferred locales in ISO639-1 language format
   * with an ISO3166-1 alpha-2 country code, separated by a hyphen
   * ex:
   * ["en-US", 'fr-FR']
   */
  export function getDeviceLocales(): string[] {
    return cacheDeviceLocales.get();
  }
  /**
   * Indicates whether the device's current locale uses the metric system
   */
  export function getLocaleUsesMetricSystem(): boolean {
    return cacheLocaleUsesMetricSystem.get();
  }

  /**
   * Check if the device's primary locale uses right-to-left (RTL) layout direction.
   * This is determined by checking if the primary language code is in the list of
   * known RTL languages (Arabic, Hebrew, Persian, Urdu, etc.)
   */
  export function isRTL(): boolean {
    return cacheIsRTL.get();
  }

  /**
   * Read the device's current timezone standard name
   */
  export function getTimeZoneName(): string {
    // TODO(1102) - handle changes to the current timezone
    return cacheTimeZoneName.get();
  }

  /**
   * Returns the given time zone's offset from GMT in seconds. (EXCLUDING daylight savings)
   * @param timeZoneName - the name of the time zone to query (Uses the local time zone if undefined)
   */
  export function getTimeZoneRawSecondsFromGMT(timeZoneName?: string): number {
    // TODO(1102) - handle changes to the current timezone
    const cacheKey = timeZoneName ?? '';
    let cacheValue = cacheTimeZoneRawSecondsFromGMT[cacheKey];
    if (cacheValue === undefined) {
      if (nativeGetTimeZoneRawSecondsFromGMT) {
        cacheValue = nativeGetTimeZoneRawSecondsFromGMT(timeZoneName);
      } else {
        console.error('Device.getTimeZoneRawSecondsFromGMT not implemented');
        cacheValue = 0;
      }
    }
    return cacheValue;
  }

  /**
   * Returns the given time zone's offset from GMT in seconds. (INCLUDING daylight savings)
   * @param timeZoneName - the name of the time zone to query (Uses the local time zone if undefined)
   */
  export function getTimeZoneDstSecondsFromGMT(timeZoneName?: string): number {
    // TODO(1102) - handle changes to the current timezone 
    const cacheKey = timeZoneName ?? '';
    let cacheValue = cacheTimeZoneDstSecondsFromGMT[cacheKey];
    if (cacheValue === undefined) {
      if (nativeGetTimeZoneDstSecondsFromGMT) {
        cacheValue = nativeGetTimeZoneDstSecondsFromGMT(timeZoneName);
      } else {
        console.error('Device.getTimeZoneDstSecondsFromGMT not implemented');
        cacheValue = 0;
      }
    }
    return cacheValue;
  }

  /**
   * Get current value of the device's uptime clock in milliseconds which measures
   * the time since boot, excluding time spent while the device is asleep.
   * On iOS, this clock is equivalent to mach_absolute_time and CACurrentMediaTime.
   * On Android, this clock is equivalent to SystemClock.uptimeMillis().
   */
  export function getUptimeMs(): number {
    return nativeGetUptimeMs?.() ?? 0;
  }

  /**
   * Check if the device is currently using dark mode
   * checkout: Device.observeDarkMode instead if possible
   */
  export function isDarkMode(): boolean {
    return darkMode;
  }

  /**
   * Start listening to the changes in the display's insets
   * @param observe
   */
  export function observeDisplayInsetChange(observe: () => void): DeviceCancelable {
    if (nativeObserveDisplayInsetChange) {
      return toSafeDeviceCancelable(nativeObserveDisplayInsetChange(observe));
    } else {
      console.error('Device.observeDisplayInsetChange not implemented');
      observe();
      return {
        cancel: () => {},
      };
    }
  }

  /**
   * Start listening to the changes in the display's size
   * @param observe
   */
  export function observeDisplaySizeChange(observe: () => void): DeviceCancelable {
    if (nativeObserveDisplaySizeChange) {
      return toSafeDeviceCancelable(nativeObserveDisplaySizeChange(observe));
    } else {
      observe();
      return {
        cancel: () => {},
      };
    }
  }

  /**
   * Start listening to the device's dark mode changes
   * @param observe
   */
  export function observeDarkMode(observe: (isDarkMode: boolean) => void): DeviceCancelable {
    if (nativeObserveDarkMode) {
      return toSafeDeviceCancelable(nativeObserveDarkMode(observe));
    } else {
      console.error('Device.observeDarkMode not implemented');
      observe(false);
      return {
        cancel: () => {},
      };
    }
  }

  /**
   * Set a back button observer on the current displayed ValdiRootView.
   * Android only
   */
  export function setBackButtonObserver(backButtonObserver: (() => boolean) | undefined): void {
    if (nativeSetBackButtonObserver) {
      nativeSetBackButtonObserver(backButtonObserver);
    }
  }

  /**
   * Cannot be cached
   * Performs haptic feedback on the device
   */
  export function performHapticFeedback(hapticType: DeviceHapticFeedbackType): void {
    if (nativePerformHapticFeedback) {
      nativePerformHapticFeedback(hapticType);
    }
  }
}
