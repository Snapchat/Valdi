export const enum DeviceSystemType {
  IOS = 'ios',
  ANDROID = 'android',
}

export const enum DeviceHapticFeedbackType {
  ACTION_SHEET = 'action_sheet',
  SELECTION = 'selection',
  VIBRATION = 'vibration',
}

export interface DeviceCancelable {
  cancel(): void;
}

export function copyToClipBoard(text: string): void;
export function getSystemType(): DeviceSystemType;
export function getSystemVersion(): string;
export function getModel(): string;
export function getDisplayWidth(): number;
export function getDisplayHeight(): number;
export function getDisplayScale(): number;
export function getWindowWidth(): number;
export function getWindowHeight(): number;
export function getDisplayLeftInset(): number;
export function getDisplayRightInset(): number;
export function getDisplayTopInset(): number;
export function getDisplayBottomInset(): number;
export function getDeviceLocales(): string[];
export function getLocaleUsesMetricSystem(): boolean;
export function getTimeZoneName(): string;
export function getTimeZoneRawSecondsFromGMT(timeZoneName: string | undefined): number;
export function getTimeZoneDstSecondsFromGMT(timeZoneName: string | undefined): number;
export function getUptimeMs(): number;
export function performHapticFeedback(hapticType: DeviceHapticFeedbackType): void;
export function observeDisplayInsetChange(observe: () => void): DeviceCancelable;
export function observeDisplaySizeChange(observe: () => void): DeviceCancelable;
export function observeDarkMode(observe: (isDarkMode: boolean) => void): DeviceCancelable;
export function isDesktop(): boolean;
export function isWeb(): boolean;
export const setBackButtonObserver: ((observer: (() => boolean) | undefined) => void) | undefined;
