import { AttributedTextNative } from './AttributedTextNative';

/**
 * @ExportModel({ios: 'SCValdiDrawingFontSpecs', android: 'com.snap.valdi.modules.drawing.FontSpecs'})
 */
export interface FontSpecs {
  font: string;
  lineHeight?: number;
}

/**
 * @ExportModel({ios: 'SCValdiDrawingSize', android: 'com.snap.valdi.modules.drawing.Size'})
 */
export interface Size {
  width: number;
  height: number;
}

/**
 * @ExportEnum({ios: 'SCValdiDrawingFontWeight', android: 'com.snap.valdi.modules.drawing.FontWeight'})
 */
export const enum FontWeight {
  LIGHT = 'light',
  NORMAL = 'normal',
  MEDIUM = 'medium',
  DEMI_BOLD = 'demi-bold',
  BOLD = 'bold',
  BLACK = 'black',
}

/**
 * @ExportEnum({ios: 'SCValdiDrawingFontStyle', android: 'com.snap.valdi.modules.drawing.FontStyle'})
 */
export const enum FontStyle {
  NORMAL = 'normal',
  ITALIC = 'italic',
}

/**
 * @ExportProxy({ios: 'SCValdiDrawingFont', android: 'com.snap.valdi.modules.drawing.Font'})
 */
export interface Font {
  measureText(text: string, maxWidth?: number, maxHeight?: number, maxLines?: number): Size;
  measureAttributedText(
    attributedText: AttributedTextNative,
    maxWidth?: number,
    maxHeight?: number,
    maxLines?: number,
  ): Size;
}

/**
 * @ExportProxy({android: 'com.snap.valdi.modules.drawing.DrawingModule', ios: 'SCValdiDrawingModule'})
 */
export interface DrawingModule {
  getFont(specs: FontSpecs): Font;
  isFontRegistered(fontName: string): boolean;
  registerFont(fontName: string, weight: FontWeight, style: FontStyle, filename: string): void;
}

export interface DrawingModuleProvider {
  // eslint-disable-next-line @typescript-eslint/naming-convention
  Drawing: DrawingModule;
}
