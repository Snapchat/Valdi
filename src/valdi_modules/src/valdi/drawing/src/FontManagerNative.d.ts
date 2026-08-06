import { IFontProvider } from 'valdi_tsx/src/IFontProvider';
import { FontStyle, FontWeight } from './DrawingModuleProvider';

/**
 * @NativeClass({
 *   marshallAsUntyped: true,
 *   ios: 'SCValdiSnapDrawingFontManager', iosImportPrefix: 'valdi_core',
 *   android: 'com.snap.valdi.snapdrawing.SnapDrawingFontManager'
 * })
 */
export interface IFontManagerNative extends IFontProvider {}

export function getDefaultFontManager(): IFontManagerNative;

export function makeScopedFontManager(fontManager: IFontManagerNative): IFontManagerNative;

export function registerFontFromData(
  fontManager: IFontManagerNative,
  fontName: string,
  weight: FontWeight,
  style: FontStyle,
  fontData: Uint8Array,
  canUseAsFallback?: boolean,
): void;

export function registerFontFromFilePath(
  fontManager: IFontManagerNative,
  fontName: string,
  weight: FontWeight,
  style: FontStyle,
  filePath: string,
  canUseAsFallback?: boolean,
): void;
