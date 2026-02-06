import { DeviceCancelable, DeviceHapticFeedbackType, DeviceSystemType } from "../src/DeviceBridge";


const noopCancelable: DeviceCancelable = { cancel() {} };

// --- helpers ---

const isBrowser = typeof window !== 'undefined' && typeof document !== 'undefined';

const ua = isBrowser ? navigator.userAgent : '';
const isIOS =
  /\b(iPad|iPhone|iPod)\b/.test(ua) ||
  // iPadOS 13+ identifies as Mac with touch
  (/\bMacintosh\b/.test(ua) && isBrowser && 'ontouchend' in window);
const isAndroid = /\bAndroid\b/i.test(ua);

const DPR = isBrowser && window.devicePixelRatio ? window.devicePixelRatio : 1;

function on<K extends keyof WindowEventMap>(
  type: K,
  fn: (ev: WindowEventMap[K]) => void
): DeviceCancelable {
  if (!isBrowser) return noopCancelable;
  window.addEventListener(type, fn as EventListener);
  return { cancel: () => window.removeEventListener(type, fn as EventListener) };
}

function getSafeAreaInset(side: 'top' | 'right' | 'bottom' | 'left'): number {
  if (!isBrowser || !window.getComputedStyle) return 0;
  // Read CSS env(safe-area-inset-*) via a temporary element
  const el = document.createElement('div');
  el.style.cssText = `
    position: absolute;
    ${side}: 0;
    width: 0;height: 0;
    padding-${side}: env(safe-area-inset-${side});
    visibility: hidden;
  `;
  document.body.appendChild(el);
  const px = getComputedStyle(el).getPropertyValue(`padding-${side}`).trim();
  el.remove();
  const n = parseFloat(px || '0');
  return Number.isFinite(n) ? n : 0;
}

// --- API ---

export function copyToClipBoard(text: string): void {
  if (!isBrowser) return;
  if (navigator.clipboard?.writeText) {
    navigator.clipboard.writeText(text).catch(() => {
      /* ignore */
    });
  } else {
    // Fallback
    const el = document.createElement('textarea');
    el.value = text;
    el.setAttribute('readonly', '');
    el.style.position = 'fixed';
    el.style.opacity = '0';
    document.body.appendChild(el);
    el.select();
    try {
      document.execCommand('copy');
    } catch {
      /* ignore */
    }
    document.body.removeChild(el);
  }
}

export function getSystemType(): DeviceSystemType {
  if (isIOS) return DeviceSystemType.IOS;
  if (isAndroid) return DeviceSystemType.ANDROID;
  // default heuristic: desktops → pretend iOS/Android? pick IOS arbitrarily.
  return DeviceSystemType.IOS;
}

export function getSystemVersion(): string {
  if (!isBrowser) return '0.0';
  // Basic UA scrape (best-effort)
  const mIOS = ua.match(/OS (\d+[_\.\d]*) like Mac OS X/);
  if (mIOS) return mIOS[1].replace(/_/g, '.');
  const mAndroid = ua.match(/Android (\d+([._]\d+)*)/i);
  if (mAndroid) return mAndroid[1].replace(/_/g, '.');
  return '0.0';
}

export function getModel(): string {
  // Browsers generally don’t expose model; return empty or generic
  if (!isBrowser) return '';
  if (isIOS) return 'iOS Device';
  if (isAndroid) return 'Android Device';
  return 'Desktop';
}

// Physical display size in CSS pixels * devicePixelRatio (approx)
export function getDisplayWidth(): number {
  if (!isBrowser) return 0;
  return Math.round((window.screen?.width || window.innerWidth || 0) * DPR);
}

export function getDisplayHeight(): number {
  if (!isBrowser) return 0;
  return Math.round((window.screen?.height || window.innerHeight || 0) * DPR);
}

export function getDisplayScale(): number {
  return DPR;
}

export function getWindowWidth(): number {
  if (!isBrowser) return 0;
  return Math.max(0, Math.floor(window.innerWidth));
}

export function getWindowHeight(): number {
  if (!isBrowser) return 0;
  return Math.max(0, Math.floor(window.innerHeight));
}

export function getDisplayLeftInset(): number {
  return isBrowser ? getSafeAreaInset('left') : 0;
}

export function getDisplayRightInset(): number {
  return isBrowser ? getSafeAreaInset('right') : 0;
}

export function getDisplayTopInset(): number {
  return isBrowser ? getSafeAreaInset('top') : 0;
}

export function getDisplayBottomInset(): number {
  return isBrowser ? getSafeAreaInset('bottom') : 0;
}

export function getDeviceLocales(): string[] {
  if (!isBrowser) return [];
  const langs = (navigator as any).languages as string[] | undefined;
  const lang = navigator.language;
  const arr = langs && langs.length ? langs : (lang ? [lang] : []);
  return arr.map(s => s.replace('_', '-'));
}

