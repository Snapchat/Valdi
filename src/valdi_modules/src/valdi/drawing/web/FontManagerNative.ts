import { IFontProvider } from 'valdi_tsx/src/IFontProvider';
import { FontStyle, FontWeight } from '../src/DrawingModuleProvider';
import { IFontManagerNative } from '../src/FontManagerNative';


type FontKey = `${string}|${FontWeight}|${FontStyle}`;

const defaultManager: IFontManagerNative = { __tag: 'IFontProvider' };
const registrations = new Map<
  FontKey,
  { source: 'data' | 'file'; payload: Uint8Array | string; canUseAsFallback: boolean }
>();

export function getDefaultFontManager(): IFontManagerNative {
  return defaultManager;
}

export function makeScopedFontManager(fontManager: IFontManagerNative): IFontManagerNative {
  // Stub: return the same instance; scope isolation not modeled here.
  return fontManager ?? defaultManager;
}

export function registerFontFromData(
  _fontManager: IFontManagerNative,
  fontName: string,
  weight: FontWeight,
  style: FontStyle,
  fontData: Uint8Array,
  canUseAsFallback = false,
): void {
  const key: FontKey = `${fontName}|${weight}|${style}`;
  registrations.set(key, { source: 'data', payload: fontData, canUseAsFallback });
}

export function registerFontFromFilePath(
  _fontManager: IFontManagerNative,
  fontName: string,
  weight: FontWeight,
  style: FontStyle,
  filePath: string,
  canUseAsFallback = false,
): void {
  const key: FontKey = `${fontName}|${weight}|${style}`;
  registrations.set(key, { source: 'file', payload: filePath, canUseAsFallback });
}