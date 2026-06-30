export const SYSTEM_FONT_NAME = 'system';
export const SYSTEM_MEDIUM_FONT_NAME = 'system-medium';
export const SYSTEM_SEMIBOLD_FONT_NAME = 'system-semibold';
export const SYSTEM_BOLD_FONT_NAME = 'system-bold';
export const SYSTEM_ITALIC_FONT_NAME = 'system-italic';
export const SYSTEM_MEDIUM_ITALIC_FONT_NAME = 'system-medium-italic';
export const SYSTEM_SEMIBOLD_ITALIC_FONT_NAME = 'system-semibold-italic';
export const SYSTEM_BOLD_ITALIC_FONT_NAME = 'system-bold-italic';

function makeFont(fontName: string, size: number): string {
  return `${fontName} ${size}`;
}

/**
 * Returns a font name for the system font
 * with the given size. The return value is suitable
 * for use as a "font" attribute of <label>, <textview>
 * or <textfield> elements.
 */
export function systemFont(size: number): string {
  return makeFont(SYSTEM_FONT_NAME, size);
}

/**
 * Returns a font name for the medium-weight system font
 * with the given size. The return value is suitable
 * for use as a "font" attribute of <label>, <textview>
 * or <textfield> elements.
 */
export function systemMediumFont(size: number): string {
  return makeFont(SYSTEM_MEDIUM_FONT_NAME, size);
}

/**
 * Returns a font name for the semibold system font
 * with the given size. The return value is suitable
 * for use as a "font" attribute of <label>, <textview>
 * or <textfield> elements.
 */
export function systemSemiboldFont(size: number): string {
  return makeFont(SYSTEM_SEMIBOLD_FONT_NAME, size);
}

/**
 * Returns a font name for the bold system font
 * with the given size. The return value is suitable
 * for use as a "font" attribute of <label>, <textview>
 * or <textfield> elements.
 */
export function systemBoldFont(size: number): string {
  return makeFont(SYSTEM_BOLD_FONT_NAME, size);
}

/**
 * Returns a font name for the italic system font
 * with the given size. The return value is suitable
 * for use as a "font" attribute of <label>, <textview>
 * or <textfield> elements.
 */
export function systemItalicFont(size: number): string {
  return makeFont(SYSTEM_ITALIC_FONT_NAME, size);
}

/**
 * Returns a font name for the medium italic system font
 * with the given size. The return value is suitable
 * for use as a "font" attribute of <label>, <textview>
 * or <textfield> elements.
 */
export function systemMediumItalicFont(size: number): string {
  return makeFont(SYSTEM_MEDIUM_ITALIC_FONT_NAME, size);
}

/**
 * Returns a font name for the semibold italic system font
 * with the given size. The return value is suitable
 * for use as a "font" attribute of <label>, <textview>
 * or <textfield> elements.
 */
export function systemSemiboldItalicFont(size: number): string {
  return makeFont(SYSTEM_SEMIBOLD_ITALIC_FONT_NAME, size);
}

/**
 * Returns a font name for the bold italic system font
 * with the given size. The return value is suitable
 * for use as a "font" attribute of <label>, <textview>
 * or <textfield> elements.
 */
export function systemBoldItalicFont(size: number): string {
  return makeFont(SYSTEM_BOLD_ITALIC_FONT_NAME, size);
}