export function getLocaleUsesMetricSystem(): boolean {
  // Non-metric: US, LR, MM
  const NON_METRIC = new Set(['US', 'LR', 'MM']);
  const locale = getDeviceLocales()[0] || 'en-US';
  try {
    // Try Intl.Locale for region
    // @ts-ignore - older TS may not have Intl.Locale in lib
    const loc = (Intl as any).Locale ? new (Intl as any).Locale(locale) : null;
    const region = loc?.maximized?.region || loc?.region || locale.split('-')[1];
    if (!region) return true;
    return !NON_METRIC.has(String(region).toUpperCase());
  } catch {
    return true;
  }
}

export function getTimeZoneName(): string {
  if (!isBrowser) return 'UTC';
  return Intl.DateTimeFormat().resolvedOptions().timeZone || 'UTC';
}

export function getTimeZoneRawSecondsFromGMT(timeZoneName: string | undefined): number {
  // Stub: return the current zone’s *standard* offset approx (ignoring DST).
  try {
    const now = new Date();
    // Try to approximate “standard” by checking January (likely standard time)
    const jan = new Date(now.getFullYear(), 0, 1);
    const fmt = new Intl.DateTimeFormat('en-US', {
      timeZone: timeZoneName || getTimeZoneName(),
      timeZoneName: 'shortOffset',
    });
    // e.g., "GMT-7" → parse
    const parts = fmt.formatToParts(jan);
    const off = parts.find(p => p.type === 'timeZoneName')?.value || 'GMT+0';
    const m = off.match(/GMT([+-])(\d{1,2})(?::(\d{2}))?/i);
    if (!m) return 0;
    const sign = m[1] === '-' ? -1 : 1;
    const h = parseInt(m[2], 10);
    const mins = m[3] ? parseInt(m[3], 10) : 0;
    return sign * (h * 3600 + mins * 60);
  } catch {
    return -new Date().getTimezoneOffset() * 60;
  }
}

export function getTimeZoneDstSecondsFromGMT(timeZoneName: string | undefined): number {
  // Stub: difference between “now” and “standard” offsets for the timezone.
  try {
    const std = getTimeZoneRawSecondsFromGMT(timeZoneName);
    const fmt = new Intl.DateTimeFormat('en-US', {
      timeZone: timeZoneName || getTimeZoneName(),
      timeZoneName: 'shortOffset',
    });
    const parts = fmt.formatToParts(new Date());
    const off = parts.find(p => p.type === 'timeZoneName')?.value || 'GMT+0';
    const m = off.match(/GMT([+-])(\d{1,2})(?::(\d{2}))?/i);
    if (!m) return 0;
    const sign = m[1] === '-' ? -1 : 1;
    const h = parseInt(m[2], 10);
    const mins = m[3] ? parseInt(m[3], 10) : 0;
    const cur = sign * (h * 3600 + mins * 60);
    return cur - std;
  } catch {
    return 0;
  }
}

const startTime = Date.now();
export function getUptimeMs(): number {
  if (typeof performance !== 'undefined' && performance.now) return performance.now();
  return Date.now() - startTime;
}

export function performHapticFeedback(_hapticType: DeviceHapticFeedbackType): void {
  // Browsers don’t have rich haptics; vibrate if available.
  if (isBrowser && navigator.vibrate) {
    navigator.vibrate(10);
  }
}

export function observeDisplayInsetChange(observe: () => void): DeviceCancelable {
  // Approximate: fire on resize and orientation changes
  const c1 = on('resize', observe);
  const c2 = on('orientationchange' as any, observe as any);
  return { cancel: () => { c1.cancel(); c2.cancel(); } };
}

export function observeDisplaySizeChange(observe: () => void): DeviceCancelable {
  return on('resize', observe);
}

export function observeDarkMode(observe: (isDarkMode: boolean) => void): DeviceCancelable {
  if (!isBrowser || !window.matchMedia) return noopCancelable;
  const mq = window.matchMedia('(prefers-color-scheme: dark)');
  const handler = () => {
    try { observe(mq.matches); } catch { /* ignore */ }
  };
  handler();
  mq.addEventListener?.('change', handler);
  // Safari < 14 fallback
  // @ts-ignore
  mq.addListener?.(handler);
  return {
    cancel: () => {
      mq.removeEventListener?.('change', handler);
      // @ts-ignore
      mq.removeListener?.(handler);
    }
  };
}

export function isDesktop(): boolean {
  if (!isBrowser) return true;
  return !(isIOS || isAndroid);
}

export function isWeb(): boolean {
  return isBrowser;
}

// On web there is no native back button; some apps emulate via history.
// Keep undefined to honor your API shape.
export const setBackButtonObserver: ((observer: (() => boolean) | undefined) => void) | undefined = undefined;