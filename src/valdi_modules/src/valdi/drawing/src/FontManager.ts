import { IFontProvider } from 'valdi_tsx/src/IFontProvider';
import { FontStyle, FontWeight } from './DrawingModuleProvider';
import {
  IFontManagerNative,
  getDefaultFontManager,
  makeScopedFontManager,
  registerFontFromData,
  registerFontFromFilePath,
} from './FontManagerNative';

let cachedDefault: FontManager | undefined = undefined;

export class FontManager {
  private constructor(readonly native: IFontManagerNative) {}

  /**
   * Return the IFontProvider instance suitable to pass as the `fontProvider`
   * attribute of the lottie element.
   */
  get fontProvider(): IFontProvider {
    return this.native;
  }

  /**
   * Returns the default font manager associated with the runtime
   */
  static getDefault(): FontManager {
    if (!cachedDefault) {
      cachedDefault = new FontManager(getDefaultFontManager());
    }
    return cachedDefault;
  }

  /**
   * Creates a scoped font manager from the given font manager instance.
   * Any registered fonts within the returned scoped font manager will impact
   * the scoped font manager only. The returned instance will inherit the fonts
   * from its parent font manager
   */
  makeScoped(): FontManager {
    return new FontManager(makeScopedFontManager(this.native));
  }

  /**
   * Registers a font into the font manager from bytes representing the font data
   *
   * Pass `canUseAsFallback` to also make the family eligible for per-character fallback, which is
   * how glyphs missing from the primary font get resolved — an emoji font must be registered this
   * way for emoji to render at all.
   */
  registerFontFromData(
    fontName: string,
    weight: FontWeight,
    style: FontStyle,
    fontData: Uint8Array,
    canUseAsFallback = false,
  ): void {
    registerFontFromData(this.native, fontName, weight, style, fontData, canUseAsFallback);
  }

  /**
   * Registers a font into the font manager from bytes from a file path
   *
   * See [registerFontFromData] for `canUseAsFallback`.
   */
  registerFontFromFilePath(
    fontName: string,
    weight: FontWeight,
    style: FontStyle,
    filePath: string,
    canUseAsFallback = false,
  ): void {
    registerFontFromFilePath(this.native, fontName, weight, style, filePath, canUseAsFallback);
  }
}
